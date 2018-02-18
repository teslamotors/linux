/*
 * escore-uart-common.c  --  Audience eS705 UART interface
 *
 * Copyright 2013 Audience, Inc.
 *
 * Author: Matt Lupfer <mlupfer@cardinalpeak.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/serial_core.h>
#include <linux/tty.h>
#include <linux/fs.h>

#include "escore.h"
#include "escore-uart-common.h"

int escore_uart_read_internal(struct escore_priv *escore, void *buf, int len)
{
	int rc;
	mm_segment_t oldfs;
	loff_t pos = 0;
	int retry = MAX_EAGAIN_RETRY;

	dev_dbg(escore->dev, "%s() size %d\n", __func__, len);

	/*
	 * we may call from user context via char dev, so allow
	 * read buffer in kernel address space
	 */
	oldfs = get_fs();
	set_fs(KERNEL_DS);

	do {
		rc = vfs_read(escore_uart.file, (char __user *)buf, len, &pos);
		if (rc == -EAGAIN) {
			usleep_range(EAGAIN_RETRY_DELAY,
				EAGAIN_RETRY_DELAY + 50);
			retry--;
		}
	} while (rc == -EAGAIN && retry);

	/* restore old fs context */
	set_fs(oldfs);

	dev_dbg(escore->dev, "%s(): read bytes %d\n", __func__, rc);

	return rc;
}

u32 escore_cpu_to_uart(struct escore_priv *escore, u32 resp)
{
	return cpu_to_be32(resp);
}

u32 escore_uart_to_cpu(struct escore_priv *escore, u32 resp)
{
	return be32_to_cpu(resp);
}

int escore_uart_read(struct escore_priv *escore, void *buf, int len)
{
	int rc;

	if (unlikely(!escore->uart_ready)) {
		pr_err("%s(): Error UART is not open\n", __func__);
		return -EIO;
	}

	rc = escore_uart_read_internal(escore, buf, len);

	if (rc < len) {
		pr_err("%s() Uart Read Failed\n", __func__);
		ESCORE_FW_RECOVERY_FORCED_OFF(escore);
		return -EIO;
	}

	return 0;
}

int escore_uart_write(struct escore_priv *escore, const void *buf, int len)
{
	int rc = 0;
	int count_remain = len;
	int bytes_wr = 0;
	mm_segment_t oldfs;
	loff_t pos = 0;

	if (unlikely(!escore->uart_ready)) {
		pr_err("%s(): Error UART is not open\n", __func__);
		return -EIO;
	}

	dev_dbg(escore->dev, "%s() size %d\n", __func__, len);


	/*
	 * we may call from user context via char dev, so allow
	 * read buffer in kernel address space
	 */
	oldfs = get_fs();
	set_fs(KERNEL_DS);

	while (count_remain > 0) {
		/* block until tx buffer space is available */
		while (tty_write_room(escore_uart.tty) < UART_TTY_WRITE_SZ)
			usleep_range(ES_TTY_BUF_AVAIL_WAIT_DELAY,
					ES_TTY_BUF_AVAIL_WAIT_DELAY + 50);

		rc = vfs_write(escore_uart.file, buf + bytes_wr,
				min(UART_TTY_WRITE_SZ, count_remain), &pos);

		if (rc < 0)
			goto err_out;

		bytes_wr += rc;
		count_remain -= rc;
	}

err_out:
	/* restore old fs context */
	set_fs(oldfs);
	if (count_remain) {
		pr_err("%s() Uart write Failed\n", __func__);
		ESCORE_FW_RECOVERY_FORCED_OFF(escore);
		rc = -EIO;
	} else {
		rc = 0;
	}
	pr_debug("%s() returning %d\n", __func__, rc);

	return rc;
}

int escore_uart_cmd(struct escore_priv *escore, u32 cmd, u32 *resp)
{
	int err = 0;
	int sr = cmd & BIT(28);
	u32 rv;
	int retry = ES_MAX_RETRIES + 1;

	dev_dbg(escore->dev,
			"%s: cmd=0x%08x sr=0x%08x\n", __func__, cmd, sr);
	INC_DISABLE_FW_RECOVERY_USE_CNT(escore);

	*resp = 0;
	cmd = cpu_to_be32(cmd);
	err = escore_uart_write(escore, &cmd, sizeof(cmd));
	if (err || sr)
		goto cmd_exit;

	do {
		usleep_range(ES_RESP_POLL_TOUT,
				ES_RESP_POLL_TOUT + 500);

		err = escore_uart_read(escore, &rv, sizeof(rv));
		dev_dbg(escore->dev, "%s: err=%d\n", __func__, err);
		*resp = be32_to_cpu(rv);
		dev_dbg(escore->dev, "%s: *resp=0x%08x\n", __func__, *resp);
		if (err) {
			dev_dbg(escore->dev,
				"%s: uart_read() fail %d\n", __func__, err);
		} else if ((*resp & ES_ILLEGAL_CMD) == ES_ILLEGAL_CMD) {
			dev_err(escore->dev, "%s: illegal command 0x%08x\n",
				__func__, be32_to_cpu(cmd));
			err = -EINVAL;
			goto cmd_exit;
		} else if (*resp == ES_NOT_READY) {
			dev_dbg(escore->dev,
				"%s: escore_uart_read() not ready\n", __func__);
			err = -EBUSY;
		} else {
			goto cmd_exit;
		}

		--retry;
	} while (retry != 0);

cmd_exit:
	update_cmd_history(be32_to_cpu(cmd), *resp);
	DEC_DISABLE_FW_RECOVERY_USE_CNT(escore);
	if (err && ((*resp & ES_ILLEGAL_CMD) != ES_ILLEGAL_CMD))
		ESCORE_FW_RECOVERY_FORCED_OFF(escore);

	return err;
}

int escore_configure_tty(struct tty_struct *tty, u32 bps, int stop)
{
	int rc = 0;

	struct ktermios termios;
#if defined(KERNEL_VERSION_3_8_0)
	termios = tty->termios;
#else
	termios = *tty->termios;
#endif
	pr_debug("%s(): Requesting baud %u\n", __func__, bps);

	termios.c_cflag &= ~(CBAUD | CSIZE | PARENB);   /* clear csize, baud */
	termios.c_cflag |= BOTHER;	      /* allow arbitrary baud */
	termios.c_cflag |= CS8;

	if (stop == 2)
		termios.c_cflag |= CSTOPB;

	/* set uart port to raw mode (see termios man page for flags) */
	termios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
		| INLCR | IGNCR | ICRNL | IXON);
	termios.c_oflag &= ~(OPOST);
	termios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

	/* set baud rate */
	termios.c_ospeed = bps;
	termios.c_ispeed = bps;

	/* Added to set baudrate dynamically */
	tty_wait_until_sent(escore_uart.tty,
		msecs_to_jiffies(ES_TTY_WAIT_TIMEOUT));

	rc = tty_set_termios(tty, &termios);

#if defined(KERNEL_VERSION_3_8_0)
	 pr_debug("%s(): New baud %u\n", __func__, tty->termios.c_ospeed);
#else
	 pr_debug("%s(): New baud %u\n", __func__, tty->termios->c_ospeed);
#endif
	return rc;
}
EXPORT_SYMBOL_GPL(escore_configure_tty);

int escore_uart_open(struct escore_priv *escore)
{
	long err = 0;
	struct file *fp = NULL;

	/* Timeout increased to 60 Seconds to avoid UART open failure in KK */
	unsigned long timeout = jiffies + msecs_to_jiffies(60000);
	int attempt = 0;

	atomic_inc(&escore->uart_users);

	if (escore->uart_ready) {
		pr_debug("%s() UART is already open, users count: %d\n",
			 __func__, atomic_read(&escore->uart_users));
		return 0;
	}

	dev_dbg(escore->dev, "%s(): start open tty\n", __func__);
	/* try to probe tty node every 100 ms for 6 sec */
	do {
		if (attempt++ > 0)
			msleep(100);
		pr_debug("%s(): probing for tty on %s (attempt %d)\n",
				__func__, UART_TTY_DEVICE_NODE, attempt);

		fp = filp_open(UART_TTY_DEVICE_NODE,
			       O_RDWR | O_NONBLOCK | O_NOCTTY, 0);
		err = PTR_ERR(fp);
	} while (time_before(jiffies, timeout) && err == -ENOENT);

	if (IS_ERR_OR_NULL(fp)) {
		dev_err(escore->dev, "%s(): UART device node open failed, err = %ld\n",
				__func__, err);
		return -ENODEV;
	}

	/* device node found */
	pr_debug("%s(): successfully opened tty\n", __func__);

	/* set uart_dev members */
	escore_uart.file = fp;
	escore_uart.tty =
		((struct tty_file_private *)fp->private_data)->tty;

	escore->uart_ready = 1;
	return 0;
}

int escore_uart_close(struct escore_priv *escore)
{

	if (atomic_dec_return(&escore->uart_users)) {
		pr_debug("%s() UART is busy to close, user count: %d\n",
			 __func__, atomic_read(&escore->uart_users));
		return 0;
	}

	filp_close(escore_uart.file, 0);
	escore->uart_ready = 0;

	pr_debug("%s() closed UART\n", __func__);
	return 0;
}

int escore_uart_wait(struct escore_priv *escore)
{
#if defined(KERNEL_VERSION_3_8_0)
	int timeout;
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&escore_uart.tty->read_wait, &wait);
	set_task_state(current, TASK_INTERRUPTIBLE);
	timeout = schedule_timeout(msecs_to_jiffies(50));

	set_task_state(current, TASK_RUNNING);
	remove_wait_queue(&escore_uart.tty->read_wait, &wait);
	return timeout;
#else

	/* wait on tty read queue until awoken or for 50ms */
	return wait_event_interruptible_timeout(
		escore_uart.tty->read_wait,
		escore_uart.tty->read_cnt,
		msecs_to_jiffies(50));
#endif
}

int escore_uart_config(struct escore_priv *escore)
{
	int rc = 0;
	/* perform baudrate configuration */
	if (escore->mode == STANDARD) {
		rc = escore_configure_tty(escore_uart.tty,
			escore_uart.baudrate_ns, UART_TTY_STOP_BITS);
	}
	return rc;
}

struct es_stream_device es_uart_streamdev = {
	.open = escore_uart_open,
	.read = escore_uart_read_internal,
	.close = escore_uart_close,
	.wait = escore_uart_wait,
	.config = escore_uart_config,
	.intf = ES_UART_INTF,
};

MODULE_DESCRIPTION("ASoC ESCORE driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:escore-codec");
