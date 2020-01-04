/*
 * tegra_spi_i2s_pcm.c  --  ALSA Soc Audio Layer
 *
 * (c) 2010 Nvidia Graphics Pvt. Ltd.
 *  http://www.nvidia.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */


#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <mach/spi_i2s.h>
#include <linux/vmalloc.h>
#include "tegra_soc.h"

#define NUM_CHANNELS 2
#define PERIODS_MIN 4
#define PERIODS_MAX 8
#define PERIOD_BYTES 1600
#define NUM_SUBSTREAMS (SNDRV_PCM_STREAM_LAST + 1)
#define KTHREAD_IRQ_PRIO (MAX_RT_PRIO >> 1)
#define ONE_I2S_FRAME_TIME 140

enum {
	SPI_STOPPING,
	SPI_STOPPED,
	SPI_RUNNING,
	SPI_EXIT
};

static const struct snd_pcm_hardware tegra_spi_i2s_pcm_hardware = {
	.info = SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID,
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.channels_min = NUM_CHANNELS,
	.channels_max = NUM_CHANNELS,
	.buffer_bytes_max = (PERIODS_MAX * PERIOD_BYTES),
	.period_bytes_min = PERIOD_BYTES,
	.period_bytes_max = PERIOD_BYTES,
	.periods_min = PERIODS_MIN,
	.periods_max = PERIODS_MAX,
};

struct tegra_spi_i2s_runtime {
	spinlock_t spi_pcm_lock;
	unsigned char dir;
	unsigned char state;
	struct task_struct *task;
	unsigned int substream_pos[NUM_SUBSTREAMS];
	unsigned int substream_bytes[NUM_SUBSTREAMS];
	struct snd_pcm_substream *substreams[NUM_SUBSTREAMS];
};
static struct tegra_spi_i2s_runtime *spi_runtime;
static u64 tegra_dma_mask = DMA_BIT_MASK(32);
static int thread_create_internal(struct task_struct *task);
static 	struct tegra_spi_i2s_gpio  *gpio_spi, *gpio_i2s;
static unsigned int spi_i2s_timeout_ms;

/* Spi specific data structure */
#define SPI_DIR_TX 0x01
#define SPI_DIR_RX 0x02
#define SPI_TXN_LEN_BYTES PERIOD_BYTES
#define SPI_BITS_PER_WORD 32

static struct completion spi_txn_status;
static struct spi_message spi_msg;
static struct spi_transfer xfer;
static struct spi_device *spi_i2s_pcm;
static int spi_i2s_pcm_probe(struct spi_device *spi);
static int spi_slave_transaction(unsigned char *tx_buf, unsigned char *rx_buf, unsigned
		int bytes_requested);
static int spi_thread(void *args);
extern void spi_tegra_abort_transfer(struct spi_device *spi);

void spi_status(int *bytes_transferred)
{
	if (completion_done(&spi_txn_status)) {
		*bytes_transferred = SPI_TXN_LEN_BYTES;
		return;
	}
	spi_tegra_abort_transfer(spi_i2s_pcm);
	*bytes_transferred = spi_msg.actual_length * 4;
	return;
}

static void spidev_complete(void *arg)
{
	complete(arg);
}

static int spi_slave_transaction(unsigned char *tx_buf, unsigned char *rx_buf,
		unsigned int bytes_requested)
{
	int status;
	memset(&xfer, 0, sizeof(xfer));
	spi_message_init(&spi_msg);
	init_completion(&spi_txn_status);
	spi_msg.context = &spi_txn_status;
	spi_msg.complete = spidev_complete;
	xfer.tx_buf = tx_buf;
	xfer.rx_buf = rx_buf;
	xfer.bits_per_word = SPI_BITS_PER_WORD;
	xfer.len = bytes_requested;
	spi_message_add_tail(&xfer, &spi_msg);
	status = spi_async(spi_i2s_pcm, &spi_msg);
	return status;
}

static int thread_create_internal(struct task_struct *task)
{
	struct sched_param sched;
	int scheduler;

	task = kthread_create(spi_thread, NULL, "spi_i2s_thread");
	if(IS_ERR(task))
		return -ENOMEM;
	scheduler = SCHED_FIFO;
	sched.sched_priority = KTHREAD_IRQ_PRIO+1;
	if (sched_setscheduler_nocheck(task, scheduler, &sched) < 0)
		printk("Failed to set task priority to %d\n",
				sched.sched_priority);
	wake_up_process(task);
	return 0;
}

static void prepare_buffer(unsigned char *buff, unsigned char index,
		unsigned int period_size)
{
	unsigned int num_bytes;
	unsigned int pos = spi_runtime->substream_pos[index];
	struct snd_pcm_runtime *runtime = spi_runtime->substreams[index]->runtime;
	unsigned int buffer_size = frames_to_bytes(runtime, runtime->buffer_size);
	unsigned char *ptr = runtime->dma_area;

	if ((pos + period_size) > buffer_size) {
		num_bytes = buffer_size - pos;
		if (index == SNDRV_PCM_STREAM_PLAYBACK) {
			memcpy(buff, ptr + pos, num_bytes);
			memcpy(buff + num_bytes, ptr, period_size - num_bytes);
		} else {
			memcpy(ptr + pos, buff, num_bytes);
			memcpy(ptr, buff + num_bytes, period_size - num_bytes);
		}
	} else {
		if (index == SNDRV_PCM_STREAM_PLAYBACK)
			memcpy(buff, ptr + pos, period_size);
		else
			memcpy(ptr + pos, buff, period_size);
	}
}

static void update_stream(unsigned char index, unsigned int period_size,
		unsigned int bytes_transferred)
{
	struct snd_pcm_runtime *runtime = spi_runtime->substreams[index]->runtime;
	unsigned int buffer_size = frames_to_bytes(runtime, runtime->buffer_size);
	unsigned int pos_old = spi_runtime->substream_pos[index];
	unsigned int pos_new = (spi_runtime->substream_pos[index] + bytes_transferred) %
		buffer_size;
	unsigned int num_bytes = spi_runtime->substream_bytes[index];

	if (pos_new < pos_old)
		num_bytes += buffer_size - pos_old + pos_new;
	else
		num_bytes += pos_new - pos_old;

	spi_runtime->substream_pos[index] = pos_new;
	spi_runtime->substream_bytes[index] = num_bytes;
}

static void send_update(unsigned char index, unsigned int period_size)
{
	if (spi_runtime->substream_bytes[index] >= period_size) {
		snd_pcm_period_elapsed(spi_runtime->substreams[index]);
		spi_runtime->substream_bytes[index] %= period_size;
	}
}

unsigned int inverted_state(unsigned int state)
{
	return (~state & 0x01);
}

static int  spi_thread(void *args)
{
	int err = 0;
	bool first = true;
	bool tx_started = false;
	unsigned int period_size = PERIOD_BYTES;
	unsigned char *buff_tx = NULL;
	unsigned char *buff_rx = NULL;
	int bytes_transferred = 0;

	buff_rx = vmalloc(period_size);
	buff_tx = vmalloc(period_size);
	for (;;) {
		spin_lock(&spi_runtime->spi_pcm_lock);
		switch (spi_runtime->state) {

		case SPI_STOPPING:
			spi_status(&bytes_transferred);
			gpio_set_value(gpio_i2s->gpio_no, inverted_state(gpio_i2s->active_state));
			gpio_set_value(gpio_spi->gpio_no, inverted_state(gpio_spi->active_state));
			spi_runtime->state = SPI_STOPPED;
		case SPI_STOPPED:
			first = true;
			tx_started = false;
			spin_unlock(&spi_runtime->spi_pcm_lock);
			msleep(50);
			break;
		case SPI_EXIT:
			goto fail_lock;
			break;
		case SPI_RUNNING:
			if (!first) {
				gpio_set_value(gpio_spi->gpio_no, inverted_state(gpio_spi->active_state));
				/* i2s interface running at 256kHz and frame size is 32 bits. */
				/* Let one i2s frame to complete. */
				udelay(ONE_I2S_FRAME_TIME);
				spi_status(&bytes_transferred);
				if (bytes_transferred == 0)
					goto restart;
				if (spi_runtime->dir & SPI_DIR_RX) {
					prepare_buffer(buff_rx, SNDRV_PCM_STREAM_CAPTURE,
						period_size);
					update_stream(SNDRV_PCM_STREAM_CAPTURE, period_size,
						bytes_transferred);
				}

				if ((tx_started) && (spi_runtime->dir & SPI_DIR_TX))
					update_stream(SNDRV_PCM_STREAM_PLAYBACK, period_size,
						bytes_transferred);
			} else {
				/* Enable cpld i2s interface. */
				gpio_set_value(gpio_i2s->gpio_no, gpio_i2s->active_state);
				first = false;
			}

			if (spi_runtime->dir & SPI_DIR_TX) {
				if (!tx_started)
					tx_started = true;
				prepare_buffer(buff_tx, SNDRV_PCM_STREAM_PLAYBACK, period_size);
			} else
				memset(buff_tx, 0, period_size);
restart:
			err = spi_slave_transaction(buff_tx, buff_rx, SPI_TXN_LEN_BYTES);
			if (err) {
				printk("SPI-I2S PCM: spi transaction failed...\n");
				goto fail_lock;
			}
			/* Enable cpld spi interface */
			gpio_set_value(gpio_spi->gpio_no, gpio_spi->active_state);
			spin_unlock(&spi_runtime->spi_pcm_lock);
			if (spi_runtime->dir & SPI_DIR_RX)
				send_update(SNDRV_PCM_STREAM_CAPTURE, period_size);
			if (spi_runtime->dir & SPI_DIR_TX)
				send_update(SNDRV_PCM_STREAM_PLAYBACK, period_size);

			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule_timeout(msecs_to_jiffies(spi_i2s_timeout_ms));
			break;
		default:
			printk("SPI-I2S PCM: invalid state...\n");
			goto fail_lock;
			break;
		}
	}

fail_lock:
	spi_runtime->state = SPI_EXIT;
	goto done;
done:
	if (buff_rx)
		vfree(buff_rx);
	if (buff_tx)
		vfree(buff_tx);
	gpio_free(gpio_i2s->gpio_no);
	gpio_free(gpio_spi->gpio_no);
	spin_unlock(&spi_runtime->spi_pcm_lock);
	return err;

}

/* pcm related data structure */
static int tegra_spi_i2s_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tegra_runtime_data *runtime_data;
	runtime_data = kzalloc(sizeof(struct tegra_runtime_data), GFP_KERNEL);
	if (!runtime_data)
		return -ENOMEM;
	runtime_data->substream = substream;
	runtime->private_data = runtime_data;
	snd_soc_set_runtime_hwparams(substream, &tegra_spi_i2s_pcm_hardware);
	return 0;
}

static int tegra_spi_i2s_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (runtime->private_data)
		kfree(runtime->private_data);
	return 0;
}

static int tegra_spi_i2s_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	return 0;
}

static int tegra_spi_i2s_pcm_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}

static int tegra_spi_i2s_pcm_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static int tegra_spi_i2s_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		spin_lock(&spi_runtime->spi_pcm_lock);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			spi_runtime->dir |= SPI_DIR_TX;
		else
			spi_runtime->dir |= SPI_DIR_RX;
		spi_runtime->substreams[substream->stream] = substream;
		spi_runtime->substream_pos[substream->stream] = 0;
		spi_runtime->substream_bytes[substream->stream] = 0;
		if (spi_runtime->state == SPI_STOPPED)
			spi_runtime->state = SPI_RUNNING;
		spin_unlock(&spi_runtime->spi_pcm_lock);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		spin_lock(&spi_runtime->spi_pcm_lock);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			spi_runtime->dir &= ~SPI_DIR_TX;
		else
			spi_runtime->dir &= ~SPI_DIR_RX;

		if (!spi_runtime->dir)
			spi_runtime->state = SPI_STOPPING;
		spin_unlock(&spi_runtime->spi_pcm_lock);
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	default:
		printk("SPI-I2S PCM: invalid trigger!\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static snd_pcm_uframes_t tegra_spi_i2s_pcm_pointer(struct snd_pcm_substream *substream)
{
	snd_pcm_uframes_t size;
	struct snd_pcm_runtime *runtime = substream->runtime;
	spin_lock(&spi_runtime->spi_pcm_lock);
	size = bytes_to_frames(runtime, spi_runtime->substream_pos[substream->stream]);
	spin_unlock(&spi_runtime->spi_pcm_lock);
	return size;
}

static int tegra_pcm_mmap(struct snd_pcm_substream *substream,
		struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
			runtime->dma_area,
			runtime->dma_addr, runtime->dma_bytes);
}

static struct snd_pcm_ops tegra_spi_i2s_pcm_ops = {
	.open = tegra_spi_i2s_pcm_open,
	.close = tegra_spi_i2s_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = tegra_spi_i2s_pcm_hw_params,
	.hw_free = tegra_spi_i2s_pcm_hw_free,
	.prepare = tegra_spi_i2s_pcm_prepare,
	.trigger = tegra_spi_i2s_pcm_trigger,
	.pointer = tegra_spi_i2s_pcm_pointer,
	.mmap = tegra_pcm_mmap,
};

static int tegra_spi_i2s_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size =  tegra_spi_i2s_pcm_hardware.buffer_bytes_max;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
			&buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;

	buf->bytes = size;
	return 0;
}

static void tegra_spi_i2s_pcm_deallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;

	dma_free_writecombine(pcm->card->dev, buf->bytes,
			buf->area, buf->addr);
	buf->area = NULL;
}

static int tegra_spi_i2s_pcm_new(struct snd_card *card, struct snd_soc_dai *dai,
		struct snd_pcm *pcm)
{
	int ret = 0;
	spi_runtime = vmalloc(sizeof(struct tegra_spi_i2s_runtime));
	if (!spi_runtime)
		return -ENOMEM;
	memset(spi_runtime, 0, sizeof(struct tegra_spi_i2s_runtime));
	spi_runtime->state = SPI_STOPPED;
	if (!card->dev->dma_mask)
		card->dev->dma_mask = &tegra_dma_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = 0xffffffff;
	if (tegra_spi_i2s_pcm_preallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK))
		goto fail;
	if (tegra_spi_i2s_pcm_preallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE)){
		tegra_spi_i2s_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
		goto fail;
	}
	if (thread_create_internal(spi_runtime->task)){
		tegra_spi_i2s_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);
		tegra_spi_i2s_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
		goto fail;
	}
	spin_lock_init(&spi_runtime->spi_pcm_lock);
	goto done;
fail:
	printk("SPI-I2S PCM: pcm_new failed!\n");
	ret = -ENOMEM;
	if (spi_runtime) {
		vfree(spi_runtime);
		spi_runtime = NULL;
	}
done:
	return ret;
}

static void tegra_spi_i2s_pcm_free(struct snd_pcm *pcm)
{
	tegra_spi_i2s_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
	tegra_spi_i2s_pcm_deallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);
	if (spi_runtime) {
		if (spi_runtime->task) {
			spin_lock(&spi_runtime->spi_pcm_lock);
			spi_runtime->state = SPI_EXIT;
			spin_unlock(&spi_runtime->spi_pcm_lock);
			kthread_stop(spi_runtime->task);
		}
		vfree(spi_runtime);
		spi_runtime = NULL;
	}
}

struct snd_soc_platform tegra_soc_spi_i2s_platform = {
	.name = "tegra-audio-spi-i2s",
	.pcm_ops = &tegra_spi_i2s_pcm_ops,
	.pcm_new = tegra_spi_i2s_pcm_new,
	.pcm_free = tegra_spi_i2s_pcm_free,
};

EXPORT_SYMBOL_GPL(tegra_soc_spi_i2s_platform);

static struct spi_driver spi_i2s_pcm_driver = {
	.driver = {
		.name  = "spi_i2s_pcm",
		.bus   = &spi_bus_type,
		.owner = THIS_MODULE,
	},
	.probe = spi_i2s_pcm_probe,
};

static int spi_i2s_pcm_probe(struct spi_device *spi)
{
	struct tegra_spi_i2s_platform_data *pdata;

	spi_i2s_pcm = spi;
	pdata = spi_i2s_pcm->dev.platform_data;
	gpio_i2s = &(pdata->gpio_i2s);
	gpio_spi = &(pdata->gpio_spi);
	spi_i2s_timeout_ms = pdata->spi_i2s_timeout_ms;
	return 0;
}

static int __init spi_i2s_pcm_init(void)
{
	return spi_register_driver(&spi_i2s_pcm_driver);
}

static int __init tegra_soc_spi_i2s_platform_init(void)
{
	return snd_soc_register_platform(&tegra_soc_spi_i2s_platform);
}

module_init(tegra_soc_spi_i2s_platform_init);
module_init(spi_i2s_pcm_init);

static void __exit tegra_soc_spi_i2s_platform_exit(void)
{
	snd_soc_unregister_platform(&tegra_soc_spi_i2s_platform);
}

module_exit(tegra_soc_spi_i2s_platform_exit);

MODULE_DESCRIPTION("Tegra APXX SPI-I2S PCM module");
MODULE_LICENSE("GPL");
