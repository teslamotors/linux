/*
 * LR388K7 touchscreen driver
 *
 * Copyright (C) 2014, Sharp Corporation
 * Copyright (c) 2015-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Author: Makoto Itsuki <itsuki.makoto@sharp.co.jp>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pm.h>
#include <linux/spi/spi.h>
#include <linux/spi/lr388k7_ts.h>
#include <linux/input/mt.h>
#include <linux/miscdevice.h>
#include <linux/sysfs.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/spinlock_types.h>

/* DEBUG */
#ifndef DEBUG_LR388K7
#define DEBUG_LR388K7
#endif

/* ACTIVE */
/* #define ACTIVE_ENABLE */
/* UDEV */
#define UDEV_ENABLE


#ifdef __min
#undef __min
#define __min(x, y) ((x) < (y) ? (x) : (y))
#else
#define __min(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifdef __max
#undef __max
#define __max(x, y) ((x) > (y) ? (x) : (y))
#else
#define __max(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifdef __negchk
#undef __negchk
#define __negchk(a) ((a) < 0 ? 0 : (a))
#else
#define __negchk(a) ((a) < 0 ? 0 : (a))
#endif


#define MAX_16BIT			0xffff
#define MAX_12BIT			0xfff
#define MAX_10BIT			0x3ff
#define K7_MAX_TOUCH_NUM (10)
#define K7_MAX_SPEED_HZ (25000000)
#define K7_RD_HEADER_SIZE (5)
#define K7_WR_OPCODE (0x02)
#define K7_RD_OPCODE (0x0B)
#define K7_STATE_CTL_ADDR (0x000024)
#define K7_INT_STATUS_ADDR (0x000000)
#define K7_IND_DONE_ADDR (0x0003E1)
#define K7_DATA_OFFSET (0x1000)
#define K7_DATA_HEADER_WORD_SIZE (0x08)
#define K7_DATA_HEADER_BYTE_SIZE (K7_DATA_HEADER_WORD_SIZE * 2)
#define K7_DATA_READ_SIZE (K7_DATA_HEADER_BYTE_SIZE + K7_RD_HEADER_SIZE)
#define K7_DATA_SIZE (8800 + K7_DATA_READ_SIZE)
#define K7_CMD_DATA_OFFSET (0x030400)
#define K7_CMD_DATA_SIZE (251 + K7_RD_HEADER_SIZE)
#define K7_Q_SIZE (8)
#define K7_DRIVER_VERSION (18)
#define K7_INT_STATUS_MASK_DATA (0x01)
#define K7_INT_STATUS_MASK_BOOT (0x02)
#define K7_INT_STATUS_MASK_CMD  (0x04)
#define K7_INT_STATUS_MASK_ERR  (0x08)
#define K7_INT_STATUS_MASK_CRC  (0x10)
#define K7_POWER_CTL_TAP_WAKEUP (0x00)
#define K7_POWER_CTL_SLEEP (0x01)
#define K7_POWER_CTL_RESUME (0x02)
#define K7_REMOTE_WAKEUP_CODE (0xEA)
#define K7_DEFAULT_MODE (3)

enum K7_SCAN_STATE_OPTIONS {
	K7_SCAN_STATE_IDLE = 0,
	K7_SCAN_STATE_ACTIVE,
};

enum K7_SLOW_SCAN_OPTIONS {
	K7_SLOW_SCAN_30 = 30,
	K7_SLOW_SCAN_60 = 60,
	K7_SLOW_SCAN_100 = 100,
	K7_SLOW_SCAN_120 = 120,
};

enum K7_NUM_TAP_OPTIONS {
	K7_NUM_TAP_2 = 2,
	K7_NUM_TAP_3,
	K7_NUM_TAP_4,
};

struct lr388k7_ts_parameter {
	u32 u32_pid;
	bool b_is_init_finish;
	bool b_is_calc_finish;
	bool b_is_suspended;
	bool b_is_reset;
	bool b_is_disabled;
	u32 u32SCK;
	u16 u16fw_ver;
	u16 u16module_ver;
	struct workqueue_struct *st_wq_k7;
	struct work_struct st_work_k7;
};

struct lr388k7 {
	struct spi_device	*spi;
	struct device           *dev;
	struct input_dev	*idev;
	struct pinctrl		*pinctrl;
	struct pinctrl_state	*spi_intf_en;
	struct pinctrl_state	*spi_intf_dis;

#if defined(ACTIVE_ENABLE)
	struct input_dev	*idev_active;
	int                     tool;
#endif
	unsigned int            gpio_clk_sel;
	char			phys[32];
	struct mutex		mutex;
	unsigned int		irq;
	unsigned int            gpio_reset;
	unsigned int            gpio_irq;
	spinlock_t		lock;
	struct timer_list	timer;
	unsigned int            max_num_touch;
	int                     max_x;
	int                     max_y;
	int                     max_z;
	int                     platform_id;
	bool                    flip_x;
	bool                    flip_y;
	bool                    swap_xy;
	bool			opened;
	bool			suspended;
	bool                    b_eraser_active;
	struct clk              *clk;
	struct regulator        *regulator_3v3;
	struct regulator        *regulator_1v8;
};

struct lr388k7_queue_info {
	u8(*p_queue)[K7_DATA_SIZE];
	u16 u16_front;
	u16 u16_rear;
};

struct lr388k7_cmd_queue_info {
	u8(*p_queue)[K7_CMD_DATA_SIZE];
	u16 u16_front;
	u16 u16_rear;
};

struct lr388k7_touch_report {
	u8 u8_num_of_touch;
	struct st_touch_point_t tc[K7_MAX_TOUCH_NUM];
};

#if defined(ACTIVE_ENABLE)
struct lr388k7_active_report {
	s16 x;
	s16 y;
	u16 z;
	u8 status;
};
#endif


#if defined(UDEV_ENABLE)
struct lr388k7_udev_event {
	u8 str[512];
};
#endif

static int dev_open(struct inode *inode, struct file *filp);
static int dev_release(struct inode *inode, struct file *filp);
static ssize_t
dev_read(struct file *filp, char __user *buf, size_t count, loff_t *pos);
static ssize_t
dev_write(struct file *filp, const char __user *buf,
	  size_t count, loff_t *pos);
static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

static const struct file_operations dev_fops = {
	.owner = THIS_MODULE,
	.open = dev_open,
	.release = dev_release,
	.read = dev_read,
	.write = dev_write,
	.unlocked_ioctl = dev_ioctl,
	.compat_ioctl = dev_ioctl,
};

static struct miscdevice lr388k7_ts_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "touch",
	.fops = &dev_fops,
};

static struct spi_device *g_spi;
static struct lr388k7_ts_parameter g_st_state;
static struct lr388k7_queue_info g_st_q;
static struct lr388k7_cmd_queue_info g_st_cmd_q;
static struct lr388k7_ts_dbg g_st_dbg;
static u8 g_u8_mode;
static u8 g_u8_scan;
static u16 g_u16_wakeup_enable;
static u32 g_u32_raw_data_size = 50 * 88 * 2;

#define IS_DBG (g_st_dbg.u8ForceCap == 1 || \
		g_st_dbg.u8Dump == 1 || \
		g_st_dbg.slowscan.enable == 1 ? true : false)

static int lr388k7_spi_write(u8 *txbuf, size_t len);
static int lr388k7_spi_read(u8 *txbuf, u8 *rxbuf, size_t len);
static void lr388k7_init_parameter(void);
static long lr388k7_spi_read_cmd_data(u8 *p, size_t count);
static int lr388k7_input_enable(struct input_dev *idev);
static int lr388k7_input_disable(struct input_dev *idev);

static void lr388k7_queue_reset(void)
{
	g_st_q.u16_rear = 0;
	g_st_q.u16_front = 0;
	g_st_cmd_q.u16_rear = 0;
	g_st_cmd_q.u16_front = 0;
}

static int lr388k7_hardreset(unsigned long wait)
{
	struct lr388k7 *ts = spi_get_drvdata(g_spi);
	if (!ts)
		return false;

	gpio_set_value_cansleep(ts->gpio_reset, 0);
	g_st_state.b_is_reset = true;
	usleep_range(wait * 1000, wait * 1000 + 1000);
	gpio_set_value_cansleep(ts->gpio_reset, 1);
	g_st_state.b_is_reset = false;
	return true;
}

#if defined(ACTIVE_ENABLE)
static int lr388k7_set_clk_sel(unsigned int sel)
{
	struct lr388k7 *ts = spi_get_drvdata(g_spi);

	if (!ts)
		return false;

	if (!gpio_is_valid(ts->gpio_clk_sel))
		return false;

	if (!sel)
		gpio_set_value_cansleep(ts->gpio_clk_sel, 0);
	else
		gpio_set_value_cansleep(ts->gpio_clk_sel, 1);
	return true;
}
#endif

static int lr388k7_queue_init(void)
{
	lr388k7_queue_reset();
	/*
	 * Memory Allocation
	 */
	g_st_q.p_queue = kmalloc(K7_DATA_SIZE * K7_Q_SIZE, GFP_KERNEL);
	if (g_st_q.p_queue == NULL)
		return -ENOMEM;

	g_st_cmd_q.p_queue = kmalloc(K7_CMD_DATA_SIZE * K7_Q_SIZE, GFP_KERNEL);
	if (g_st_cmd_q.p_queue == NULL) {
		kfree(g_st_q.p_queue);
		return -ENOMEM;
	}

	return 0;
}

static void lr388k7_queue_free(void)
{
	if (!g_st_q.p_queue)
		return;
	kfree(g_st_q.p_queue);
	g_st_q.p_queue = NULL;

	if (!g_st_cmd_q.p_queue)
		return;
	kfree(g_st_cmd_q.p_queue);
	g_st_cmd_q.p_queue = NULL;
}


static int lr388k7_queue_is_full(void)
{
	u16 u16_front = g_st_q.u16_front;
	if (g_st_q.u16_rear + 1 == u16_front)
		return 1;

	if ((g_st_q.u16_rear == (K7_Q_SIZE - 1)) && (u16_front == 0))
		return 1;

	return 0;
}

static int lr388k7_queue_is_empty(void)
{
	if (g_st_q.u16_rear == g_st_q.u16_front)
		return 1;
	return 0;
}

static int lr388k7_cmd_queue_is_full(void)
{
	u16 u16_front = g_st_cmd_q.u16_front;
	if (g_st_cmd_q.u16_rear + 1 == u16_front)
		return 1;

	if ((g_st_cmd_q.u16_rear == (K7_Q_SIZE - 1)) && (u16_front == 0))
		return 1;

	return 0;
}

static int lr388k7_cmd_queue_is_empty(void)
{
	if (g_st_cmd_q.u16_rear == g_st_cmd_q.u16_front)
		return 1;
	return 0;
}

static void *lr388k7_enqueue_start(void)
{
	if (!g_st_q.p_queue)
		return NULL;

	if (!lr388k7_queue_is_full())
		return &g_st_q.p_queue[g_st_q.u16_rear];

	return NULL;
}

static void lr388k7_enqueue_finish(void)
{
	if (g_st_q.u16_rear == (K7_Q_SIZE - 1))
		g_st_q.u16_rear = 0;
	else
		g_st_q.u16_rear++;
}

static void *lr388k7_dequeue_start(void)
{
	if (!lr388k7_queue_is_empty())
		return &g_st_q.p_queue[g_st_q.u16_front];

	return NULL;
}

static void lr388k7_dequeue_finish(void)
{
	if (g_st_q.u16_front == (K7_Q_SIZE - 1))
		g_st_q.u16_front = 0;
	else
		g_st_q.u16_front++;
}

static int lr388k7_queue_read_raw_data(u8 *p, u32 u32_len)
{
	u8 *p_queue;
	u32 u32_ret;
	p_queue = lr388k7_dequeue_start();
	if (!p_queue)
		return 0;

	u32_ret = copy_to_user(p, p_queue, u32_len);
	if (u32_ret != 0)
		return 0;

	lr388k7_dequeue_finish();
	return 1;
}

static void *lr388k7_cmd_enqueue_start(void)
{
	if (!g_st_cmd_q.p_queue)
		return NULL;

	if (!lr388k7_cmd_queue_is_full())
		return &g_st_cmd_q.p_queue[g_st_cmd_q.u16_rear];

	return NULL;
}

static void lr388k7_cmd_enqueue_finish(void)
{
	if (g_st_cmd_q.u16_rear == (K7_Q_SIZE - 1))
		g_st_cmd_q.u16_rear = 0;
	else
		g_st_cmd_q.u16_rear++;
}

static void *lr388k7_cmd_dequeue_start(void)
{
	if (!lr388k7_cmd_queue_is_empty())
		return &g_st_cmd_q.p_queue[g_st_cmd_q.u16_front];

	return NULL;
}

static void lr388k7_cmd_dequeue_finish(void)
{
	if (g_st_cmd_q.u16_front == (K7_Q_SIZE - 1))
		g_st_cmd_q.u16_front = 0;
	else
		g_st_cmd_q.u16_front++;
}

static int lr388k7_cmd_queue_read(u8 *p, u32 u32_len)
{
	u8 *p_queue;
	u32 u32_ret;
	p_queue = lr388k7_cmd_dequeue_start();
	if (!p_queue)
		return 0;

	u32_ret = copy_to_user(p, p_queue, u32_len);
	if (u32_ret != 0)
		return 0;

	lr388k7_cmd_dequeue_finish();
	return 1;
}

static u8 lr388k7_clear_irq(void)
{
	u8 u8_tx_buf[K7_RD_HEADER_SIZE + 1], u8_rx_buf[K7_RD_HEADER_SIZE + 1];
	u8 u8Ret = 0;
	size_t count = 1;

	if (g_st_state.b_is_reset)
		return u8Ret;

	u8_tx_buf[0] = K7_RD_OPCODE;
	u8_tx_buf[1] = (K7_INT_STATUS_ADDR >> 16) & 0xFF;
	u8_tx_buf[2] = (K7_INT_STATUS_ADDR >>  8) & 0xFF;
	u8_tx_buf[3] = (K7_INT_STATUS_ADDR >>  0) & 0xFF;
	u8_tx_buf[4] = 0x00;

	g_st_state.u32SCK = 0;

	if (lr388k7_spi_read(u8_tx_buf, u8_rx_buf, count))
		u8Ret = u8_rx_buf[K7_RD_HEADER_SIZE];

	g_st_state.u32SCK = g_spi->max_speed_hz;

	return u8Ret;
}

static long lr388k7_ts_get_mode(u8 *p)
{
	ssize_t status = 0;

	if (p == 0) {
		status = -EFAULT;
		goto exit_get_mode;
	}

	if (copy_to_user(p, &g_u8_mode, sizeof(u8)))
		status = -EFAULT;

 exit_get_mode:
	return status;
}

static long lr388k7_ts_get_debug_status(u8 *p)
{
	ssize_t status = 0;

	if (p == 0) {
		status = -EFAULT;
		goto exit_get_debug_status;
	}

	if (copy_to_user(p, &g_st_dbg, sizeof(struct lr388k7_ts_dbg)))
		status = -EFAULT;

 exit_get_debug_status:
	return status;
}

static void lr388k7_indicate_done(void)
{
#if 0
	u8 u8_buf[5];
	size_t count = 0;

	u8_buf[count++] = K7_WR_OPCODE;
	u8_buf[count++] = (K7_IND_DONE_ADDR >> 16) & 0xFF;
	u8_buf[count++] = (K7_IND_DONE_ADDR >>  8) & 0xFF;
	u8_buf[count++] = (K7_IND_DONE_ADDR >>  0) & 0xFF;
	u8_buf[count++] = 0x01;
	lr388k7_spi_write(u8_buf, count);
#endif
}

static int lr388k7_read_data(u8 *p)
{
	u8 *p_tx_buf;
	size_t count = 0;
	ssize_t status = 0;

	/*
	 * Memory Allocation
	 */
	count = (size_t)g_u32_raw_data_size;

	p_tx_buf = kmalloc(K7_DATA_READ_SIZE + count, GFP_KERNEL);
	if (p_tx_buf == NULL) {
		status = -ENOMEM;
		goto exit_read_data;
	}

	p_tx_buf[0] = K7_RD_OPCODE;
	p_tx_buf[1] = (K7_DATA_OFFSET >> 16) & 0xFF;
	p_tx_buf[2] = (K7_DATA_OFFSET >>  8) & 0xFF;
	p_tx_buf[3] = (K7_DATA_OFFSET >>  0) & 0xFF;
	p_tx_buf[4] = 0x00;

	if (lr388k7_spi_read(p_tx_buf, p, K7_DATA_HEADER_BYTE_SIZE + count))
		status = count;
	else{
		status = -EFAULT;
		goto exit_read_data_free_tx;
	}

 exit_read_data_free_tx:
	kfree(p_tx_buf);
 exit_read_data:
	if (status < 0)
		return 0;
	return 1;
}

int lr388k7_send_signal(int pid, int n_info)
{
	struct siginfo info;
	struct task_struct *t;
	int ret = 0;

	static DEFINE_MUTEX(lock);

	if (!pid)
		return ret;

	mutex_lock(&lock);

	/*
	 * Set signal parameters
	 */
	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = LR388K7_SIGNAL;
	info.si_code = SI_QUEUE;
	info.si_int = n_info;/*real time signals may have 32 bits of data. */

	rcu_read_lock();
	/* To avoid link error for module build, it was replaced by pid_task.
	t = find_task_by_vpid(pid);
	*/
	t = pid_task(find_vpid(pid), PIDTYPE_PID);
	rcu_read_unlock();
	if (t == NULL) {
		dev_err(&g_spi->dev, " : no such pid\n");
		ret = -ENODEV;
	} else{
		ret = send_sig_info(LR388K7_SIGNAL, &info, t);
	}

	if (ret < 0)
		dev_err(&g_spi->dev, " : sending signal\n");

	mutex_unlock(&lock);

	return ret;
}

static void lr388k7_read_spec_size(void)
{
	int n_ret;
	u8 *p_buf;

	p_buf = kmalloc(K7_DATA_SIZE, GFP_KERNEL);
	if (p_buf) {
		n_ret = lr388k7_read_data((u8 *)p_buf);
		kfree(p_buf);
	} else {
		dev_err(&g_spi->dev, " : %d\n", -ENOMEM);
	}
}

static void lr388k7_event_handler(u8 u8Status)
{
	void *p_q_buf;
	void *p_cmd_q_buf;
	int n_ret;

	if (u8Status & K7_INT_STATUS_MASK_BOOT) {
		/*
		 * Let module know device has booted up.
		 */
#if defined(DEBUG_LR388K7)
		dev_info(&g_spi->dev, "boot\n");
#endif
		lr388k7_send_signal(g_st_state.u32_pid, LR388K7_SIGNAL_BOOT);
	}

	if (u8Status & K7_INT_STATUS_MASK_CRC)
		lr388k7_send_signal(g_st_state.u32_pid, LR388K7_SIGNAL_CRCE);

	if (u8Status & K7_INT_STATUS_MASK_DATA) {

		/*
		 * Handle data
		 */
		if (g_st_state.b_is_init_finish) {

			/*
			 * Get pointer of queue buffer
			 */
			p_q_buf = lr388k7_enqueue_start();
			if (p_q_buf) {
				n_ret = lr388k7_read_data((u8 *)p_q_buf);
				if (n_ret)
					lr388k7_enqueue_finish();
			} else {
				lr388k7_read_spec_size();
			}
		} else {
			lr388k7_read_spec_size();
		}

		if (g_st_state.b_is_calc_finish) {
			g_st_state.b_is_calc_finish = 0;
			lr388k7_send_signal(
					    g_st_state.u32_pid,
					    LR388K7_SIGNAL_INTR
					    );
		}
		lr388k7_indicate_done();
	}

	if (u8Status & K7_INT_STATUS_MASK_CMD) {

		/*
		 * Process command result
		 */
		p_cmd_q_buf = lr388k7_cmd_enqueue_start();
		if (p_cmd_q_buf) {
			n_ret = lr388k7_spi_read_cmd_data(
							  (u8 *)p_cmd_q_buf,
							  K7_CMD_DATA_SIZE
							  - K7_RD_HEADER_SIZE
							  );
			if (n_ret) {
				lr388k7_cmd_enqueue_finish();
				lr388k7_send_signal(
						    g_st_state.u32_pid,
						    LR388K7_SIGNAL_CMDR
						    );
			}
		}
	}
}

static void lr388k7_work_handler(struct work_struct *work)
{
	struct lr388k7 *ts = spi_get_drvdata(g_spi);
	u8 u8_status;
	int irq_value;

	if (g_st_state.b_is_suspended) {
		dev_dbg(
			&g_spi->dev,
			"[WARN] work handler is called regardless of b_is_suspended\n");
		return;
	}

	mutex_lock(&ts->mutex);

	/*
	 * Clear Interrupt
	 */
	u8_status = lr388k7_clear_irq();

	while (u8_status) {

		lr388k7_event_handler(u8_status);

		/*
		 * Check if irq is already cleared.
		 */
		irq_value = gpio_get_value(ts->gpio_irq);

		if (!irq_value)
			u8_status = lr388k7_clear_irq();
		else
			u8_status = 0;
	}

	mutex_unlock(&ts->mutex);
}

static irqreturn_t lr388k7_wakeup_thread(int irq, void *_ts)
{
	struct lr388k7 *ts = (struct lr388k7 *)_ts;
	struct input_dev *input_dev = ts->idev;
	u8 u8_status;

	dev_info(&g_spi->dev, "[ENTER] Wakeup thread\n");

	/* just try to clear */
	u8_status = lr388k7_clear_irq();

	dev_info(&g_spi->dev, "Tap wakeup\n");

	input_report_key(input_dev, KEY_POWER, 1);
	input_sync(input_dev);
	input_report_key(input_dev, KEY_POWER, 0);
	input_sync(input_dev);

	if (g_st_state.b_is_suspended)
		g_st_state.b_is_suspended = false;

	dev_info(&g_spi->dev, "[EXIT] Wakeup thread\n");

	return IRQ_HANDLED;
}

static irqreturn_t lr388k7_irq_thread(int irq, void *_ts)
{
	struct lr388k7 *ts = (struct lr388k7 *)_ts;
	struct input_dev *input_dev = ts->idev;

	if (g_u8_scan == K7_SCAN_STATE_IDLE) {
		input_event(input_dev, EV_MSC, MSC_ACTIVITY, 1);
		input_sync(input_dev);
	}

	if (g_st_state.b_is_suspended) {
		if (g_st_dbg.wakeup.enable == 1) {
			dev_info(&g_spi->dev, "Throw IRQ_WAKE_THREAD\n");
			return IRQ_WAKE_THREAD;
		}
		return IRQ_HANDLED;
	}

	queue_work(g_st_state.st_wq_k7, &g_st_state.st_work_k7);
	return IRQ_HANDLED;
}

/* must be called with ts->mutex held */
static void __lr388k7_disable(struct lr388k7 *ts)
{
	disable_irq(ts->irq);
}

/* must be called with ts->mutex held */
static void __lr388k7_enable(struct lr388k7 *ts)
{
	enable_irq(ts->irq);
}

static ssize_t lr388k7_ts_force_cap_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	return snprintf(buf, 4, "%d\n", g_st_dbg.u8ForceCap);
}

static ssize_t lr388k7_ts_force_cap_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf,
					  size_t count)
{
	ssize_t ret;

	ret = (ssize_t)count;

	if (g_st_state.b_is_suspended)
		return ret;

	if (count != 1)
		return ret;

	if (buf[0] == '0')
		g_st_dbg.u8ForceCap = 0;
	else if (buf[0] == '1')
		g_st_dbg.u8ForceCap = 1;

	lr388k7_send_signal(g_st_state.u32_pid, LR388K7_SIGNAL_CTRL);

	return ret;
}

static ssize_t lr388k7_ts_dump_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	return snprintf(buf, 4, "%d\n", g_st_dbg.u8Dump);
}

static ssize_t lr388k7_ts_dump_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t count)
{
	ssize_t ret;

	ret = (ssize_t)count;

	if (g_st_state.b_is_suspended)
		return ret;

	if (count != 1)
		return ret;

	if (buf[0] == '0')
		g_st_dbg.u8Dump = 0;
	else if (buf[0] == '1')
		g_st_dbg.u8Dump = 1;

	lr388k7_send_signal(g_st_state.u32_pid, LR388K7_SIGNAL_CTRL);

	return ret;
}

static ssize_t lr388k7_ts_report_mode_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	return snprintf(buf, 4, "%d\n", g_u8_mode);
}

static ssize_t lr388k7_ts_report_mode_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t count)
{
	ssize_t ret;
	u8 prev_mode = g_u8_mode;

	ret = (ssize_t)count;

	if (count != 2)
		return ret;

	if (buf[0] >= 0x30 && buf[0] <= 0x36)
		g_u8_mode = (u8)buf[0] - 0x30;

	if (prev_mode != g_u8_mode)
		lr388k7_send_signal(g_st_state.u32_pid,
				    LR388K7_SIGNAL_MODE);

	return ret;
}

static ssize_t lr388k7_ts_version_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return snprintf(buf, PAGE_SIZE, "FW %d, Driver %d, Module %d\n",
		       g_st_state.u16fw_ver,
		       K7_DRIVER_VERSION,
		       g_st_state.u16module_ver
		       );
}

static ssize_t lr388k7_ts_version_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}


static ssize_t lr388k7_ts_slowscan_enable_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
#ifdef ENABLE_SLOW_SCAN
	unsigned long val;
	ssize_t err;
	ssize_t ret;
	if (count < 2)
		return -EINVAL;

	if (g_st_state.b_is_suspended)
		return -EINVAL;

	ret = (ssize_t) count;

	if (count == 2) {
		if (buf[0] == '0') {
			g_st_dbg.slowscan.enable = 0;
			lr388k7_send_signal(g_st_state.u32_pid,
					    LR388K7_SIGNAL_CTRL);
		} else if (buf[0] == '1') {
			g_st_dbg.slowscan.enable = 1;
			lr388k7_send_signal(g_st_state.u32_pid,
					    LR388K7_SIGNAL_CTRL);
		}
	} else if ((buf[0] == '2') && (buf[1] == ' ')) {
		err = kstrtoul(&buf[2], 10, &val);

		if (err) {
			ret = err;
		} else {
			/* check if prefined value*/
			if ((val == K7_SLOW_SCAN_30) ||
			    (val == K7_SLOW_SCAN_60) ||
			    (val == K7_SLOW_SCAN_100) ||
			    (val == K7_SLOW_SCAN_120)) {
				g_st_dbg.slowscan.scan_rate = (u16)val;
				g_st_dbg.slowscan.enable = 1;
				lr388k7_send_signal(g_st_state.u32_pid,
						    LR388K7_SIGNAL_CTRL);
			}
		}
	}

	return ret;
#else
	return count;
#endif
}

static ssize_t lr388k7_ts_slowscan_enable_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
#ifdef ENABLE_SLOW_SCAN
	return snprintf(buf, PAGE_SIZE, "Slow Scan:%s Scan Rate:%dHz\n",
		       g_st_dbg.slowscan.enable ?
		       "Enabled" : "Disabled",
		       g_st_dbg.slowscan.scan_rate);
#else
	return snprintf(buf, PAGE_SIZE, "Not implemented yet\n");
#endif
}

static ssize_t lr388k7_ts_wakeup_enable_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	ssize_t ret;

	if (count < 2)
		return -EINVAL;

	if (g_st_state.b_is_suspended)
		return -EINVAL;

	ret = (ssize_t) count;

	if (count == 2) {
		if (buf[0] == '0') {
			g_st_dbg.wakeup.enable = 0;
			lr388k7_send_signal(g_st_state.u32_pid,
					    LR388K7_SIGNAL_CTRL);
		} else if (buf[0] == '2') {
			if (g_st_dbg.wakeup.enable <= 1)
				g_u16_wakeup_enable = g_st_dbg.wakeup.enable;
			g_st_dbg.wakeup.enable = 2;
			lr388k7_send_signal(g_st_state.u32_pid,
					    LR388K7_SIGNAL_CTRL);
		}
	} else if ((buf[0] == '1') && (buf[1] == ' ')) {
		/* check if prefined value*/
		if ((buf[2] == '2') ||
		    (buf[2] == '3') ||
		    (buf[2] == '4')) {
			g_st_dbg.wakeup.num_tap = buf[2] - 0x30;
			g_st_dbg.wakeup.enable = 1;
			lr388k7_send_signal(g_st_state.u32_pid,
					    LR388K7_SIGNAL_CTRL);
		}
	}

	return ret;
}

static ssize_t lr388k7_ts_wakeup_enable_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			"wakeup_enable:%s(%d) Number of taps:%d\n",
		       g_st_dbg.wakeup.enable == 1 ?
		       "Enabled" : "Disabled",
		       g_st_dbg.wakeup.enable,
		       g_st_dbg.wakeup.num_tap);
}

static ssize_t lr388k7_ts_test_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	ssize_t ret;

	if (count != 2)
		return -EINVAL;

	ret = (ssize_t) count;

	if (buf[0] == '1') {
		g_st_dbg.u8Test = 1;
		lr388k7_send_signal(g_st_state.u32_pid,
				    LR388K7_SIGNAL_CTRL);
	} else if (buf[0] == '0') {
		g_st_dbg.u8Test = 0;
		lr388k7_send_signal(g_st_state.u32_pid,
				    LR388K7_SIGNAL_CTRL);
	}

	return ret;
}

static ssize_t lr388k7_ts_test_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", g_st_dbg.u8Test == 1 ?
		       "Enabled" : "Disabled");
}

#if defined(DEBUG_LR388K7)
static ssize_t lr388k7_ts_check_state_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	ssize_t ret;
	ret = (ssize_t) count;
	return ret;
}

static ssize_t lr388k7_ts_check_state_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct lr388k7 *ts = spi_get_drvdata(g_spi);
	u8 u8_tx_buf[K7_RD_HEADER_SIZE + 5], u8_rx_buf[K7_RD_HEADER_SIZE + 5];
	u8 u8Ret;
	u8 u8HWR;
	u32 u32RES, u32FWR;
	size_t count = 0;

	u8_tx_buf[count++] = K7_RD_OPCODE;
	u8_tx_buf[count++] = (K7_STATE_CTL_ADDR >> 16) & 0xFF;
	u8_tx_buf[count++] = (K7_STATE_CTL_ADDR >>  8) & 0xFF;
	u8_tx_buf[count++] = (K7_STATE_CTL_ADDR >>  0) & 0xFF;
	u8_tx_buf[count++] = 0x00;

	g_st_state.u32SCK = 0;

	if (lr388k7_spi_read(u8_tx_buf, u8_rx_buf, count)) {
		u8Ret = u8_rx_buf[K7_RD_HEADER_SIZE];
	} else {
		return snprintf(buf, PAGE_SIZE,
			"status :IRQ(%s) Failed to read\n",
		       gpio_get_value(ts->gpio_irq) ?
		       "High" : "Low"
		       );
	}

	count = 0;
	u8_tx_buf[count++] = K7_RD_OPCODE;
	u8_tx_buf[count++] = 0x00;
	u8_tx_buf[count++] = 0x02;
	u8_tx_buf[count++] = 0xAC;
	u8_tx_buf[count++] = 0x00;

	if (lr388k7_spi_read(u8_tx_buf, u8_rx_buf, count)) {
		u8HWR = u8_rx_buf[K7_RD_HEADER_SIZE];
	} else {
		g_st_state.u32SCK = g_spi->max_speed_hz;
		return snprintf(buf, PAGE_SIZE,
			"status :IRQ(%s) Failed to read\n",
		       gpio_get_value(ts->gpio_irq) ?
		       "High" : "Low"
		       );
	}

	count = 0;
	u8_tx_buf[count++] = K7_RD_OPCODE;
	u8_tx_buf[count++] = 0x00;
	u8_tx_buf[count++] = 0x02;
	u8_tx_buf[count++] = 0xB0;
	u8_tx_buf[count++] = 0x00;
	u8_tx_buf[count++] = 0x00;
	u8_tx_buf[count++] = 0x00;
	u8_tx_buf[count++] = 0x00;

	if (lr388k7_spi_read(u8_tx_buf, u8_rx_buf, count)) {
		u32FWR = u8_rx_buf[K7_RD_HEADER_SIZE] |
			u8_rx_buf[K7_RD_HEADER_SIZE + 1] << 8 |
			u8_rx_buf[K7_RD_HEADER_SIZE + 2] << 16 |
			u8_rx_buf[K7_RD_HEADER_SIZE + 3] << 24;
	} else {
		g_st_state.u32SCK = g_spi->max_speed_hz;
		return snprintf(buf, PAGE_SIZE,
			"status :IRQ(%s) Failed to read\n",
		       gpio_get_value(ts->gpio_irq) ?
		       "High" : "Low"
		       );
	}

	count = 0;
	u8_tx_buf[count++] = K7_RD_OPCODE;
	u8_tx_buf[count++] = 0x00;
	u8_tx_buf[count++] = 0x00;
	u8_tx_buf[count++] = 0x40;
	u8_tx_buf[count++] = 0x00;
	u8_tx_buf[count++] = 0x00;
	u8_tx_buf[count++] = 0x00;
	u8_tx_buf[count++] = 0x00;

	if (lr388k7_spi_read(u8_tx_buf, u8_rx_buf, count)) {
		u32RES = u8_rx_buf[K7_RD_HEADER_SIZE] |
			u8_rx_buf[K7_RD_HEADER_SIZE + 1] << 8 |
			u8_rx_buf[K7_RD_HEADER_SIZE + 2] << 16 |
			u8_rx_buf[K7_RD_HEADER_SIZE + 3] << 24;
	} else {
		g_st_state.u32SCK = g_spi->max_speed_hz;
		return snprintf(buf, PAGE_SIZE,
			"status :IRQ(%s) Failed to read\n",
		       gpio_get_value(ts->gpio_irq) ?
		       "High" : "Low"
		       );
	}

	g_st_state.u32SCK = g_spi->max_speed_hz;

	return snprintf(
		       buf, PAGE_SIZE,
		       "IRQ(%s) status=0x%02X, HWRev=%d, FWRev=0x%0X, Res=0x%04X\n",
		       gpio_get_value(ts->gpio_irq) ?
		       "High" : "Low",
		       u8Ret,
		       u8HWR,
		       u32FWR,
		       u32RES
		       );
}
#endif

#define LR388K7_LOG_MAX_SIZE (2 * 1024 * 1024)
#define LR388K7_LOG_DATA_SIZE (512)

struct lr388k7_log {
	unsigned long seq_num;
	unsigned long time;
	unsigned char data[LR388K7_LOG_DATA_SIZE];
};

DEFINE_SPINLOCK(lr388k7log_lock);
#define LR388K7_LOG_ENTRY (LR388K7_LOG_MAX_SIZE / sizeof(struct lr388k7_log))
static struct lr388k7_log glog[LR388K7_LOG_ENTRY];
static int log_size = sizeof(glog) / sizeof(glog[0]);
static unsigned long g_seq_num;
static int g_log_front;
static int g_log_rear;


static ssize_t lr388k7_ts_log_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	ssize_t ret;
	struct lr388k7_log log;
	unsigned long flags;

	ret = (ssize_t) count;

	g_seq_num++;
	log.seq_num = g_seq_num;
	log.time = jiffies;
	memcpy(log.data, buf, sizeof(log.data));
	spin_lock_irqsave(&lr388k7log_lock, flags);

	if (((g_log_rear + 1) % log_size) == g_log_front) {
		g_log_front++;
		if (g_log_front >= log_size)
			g_log_front = 0;
		g_log_rear++;
		if (g_log_rear >= log_size)
			g_log_rear = 0;
	} else {
		g_log_rear++;
		if (g_log_rear >= log_size)
			g_log_rear = 0;
	}

	glog[g_log_rear] = log;

	spin_unlock_irqrestore(&lr388k7log_lock, flags);

	return ret;
}

static ssize_t lr388k7_ts_log_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	char *s;
	unsigned long flags;
	struct lr388k7_log log;

	s = buf;

	snprintf(s, PAGE_SIZE,
		"FW %d, Driver %d, Module %d\n",
		g_st_state.u16fw_ver,
		K7_DRIVER_VERSION,
		g_st_state.u16module_ver
		);

	s += strlen(s);

	for (; (s - buf) + LR388K7_LOG_DATA_SIZE < PAGE_SIZE; ) {
		spin_lock_irqsave(&lr388k7log_lock, flags);

		if (g_log_rear == g_log_front) {
			spin_unlock_irqrestore(&lr388k7log_lock, flags);
			return (ssize_t)(s - buf);
		}

		g_log_front++;
		if (g_log_front >= log_size)
			g_log_front = 0;
		log = glog[g_log_front];
		spin_unlock_irqrestore(&lr388k7log_lock, flags);

		snprintf(s, PAGE_SIZE, "[%08lx|%08lx] %s",
			log.seq_num,
			log.time,
			log.data);
		s += strlen(s);
	}

	return (ssize_t)(s - buf);
}

static DEVICE_ATTR(force_cap, 0660, lr388k7_ts_force_cap_show,
			lr388k7_ts_force_cap_store);
static DEVICE_ATTR(dump, 0660, lr388k7_ts_dump_show,
			lr388k7_ts_dump_store);
static DEVICE_ATTR(report_mode, 0640, lr388k7_ts_report_mode_show,
			lr388k7_ts_report_mode_store);
static DEVICE_ATTR(version, 0640,
		   lr388k7_ts_version_show,
		   lr388k7_ts_version_store);
static DEVICE_ATTR(slowscan_enable, 0640,
		   lr388k7_ts_slowscan_enable_show,
		   lr388k7_ts_slowscan_enable_store);
static DEVICE_ATTR(wakeup_enable, 0640,
		   lr388k7_ts_wakeup_enable_show,
		   lr388k7_ts_wakeup_enable_store);
static DEVICE_ATTR(test, 0640,
		   lr388k7_ts_test_show,
		   lr388k7_ts_test_store);
#if defined(DEBUG_LR388K7)
static DEVICE_ATTR(check_state, 0660,
		   lr388k7_ts_check_state_show,
		   lr388k7_ts_check_state_store);

static DEVICE_ATTR(log, 0660,
		   lr388k7_ts_log_show,
		   lr388k7_ts_log_store);
#endif

static struct attribute *lr388k7_ts_attributes[] = {
	&dev_attr_force_cap.attr,
	&dev_attr_dump.attr,
	&dev_attr_report_mode.attr,
	&dev_attr_version.attr,
	&dev_attr_slowscan_enable.attr,
	&dev_attr_wakeup_enable.attr,
	&dev_attr_test.attr,
#if defined(DEBUG_LR388K7)
	&dev_attr_check_state.attr,
	&dev_attr_log.attr,
#endif
	NULL
};

static const struct attribute_group lr388k7_ts_attr_group = {
	.attrs = lr388k7_ts_attributes,
};

static int lr388k7_open(struct input_dev *input)
{
	struct lr388k7 *ts = input_get_drvdata(input);

	dev_info(&ts->spi->dev, "[ENTER] open\n");

	mutex_lock(&ts->mutex);

	__lr388k7_enable(ts);

	ts->opened = true;

	mutex_unlock(&ts->mutex);

	dev_info(&ts->spi->dev, "[EXIT] open\n");
	return 0;
}

static void lr388k7_close(struct input_dev *input)
{
	struct lr388k7 *ts = input_get_drvdata(input);

	dev_info(&ts->spi->dev, "[ENTER] close\n");
	mutex_lock(&ts->mutex);

	__lr388k7_disable(ts);

	ts->opened = false;

	mutex_unlock(&ts->mutex);
	dev_info(&ts->spi->dev, "[EXIT] close\n");
}


#if defined(UDEV_ENABLE)
static void lr388k7_send_udev_event(void *p)
{
	struct lr388k7_udev_event *p_udev_event;
	char *envp[2];
	char *event;

	if (!p)
		return;

	p_udev_event = kmalloc(
			       sizeof(struct lr388k7_udev_event),
			       GFP_KERNEL);

	if (!p_udev_event)
		return;

	if (copy_from_user(p_udev_event,
			  p,
			  sizeof(struct lr388k7_udev_event))) {
		kfree(p_udev_event);
		return;
	}

	event = kasprintf(GFP_KERNEL, "TRIGGER=%s", p_udev_event->str);
	if (event) {
		envp[0] = event;
		envp[1] = NULL;
		kobject_uevent_env(&lr388k7_ts_miscdev.this_device->kobj,
				   KOBJ_CHANGE,
				   envp);
		kfree(event);
	}
	kfree(p_udev_event);
}
#endif

#if defined(ACTIVE_ENABLE)
static void lr388k7_active_report(void *p)
{
	struct lr388k7_active_report *p_active_report;
	struct lr388k7 *ts = spi_get_drvdata(g_spi);
	struct input_dev *input_dev = ts->idev_active;
	int x, y, z;
	int tool = 0;
	u8 status;

	if (!p)
		return;

	/*
	 * Memory Allocation
	 */
	p_active_report = kmalloc(
				 sizeof(struct lr388k7_active_report),
				 GFP_KERNEL);
	if (!p_active_report)
		return;

	if (copy_from_user(p_active_report,
			   p,
			   sizeof(struct lr388k7_active_report))) {
		kfree(p_active_report);
		return;
	}

	status = p_active_report->status;

	/* Get current tool */
	if (status & 0x0C)
		tool = BTN_TOOL_RUBBER;
	else
		tool = BTN_TOOL_PEN;

	if (!status)
		tool = 0; /* Reset */

	/* Check if tool changed then notify input subsystem */
	if (ts->tool != tool) {
		if (ts->tool) {
			/* Reset old tool state */
			input_report_key(input_dev, BTN_TOUCH, 0);
			input_report_key(input_dev, BTN_STYLUS, 0);
			input_report_abs(input_dev, ABS_PRESSURE, 0);
		}
		input_report_key(input_dev, ts->tool, 0);
		input_sync(input_dev);

		/* Update current*/
		ts->tool = tool;
		if (tool)
			input_report_key(input_dev, tool, 1);
	}

	if (tool) {
		x = p_active_report->x;
		y = p_active_report->y;
		z = p_active_report->z;

		input_report_abs(input_dev, ABS_X, x);
		input_report_abs(input_dev, ABS_Y, y);
		input_report_abs(input_dev, ABS_PRESSURE, z);
		input_report_key(input_dev, BTN_TOUCH, status & 0x09);
		input_report_key(input_dev, BTN_STYLUS, status & 0x10);
		input_sync(input_dev);
	}

	kfree(p_active_report);
}
#endif

#if defined(PROTOCOL_A)
static void lr388k7_touch_report(void *p)
{
	u8 u8_num, i, u8_num_of_touch;
	struct lr388k7_touch_report *p_touch_report;
	struct lr388k7 *ts = spi_get_drvdata(g_spi);
	struct input_dev *input_dev = ts->idev;
	bool b_is_eraser = false;

	if (!p)
		return;

	/*
	 * Memory Allocation
	 */
	p_touch_report = kmalloc(
				 sizeof(struct lr388k7_touch_report),
				 GFP_KERNEL);
	if (!p_touch_report)
		return;

	if (copy_from_user(p_touch_report,
			   p,
			   sizeof(struct lr388k7_touch_report))) {
		kfree(p_touch_report);
		return;
	}

	u8_num = p_touch_report->u8_num_of_touch;

	if (u8_num == 0) {
		if (ts->b_eraser_active) {
			ts->b_eraser_active = false;
			input_report_key(input_dev, BTN_TOOL_RUBBER, 0);
		}
		input_mt_sync(input_dev);
		input_sync(input_dev);
		kfree(p_touch_report);
		return;
	}
	u8_num_of_touch = 0;

	for (i = 0; i < u8_num; i++) {

#if defined(DEBUG_LR388K7_REPORT)
		dev_info(&g_spi->dev, "ID=%2d, status=%02d, x=%5d, y=%5d, w=%d, h=%d, z=%5d, num=%2d\n",
			 p_touch_report->tc[i] . id,
			 p_touch_report->tc[i] . status,
			 p_touch_report->tc[i] . x,
			 p_touch_report->tc[i] . y,
			 p_touch_report->tc[i] . width,
			 p_touch_report->tc[i] . height,
			 p_touch_report->tc[i] . z,
			 u8_num
			 );
#endif

		if ((p_touch_report->tc[i] . status & 0x80) != 0)
			b_is_eraser = true;
		else
			b_is_eraser = false;

		if (b_is_eraser) {
			input_report_key(input_dev, BTN_TOOL_RUBBER, 1);
			ts->b_eraser_active = true;
		} else if ((p_touch_report->tc[i] . status & 0x7F) <= 3 ||
			  (p_touch_report->tc[i] . status & 0x7F) == 8)
			input_report_abs(input_dev,
					 ABS_MT_TOOL_TYPE, MT_TOOL_FINGER);
		else if ((p_touch_report->tc[i] . status & 0x7F) <= 7 ||
			 (p_touch_report->tc[i] . status & 0x7F) == 12)
			input_report_abs(input_dev,
					 ABS_MT_TOOL_TYPE, MT_TOOL_PEN);
		input_report_abs(input_dev,
				 ABS_MT_TRACKING_ID,
				 p_touch_report->tc[i] .id);
		input_report_abs(input_dev,
				 ABS_MT_POSITION_X,
				 p_touch_report->tc[i] . x);
		input_report_abs(input_dev,
				 ABS_MT_POSITION_Y,
				 p_touch_report->tc[i] . y);
		input_report_abs(input_dev,
				 ABS_MT_PRESSURE,
				 p_touch_report->tc[i] . z);
		input_mt_sync(input_dev);
		u8_num_of_touch++;
	}
	input_sync(input_dev);

	kfree(p_touch_report);
}
#else /* PROTOCOL_B */
static void lr388k7_touch_report(void *p)
{
	int num, i;
	struct lr388k7_touch_report touch_report;
	struct lr388k7 *ts = spi_get_drvdata(g_spi);
	struct input_dev *input_dev = ts->idev;

	if (!p)
		return;

	if (copy_from_user((void *)&touch_report,
			   p,
			   sizeof(struct lr388k7_touch_report))) {
		return;
	}

	num = touch_report.u8_num_of_touch > K7_MAX_TOUCH_NUM ?
		K7_MAX_TOUCH_NUM : touch_report.u8_num_of_touch;

	for (i = 0; i < num; i++) {

#if defined(DEBUG_LR388K7_REPORT)
		dev_info(&g_spi->dev, "ID=%2d, status=%02d, x=%5d, y=%5d, w=%d, h=%d, z=%5d, num=%2d\n",
			 touch_report.tc[i] . id,
			 touch_report.tc[i] . status,
			 touch_report.tc[i] . x,
			 touch_report.tc[i] . y,
			 touch_report.tc[i] . width,
			 touch_report.tc[i] . height,
			 touch_report.tc[i] . z,
			 num
			 );
#endif
		input_mt_slot(input_dev,
			      touch_report.tc[i] . id);
		if (((touch_report.tc[i] . status & 0x7F) == 8) ||
		    ((touch_report.tc[i] . status & 0x7F) == 12) ||
		    ((touch_report.tc[i] . status & 0x7F) == 14)) {
			input_mt_report_slot_state(input_dev,
						   MT_TOOL_FINGER,
						   false);
			continue;
		}

		if ((touch_report.tc[i] . status & 0x7F) <= 3)
			input_report_abs(input_dev,
					 ABS_MT_TOOL_TYPE, MT_TOOL_FINGER);
		else if (((touch_report.tc[i] . status & 0x7F) <= 7) ||
			 ((touch_report.tc[i] . status & 0x7F) > 12))
			input_report_abs(input_dev,
					 ABS_MT_TOOL_TYPE, MT_TOOL_PEN);

		if (ts->flip_x)
			touch_report.tc[i].x =
				__negchk((ts->max_x - touch_report.tc[i].x));
		if (ts->flip_y)
			touch_report.tc[i].y =
				__negchk((ts->max_y - touch_report.tc[i].y));
		if (ts->swap_xy) {
			u16 tmp;
			tmp = touch_report.tc[i].x;
			touch_report.tc[i].x = touch_report.tc[i].y;
			touch_report.tc[i].y = tmp;
		}

		input_report_abs(input_dev,
				 ABS_MT_TRACKING_ID,
				 touch_report.tc[i] .id);
		input_report_abs(input_dev,
				 ABS_MT_POSITION_X,
				 touch_report.tc[i].x);
		input_report_abs(input_dev,
				 ABS_MT_POSITION_Y,
				 touch_report.tc[i].y);
		input_report_abs(input_dev,
				 ABS_MT_PRESSURE,
				 touch_report.tc[i] . z);
	}
	input_sync(input_dev);
}
#endif /* #if defined(PROTOCOL_A) */


static int access_num;
static spinlock_t dev_spin_lock;

static int dev_open(struct inode *inode, struct file *filp)
{
#if defined(DEBUG_LR388K7)
	dev_info(&g_spi->dev, "dev_open\n");
#endif

	spin_lock(&dev_spin_lock);

	if (access_num) {
		spin_unlock(&dev_spin_lock);
		dev_err(&g_spi->dev, "already open : %d\n", -EBUSY);
		return -EBUSY;
	}

	access_num++;
	spin_unlock(&dev_spin_lock);

	return 0;
}

static int dev_release(struct inode *inode, struct file *filp)
{
	struct lr388k7 *ts = spi_get_drvdata(g_spi);

#if defined(DEBUG_LR388K7)
	dev_info(&g_spi->dev, "dev_release\n");
#endif
	if (!ts)
		return -EINVAL;

	g_st_state.b_is_init_finish = 0;

	spin_lock(&dev_spin_lock);
	access_num--;
	spin_unlock(&dev_spin_lock);

	/* Reset assert */
	gpio_set_value_cansleep(ts->gpio_reset, 0);
	g_st_state.b_is_reset = true;

	return 0;
}

/**
 * @brief lr388k7 spi interface
 * @param[in] txbuf command to read data
 * @param[in] rxbuf data to be read
 * @param[in] len length of data
 * @retval TRUE if success, otherwise FALSE
 */
static int lr388k7_spi_read(u8 *txbuf, u8 *rxbuf, size_t len)
{
	static DEFINE_MUTEX(lock);

	int status;
	struct spi_message msg;
	struct spi_transfer t = {
		.bits_per_word	= 8,
		.tx_buf = txbuf,
		.rx_buf = rxbuf,
		.speed_hz =
		g_st_state.u32SCK == 0 ? 12000000 : g_spi->max_speed_hz,
	};

	mutex_lock(&lock);

	t.len = len + K7_RD_HEADER_SIZE;

	spi_message_init(&msg);
	spi_message_add_tail(&t, &msg);
	status = spi_sync(g_spi, &msg);

	mutex_unlock(&lock);

	if (status)
		return false;

	return true;
}

static long lr388k7_spi_read_cmd_data(u8 *p, size_t count)
{
	u8 *p_tx_buf;
	ssize_t status = 0;

	/*
	 * Memory Allocation
	 */
	p_tx_buf = kmalloc(count + K7_RD_HEADER_SIZE, GFP_KERNEL);
	if (p_tx_buf == NULL) {
		status = -ENOMEM;
		goto exit_dev_read_cmd_data;
	}

	/*
	 * Clear and Set command to read data
	 */
	memset(p_tx_buf, 0, count + K7_RD_HEADER_SIZE);

	p_tx_buf[0] = K7_RD_OPCODE;
	p_tx_buf[1] = (K7_CMD_DATA_OFFSET >> 16) & 0xFF;
	p_tx_buf[2] = (K7_CMD_DATA_OFFSET >>  8) & 0xFF;
	p_tx_buf[3] = (K7_CMD_DATA_OFFSET >>  0) & 0xFF;
	p_tx_buf[4] = 0x00;

	/*
	 * Read data
	 */
	if (lr388k7_spi_read(p_tx_buf, p, count))
		status = count;
	else
		status = -EFAULT;

	kfree(p_tx_buf);

 exit_dev_read_cmd_data:
	return status;
}

static long lr388k7_spi_read_wo_limit(u8 *p, size_t count)
{
	u8 *p_buf;
	u8 *p_tx_buf;
	ssize_t status = 0;

	/*
	 * Memory Allocation
	 */
	p_buf = kmalloc(count + K7_RD_HEADER_SIZE, GFP_KERNEL);
	if (p_buf == NULL) {
		status = -ENOMEM;
		goto ext_dev_read_wo_limit;
	}

	p_tx_buf = kmalloc(count + K7_RD_HEADER_SIZE, GFP_KERNEL);
	if (p_tx_buf == NULL) {
		status = -ENOMEM;
		goto ext_dev_read_wo_limit_free;
	}

	/*
	 * Clear and Set command to read data
	 */
	memset(p_tx_buf, 0, count + K7_RD_HEADER_SIZE);

	if (copy_from_user(p_tx_buf, p, K7_RD_HEADER_SIZE)) {
		status = -EFAULT;
		goto ext_dev_read_wo_limit_free_tx;
	}

	/*
	 * Read data
	 */
	if (lr388k7_spi_read(p_tx_buf, p_buf, count)) {
		if (copy_to_user(p, &p_buf[K7_RD_HEADER_SIZE], count)) {
			status = -EFAULT;
			goto ext_dev_read_wo_limit_free_tx;
		}

		status = count;

	} else
		status = -EFAULT;

 ext_dev_read_wo_limit_free_tx:
	kfree(p_tx_buf);
 ext_dev_read_wo_limit_free:
	kfree(p_buf);
 ext_dev_read_wo_limit:
	return status;
}

static long lr388k7_spi_read_from_ioctl(u8 *p, size_t count)
{
	u8 *p_buf;
	u8 *p_tx_buf;
	ssize_t status = 0;

	/*
	 * Memory Allocation
	 */
	p_buf = kmalloc(count + K7_RD_HEADER_SIZE, GFP_KERNEL);
	if (p_buf == NULL) {
		status = -ENOMEM;
		goto ext_dev_read_from_ioctl;
	}

	p_tx_buf = kmalloc(count + K7_RD_HEADER_SIZE, GFP_KERNEL);
	if (p_tx_buf == NULL) {
		status = -ENOMEM;
		goto ext_dev_read_from_ioctl_free;
	}

	/*
	 * Clear and Set command to read data
	 */
	memset(p_tx_buf, 0, count + K7_RD_HEADER_SIZE);

	if (copy_from_user(p_tx_buf, p, K7_RD_HEADER_SIZE)) {
		status = -EFAULT;
		goto ext_dev_read_from_ioctl_free_tx;
	}

	g_st_state.u32SCK = 0;
	/*
	 * Read data
	 */
	if (lr388k7_spi_read(p_tx_buf, p_buf, count)) {
		if (copy_to_user(p, &p_buf[K7_RD_HEADER_SIZE], count)) {
			status = -EFAULT;
			goto ext_dev_read_from_ioctl_free_tx;
		}

		status = count;

	} else
		status = -EFAULT;

 ext_dev_read_from_ioctl_free_tx:
	kfree(p_tx_buf);
 ext_dev_read_from_ioctl_free:
	kfree(p_buf);
 ext_dev_read_from_ioctl:
	g_st_state.u32SCK = g_spi->max_speed_hz;
	return status;
}

static ssize_t
dev_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
#if 0
	u8 *p_buf;
	ssize_t status = 0;

	p_buf = kmalloc(count + K7_RD_HEADER_SIZE, GFP_KERNEL);
	if (!p_buf) {
		status = -ENOMEM;
		goto exit_dev_read;
	}

	memset(p_buf, 0, count + K7_RD_HEADER_SIZE);

	if (copy_from_user(p_buf, buf, K7_RD_HEADER_SIZE)) {
		status = -EFAULT;
		goto exit_dev_read_free;
	}

	if (lr388k7_spi_read(p_buf, count)) {
		if (copy_to_user(buf, &p_buf[K7_RD_HEADER_SIZE], count)) {
			status = -EFAULT;
			goto exit_dev_read_free;
		}

		status = count;

	} else
		status = -EFAULT;

 exit_dev_read_free:
	kfree(p_buf);
 exit_dev_read:
	return status;
#else
	return 0;
#endif
}

/**
 * @brief lr388k7 spi interface
 * @param[in] txbuf data to be written
 * @param[in] len length of data
 * @retval TRUE if success, otherwise FALSE
 */
static int lr388k7_spi_write(u8 *txbuf, size_t len)
{
	static DEFINE_MUTEX(lock);
	int status;
	struct spi_message msg;
	struct spi_transfer t = {
		.bits_per_word = 8,
		.tx_buf = txbuf,
		.len = len,
		.speed_hz =
		len < 16 ? 12000000 : g_spi->max_speed_hz,
	};

	mutex_lock(&lock);

	spi_message_init(&msg);
	spi_message_add_tail(&t, &msg);
	/*It returns zero on succcess,else a negative error code. */
	status = spi_sync(g_spi, &msg);

	mutex_unlock(&lock);

	if (status)
		return false;

	return true;
}

static u32 lr388k7_get_platform_id(u8 *p)
{
	u32 ret;
	struct lr388k7 *ts = spi_get_drvdata(g_spi);

	ret = copy_to_user(p,
			   &ts->platform_id,
			   sizeof(ts->platform_id));

	return ret;
}

static u32 lr388k7_get_value(u8 *arg, unsigned int index)
{
	u32 ret = 0;
	switch (index) {
	case LR388K7_GET_PLATFORM_ID:
		ret = lr388k7_get_platform_id(arg);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}


static ssize_t
dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *pos)
{
	u8 *p_buf;
	ssize_t status = 0;

	p_buf = kmalloc(count, GFP_KERNEL);
	if (!p_buf) {
		status = -ENOMEM;
		goto exit_dev_write;
	}

	if (copy_from_user(p_buf, buf, count)) {
		status = -EFAULT;
		goto exit_dev_write_free;
	}

	if (lr388k7_spi_write(p_buf, count))
		status = count;
	else
		status = -EFAULT;

 exit_dev_write_free:
	kfree(p_buf);
 exit_dev_write:
	return status;
}

static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = true;
	unsigned int index;

	index = (cmd >> 16) & 0xFFFF;

	switch (cmd & 0xFFFF) {
	case LR388K7_IOCTL_TOUCH_REPORT:
		lr388k7_touch_report((void *) arg);
		break;

	case LR388K7_IOCTL_READ_REGISTER:
		ret = lr388k7_spi_read_from_ioctl((u8 *) arg, index);
		break;

	case LR388K7_IOCTL_READ_WO_LIMIT:
		ret = lr388k7_spi_read_wo_limit((u8 *) arg, index);
		break;

	case LR388K7_IOCTL_SET_HAL_PID:
		g_st_state.u32_pid = arg;
		g_st_state.b_is_calc_finish = 1;
		break;

	case LR388K7_IOCTL_READ_RAW_DATA:
		ret = lr388k7_queue_read_raw_data((u8 *) arg, index);
		break;

	case LR388K7_IOCTL_READ_CMD_DATA:
		ret = lr388k7_cmd_queue_read((u8 *) arg, index);
		break;

	case LR388K7_IOCTL_FINISH_CALC:
		g_st_state.b_is_calc_finish = 1;
		break;

	case LR388K7_IOCTL_SET_RAW_DATA_SIZE:
		g_u32_raw_data_size = arg;
		break;

	case LR388K7_IOCTL_SET_REVISION:
		g_st_state.u16fw_ver = (arg & 0xFFFF0000) >> 16;
		g_st_state.u16module_ver = arg & 0x0000FFFF;

		dev_info(&g_spi->dev, "Version %d.%d.%d\n",
			 g_st_state.u16fw_ver,
			 K7_DRIVER_VERSION,
			 g_st_state.u16module_ver
			 );
		break;

	case LR388K7_IOCTL_GET_DEBUG_STATUS:
		lr388k7_ts_get_debug_status((u8 *)arg);
		break;

	case LR388K7_IOCTL_SET_MODE:
		g_u8_mode = (u8)arg;
		break;

	case LR388K7_IOCTL_GET_MODE:
		lr388k7_ts_get_mode((u8 *)arg);
		break;

	case LR388K7_IOCTL_RESET:
		ret = lr388k7_hardreset(arg);
		break;

	case LR388K7_IOCTL_CLEAR_BUFFER:
		lr388k7_queue_reset();
		g_st_state.b_is_init_finish = arg;
		if (g_st_state.b_is_init_finish)
			lr388k7_send_signal(g_st_state.u32_pid,
					    LR388K7_SIGNAL_MODE);
		break;

	case LR388K7_IOCTL_ACTIVE_REPORT:
#if defined(ACTIVE_ENABLE)
		lr388k7_active_report((void *) arg);
#endif
		break;

	case LR388K7_IOCTL_GET_VALUE:
		ret = lr388k7_get_value((u8 *)arg, index);
		break;

	case LR388K7_IOCTL_SET_CLK_SEL:
#if defined(ACTIVE_ENABLE)
		lr388k7_set_clk_sel((unsigned int)arg);
#endif
		break;

	case LR388K7_IOCTL_SET_STATE:
		g_u8_scan = (u8)arg;
		break;

#if defined(UDEV_ENABLE)
	case LR388K7_IOCTL_SEND_UDEV:
		lr388k7_send_udev_event((void *)arg);
		break;
#endif

	default:
		ret = false;
		break;
	}

	return ret;
}

static void lr388k7_init_parameter(void)
{
	unsigned long flags;
	g_u8_mode = K7_DEFAULT_MODE;
	g_u8_scan = K7_SCAN_STATE_IDLE;
	g_st_state.u32_pid = 0;
	g_st_state.b_is_init_finish = false;
	g_st_state.b_is_suspended = false;
	g_st_state.b_is_reset = false;
	g_st_state.b_is_disabled = false;
	g_st_state.u32SCK = 0;
	lr388k7_queue_reset();

	g_st_dbg.slowscan.enable = 0;
	g_st_dbg.slowscan.scan_rate = K7_SLOW_SCAN_60;
	g_st_dbg.wakeup.enable = 0;
	g_st_dbg.wakeup.num_tap = K7_NUM_TAP_2;
	g_st_dbg.u8ForceCap = 0;
	g_st_dbg.u8Dump = 0;
	g_st_dbg.u8Test = 0;
	g_u16_wakeup_enable = 0;

	access_num = 0;

	spin_lock_irqsave(&lr388k7log_lock, flags);
	g_log_front = 0;
	g_log_rear = 0;
	spin_unlock_irqrestore(&lr388k7log_lock, flags);
}

static void lr388k7_init_ts(void)
{
	lr388k7_init_parameter();
	memset(&g_st_state, 0, sizeof(struct lr388k7_ts_parameter));

	g_st_state.st_wq_k7 = create_singlethread_workqueue("lr388k7_work");
	INIT_WORK(&g_st_state.st_work_k7, lr388k7_work_handler);
}

static struct lr388k7_platform_data *lr388k7_parse_dt(struct device *dev,
						      int irq)
{
	struct lr388k7_platform_data *pdata;
	struct device_node *np = dev->of_node;
	int ret, val, irq_gpio;
#if defined(ACTIVE_ENABLE)
	const char *str;
#endif
	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->gpio_reset = of_get_named_gpio_flags(np, "reset-gpio", 0, NULL);
	if (!gpio_is_valid(pdata->gpio_reset)) {
		dev_err(dev, "Invalid reset-gpio\n");
		return ERR_PTR(-EINVAL);
	}
	ret = gpio_request(pdata->gpio_reset, "reset-gpio");
	if (ret < 0) {
		dev_err(dev, "gpio_request failed\n");
		return ERR_PTR(-EINVAL);
	}
	gpio_direction_output(pdata->gpio_reset, 0);

	ret = of_property_read_u32(np, "interrupts", &irq_gpio);
	if (!gpio_is_valid(irq_gpio)) {
		dev_err(dev, "Invalid irq-gpio\n");
		ret = -EINVAL;
		goto exit_release_reset_gpio;
	}
	ret = gpio_request(irq_gpio, "irq-gpio");
	if (ret < 0) {
		dev_err(dev, "irq_request fail\n");
		ret = -EINVAL;
		goto exit_release_reset_gpio;
	}
	gpio_direction_input(irq_gpio);
	pdata->gpio_irq = irq_gpio;

	ret = of_property_read_u32(np, "swap-xy", &val);
	if (ret < 0)
		val = 0;
	pdata->ts_swap_xy = val != 0 ? 1 : 0;

	ret = of_property_read_u32(np, "flip-x", &val);
	if (ret < 0)
		val = 0;
	pdata->ts_flip_x = val != 0 ? 1 : 0;

	ret = of_property_read_u32(np, "flip-y", &val);
	if (ret < 0)
		val = 0;
	pdata->ts_flip_y = val != 0 ? 1 : 0;

	ret = of_property_read_u32(np, "x-max", &val);
	if (ret < 0)
		goto exit_release_all_gpio;
	pdata->ts_x_max = val;

	ret = of_property_read_u32(np, "y-max", &val);
	if (ret < 0)
		goto exit_release_all_gpio;
	pdata->ts_y_max = val;

	ret = of_property_read_u32(np, "z-max", &val);
	if (ret < 0)
		goto exit_release_all_gpio;
	pdata->ts_pressure_max = val;

	ret = of_property_read_u32(np, "touch-num-max", &val);
	if (ret < 0)
		goto exit_release_all_gpio;
	pdata->ts_touch_num_max = val;

#if defined(ACTIVE_ENABLE)
	ret = of_property_read_string(np, "name-of-clock", &str);
	if (ret < 0)
		goto exit_release_all_gpio;
	pdata->name_of_clock = (char *)str;

	ret = of_property_read_string(np, "name-of-clock-con", &str);
	if (ret < 0)
		goto exit_release_all_gpio;
	pdata->name_of_clock_con = (char *)str;

#endif
	pdata->gpio_clk_sel = of_get_named_gpio_flags(np,
						      "clock-sel-gpio",
						      0,
						      NULL);
	if (gpio_is_valid(pdata->gpio_clk_sel)) {
		ret = gpio_request(pdata->gpio_clk_sel, "clock-sel-gpio");
		if (ret)
			dev_err(dev, "gpio_request(clk_sel) failed\n");
		else
			gpio_direction_output(pdata->gpio_clk_sel, 0);
	}

	ret = of_property_read_u32(np, "platform-id", &val);
	if (ret < 0)
		goto exit_release_all_gpio;
	pdata->platform_id = val;

	return pdata;

 exit_release_all_gpio:
	gpio_free(irq_gpio);
 exit_release_reset_gpio:
	gpio_free(pdata->gpio_reset);
	return ERR_PTR(ret);
}

static void init_spi_pinctrl(struct lr388k7 *ts, struct device *dev)
{
	struct pinctrl *pin = NULL;
	struct pinctrl_state *active, *inactive;

	pin = devm_pinctrl_get(dev);
	if (IS_ERR(pin)) {
		dev_err(dev, "missing pinctrl device\n");
		return;
	}
	ts->pinctrl = pin;

	active = pinctrl_lookup_state(pin, "spi_intf_normal");
	if (IS_ERR_OR_NULL(active)) {
		dev_err(dev, "missing spi_intf_normal state\n");
		goto out;
	}
	ts->spi_intf_en = active;
	inactive = pinctrl_lookup_state(pin, "spi_intf_lowpower");
	if (IS_ERR_OR_NULL(active)) {
		dev_err(dev, "missing spi_intf_lowpower state\n");
		goto out;
	}
	ts->spi_intf_dis = inactive;

	return;
out:
	if (ts->pinctrl)
		ts->pinctrl = NULL;

}

static int lr388k7_probe(struct spi_device *spi)
{
	struct lr388k7_platform_data *pdata;/* = spi->dev.platform_data;*/
	struct lr388k7 *ts;
	struct input_dev *input_dev;
#if defined(ACTIVE_ENABLE)
	struct input_dev *input_dev_active;
#endif
	struct device *dev;
	int error;

	dev = &spi->dev;
	dev_info(dev, "[ENTER] probe\n");

	lr388k7_init_ts();

	g_spi = spi;

	if (spi->irq <= 0) {
		dev_err(dev, "no irq\n");
		return -ENODEV;
	}

	spi->mode = SPI_MODE_3;
	spi->bits_per_word = 8;
	if (!spi->max_speed_hz)
		spi->max_speed_hz = K7_MAX_SPEED_HZ;

	error = spi_setup(spi);
	if (error)
		return error;

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	input_dev = input_allocate_device();
#if defined(ACTIVE_ENABLE)
	input_dev_active = input_allocate_device();
	if (!ts || !input_dev || !input_dev_active) {
#else
	if (!ts || !input_dev) {
#endif
		error = -ENOMEM;
		goto err_free_mem;
	}


	ts->spi = spi;
	ts->dev = dev;
	ts->idev = input_dev;
#if defined(ACTIVE_ENABLE)
	ts->idev_active = input_dev_active;
#endif
	ts->irq = spi->irq;

	if (dev->of_node) {

		dev_info(dev, "DT support\n");
		/* Loading data from DT */
		pdata = lr388k7_parse_dt(dev, spi->irq);
		if (IS_ERR(pdata)) {
			dev_err(dev, "failed to parse device tree\n");
			error = -EINVAL;
			goto err_free_mem;
		}
		dev->platform_data = pdata;
	} else {
		/* Non-DT */
		dev_info(dev, "Non-DT\n");
	}

	pdata = dev->platform_data;

	ts->max_num_touch = __min(pdata->ts_touch_num_max, K7_MAX_TOUCH_NUM);
	ts->max_x       = pdata->ts_x_max ? : MAX_16BIT;
	ts->max_y       = pdata->ts_y_max ? : MAX_16BIT;
	ts->max_z       = pdata->ts_pressure_max ? : MAX_16BIT;
	ts->swap_xy     = pdata->ts_swap_xy ? true : false;
	ts->flip_x      = pdata->ts_flip_x ? true : false;
	ts->flip_y      = pdata->ts_flip_y ? true : false;
	ts->gpio_reset  = pdata->gpio_reset;
	ts->gpio_irq  = pdata->gpio_irq;
	ts->b_eraser_active = false;
#if defined(ACTIVE_ENABLE)
	ts->tool        = 0;
#endif
	ts->gpio_clk_sel = pdata->gpio_clk_sel;
	ts->platform_id = pdata->platform_id;

	mutex_init(&ts->mutex);

	spin_lock_init(&ts->lock);

	snprintf(ts->phys,
		 sizeof(ts->phys),
		 "%s/input-ts",
		 dev_name(dev));

	/* misc */
	if (misc_register(&lr388k7_ts_miscdev) != 0) {
		dev_err(dev, "cannot register miscdev\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

#if defined(DEBUG_LR388K7)
	dev_info(&spi->dev, "Success register miscdev\n");
#endif

	if (lr388k7_queue_init()) {
		dev_err(dev, "Cannot allocate queue memory\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

	if (gpio_is_valid(ts->gpio_clk_sel)) {
#if defined(ACTIVE_ENABLE)
		/* Select external clock */
		gpio_set_value_cansleep(ts->gpio_clk_sel, 1);
#else
		/* Select internal clock */
		gpio_set_value_cansleep(ts->gpio_clk_sel, 0);
#endif
	}
	/* Reset assert */
	gpio_set_value_cansleep(ts->gpio_reset, 0);
	g_st_state.b_is_reset = true;

	init_spi_pinctrl(ts, dev);

	/* regulator */
	ts->regulator_3v3 = devm_regulator_get(&g_spi->dev, "avdd");
	if (IS_ERR(ts->regulator_3v3)) {
		dev_err(dev,
			"LR388K7 TS: regulator_get failed: %ld\n",
			PTR_ERR(ts->regulator_3v3));
		return -ENODEV;
	}

	ts->regulator_1v8 = devm_regulator_get(&g_spi->dev, "dvdd");
	if (IS_ERR(ts->regulator_1v8)) {
		dev_err(dev,
			"LR388K7 TS: regulator_get failed: %ld\n",
			PTR_ERR(ts->regulator_1v8));
		return -ENODEV;
	}

	/* Enable 1v8 first*/
	error = regulator_enable(ts->regulator_1v8);
	if (error < 0)
		dev_err(dev,
			"LR388K7 TS: regulator enable failed: %d\n",
			error);
	usleep_range(5000, 6000);
	error = regulator_enable(ts->regulator_3v3);
	if (error < 0)
		dev_err(dev,
			"LR388K7 TS: regulator enable failed: %d\n",
			error);

	input_dev->name = "touch";
	input_dev->phys = ts->phys;
	input_dev->id.bustype = BUS_SPI;
	input_dev->dev.parent = &spi->dev;
	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(KEY_POWER, input_dev->keybit);

	if (ts->swap_xy) {
		input_set_abs_params(input_dev,
				     ABS_MT_POSITION_X,
				     0,
				     ts->max_y,
				     0,
				     0);
		input_set_abs_params(input_dev,
				     ABS_MT_POSITION_Y,
				     0,
				     ts->max_x,
				     0,
				     0);
	} else {
		input_set_abs_params(input_dev,
				     ABS_MT_POSITION_X,
				     0,
				     ts->max_x,
				     0,
				     0);
		input_set_abs_params(input_dev,
				     ABS_MT_POSITION_Y,
				     0, ts->max_y,
				     0,
				     0);
	}
	input_set_abs_params(input_dev,
			     ABS_MT_PRESSURE,
			     0,
			     ts->max_z,
			     0,
			     0);
	input_set_abs_params(input_dev,
			     ABS_MT_TOOL_TYPE,
			     0,
			     MT_TOOL_MAX,
			     0,
			     0);
#if defined(PROTOCOL_A)
	__set_bit(BTN_TOOL_RUBBER, input_dev->keybit);

	input_set_abs_params(input_dev,
			     ABS_MT_TRACKING_ID,
			     0,
			     ts->max_num_touch,
			     0,
			     0);
#else /* PROTOCOL B */
	error = input_mt_init_slots(input_dev,
				    ts->max_num_touch,
				    0);
	if (error) {
		dev_err(dev, "Failed to mt_init_slots err: %d\n", error);
		goto err_free_mem;
	}

#endif

	input_set_capability(input_dev, EV_MSC, MSC_ACTIVITY);

	input_dev->open = lr388k7_open;
	input_dev->close = lr388k7_close;
	input_dev->enable = lr388k7_input_enable;
	input_dev->disable = lr388k7_input_disable;
	input_dev->enabled = true;
	input_set_drvdata(input_dev, ts);

#if defined(ACTIVE_ENABLE)
	input_dev_active->name = "touch_active";
	input_dev_active->phys = ts->phys;
	input_dev_active->id.bustype = BUS_SPI;
	input_dev_active->dev.parent = &spi->dev;

	__set_bit(EV_ABS, input_dev_active->evbit);
	__set_bit(EV_KEY, input_dev_active->evbit);
	__set_bit(INPUT_PROP_POINTER, input_dev_active->propbit);
	__set_bit(BTN_TOUCH, input_dev_active->keybit);
	__set_bit(BTN_TOOL_PEN, input_dev_active->keybit);
	__set_bit(BTN_TOOL_RUBBER, input_dev_active->keybit);
	__set_bit(BTN_STYLUS, input_dev_active->keybit);

	if (ts->swap_xy) {
		input_set_abs_params(input_dev_active,
				     ABS_X,
				     0,
				     ts->max_y,
				     0,
				     0);
		input_set_abs_params(input_dev_active,
				     ABS_Y,
				     0,
				     ts->max_x,
				     0,
				     0);
	} else {
		input_set_abs_params(input_dev_active,
				     ABS_X,
				     0,
				     ts->max_x,
				     0,
				     0);
		input_set_abs_params(input_dev_active,
				     ABS_Y,
				     0, ts->max_y,
				     0,
				     0);
	}
	input_set_abs_params(input_dev_active,
			     ABS_PRESSURE,
			     0,
			     MAX_10BIT,
			     0,
			     0);
	input_set_drvdata(input_dev_active, ts);
#endif

	error = request_threaded_irq(spi->irq,
				     lr388k7_irq_thread,
				     lr388k7_wakeup_thread,
				     IRQF_TRIGGER_FALLING,
				     "lr388k7_ts", ts);

	if (error) {
		dev_err(dev, "Failed to request irq, err: %d\n", error);
		goto err_free_mem;
	}

	disable_irq(ts->irq);

	spi_set_drvdata(spi, ts);

	error = sysfs_create_link(&dev->kobj,
				  &lr388k7_ts_miscdev.this_device->kobj,
				  "touch");
	if (error) {
		dev_err(dev, "failed to create sysfs group\n");
		goto err_clear_drvdata;
	}

	error = input_register_device(ts->idev);
	if (error) {
		dev_err(dev,
			"Failed to register input device, err: %d\n", error);
		goto err_clear_drvdata;
	}

#if defined(ACTIVE_ENABLE)
	error = input_register_device(ts->idev_active);
	if (error) {
		dev_err(dev,
			"Failed to register input active device , err: %d\n",
			error);
		goto err_clear_drvdata;
	}
#endif

	error = sysfs_create_group(&lr388k7_ts_miscdev.this_device->kobj,
				   &lr388k7_ts_attr_group);
	if (error) {
		dev_err(dev, "failed to create sysfs group\n");
		goto err_clear_drvdata;
	}

	/* Enable async suspend/resume to reduce LP0 latency */
	device_enable_async_suspend(dev);

	dev_info(dev, "[EXIT] probe\n");
	return 0;

 err_clear_drvdata:
	spi_set_drvdata(spi, NULL);
	free_irq(spi->irq, ts);
 err_free_mem:
#if defined(ACTIVE_ENABLE)
	input_free_device(input_dev_active);
#endif
	input_free_device(input_dev);
	kfree(ts);
	return error;
}

static int lr388k7_remove(struct spi_device *spi)
{
	struct lr388k7 *ts = spi_get_drvdata(spi);

	lr388k7_queue_free();

	if (g_st_state.st_wq_k7)
		destroy_workqueue(g_st_state.st_wq_k7);

	sysfs_remove_group(&lr388k7_ts_miscdev.this_device->kobj,
			   &lr388k7_ts_attr_group);

	sysfs_remove_link(&ts->dev->kobj, "touch");

	misc_deregister(&lr388k7_ts_miscdev);
	free_irq(ts->spi->irq, ts);

	gpio_free(ts->gpio_reset);
	gpio_free(ts->gpio_irq);
	if (gpio_is_valid(ts->gpio_clk_sel))
		gpio_free(ts->gpio_clk_sel);

	input_unregister_device(ts->idev);

	kfree(ts);

	spi_set_drvdata(spi, NULL);
	return 0;
}

static void lr388k7_shutdown(struct spi_device *spi)
{
	struct lr388k7 *ts = spi_get_drvdata(spi);

	free_irq(ts->irq, ts);

	if (ts->regulator_3v3)
		regulator_disable(ts->regulator_3v3);

	if (ts->regulator_1v8)
		regulator_disable(ts->regulator_1v8);
}

static void lr388k7_ctrl_resume(struct lr388k7 *ts)
{
	u8 u8_buf[5];
	size_t count = 0;

	u8_buf[count++] = K7_WR_OPCODE;
	u8_buf[count++] = (K7_STATE_CTL_ADDR >> 16) & 0xFF;
	u8_buf[count++] = (K7_STATE_CTL_ADDR >>  8) & 0xFF;
	u8_buf[count++] = (K7_STATE_CTL_ADDR >>  0) & 0xFF;
	u8_buf[count++] = K7_POWER_CTL_RESUME;
	lr388k7_spi_write(u8_buf, count);
}

static void lr388k7_start(struct lr388k7 *ts)
{
	int error;

	g_st_state.b_is_suspended = false;

	mutex_lock(&ts->mutex);

	if (g_st_dbg.wakeup.enable == 1) {
		lr388k7_ctrl_resume(ts);
		mutex_unlock(&ts->mutex);
		return;
	}

	/* Reset assert */
	gpio_set_value_cansleep(ts->gpio_reset, 0);
	g_st_state.b_is_reset = true;

	/*
	 * Enable regulator, if necessary
	 */
	/* Enable 1v8 first*/
	error = regulator_enable(ts->regulator_1v8);
	if (error < 0)
		dev_err(&g_spi->dev,
			"LR388K7 TS: regulator enable failed: %d\n", error);

	error = regulator_enable(ts->regulator_3v3);
	if (error < 0)
		dev_err(&g_spi->dev,
			"LR388K7 TS: regulator enable failed: %d\n", error);

	usleep_range(5000, 6000);

	if (ts->spi_intf_en)
		pinctrl_select_state(ts->pinctrl,
			ts->spi_intf_en);

	/*
	 * Enable clock, if necessary
	 */
	if (ts->clk)
		clk_enable(ts->clk);

	/* Reset deassert */
	gpio_set_value_cansleep(ts->gpio_reset, 1);
	g_st_state.b_is_reset = false;

	usleep_range(12000, 13000);

	mutex_unlock(&ts->mutex);
}

static void lr388k7_ctrl_suspend(struct lr388k7 *ts)
{
	u8 u8_buf[5];
	size_t count = 0;
	int error;
	u8 u8_status = 0;
	int irq_value;

	mutex_lock(&ts->mutex);

	if (g_st_dbg.wakeup.enable == 1) {
		u8_buf[count++] = K7_WR_OPCODE;
		u8_buf[count++] = (K7_STATE_CTL_ADDR >> 16) & 0xFF;
		u8_buf[count++] = (K7_STATE_CTL_ADDR >>  8) & 0xFF;
		u8_buf[count++] = (K7_STATE_CTL_ADDR >>  0) & 0xFF;
		u8_buf[count++] = K7_POWER_CTL_TAP_WAKEUP;
		lr388k7_spi_write(u8_buf, count);

		irq_value = gpio_get_value(ts->gpio_irq);

		if (!irq_value) {
			u8_status = lr388k7_clear_irq();

			lr388k7_event_handler(u8_status);
		}

		mutex_unlock(&ts->mutex);
		return;
	}

	if (ts->spi_intf_dis)
		pinctrl_select_state(ts->pinctrl,
				ts->spi_intf_dis);

	/* Disable (3.3V) */
	if (ts->regulator_3v3) {
		error = regulator_disable(ts->regulator_3v3);
		if (error < 0)
			dev_err(&g_spi->dev,
				"lr388k7 regulator 3.3V disable failed: %d\n",
				error);
	}
	/* handle platforms w/ and w/out regulator switches */
	/* 2) delay for platforms w/ regulator switches */
	/* usleep_range(15000, 20000);	*/ /*msleep(15); */
	/* 3) disable clock */
	if (ts->clk)
		clk_disable(ts->clk);
	/* 4) disable 1.8 */
	if (ts->regulator_1v8 && ts->regulator_3v3) {
		error = regulator_disable(ts->regulator_1v8);
		if (error < 0)
			dev_err(&g_spi->dev,
				"lr388k7 1.8V disable failed: %d\n",
				error);
	}

	/* Reset assert */
	gpio_set_value_cansleep(ts->gpio_reset, 0);
	g_st_state.b_is_reset = true;

	mutex_unlock(&ts->mutex);
}

static void lr388k7_stop(struct lr388k7 *ts)
{
	flush_workqueue(g_st_state.st_wq_k7);

	lr388k7_ctrl_suspend(ts);
}

static int lr388k7_suspend(struct lr388k7 *ts)
{

	lr388k7_stop(ts);

	lr388k7_send_signal(g_st_state.u32_pid, LR388K7_SIGNAL_SLEP);

	g_st_state.b_is_suspended = true;

	return 0;
}

static int lr388k7_resume(struct lr388k7 *ts)
{
	u8 u8_status = 0;
	int irq_value;

	lr388k7_start(ts);

	lr388k7_send_signal(g_st_state.u32_pid, LR388K7_SIGNAL_WAKE);


	if (g_st_dbg.wakeup.enable == 2)
		g_st_dbg.wakeup.enable = g_u16_wakeup_enable;

	if (IS_DBG) {
		g_st_dbg.u8ForceCap = 0;
		g_st_dbg.u8Dump = 0;
		g_st_dbg.slowscan.enable = 0;

		lr388k7_send_signal(g_st_state.u32_pid, LR388K7_SIGNAL_CTRL);
	}

	if (g_st_dbg.wakeup.enable == 0)
		return 0;

	/* check irq */
	irq_value = gpio_get_value(ts->gpio_irq);
	if (!irq_value) {

		u8_status = lr388k7_clear_irq();

		while (u8_status) {

			lr388k7_event_handler(u8_status);
			irq_value = gpio_get_value(ts->gpio_irq);

			if (!irq_value)
				u8_status = lr388k7_clear_irq();
			else
				u8_status = 0;
		}
	}

	return 0;
}

#ifdef CONFIG_PM
static int lr388k7_dev_pm_suspend(struct device *dev)
{
	struct lr388k7 *ts = dev_get_drvdata(dev);

	if (!g_st_state.b_is_suspended && !g_st_state.b_is_disabled) {
		/* only called when input device is not disabled/enabled via
		 * /sys/class/input/input0/enabled interface.
		 * Android uses sysfs by default and will not run into here
		 */
		lr388k7_suspend(ts);
#if defined(CONFIG_ANDROID)
		dev_info(ts->dev, "disabled without input powerhal support.\n");
#endif
	}
	if (g_st_dbg.wakeup.enable == 1)
		enable_irq_wake(ts->irq);

	return 0;
}

static int lr388k7_dev_pm_resume(struct device *dev)
{
	struct lr388k7 *ts = dev_get_drvdata(dev);

	if (!g_st_state.b_is_disabled) {
		/* only called when input device is not disabled/enabled via
		 * /sys/class/input/input0/enabled interface.
		 * Android uses sysfs by default and will not run into here
		 */
		lr388k7_resume(ts);
#if defined(CONFIG_ANDROID)
		dev_info(ts->dev, "enabled without input powerhal support.\n");
#endif
	}
	if (g_st_dbg.wakeup.enable == 1)
		disable_irq_wake(ts->irq);

	return 0;
}
#endif /*CONFIG_PM */

static int lr388k7_input_enable(struct input_dev *idev)
{
	struct lr388k7 *ts = input_get_drvdata(idev);
	int err = 0;
#if defined(DEBUG_LR388K7)
	dev_info(ts->dev, "[ENTER] input enable\n");
#endif

	g_st_state.b_is_disabled = false;
	err = lr388k7_resume(ts);

#if defined(DEBUG_LR388K7)
	dev_info(ts->dev, "[EXIT] input enable\n");
#endif
	return 0;
}

static int lr388k7_input_disable(struct input_dev *idev)
{
	struct lr388k7 *ts = input_get_drvdata(idev);
	int err = 0;

#if defined(DEBUG_LR388K7)
	dev_info(ts->dev, "[ENTER] input disable\n");
#endif

	err = lr388k7_suspend(ts);
	g_st_state.b_is_disabled = true;

#if defined(DEBUG_LR388K7)
	dev_info(ts->dev, "[EXIT] input disable\n");
#endif
	return 0;
}

#if defined(CONFIG_PM)
static const struct dev_pm_ops lr388k7_pm_ops = {
	.suspend = lr388k7_dev_pm_suspend,
	.resume = lr388k7_dev_pm_resume,
};
#endif

static const struct of_device_id lr388k7_ts_match[] = {
	{ .compatible = "sharp,lr388k7_ts" },
	{ },
};
MODULE_DEVICE_TABLE(of, lr388k7_ts_match);

static struct spi_driver lr388k7_driver = {
	.driver	= {
		.name	= "lr388k7_ts",
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm     = &lr388k7_pm_ops,
#endif
		.of_match_table = lr388k7_ts_match,
	},
	.probe	= lr388k7_probe,
	.remove	= lr388k7_remove,
	.shutdown = lr388k7_shutdown,
};

module_spi_driver(lr388k7_driver);

MODULE_AUTHOR("Makoto Itsuki <itsuki.makoto@sharp.co.jp>");
MODULE_DESCRIPTION("LR388K7 Touchscreen Driver");
MODULE_LICENSE("GPL");
