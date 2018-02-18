/*
 * escore-uart.c  --  Audience eS705 UART interface
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
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/completion.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/kthread.h>
#include <linux/serial_core.h>
#include <linux/tty.h>

#include "escore.h"
#include "escore-uart.h"
#include "escore-uart-common.h"
#include "escore-cdev.h"

static int escore_uart_boot_setup(struct escore_priv *escore);
static int escore_uart_boot_finish(struct escore_priv *escore);
static int escore_uart_probe(struct platform_device *dev);
static int escore_uart_remove(struct platform_device *dev);
static int escore_uart_abort_config(struct escore_priv *escore);
struct escore_uart_device escore_uart;
#ifdef ESCORE_FW_LOAD_BUF_SZ
#undef ESCORE_FW_LOAD_BUF_SZ
#endif
#define ESCORE_FW_LOAD_BUF_SZ 1024
#define ES_MAX_READ_RETRIES 100

static u32 escore_default_uart_baud = 460800;

static u32 escore_uarths_baud[UART_RATE_MAX] = {
	460800, 921600, 1000000, 1024000, 1152000,
	2000000, 2048000, 3000000, 3072000 };

static int set_sbl_baud_rate(struct escore_priv *escore, u32 sbl_rate_req_cmd)
{
	char msg[4] = {0};
	char *buf;
	u8 bytes_to_read;
	int rc;
	u32 baudrate_change_resp;

	pr_debug("Setting uart baud rate %x\n", sbl_rate_req_cmd);

	sbl_rate_req_cmd = cpu_to_be32(sbl_rate_req_cmd);
	rc = escore_uart_write(escore, &sbl_rate_req_cmd,
			sizeof(sbl_rate_req_cmd));
	if (rc) {
		pr_err("%s(): Baud rate setting for UART fail %d\n",
		       __func__, rc);
		rc = -EIO;
		return rc;
	}

	baudrate_change_resp = 0;

	/* Sometimes an extra byte (0x00) is received over UART
	 * which should be discarded.
	 */
	rc = escore_uart_read(escore, msg, sizeof(char));
	if (rc < 0) {
		pr_err("%s(): Set Rate Request read rc = %d\n",
				__func__, rc);
		return rc;
	}

	bytes_to_read = sizeof(baudrate_change_resp);
	if (msg[0] == 0) {
		/* Discard this byte */
		pr_debug("%s(): Received extra zero\n", __func__);
		buf = &msg[0];
	} else {
		/* 1st byte was valid, now read only remaining bytes */
		bytes_to_read -= sizeof(char);
		buf = &msg[1];
	}

	rc = escore_uart_read(escore, buf, bytes_to_read);
	if (rc < 0) {
		pr_err("%s(): Set Rate Request read rc = %d\n",
				__func__, rc);
		return rc;
	}

	baudrate_change_resp |= *(u32 *)msg;

	if (baudrate_change_resp != sbl_rate_req_cmd) {
		pr_err("%s(): Invalid response to Rate Request :0x%x, 0x%x\n",
				__func__, baudrate_change_resp,
				sbl_rate_req_cmd);
		return -EINVAL;
	}

	return rc;
}

static int escore_uart_wakeup(struct escore_priv *escore)
{
	char wakeup_byte = 'A';
	int ret = 0;

	ret = escore_uart_open(&escore_priv);
	if (ret) {
		dev_err(escore->dev, "%s(): escore_uart_open() failed %d\n",
				__func__, ret);
		goto escore_uart_open_failed;
	}
	ret = escore_uart_write(&escore_priv, &wakeup_byte, 1);
	if (ret < 0) {
		dev_err(escore->dev, "%s() UART wakeup failed:%d\n",
				__func__, ret);
		goto escore_uart_wakeup_exit;
	}

	/* Flush tty buffer to avoid extra byte
	 * received in next read cycle */
	ret = tty_perform_flush(escore_uart.tty, TCIOFLUSH);
	if (ret)
		dev_err(escore->dev,
				"%s(): TTY buffer Flush is failed %d",
				__func__, ret);

escore_uart_wakeup_exit:
	escore_uart_close(&escore_priv);
escore_uart_open_failed:
	return ret;
}

static int escore_uart_abort_config(struct escore_priv *escore)
{
	u8 sbl_sync_cmd = ESCORE_SBL_SYNC_CMD;
	int rc;

	rc = escore_configure_tty(escore_uart.tty,
			escore_uart.baudrate_sbl, UART_TTY_STOP_BITS);
	if (rc) {
		pr_err("%s(): config UART failed, rc = %d\n", __func__, rc);
		return rc;
	}

	rc = escore_uart_write(escore, &sbl_sync_cmd, 1);
	if (rc) {
		pr_err("%s(): UART SBL sync write failed, rc = %d\n",
			__func__, rc);
		return rc;
	}

	return rc;
}

int escore_uart_boot_setup(struct escore_priv *escore)
{
	u8 retry = ESCORE_UART_SBL_SYNC_WRITE_RETRY;

	u8 sbl_sync_cmd = ESCORE_SBL_SYNC_CMD;
	u8 sbl_boot_cmd = ESCORE_SBL_BOOT_CMD;
	u32 sbl_rate_req_cmd = ESCORE_SBL_SET_RATE_REQ_CMD << 16;
	char msg[4] = {0};
	int uart_tty_baud_rate = UART_TTY_BAUD_RATE_460_8_K;
	int rc;
	u8 i;

	/* set speed to bootloader baud */
	escore_configure_tty(escore_uart.tty,
		escore_uart.baudrate_sbl, UART_TTY_STOP_BITS);

	pr_debug("%s()\n", __func__);

	/* Drop any prior data as we just reset the chip before this */
	rc = tty_perform_flush(escore_uart.tty, TCIOFLUSH);
	if (rc)
		pr_err("%s(): TTY buffer Flush failed %d", __func__, rc);

	/* SBL SYNC BYTE 0x00 */
	pr_debug("%s(): write ESCORE_SBL_SYNC_CMD = 0x%02x\n", __func__,
		sbl_sync_cmd);
	memcpy(msg, (char *)&sbl_sync_cmd, 1);
	do {
		rc = escore_uart_write(escore, msg, 1);
	} while (rc && retry--);

	if (rc) {
		rc = -EIO;
		goto escore_bootup_failed;
	}

	pr_debug("%s(): firmware load sbl sync write rc=%d\n", __func__, rc);
	memset(msg, 0, 4);

	usleep_range(10000, 10500);

	retry = ESCORE_UART_SBL_SYNC_READ_RETRY;
	do {
		rc = escore_uart_read(escore, msg, 1);
	} while (rc && retry--);

	if (rc) {
		pr_err("%s(): firmware load failed sync ack rc = %d\n",
			__func__, rc);
		goto escore_bootup_failed;
	}
	pr_debug("%s(): sbl sync ack = 0x%02x\n", __func__, msg[0]);
	if (msg[0] != ESCORE_SBL_SYNC_ACK) {
		pr_err("%s(): firmware load failed sync ack pattern", __func__);
		rc = -EIO;
		goto escore_bootup_failed;
	}

	/* UART Baud rate and Clock setting:-
	 *
	 * Default baud rate will be 460.8Khz and external clock
	 * will be to 6MHz unless high speed firmware download
	 * is enabled.
	 */
	if (escore->pdata->enable_hs_uart_intf) {

#if defined(CONFIG_SND_SOC_ES_UARTHS_BAUD)
		escore_default_uart_baud = CONFIG_SND_SOC_ES_UARTHS_BAUD;
#endif
		/* Set Baud rate and external clock */
		uart_tty_baud_rate = ES_UARTHS_BAUD_RATE_3000K;
		for (i = 0; i < ARRAY_SIZE(escore_uarths_baud); i++) {
			if (escore_uarths_baud[i] == escore_default_uart_baud)
				uart_tty_baud_rate = i;
		}

		sbl_rate_req_cmd |= uart_tty_baud_rate << 8;
		sbl_rate_req_cmd |= (escore->pdata->ext_clk_rate & 0xff);

		rc = set_sbl_baud_rate(escore, sbl_rate_req_cmd);
		if (rc < 0) {
			pr_err("%s(): set_sbl_baud_rate fail %d\n",
			       __func__, rc);
			goto escore_bootup_failed;
		}

		escore_configure_tty(escore_uart.tty,
				escore_uarths_baud[uart_tty_baud_rate],
				UART_TTY_STOP_BITS);
	}

	msleep(20);

	/* SBL BOOT BYTE 0x01 */
	memset(msg, 0, 4);
	pr_debug("%s(): write ESCORE_SBL_BOOT_CMD = 0x%02x\n", __func__,
		sbl_boot_cmd);
	memcpy(msg, (char *)&sbl_boot_cmd, 1);
	rc = escore_uart_write(escore, msg, 1);
	if (rc) {
		pr_err("%s(): firmware load failed sbl boot write %d\n",
		       __func__, rc);
		rc = -EIO;
		goto escore_bootup_failed;
	}

	/* SBL BOOT BYTE ACK 0x01 */
	msleep(20);
	memset(msg, 0, 4);
	retry = ESCORE_UART_SBL_BOOT_ACK_RETRY;
	do {
		rc = escore_uart_read(escore, msg, 1);
	} while (rc && retry--);

	if (rc) {
		pr_err("%s(): firmware load failed boot ack %d\n",
		       __func__, rc);
		goto escore_bootup_failed;
	}
	pr_debug("%s(): sbl boot ack = 0x%02x\n", __func__, msg[0]);

	if (msg[0] != ESCORE_SBL_BOOT_ACK) {
		pr_err("%s(): firmware load failed boot ack pattern", __func__);
		rc = -EIO;
		goto escore_bootup_failed;
	}
	rc = 0;

escore_bootup_failed:
	return rc;
}

int escore_uart_boot_finish(struct escore_priv *escore)
{
	int rc;
	u32 sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_POLLING;
	u32 sync_ack;
	int sync_retry = ES_SYNC_MAX_RETRY;

	/* Use Polling method if gpio-a is not defined */
	/*
	 * Give the chip some time to become ready after firmware
	 * download. (FW is still transferring)
	 */
	msleep(35);

	/* now switch to firmware baud to talk to chip */
	if (escore->mode == SBL) {
		escore_configure_tty(escore_uart.tty,
			escore_uart.baudrate_ns, UART_TTY_STOP_BITS);
	} else {
		escore_configure_tty(escore_uart.tty,
			escore_uart.baudrate_vs, UART_TTY_STOP_BITS);
	}

	/* Discard extra bytes from escore after firmware load. Host gets
	 * extra bytes after VS firmware download as per Bug 19441
	 * and as per bug #22191, in case of eS755 four extra bytes are received
	 * instead of 3 extra bytes.
	 * Also, as per the bug RDBNK-1455, extra bytes received after
	 * configuring the tty to new speed post VS fw download.
	 * To make it generic, host flushes extra bytes on UART.
	 * There is no need to check for response because host may get less
	 * bytes.
	 */
	rc = tty_perform_flush(escore_uart.tty, TCIOFLUSH);
	if (rc)
		dev_err(escore->dev,
				"%s(): TTY buffer Flush is failed %d",
				__func__, rc);

	/* sometimes earSmart chip sends success in second sync command */
	do {
		pr_debug("%s(): write ES_SYNC_CMD = 0x%08x\n",
			__func__, sync_cmd);
		rc = escore_uart_cmd(escore, sync_cmd, &sync_ack);
		if (rc) {
			pr_err("%s(): firmware load failed sync write - %d\n",
				__func__, rc);
			continue;
		}
		pr_debug("%s(): sync_ack = 0x%08x\n", __func__, sync_ack);
		if (sync_ack != ES_SYNC_ACK) {
			pr_err("%s(): firmware load failed sync ack pattern",
				__func__);
			rc = -EIO;
		} else {
			pr_info("%s(): firmware load success\n", __func__);
			break;
		}
	} while (sync_retry--);

	return rc;
}

/* This function must be called with access_lock acquired */
static int escore_uart_int_osc_calibration(struct escore_priv *escore)
{
	u16 configure_uart = cpu_to_be16(0x00);
	int sync_retry = ES_SYNC_MAX_RETRY;
	u32 resp;
	int rc = 0;

	if (escore->bus.ops.high_bw_open) {
		rc = escore->bus.ops.high_bw_open(escore);
		if (rc < 0) {
			dev_err(escore->dev, "%s(): high_bw_open failed %d\n",
			__func__, rc);
			goto exit_osc_calib;
		}
	}

	do {
		rc = escore_cmd(escore,
				(ES_INT_OSC_MEASURE_START_VS << 16),
				&resp);
		if (rc) {
			dev_err(escore->dev, "%s(): sending start UART calibration command failed %d",
					__func__, rc);
			continue;
		}

		rc = escore->bus.ops.high_bw_write(escore,
			&configure_uart, sizeof(configure_uart));
		if (rc < 0) {
			dev_err(escore->dev, "%s(): sending configure command on uart failed %d",
					__func__, rc);
			continue;
		}

		usleep_range(5000, 5005);

		rc = escore_cmd(escore,
				(ES_INT_OSC_MEASURE_QUERY_VS << 16),
				&resp);
		if (rc) {
			dev_err(escore->dev, "%s(): sending check UART calibration command failed %d",
					__func__, rc);
			continue;
		} else if (resp ==
				(ES_INT_OSC_MEASURE_QUERY_VS << 16)) {
			dev_dbg(escore->dev, "%s(): caliberation was successful",
					__func__);
		} else {
			dev_err(escore->dev, "%s(): caliberation response 0x%08x",
					__func__, resp);
			continue;
		}
		rc = escore->bus.ops.high_bw_cmd(escore, ES_SYNC_CMD << 16,
								&resp);
		if (rc) {
			dev_err(escore->dev, "%s(): Sync cmd failed %d",
							__func__, rc);
		} else if (resp == (ES_SYNC_CMD << 16)) {
			dev_dbg(escore->dev, "%s(): Sync successful", __func__);
			break;
		} else {
			dev_dbg(escore->dev, "%s(): sync response 0x%08x",
							__func__, resp);

			/* Flush tty buffer to avoid extra byte
			 * received in next read cycle */
			rc = tty_perform_flush(escore_uart.tty, TCIOFLUSH);
			if (rc)
				dev_err(escore->dev,
					"%s(): TTY buffer Flush is failed %d",
					__func__, rc);
		}
	} while (--sync_retry);

	dev_dbg(escore->dev, "%s(), number of tries made : %d",
		__func__, ES_SYNC_MAX_RETRY - sync_retry + 1);

	if (!sync_retry) {
		dev_err(escore->dev,
		"%s(): UART caliberation failed. Continuing with Ext OSC",
							__func__);
		/* In case of any issue switch the clock to ext oscillator */
		rc = escore_switch_ext_osc(escore);
		if (rc) {
			dev_err(escore->dev,
				"%s(): Error switching to external OSC %d\n",
				__func__, rc);
		}
	}
	usleep_range(2000, 2005);

	if (escore->bus.ops.high_bw_close) {
		dev_dbg(escore->dev, "%s(), closing the UART",
							__func__);
		escore->bus.ops.high_bw_close(escore);
	}

exit_osc_calib:
	return rc;
}

static struct platform_device *escore_uart_pdev;

static void escore_uart_setup_pri_intf(struct escore_priv *escore)
{
	escore->bus.ops.read = escore_uart_read;
	escore->bus.ops.write = escore_uart_write;
	escore->bus.ops.cmd = escore_uart_cmd;
	escore->streamdev = es_uart_streamdev;
	escore->escore_uart_wakeup = escore_uart_wakeup;
	escore->bus.ops.cpu_to_bus = escore_cpu_to_uart;
	escore->bus.ops.bus_to_cpu = escore_uart_to_cpu;
}

static int escore_uart_setup_high_bw_intf(struct escore_priv *escore)
{
	int rc;

	escore->bus.ops.high_bw_write = escore_uart_write;
	escore->bus.ops.high_bw_read = escore_uart_read;
	escore->bus.ops.high_bw_cmd = escore_uart_cmd;
	escore->bus.ops.high_bw_open = escore_uart_open;
	escore->bus.ops.high_bw_close = escore_uart_close;
	escore->boot_ops.escore_abort_config = escore_uart_abort_config;
	escore->boot_ops.setup = escore_uart_boot_setup;
	escore->boot_ops.finish = escore_uart_boot_finish;
	escore->bus.ops.cpu_to_high_bw_bus = escore_cpu_to_uart;
	escore->bus.ops.high_bw_bus_to_cpu = escore_uart_to_cpu;
	escore->bus.ops.high_bw_calibrate = escore_uart_int_osc_calibration;
	escore->escore_uart_wakeup = escore_uart_wakeup;

#if defined(CONFIG_SND_SOC_ES_UART_SBL_BAUD)
	escore_uart.baudrate_sbl = CONFIG_SND_SOC_ES_UART_SBL_BAUD;
#else
	escore_uart.baudrate_sbl = UART_TTY_BAUD_RATE_460_8_K;
#endif

#if defined(CONFIG_SND_SOC_ES_UART_NS_BAUD)
	escore_uart.baudrate_ns = CONFIG_SND_SOC_ES_UART_NS_BAUD;
#else
	escore_uart.baudrate_ns = UART_TTY_BAUD_RATE_3_M;
#endif

#if defined(CONFIG_SND_SOC_ES_UART_VS_BAUD)
	escore_uart.baudrate_vs = CONFIG_SND_SOC_ES_UART_VS_BAUD;
#else
	escore_uart.baudrate_vs = UART_TTY_BAUD_RATE_3_M;
#endif

	rc = escore->probe(escore->dev);
	if (rc)
		goto out;

	mutex_lock(&escore->access_lock);
	rc = escore->boot_ops.bootup(escore);
	mutex_unlock(&escore->access_lock);

out:
	return rc;
}

int escore_uart_probe_thread(void *ptr)
{
	int rc = 0;
	struct device *dev = (struct device *) ptr;
	struct escore_priv *escore = &escore_priv;
	int probe_intf = ES_INVAL_INTF;

	rc = escore_uart_open(escore);
	if (rc) {
		dev_err(dev, "%s(): es705_uart_open() failed %d\n",
			__func__, rc);
		return rc;
	}

	/* device node found, continue */
	dev_dbg(dev, "%s(): successfully opened tty\n",
		  __func__);

	if (escore->pri_intf == ES_UART_INTF) {
		escore->bus.setup_prim_intf = escore_uart_setup_pri_intf;
		probe_intf = ES_UART_INTF;
	}
	if (escore->high_bw_intf == ES_UART_INTF) {
		escore->bus.setup_high_bw_intf = escore_uart_setup_high_bw_intf;
		probe_intf = ES_UART_INTF;
	}

	rc = escore_probe(escore, dev, probe_intf, ES_CONTEXT_THREAD);
	if (rc) {
		dev_err(dev, "%s(): UART common probe fail %d\n",
			__func__, rc);
		goto bootup_error;
	}

	escore_uart_close(escore);

	return rc;

bootup_error:
	release_firmware(escore_priv.standard);
	escore_uart_close(escore);
	snd_soc_unregister_codec(escore_priv.dev);
	pr_err("%s(): exit with error\n", __func__);
	return rc;
}

static int escore_uart_probe(struct platform_device *dev)
{
	int rc = 0;
	static struct task_struct *uart_probe_thread;

	dev_dbg(&dev->dev, "%s():\n", __func__);

	uart_probe_thread = kthread_run(escore_uart_probe_thread,
					(void *) &dev->dev,
					"escore uart thread");
	if (IS_ERR_OR_NULL(uart_probe_thread)) {
		pr_err("%s(): can't create escore UART probe thread = %p\n",
			__func__, uart_probe_thread);
		rc = -ENOMEM;
	}

	return rc;
}

static int escore_uart_remove(struct platform_device *dev)
{
	int rc = 0;

	if (escore_uart.file)
		escore_uart_close(&escore_priv);

	escore_uart.tty = NULL;
	escore_uart.file = NULL;

	snd_soc_unregister_codec(escore_priv.dev);

	return rc;
}

static struct escore_pdata escore_uart_pdata = {
	.probe = escore_uart_probe,
	.remove = escore_uart_remove,
};

/* FIXME: Kludge for escore_bus_init abstraction */
int escore_uart_bus_init(struct escore_priv *escore)
{
	int rc = 0;

	atomic_set(&escore->uart_users, 0);

	escore_uart_pdev = platform_device_alloc("escore-codec.uart", -1);
	if (!escore_uart_pdev) {
		pr_err("%s: UART platform device allocation failed\n",
		       __func__);
		rc = -ENOMEM;
		goto out;
	}

	rc = platform_device_add_data(escore_uart_pdev, &escore_uart_pdata,
				sizeof(escore_uart_pdata));
	if (rc) {
		pr_err("%s: Error adding UART device data %d\n", __func__, rc);
		platform_device_put(escore_uart_pdev);
		goto out;
	}

	rc = platform_device_add(escore_uart_pdev);
	if (rc) {
		pr_err("%s: Error adding UART device %d\n", __func__, rc);
		platform_device_put(escore_uart_pdev);
	}
out:
	return rc;
}

MODULE_DESCRIPTION("ASoC ESCORE driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:escore-codec");
