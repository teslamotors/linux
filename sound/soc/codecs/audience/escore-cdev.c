/* escore-cdev.c -- Character device interface.
 *
 * Author: Marc Butler <mbutler@audience.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This interface is intended to be used during integration and
 * development.
 *
 * Currently only a single node is registered.
 *
 * COMMAND CHARACTER DEVICE
 * ========================
 * Implements a modestly robust "lucky charms" macro format
 * parser. That allows user to cat macros directly into the device
 * node.
 *
 * The macro format is line oriented. Here are some perl style regexes
 * to describe the format:
 *
 * - Comments: semicolon to end of line.
 * ;.*$
 *
 * - Preset identifier only one per line.
 * ^\s+!Preset\s+id:\s+(\d+)\s*(;.*)
 *
 * - Commands appear as pairs of 16 bit hex values each prefixed with 0x.
 * 0x[0-9a-f]{1,4}\s+0x[0-9a-f]{1,4}
 *
 * STREAMING CHARACTER DEVICE
 * ==========================
 * The streaming character device implements an interface allowing the
 * unified driver to output streaming data via a connected HW interface.
 * This data may be consumed by open/read/close operations on the character
 * device.  The driver expects all streaming configuration to be set via
 * another method, for example the command character device, before the
 * streaming cdev is opened.
 *
 * In general, the streaming node performs the following operations:
 * - open(): prepare the HW interface for streaming (if needed)
 *           begin streaming via escore_cmd API call
 * - producer kthread: services the HW interface and writes data into
 *                     a circular buffer in kernel space
 * - poll(): implemented so clients can use poll/epoll/select
 * - read(): reads data out of the circular buffer and copies the
 *           data to user space
 * - release/close(): stop the producer thread, stop streaming,
 *		      closes the HW interface (if needed),
 *                    free all resources, discard stale data
 *
 * If userspace does not read the data from the character device fast
 * enough, the producer thread will consume and overwrite the oldest
 * data in the circular buffer first.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/circ_buf.h>
#include <linux/slab.h>
#include <linux/atomic.h>

#include "escore.h"
#include "escore-uart-common.h"
/* Index offset for the 3 character devices types. */
enum {
	CDEV_COMMAND,
	CDEV_FIRMWARE,
	CDEV_STREAMING,
	CDEV_DATABLOCK,
	CDEV_DATALOGGING,
	CDEV_CMD_HISTORY,
	CDEV_MAX_DEV,
};

/* Character Devices:
 *  - Command
 *  - Firmware Download
 *  - Streaming
 */
#define CDEV_COUNT CDEV_MAX_DEV
#define ES_STREAMING_READ_TIMEOUT		4000

static int cdev_major;
static int cdev_minor;
static struct class *cdev_class;
static struct device *cdev_inst;
static struct device *devices[CDEV_COUNT];

/* streaming character device internals */
static int streaming_producer(void *ptr);
static char *streaming_consume_page(int *length);
static char *streaming_page_alloc(void);
static void streaming_page_free(char *old_page);

static char *stream_read_page;
static int stream_read_off;

#define CB_SIZE 128 /* MUST be power-of-two */
struct stream_circ_buf {
	char *buf[CB_SIZE];
	int length[CB_SIZE];
	int head;
	int tail;
};
struct stream_circ_buf stream_circ;

struct mutex stream_consumer_mutex;

static atomic_t cb_pages_out = ATOMIC_INIT(0);

/* command character device internals */

#define READBUF_SIZE 128
static struct timespec read_time;
static char readbuf[READBUF_SIZE];
static char *cmd_history_buf;

enum parse_token {
	PT_NIL, PT_PRESET, PT_ID, PT_HEXWORD
};

#define PARSE_BUFFER_SIZE PAGE_SIZE
/* The extra space allows the buffer to be zero terminated even if the
 * last newline is also the last character.
 */
static char parse_buffer[PARSE_BUFFER_SIZE + sizeof(u32)];

static int parse_have;		/* Bytes currently in buffer. */
static int last_token;		/* Used to control parser state. */
static int (*parse_cb_preset)(void *, int);
static int (*parse_cb_cmd)(void *, u32);

int macro_preset_id(void *ctx, int id)
{
	pr_debug("escore: ignored preset id = %i\n", id);
	return 0;
}

int macro_cmd(void *ctx, u32 cmd)
{
	struct escore_priv *escore = (struct escore_priv *)ctx;
	u32 resp;
	int rc;
	rc = escore_cmd_locked(escore, cmd, &resp);
	if (!(cmd & BIT(28))) {
		pr_debug("escore: cmd=0x%08x Response:0x%08x\n", cmd,
				escore->bus.last_response);
	} else {
		pr_debug("escore: cmd=0x%08x\n", cmd);
	}
	return rc;
}

/* Line oriented parser that extracts tokens from the shared
 * parse_buffer.
 *
 * FIXME: Add callback mechanism to actually act on commands and preset ids.
 */
static int parse(void *cb_ctx)
{
	char *cur, *tok;
	u16 w;
	u32 cmd;
	int err;
	int id;

	cur = parse_buffer;
	cmd = 0;
	err = 0;
	while ((tok = strsep(&cur, " \t\r\n")) != cur) {
		/* Comments extend to eol. */
		if (*tok == ';') {
			while (*cur != 0 && *cur != '\n')
				cur++;
			continue;
		}

		switch (last_token) {
		case PT_NIL:
			if (*tok == '0' &&
			    sscanf(tok, "0x%hx", &w) == 1) {
				last_token = PT_HEXWORD;
				cmd = w << 16;
			} else if (strnicmp("!Preset", tok, 7) == 0) {
				last_token = PT_PRESET;
			} else if (*tok != 0) {
				pr_debug("invalid token: '%s'", tok);
				err = -EINVAL;
				goto EXIT;
			}
			break;
		case PT_PRESET:
			if (strnicmp(tok, "id:", 3) == 0) {
				last_token = PT_ID;
			} else {
				pr_debug("expecting 'id:' got '%s'\n", tok);
				err = -EINVAL;
				goto EXIT;
			}
			break;
		case PT_ID:
			if (sscanf(tok, "%d", &id) == 1) {
				parse_cb_preset(cb_ctx, id);
				last_token = PT_NIL;
			} else {
				pr_debug("expecting preset id: got '%s'\n",
					 tok);
				err = -EINVAL;
				goto EXIT;
			}
			break;
		case PT_HEXWORD:
			if (last_token == PT_HEXWORD &&
			    sscanf(tok, "0x%hx", &w) == 1) {
				parse_cb_cmd(cb_ctx, cmd | w);
				last_token = PT_NIL;
			} else {
				pr_debug("expecting hex word: got '%s'\n",
					 tok);
				err = -EINVAL;
				goto EXIT;
			}
			break;
		}
	}

EXIT:
	return err;
}

static int command_open(struct inode *inode, struct file *filp)
{
	struct escore_priv *escore;
	int err = 0;
	unsigned major;
	unsigned minor;

	pr_debug("called: %s\n", __func__);

	major = imajor(inode);
	minor = iminor(inode);
	if (major != cdev_major || minor < 0 || minor >= CDEV_COUNT) {
		pr_warn("escore: no such device major=%u minor=%u\n",
			 major, minor);
		err = -ENODEV;
		goto OPEN_ERR;
	}

	escore = container_of((inode)->i_cdev, struct escore_priv,
			cdev_command);

	if (inode->i_cdev != &escore->cdev_command) {
		dev_err(escore->dev, "open: error bad cdev field\n");
		err = -ENODEV;
		goto OPEN_ERR;
	}

	filp->private_data = escore;

	/* Initialize parser. */
	last_token = PT_NIL;
	parse_have = 0;
	parse_cb_preset = macro_preset_id;
	parse_cb_cmd = macro_cmd;
OPEN_ERR:
	return err;
}

static int command_release(struct inode *inode, struct file *filp)
{
	struct escore_priv *escore;
	escore = (struct escore_priv *)filp->private_data;
	return 0;
}

static loff_t command_llseek(struct file *filp, loff_t off, int whence)
{
	/*
	 * Only is lseek(fd, 0, SEEK_SET) to allow multiple calls to
	 * read().
	 */
	if (off != 0 || whence != SEEK_SET)
		return -ESPIPE;

	filp->f_pos = 0;
	return 0;
}

static ssize_t command_read(struct file *filp, char __user *buf,
			       size_t count, loff_t *f_pos)
{
	struct escore_priv *escore;
	u32 resp;
	size_t slen;
	int err;
	size_t cnt;

	escore = (struct escore_priv *)filp->private_data;
	BUG_ON(!escore);
	err = cnt = 0;

	if (timespec_compare(&read_time, &escore->last_resp_time) != 0) {
		resp = escore_resp(escore);
		memcpy(&read_time, &escore->last_resp_time, sizeof(read_time));
		snprintf(readbuf, READBUF_SIZE,
			 "%li.%4li 0x%04hx 0x%04hx\n",
			 read_time.tv_sec, read_time.tv_nsec,
			 resp >> 16, resp & 0xffff);
	}

	slen = strnlen(readbuf, READBUF_SIZE);
	if (*f_pos >= slen)
		goto OUT;	/* End of file. */

	slen -= *f_pos;
	cnt = min(count, slen);
	err = copy_to_user(buf, readbuf + *f_pos, cnt);
	if (err) {
		dev_dbg(escore->dev, "%s copy_to_user = %d\n", __func__, err);
		goto OUT;
	}
	*f_pos += cnt;

OUT:
	return (err) ? err : cnt;
}

static ssize_t command_write(struct file *filp, const char __user *buf,
				size_t count, loff_t *f_pos)
{
	struct escore_priv *escore;
	size_t used;
	int rem;
	const char __user *ptr;
	int err;

	pr_debug("called: %s\n", __func__);

	escore = (struct escore_priv *)filp->private_data;
	BUG_ON(!escore);

	err = 0;
	used = 0;
	ptr = buf;
	while (used < count) {
		int space, frag;
		char *data, *end;
		char last;

		space = PARSE_BUFFER_SIZE - parse_have;
		if (space == 0) {
			pr_debug("escore: line too long - exhausted buffer\n");
			err = -EFBIG;
			goto OUT;
		}

		/* Top up the parsing buffer. */
		rem = count - used;
		frag = min(space, rem);
		data = parse_buffer + parse_have;
		pr_debug("escore: copying fragment size = %i\n", frag);
		err  = copy_from_user(data, ptr, frag);
		if (err) {
			pr_debug("escore: error copying user data\n");
			err = -EFAULT;
			goto OUT;
		}
		used += frag;

		/* Find the last newline and terminated the buffer
		 * there with 0 making a string.
		 */
		end = parse_buffer + parse_have + frag - 1;
		while (*end != '\n' && end >= parse_buffer)
			end--;
		end += 1;
		last = *end;
		*end = 0;

		err = parse(escore);
		if (err) {
			pr_debug("escore: parsing error");
			err = -EINVAL;
			goto OUT;
		}

		*end = last;
		parse_have = data + frag - end;
		pr_debug("used = %zu parse_have = %i\n", used, parse_have);
		if (parse_have > 0)
			memmove(parse_buffer, end, parse_have);
	}

	/*
	 * There are no obviously useful semantics for using file
	 * position: so don't increment.
	 */
OUT:
	return (err) ? err : count;
}

const struct file_operations command_fops = {
	.owner = THIS_MODULE,
	.llseek = command_llseek,
	.read = command_read,
	.write = command_write,
	.open = command_open,
	.release = command_release
};

static int firmware_open(struct inode *inode, struct file *filp)
{
	struct escore_priv *escore;
	int ret = 0;
	int retries = 1;

	escore = container_of((inode)->i_cdev, struct escore_priv,
			cdev_firmware);
	BUG_ON(!escore);
	pr_debug("%s  escore=%p\n", __func__, escore);
	filp->private_data = escore;

	if (escore->bus.ops.high_bw_open) {
		ret = escore->bus.ops.high_bw_open(escore);
		if (ret) {
			dev_err(escore->dev, "%s(): high_bw_open failed %d\n",
			__func__, ret);
			goto escore_high_bw_open_failed;
		}
	}

	/* FIXME lock access here - needs to be coordinate with
	 * firmware download related functions.
	 */

	/* Reset and ready chip for firmware download. */
	escore_gpio_reset(escore);
	/* FIXME need to test with hardware gpio support */
	if (escore->boot_ops.setup) {
		do {
			ret = escore->boot_ops.setup(escore);
			if (ret != -EIO)
				break;
		} while (retries--);
	}

	if (ret) {
		dev_err(escore->dev, "bus specific boot setup failed: %d\n",
				ret);
		return ret;
	}
	pr_debug("firmware download setup ok\n");

escore_high_bw_open_failed:
	return ret;
}

static int firmware_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct escore_priv *escore;

	/* FIXME should determine how long to sleep based on time of
	 * last write
	 */
	msleep(20);

	escore = filp->private_data;
	BUG_ON(!escore);

	if (escore->boot_ops.finish) {
		ret = escore->boot_ops.finish(escore);
		if (ret) {
			dev_err(escore->dev,
				"bus specific boot finish failed: %d\n",
				ret);
			goto firmware_release_exit;
		}
	}

	/* Previous interrupt configuration would have been lost after
	 * the new firmware download. It needs to be reconfigured again */
	ret = escore_reconfig_intr(escore);
	if (ret < 0)
		dev_err(escore->dev, "%s(): Interrupt setup failed %d\n",
				__func__, ret);
firmware_release_exit:
	if (escore->bus.ops.high_bw_close) {
		int rc = 0;
		rc = escore->bus.ops.high_bw_close(escore);
		if (rc) {
			dev_err(escore->dev, "%s(): high_bw_close failed %d\n",
				__func__, rc);
			ret = rc;
		}
	}

	pr_debug("successful download of firmware\n");
	return ret;
}

/* Temporary buffer used when moving firmware data from user-space to
 * kernel-space.
 */
#define FW_BUF_SIZE ((size_t)1024)
static char fw_buf[PAGE_SIZE];

static ssize_t firmware_write(struct file *filp, const char __user *buf,
			      size_t count, loff_t *f_pos)
{
	int ret;
	struct escore_priv *escore;
	size_t bs;
	size_t wr;

	escore = filp->private_data;
	BUG_ON(!escore);

	pr_debug("firmware write count=%zd bytes\n", count);

	wr = bs = 0;
	while (wr < count) {
		bs = min(count - wr, FW_BUF_SIZE);
		BUG_ON(bs == 0);
		pr_debug("wr=%zd bs=%zd buf+wr=%p\n", wr, bs, buf + wr);
		ret = copy_from_user(fw_buf, buf + wr, bs);
		if (ret) {
			dev_err(escore->dev,
				"error loading firmware: %d\n", ret);
			return ret;
		}

		/* Transfer to the chip is an all or nothing operation. */
		ret = escore->bus.ops.high_bw_write(escore, fw_buf, bs);
		pr_debug("dev_write ret=%d\n", ret);
		if (ret < 0) {
			pr_debug("%s ret=%d\n", __func__, ret);
			return ret;
		}
		*f_pos += bs;
		wr += bs;
	}

	return wr;
}

const struct file_operations firmware_fops = {
	.owner = THIS_MODULE,
	.write = firmware_write,
	.open = firmware_open,
	.release = firmware_release
};

static char *streaming_page_alloc(void)
{
	char *new_page;
	new_page = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (new_page)
		atomic_inc(&cb_pages_out);
	return new_page;
}

static void streaming_page_free(char *old_page)
{
	if (!old_page)
		return;
	kfree(old_page);
	atomic_dec(&cb_pages_out);
}

static int stream_datalog_open(struct escore_priv *escore, struct inode *inode,
	struct file *filp)
{
	int err;
	unsigned major;
	unsigned minor;
	struct task_struct *stream_thread;

	/*
	* check for required function implementations
	* note that streamdev.wait is deliberately omitted
	*/
	if (!escore->streamdev.read  ||
			escore->streamdev.intf < 0) {
		dev_err(escore->dev, "streaming not configured");
		return -ENODEV;
	}

	pr_debug("%s(): streaming mutex lock killable\n", __func__);
	err = mutex_lock_killable(&escore->streaming_mutex);
	if (err) {
		dev_dbg(escore->dev, "did not get streaming lock: %d\n", err);
		err = -EBUSY;
		goto streamdev_mutex_lock_err;
	}

	major = imajor(inode);
	minor = iminor(inode);
	if (major != cdev_major || minor < 0 || minor >= CDEV_COUNT) {
		pr_warn("escore: no such device major=%u minor=%u\n",
			major, minor);
		err = -ENODEV;
		goto streamdev_invalid_param_err;
	}

	if (inode->i_cdev != &escore->cdev_streaming &&
			inode->i_cdev != &escore->cdev_datalogging) {
		dev_err(escore->dev, "open: error bad cdev field\n");
		err = -ENODEV;
		goto streamdev_invalid_param_err;
	}

	filp->private_data = escore;

	if (escore->streamdev.open) {
		err = escore->streamdev.open(escore);
		if (err) {
			dev_err(escore->dev,
				"%s(): can't open streaming device = %d\n",
				__func__, err);
			goto streamdev_open_err;
		}
		/* Set NS baud rate after VS to NS switching */
		if (escore->streamdev.config)
			escore->streamdev.config(escore);
	}

	/* initialize stream buffer */
	mutex_init(&stream_consumer_mutex);
	memset(&stream_circ, 0, sizeof(stream_circ));

	stream_read_page = NULL;
	stream_read_off = 0;

	/* start thread to buffer streaming data */
	stream_thread = kthread_run(streaming_producer, (void *)
				escore, "escore stream thread");
	if (IS_ERR_OR_NULL(stream_thread)) {
		pr_err("%s(): can't create escore streaming thread = %p\n",
			__func__, stream_thread);
		err = -ENOMEM;
		goto streamdev_thread_create_err;
	}
	set_user_nice(stream_thread, -20);
	escore->stream_thread = stream_thread;

	/* start streaming over UART */
	if (inode->i_cdev == &escore->cdev_streaming) {
		if (escore->set_streaming(escore, ESCORE_STREAM_ENABLE)) {
			dev_dbg(escore->dev, "failed to turn on streaming\n");
			err = -EBUSY;
			goto streamdev_set_streaming_err;
		}
	} else if (inode->i_cdev == &escore->cdev_datalogging) {
		if ((escore->set_datalogging(escore,
			ESCORE_DATALOGGING_CMD_ENABLE)) ||
				(escore->bus.last_response !=
					ESCORE_DATALOGGING_CMD_ENABLE)) {
			dev_dbg(escore->dev, "%s(): failed to turn on datalogging: %d\n",
				__func__, err);
			err = -EBUSY;
			goto streamdev_set_streaming_err;
		}
	}

	return err;

streamdev_set_streaming_err:
	dev_dbg(escore->dev, "%s(): stopping stream kthread\n", __func__);
	kthread_stop(escore->stream_thread);
streamdev_thread_create_err:
	if (escore->streamdev.close)
		escore->streamdev.close(escore);
streamdev_open_err:
streamdev_invalid_param_err:
	pr_debug("%s(): streaming mutex unlock\n", __func__);
	mutex_unlock(&escore->streaming_mutex);
streamdev_mutex_lock_err:

	return err;
}

static int stream_datalog_release(struct escore_priv *escore,
	struct inode *inode)
{
	int err = 0;
	char *page;
	int length;

	/* stop streaming over UART */
	if (inode->i_cdev == &escore->cdev_streaming) {
		if (escore->set_streaming(escore, ESCORE_STREAM_DISABLE)) {
			dev_warn(escore->dev,
				"%s(): failed to turn off streaming: %d\n",
				__func__, err);
			dev_dbg(escore->dev,
				"%s(): Stop collecting streaming from chip"
				"and releasing all locks\n", __func__);
		}
	} else if (inode->i_cdev == &escore->cdev_datalogging) {
		if ((escore->set_datalogging(escore,
			ESCORE_DATALOGGING_CMD_DISABLE)) ||
				(escore->bus.last_response !=
					 ESCORE_DATALOGGING_CMD_DISABLE)) {
			dev_warn(escore->dev,
				"%s(): failed to turn off datalogging: %d\n",
				__func__, err);
			dev_dbg(escore->dev,
				"%s(): Stop collecting datalogging from chip"
				"and releasing all locks\n", __func__);
		}
	} else {
		dev_err(escore->dev, "%s(): bad cdev field\n", __func__);
		err = -ENODEV;
		goto streamdev_set_streaming_err;
	}

	/* ignore threadfn return value */
	pr_debug("%s(): stopping stream kthread\n", __func__);

	/* stop streaming thread if running */
	if (atomic_read(&escore->stream_thread->usage) > 0)
		kthread_stop(escore->stream_thread);

	/* free any pages on the circular buffer */
	while ((page = streaming_consume_page(&length)))
		streaming_page_free(page);

	if (stream_read_page) {
		streaming_page_free(stream_read_page);
		stream_read_page = NULL; /* prevents double free */
	}

	BUG_ON(atomic_read(&cb_pages_out));

	escore->streamdev.no_more_bit = 0;

	if (escore->streamdev.close)
		escore->streamdev.close(escore);

	pr_debug("%s(): streaming mutex unlock\n", __func__);
	mutex_unlock(&escore->streaming_mutex);

streamdev_set_streaming_err:
	return err;
}

static int streaming_open(struct inode *inode, struct file *filp)
{
	struct escore_priv *escore;
	int err;

	pr_debug("called: %s\n", __func__);

	escore = container_of((inode)->i_cdev, struct escore_priv,
			cdev_streaming);

	mutex_lock(&escore->access_lock);
	err = escore_pm_get_sync();
	mutex_unlock(&escore->access_lock);

	if (err < 0) {
		pr_err("%s(): pm_get_sync failed :%d\n", __func__, err);
		return err;
	}

	err = stream_datalog_open(escore, inode, filp);
	if (err)
		dev_err(escore->dev, "%s(): stream_datalog_open err = %d\n",
			__func__, err);
	return err;
}

static int streaming_release(struct inode *inode, struct file *filp)
{
	struct escore_priv *escore;
	int err = 0;

	escore = (struct escore_priv *)filp->private_data;

	err = stream_datalog_release(escore, inode);
	if (err)
		dev_err(escore->dev, "%s(): stream_datalog_release error = %d\n",
			__func__, err);

	mutex_lock(&escore->access_lock);
	escore_pm_put_autosuspend();
	mutex_unlock(&escore->access_lock);
	return err;
}

static int datalogging_open(struct inode *inode, struct file *filp)
{
	struct escore_priv *escore;
	int err;

	escore = container_of((inode)->i_cdev, struct escore_priv,
			cdev_datalogging);

	if (escore->streamdev.intf != ES_UART_INTF) {
		dev_err(escore->dev, "%s():  streamdev not using UART interface\n",
			__func__);
		return -EPERM;
	}

	err = stream_datalog_open(escore, inode, filp);
	if (err)
		dev_err(escore->dev, "%s(): stream_datalog_open err = %d\n",
			__func__, err);
	return err;
}

static int datalogging_release(struct inode *inode, struct file *filp)
{
	struct escore_priv *escore;
	int err;

	escore = (struct escore_priv *)filp->private_data;

	err = stream_datalog_release(escore, inode);
	if (err)
		dev_err(escore->dev, "%s(): stream_datalog_release error = %d\n",
			__func__, err);
	return err;
}

static char *streaming_consume_page(int *length)
{
	char *page = NULL;
	int chead, ctail;

	mutex_lock(&stream_consumer_mutex);

	chead = ACCESS_ONCE(stream_circ.head);
	ctail = stream_circ.tail;

	if (CIRC_CNT(chead, ctail, CB_SIZE) >= 1) {
		smp_read_barrier_depends();

		page = stream_circ.buf[ctail];
		*length = stream_circ.length[ctail];
		smp_mb();

		stream_circ.tail = (ctail + 1) & (CB_SIZE - 1);
	}

	mutex_unlock(&stream_consumer_mutex);

	return page;
}

static ssize_t streaming_read(struct file *filp, char __user *buf,
			      size_t count, loff_t *f_pos)
{
	struct escore_priv *escore;
	int user_pos = 0;
	int copy_len;
	int count_remain = count;
	unsigned long bytes_read = 0;
	int length = 0;
	int err;

	pr_debug("called: %s\n", __func__);
	escore = (struct escore_priv *)filp->private_data;
	if (escore->streamdev.no_more_bit) {
		dev_dbg(escore->dev, "%s() returning as no more data\n",
			__func__);
		escore->streamdev.no_more_bit = 0;
		return 0;
	}


	/* read a page off of the circular buffer */
	if (!stream_read_page || stream_read_off == PAGE_SIZE) {
read_next_page:
		if (stream_read_page)
			streaming_page_free(stream_read_page);

		stream_read_page = streaming_consume_page(&length);
		while (!stream_read_page) {
			err = wait_event_interruptible_timeout(
				escore->stream_in_q,
				(stream_read_page =
				streaming_consume_page(&length)),
				msecs_to_jiffies(ES_STREAMING_READ_TIMEOUT));

			if (err == -ERESTARTSYS) {
				/* return short read or -EINTR */
				if (count - count_remain > 0)
					err = count - count_remain;
				else
					err = -EINTR;

				goto ERR_OUT;
			} else if (err == 0)
				goto ERR_OUT;
		}

		stream_read_off = 0;
	}

	while (count_remain > 0) {
		copy_len = min((int)count_remain, (int) length -
			stream_read_off);

		err = copy_to_user(buf + user_pos, stream_read_page +
			stream_read_off, copy_len);

		if (err) {
			dev_dbg(escore->dev, "%s copy_to_user = %d\n",
				__func__, err);
			err = -EIO;
			goto ERR_OUT;
		}

		user_pos += copy_len;
		stream_read_off += copy_len;
		count_remain -= copy_len;
		bytes_read += copy_len;

		if (stream_read_off == PAGE_SIZE && count_remain > 0)
			goto read_next_page;

		if (length < PAGE_SIZE) {
			escore->streamdev.no_more_bit = 1;
			dev_dbg(escore->dev,
				"%s() size is less than PAGE_SIZE %d\n",
				__func__, length);
			break;
		}
	}

	dev_dbg(escore->dev, "%s() bytes_read read  = %ld\n",
					__func__, bytes_read);
	return bytes_read;

ERR_OUT:
	return err;
}

static int streaming_producer(void *ptr)
{
	struct escore_priv *escore;
	char *buf;
	char *consume_page;
	int rlen = 0;		/* bytes read into buf buffer */
	int rlen_last = 0;	/* bytes read on last read call */
	int length;
	int chead, ctail;
	int data_ready = 1;
	unsigned long bytes_read = 0;

	buf = streaming_page_alloc();
	if (!buf)
		return -ENOMEM;

	pr_debug("called: %s\n", __func__);
	escore = (struct escore_priv *) ptr;

	/*
	 * loop while the thread isn't kthread_stop'd AND
	 * keep looping after the kthread_stop to throw away any data
	 * that may be in the UART receive buffer
	 */
	do {
		if (rlen == PAGE_SIZE) {
			chead = stream_circ.head;
			ctail = ACCESS_ONCE(stream_circ.tail);

			if (CIRC_SPACE(chead, ctail, CB_SIZE) < 1) {
				/* consume oldest slot */
				dev_dbg(escore->dev,
						"%s(): lost page of stream buffer\n",
						__func__);
				consume_page = streaming_consume_page(&length);
				if (consume_page)
					streaming_page_free(consume_page);

				chead = stream_circ.head;
				ctail = ACCESS_ONCE(stream_circ.tail);
			}

			/* insert */
			stream_circ.buf[chead] = buf;
			stream_circ.length[chead] = rlen;
			smp_wmb(); /* commit data */
			stream_circ.head = (chead + 1) & (CB_SIZE - 1);

			/* awake any reader blocked in select, poll, epoll */
			wake_up_interruptible(&escore->stream_in_q);

			buf = streaming_page_alloc();
			if (!buf)
				return -ENOMEM;
			rlen = 0;
		}
		if (escore->streamdev.wait)
			data_ready = escore->streamdev.wait(escore);

		if (data_ready > 0) {
			rlen_last = escore->streamdev.read(escore, buf + rlen,
					PAGE_SIZE - rlen);
			if (rlen_last < 0) {
				dev_err(escore->dev, "%s(): read error on streamdev: %d\n",
						__func__, rlen_last);
			} else {
				rlen += rlen_last;
			}
		}
	} while (!kthread_should_stop());

	dev_dbg(escore->dev, "%s(): end capture streaming data\n", __func__);
	dev_dbg(escore->dev, "%s(): data ready = %d\n", __func__, data_ready);
	dev_dbg(escore->dev, "%s() bytes_read = %ld\n", __func__, bytes_read);

	streaming_page_free(buf);

	return 0;
}

static unsigned int streaming_poll(struct file *filp, poll_table *wait)
{
	struct escore_priv *escore = filp->private_data;
	int chead, ctail;
	unsigned int mask = 0;

	poll_wait(filp, &escore->stream_in_q, wait);

	chead = ACCESS_ONCE(stream_circ.head);
	ctail = stream_circ.tail;

	if (CIRC_CNT(chead, ctail, CB_SIZE) >= 1)
		mask |= POLLIN | POLLRDNORM; /* readable */

	return mask;
}

const struct file_operations streaming_fops = {
	.owner = THIS_MODULE,
	.read = streaming_read,
	.open = streaming_open,
	.release = streaming_release,
	.poll = streaming_poll
};

static const struct file_operations datalogging_fops = {
	.owner = THIS_MODULE,
	.read = streaming_read,
	.open = datalogging_open,
	.release = datalogging_release,
	.poll = streaming_poll
};

/* must init on open, update later, protected by open lock */
static char *data_buf;

/* IOCTL to transfer data between user space and kernel */
static long datablock_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct escore_priv * const escore
				= (struct escore_priv *)filp->private_data;
	void __user *argp = (void __user *)arg;
	struct escore_user_buf datablock_copy;
	int rc = 0;
	int buffer_size = 0;

	pr_debug("%s():  datablock_ioctl called\n", __func__);

	/* Copy structure */
	rc = copy_from_user(&datablock_copy, argp, sizeof(datablock_copy));
	if (rc) {
		pr_err("%s(): structur copy from user is failed rc: %d\n",
						__func__, rc);
		return -EFAULT;
	}

	buffer_size = datablock_copy.buf_size;
	switch (_IOC_NR(cmd)) {
	case _IOC_NR(ADNC_WRITE_HOTWORD_IOCTL):
		pr_debug("%s(): User buffer size = %d actual buffer_size %d\n ",
		 __func__, datablock_copy.buf_size, buffer_size);

		/* Free previously allocated buffer and reset size */
		kfree(escore->hotword_buf);
		escore->hotword_buf_size = 0;

		/* Allocate memory to store hotword buffer */
		escore->hotword_buf = kzalloc(buffer_size,
						GFP_KERNEL);
		if (!escore->hotword_buf) {
			pr_err("%s(): could not allocate memory\n", __func__);
			rc = -ENOMEM;
			break;
		}

		/* Copy data */
		rc = copy_from_user(escore->hotword_buf,
				(char *)datablock_copy.buf, buffer_size);
		if (rc) {
			pr_err("%s(): Data copy from user is failed rc: %d\n",
							__func__, rc);
			rc = -EFAULT;
		} else
			escore->hotword_buf_size = datablock_copy.buf_size;
		break;
	default:
		pr_err("%s(): Invalid command : 0x%X\n", __func__, cmd);
		rc = -EINVAL;
		break;
	}

	return rc;
}

/* just read the global read buffer */
static ssize_t datablock_read(struct file *filp, char __user *buf,
					size_t count, loff_t *f_pos)
{
	struct escore_priv * const escore
				= (struct escore_priv *)filp->private_data;
	ssize_t rcnt = count < escore->datablock_dev.rdb_read_count ?
				count : escore->datablock_dev.rdb_read_count;
	int rc;

	BUG_ON(!escore);

	if (escore->datablock_dev.rdb_read_buffer == NULL)
		return 0;

	/* Mutex required against simultaneous read/writes on the RDB buffer */
	rc = mutex_lock_killable(&escore->datablock_dev.datablock_read_mutex);
	if (rc) {
		pr_err("%s() failed to get datablock read lock: %d\n",
				__func__, rc);
		return rc;
	}

	rc = copy_to_user(buf, escore->datablock_dev.rdb_read_buffer + *f_pos,
			rcnt);
	mutex_unlock(&escore->datablock_dev.datablock_read_mutex);
	if (rc) {
		pr_err("%s() copy to user failed: %d\n", __func__, rc);
		return rc;
	}

	/* update position & count to support multiple reads in chunks smaller
	 * than the buffer */
	*f_pos += rcnt;
	escore->datablock_dev.rdb_read_count -= rcnt;
	if (!escore->datablock_dev.rdb_read_count) {
		kfree(escore->datablock_dev.rdb_read_buffer);
		escore->datablock_dev.rdb_read_buffer = NULL;
	}

	return rcnt;
}

static int datablock_write_dispatch(struct escore_priv * const escore,
				u32 cmd, size_t count)
{
	char *data = data_buf + sizeof(cmd);
	int remains = (int)count - sizeof(cmd);
	int err = 0;
	u32 cmd_id = cmd >> 16;
	u32 rspn;
	struct es_datablock_device *datablock_dev = &escore->datablock_dev;

	if (cmd_id == ES_WDB_CMD) {
		err = escore_datablock_write(escore, data, remains);
		/* If WDB is sucessful, it will return count = remains.
		 * Write dispatch should returns total no. of bytes written
		 * which will include 4 WDB command bytes. So total no. of
		 * bytes written will be 4 bytes more than return value of
		 * escore_datablock_write().
		 */
		if (err > 0)
			err += 4;
	} else if (cmd_id == ES_RDB_CMD) {
		int size;
		err = mutex_lock_killable(&datablock_dev->datablock_read_mutex);
		if (unlikely(err)) {
			dev_err(escore->dev,
				"%s(): lock failed for datablock read: %d\n",
				__func__, err);
			return err;
		}

		/* Reset read data block size */
		datablock_dev->rdb_read_count = 0;

		size = escore_get_rdb_size(escore, (cmd & 0xFFFF));
		if (size < 0) {
			mutex_unlock(&datablock_dev->datablock_read_mutex);
			return size;
		}

		/* free previously malloced data (if any) & malloc with new
		 * size */
		kfree(datablock_dev->rdb_read_buffer);
		datablock_dev->rdb_read_buffer = kmalloc(size, GFP_KERNEL);
		if (datablock_dev->rdb_read_buffer == NULL) {
			dev_err(escore->dev,
				"%s(): Not enough memory to read RDB data\n",
				__func__);
			mutex_unlock(&datablock_dev->datablock_read_mutex);
			return -ENOMEM;
		}

		/* include the ID of the ES block to be read */
		err = escore_datablock_read(escore,
					datablock_dev->rdb_read_buffer, size);
		/* If there is no error, this function should return 0 */
		if (err > 0) {
			err = 0;
			/* Store read data block size */
			datablock_dev->rdb_read_count = size;
		}

		mutex_unlock(&datablock_dev->datablock_read_mutex);
	} else {
		err = escore_cmd(escore, cmd, &rspn);
		if (err) {
			dev_err(escore->dev, "%s(): cmd err: %d\n",
				 __func__, err);
			goto OUT;
		}
		dev_dbg(escore->dev, "%s(): cmd ack: 0x%08x\n",
					__func__, rspn);
	}
OUT:
	return err;
}

static ssize_t datablock_write(struct file *filp, const char __user *buf,
				size_t count, loff_t *f_pos)
{
	struct escore_priv * const escore
				= (struct escore_priv *)filp->private_data;
	u32 cmd;
	int err = 0;

	BUG_ON(!escore);
	dev_dbg(escore->dev, "%s() entry: count: %zd\n", __func__, count);

	if (count > sizeof(parse_buffer)) {
		dev_err(escore->dev, "%s() invalid counter size\n", __func__);
		err = -EINVAL;
		goto OUT;
	}

	err = copy_from_user(data_buf, buf, count);
	if (err) {
		dev_err(escore->dev, "%s(): copy_from_user err: %d\n",
							__func__, err);
		goto OUT;
	}
	memmove(&cmd, data_buf, sizeof(cmd));

	dev_dbg(escore->dev, "%s(): cmd: 0x%08x\n", __func__, cmd);

	mutex_lock(&escore->access_lock);
	err = datablock_write_dispatch(escore, cmd, count);
	mutex_unlock(&escore->access_lock);
OUT:
	return (err) ? err : count;
}

static int datablock_open(struct inode *inode, struct file *filp)
{
	struct escore_priv * const escore =
			container_of((inode)->i_cdev, struct escore_priv,
							cdev_datablock);
	unsigned major = imajor(inode);
	unsigned minor = iminor(inode);
	int err = 0;

	data_buf = parse_buffer;

	if (!escore) {
		pr_err("bad escore: %p\n", escore);
		err = -EINVAL;
		goto OPEN_ERR;
	}

	if (major != cdev_major || minor < 0 || minor >= CDEV_COUNT) {
		dev_warn(escore->dev, "%s(): no such device major=%u minor=%u\n",
							__func__, major, minor);
		err = -ENODEV;
		goto OPEN_ERR;
	}

	if (inode->i_cdev != &escore->cdev_datablock) {
		dev_err(escore->dev, "%s(): open: error bad cdev field\n",
								__func__);
		err = -ENODEV;
		goto OPEN_ERR;
	}

	mutex_lock(&escore->access_lock);
	err = escore_pm_get_sync();
	mutex_unlock(&escore->access_lock);
	if (err < 0) {
		pr_err("%s(): pm_get_sync failed :%d\n", __func__, err);
		return err;
	}

	err = escore_datablock_open(escore);
	if (err) {
		dev_err(escore->dev, "%s(): can't open datablock device = %d\n",
			__func__, err);
		goto OPEN_ERR;
	}

	err = mutex_lock_killable(&escore->datablock_dev.datablock_mutex);
	if (err) {
		dev_dbg(escore->dev, "%s(): did not get lock: %d\n",
			__func__, err);
		err = -EBUSY;
		goto OPEN_ERR;
	}
	filp->private_data = escore;

	dev_dbg(escore->dev, "%s(): open complete\n", __func__);

OPEN_ERR:
	return err;
}

static int datablock_release(struct inode *inode, struct file *filp)
{
	struct escore_priv * const escore
			= (struct escore_priv *)filp->private_data;
	BUG_ON(!escore);
	mutex_unlock(&escore->datablock_dev.datablock_mutex);

	mutex_lock(&escore->access_lock);
	escore_pm_put_autosuspend();
	mutex_unlock(&escore->access_lock);

	escore_datablock_close(escore);
	dev_dbg(escore->dev, "%s(): release complete\n", __func__);
	return 0;
}

static int cmd_history_open(struct inode *inode, struct file *filp)
{
	struct escore_priv *escore;
	int rc = 0;
	unsigned major;
	unsigned minor;
	int index, i, j = 0;
	struct timespec *ts;

	pr_debug("called: %s\n", __func__);

	major = imajor(inode);
	minor = iminor(inode);
	if (major != cdev_major || minor < 0 || minor >= CDEV_COUNT) {
		pr_warn("escore: no such device major=%u minor=%u\n",
			 major, minor);
		rc = -ENODEV;
		goto out;
	}

	escore = container_of((inode)->i_cdev, struct escore_priv,
			cdev_cmd_history);

	if (inode->i_cdev != &escore->cdev_cmd_history) {
		pr_err("open: error bad cdev field\n");
		rc = -ENODEV;
		goto out;
	}

	filp->private_data = escore;

	cmd_history_buf = kmalloc(
		ES_MAX_ROUTE_MACRO_CMD * ES_MAX_CMD_HISTORY_LINE_SIZE,
		GFP_KERNEL);
	if (!cmd_history_buf) {
		dev_err(escore->dev, "%s(): buffer alloc failed\n", __func__);
		return -ENOMEM;
	}

	if (filp->f_flags == O_WRONLY)
		return 0;

	for (i = 0; i < ES_MAX_ROUTE_MACRO_CMD; i++) {
		index = i + cmd_hist_index;
		index %= ES_MAX_ROUTE_MACRO_CMD;
		if (cmd_hist[index].cmd) {
			ts = &cmd_hist[index].timestamp;
			j += snprintf(cmd_history_buf + j, PAGE_SIZE,
					"[%5lu.%03lu] ",
					ts->tv_sec, ts->tv_nsec / (1000*1000));
			j += snprintf(cmd_history_buf + j, PAGE_SIZE,
					"0x%04x 0x%04x; ",
					cmd_hist[index].cmd >> 16,
					cmd_hist[index].cmd & 0xffff);
			if (cmd_hist[index].resp)
				j += snprintf(cmd_history_buf + j, PAGE_SIZE,
						"resp = 0x%04x 0x%04x; ",
						cmd_hist[index].resp >> 16,
						cmd_hist[index].resp & 0xffff);
			j += snprintf(cmd_history_buf + j, PAGE_SIZE, "\n");
		}
	}

	escore->cmd_history_size = j;

out:
	return rc;
}

static ssize_t cmd_history_read(struct file *filp, char __user *buf,
					size_t count, loff_t *f_pos)
{
	struct escore_priv * const escore
			= (struct escore_priv *)filp->private_data;
	static int done, pos;
	unsigned int size;
	int rc;

	if (done || !escore->cmd_history_size) {
		pr_info("%s() done\n", __func__);
		done = 0;
		pos = 0;
		return 0;
	}

	size = escore->cmd_history_size > count ?
			count : escore->cmd_history_size;

	rc = copy_to_user(buf, cmd_history_buf + pos, size);
	if (rc) {
		pr_err("%s() error in copy_to_user() %d\n", __func__, rc);
		return rc;
	}
	escore->cmd_history_size -= size;
	pos += size;

	if (!escore->cmd_history_size)
		done = 1;

	return	size;
}

static ssize_t cmd_history_write(struct file *filp, const char __user *buf,
				size_t count, loff_t *f_pos)
{
	int err;

	BUG_ON(!cmd_history_buf);

	err = copy_from_user(cmd_history_buf, buf, count);
	if (err) {
		pr_err("%s(): copy_from_user err: %d\n", __func__, err);
		goto out;
	}

	pr_info("%s(): requested - %s\n", __func__, cmd_history_buf);
	if (!strncmp(cmd_history_buf, "clear", 5)) {
		memset(cmd_hist, 0,  ES_MAX_ROUTE_MACRO_CMD *
						sizeof(cmd_hist[0]));
		cmd_hist_index = 0;
	} else
		pr_err("%s(): Invalid command: %s\n",
				__func__, cmd_history_buf);
out:
	return (err) ? err : count;

}

static int cmd_history_release(struct inode *inode, struct file *filp)
{
	struct escore_priv * const escore
			= (struct escore_priv *)filp->private_data;

	kfree(cmd_history_buf);
	cmd_history_buf = NULL;
	escore->cmd_history_size = 0;
	return 0;
}
static const struct file_operations datablock_fops = {
	.owner = THIS_MODULE,
	.read = datablock_read,
	.write = datablock_write,
	.open = datablock_open,
	.release = datablock_release,
	.unlocked_ioctl = datablock_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = datablock_ioctl,
#endif
};


static const struct file_operations cmd_history_fops = {
	.owner = THIS_MODULE,
	.read = cmd_history_read,
	.write = cmd_history_write,
	.open = cmd_history_open,
	.release = cmd_history_release,
};

static int create_cdev(struct escore_priv *escore, struct cdev *cdev,
		       const struct file_operations *fops, unsigned int index)
{
	int devno;
	struct device *dev;
	int err;

	devno = MKDEV(cdev_major, cdev_minor + index);
	cdev_init(cdev, fops);
	cdev->owner = THIS_MODULE;
	err = cdev_add(cdev, devno, 1);
	if (err) {
		dev_err(escore->dev, "failed to add cdev=%d error: %d",
			index, err);
		return err;
	}

	dev = device_create(cdev_class, NULL, devno, NULL,
			    ESCORE_CDEV_NAME "%d", cdev_minor + index);
	if (IS_ERR(dev)) {
		err = PTR_ERR(dev);
		dev_err(escore->dev, "device_create cdev=%d failed: %d\n",
			index, err);
		cdev_del(cdev);
		return err;
	}
	devices[index] = dev;

	return 0;
}

static void cdev_destroy(struct cdev *cdev, int index)
{
	int devno;
	devno = MKDEV(cdev_major, cdev_minor + index);
	device_destroy(cdev_class, devno);
	cdev_del(cdev);
}

int escore_cdev_init(struct escore_priv *escore)
{
	int err;
	dev_t dev;

	pr_debug("called: %s\n", __func__);

	/* initialize to required setup values */

	cdev_major = cdev_minor = 0;
	cdev_inst = NULL;

	/* reserve character device */

	err = alloc_chrdev_region(&dev, cdev_minor, CDEV_COUNT,
				  ESCORE_CDEV_NAME);
	if (err) {
		dev_err(escore->dev, "unable to allocate char dev = %d", err);
		goto err_chrdev;
	}
	cdev_major = MAJOR(dev);
	dev_dbg(escore->dev, "char dev major = %d", cdev_major);

	/* register device class */

	cdev_class = class_create(THIS_MODULE, ESCORE_CDEV_NAME);
	if (IS_ERR(cdev_class)) {
		err = PTR_ERR(cdev_class);
		dev_err(escore->dev, "unable to create %s class = %d\n",
			ESCORE_CDEV_NAME, err);
		goto err_class;
	}

	err = create_cdev(escore, &escore->cdev_command, &command_fops,
			  CDEV_COMMAND);
	if (err)
		goto err_command;
	dev_dbg(escore->dev, "command cdev initialized.\n");

	err = create_cdev(escore, &escore->cdev_firmware, &firmware_fops,
			  CDEV_FIRMWARE);
	if (err)
		goto err_firmware;
	dev_dbg(escore->dev, "firmware cdev initialized.\n");

	err = create_cdev(escore, &escore->cdev_streaming, &streaming_fops,
			  CDEV_STREAMING);
	if (err)
		goto err_streaming;
	dev_dbg(escore->dev, "streaming cdev initialized.\n");

	err = create_cdev(escore, &escore->cdev_datablock, &datablock_fops,
			  CDEV_DATABLOCK);
	if (err)
		goto err_datablock;
	dev_dbg(escore->dev, "datablock cdev initialized.\n");

	err = create_cdev(escore, &escore->cdev_datalogging, &datalogging_fops,
			  CDEV_DATALOGGING);
	if (err)
		goto err_datalogging;
	dev_dbg(escore->dev, "%s(): datalogging cdev initialized.\n", __func__);

	err = create_cdev(escore, &escore->cdev_cmd_history, &cmd_history_fops,
			CDEV_CMD_HISTORY);
	if (err)
		goto err_cmd_history;

	dev_dbg(escore->dev, "%s(): cmd_history cdev initialized.\n", __func__);

	return err;

err_cmd_history:
	cdev_destroy(&escore->cdev_datalogging, CDEV_DATALOGGING);
err_datalogging:
	cdev_destroy(&escore->cdev_datablock, CDEV_DATABLOCK);
err_datablock:
	cdev_destroy(&escore->cdev_streaming, CDEV_STREAMING);
err_streaming:
	cdev_destroy(&escore->cdev_firmware, CDEV_FIRMWARE);
err_firmware:
	cdev_destroy(&escore->cdev_command, CDEV_COMMAND);
err_command:
	class_destroy(cdev_class);
err_class:
	unregister_chrdev_region(MKDEV(cdev_major, cdev_minor), CDEV_COUNT);
err_chrdev:
	dev_err(escore->dev, "setup failure: no cdevs available!\n");
	return err;
}

void escore_cdev_cleanup(struct escore_priv *escore)
{
	cdev_destroy(&escore->cdev_datalogging, CDEV_DATALOGGING);
	cdev_destroy(&escore->cdev_streaming, CDEV_STREAMING);
	cdev_destroy(&escore->cdev_firmware, CDEV_FIRMWARE);
	cdev_destroy(&escore->cdev_command, CDEV_COMMAND);
	cdev_destroy(&escore->cdev_command, CDEV_DATABLOCK);
	class_destroy(cdev_class);
	unregister_chrdev_region(MKDEV(cdev_major, cdev_minor), CDEV_COUNT);
}
