/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/device.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/nvs.h>
#include <linux/crc32.h>

#include "nvs_dfsh.h"

#define DFSH_DRIVER_VERSION		(1)
#define DFSH_NAME				"dfsh"
#define DFSH_POR_DELAY_MS		(100)
#define DFSH_RESET_DELAY_MS		(50)

/* regulator names in order of powering on */
static char *dfsh_vregs[] = {
	"vdd-1v8",
};

struct sensor_cfg snsr_list[] = {
	{
		.name			= "frame_sync",
		.snsr_id		= 0,
		.kbuf_sz		= 1024,
		.timestamp_sz		= 8,
		.snsr_data_n		= 8,
		.ch_n			= 1,
		.ch_sz			= 8,
		.part			= "STM32L151x",
		.vendor			= "STMicroElectronics",
		.version		= 1,
		.delay_us_min		= 10000,
		.delay_us_max		= 255000,
		.matrix			= { 1, 0, 0, 0, 1, 0, 0, 0, 1 },
	},
	{
		.name			= "accelerometer",
		.kbuf_sz		= 1024,
		.timestamp_sz		= 8,
		.snsr_data_n		= 6,
		.ch_n			= 3,
		.ch_sz			= -2,
		.part			= "IMU-20628",
		.vendor			= "InvenSense",
		.version		= 1,
		.float_significance = NVS_FLOAT_NANO,
		.max_range		= {
			.ival		= 39,
			.fval		= 220000000,
		},
		.resolution		= {
			.ival		= 0,
			.fval		= 118498,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 250000,
		},
		.delay_us_min		= 10000,
		.delay_us_max		= 255000,
		.matrix			= { 1, 0, 0, 0, 1, 0, 0, 0, 1 },
		.scale			= {
		/* 0.000118498 (4.0 * 0.970737134 / 32768.0) */
			.ival		= 0,
			.fval		= 118498,
		},
	},
	{
		.name			= "gyroscope",
		.kbuf_sz		= 1024,
		.timestamp_sz		= 8,
		.snsr_data_n		= 6,
		.ch_n			= 3,
		.ch_sz			= -2,
		.part			= "IMU-20628",
		.vendor			= "InvenSense",
		.version		= 1,
		.float_significance = NVS_FLOAT_NANO,
		.max_range		= {
			.ival		= 34,
			.fval		= 906585000,
		},
		.resolution		= {
			.ival		= 0,
			.fval		= 133158,
		},
		.milliamp		= {
			.ival		= 6,
			.fval		= 500000,
		},
		.delay_us_min		= 10000,
		.delay_us_max		= 255000,
		.matrix			= { 1, 0, 0, 0, 1, 0, 0, 0, 1 },
		.scale			= {
		/* 0.000133158 (250.0f * 3.14159265f / 180.0f / 32768.0) */
			.ival		= 0,
			.fval		= 133158,
		},
	},
	{
		.name			= "magnetic_field",
		.kbuf_sz		= 1024,
		.timestamp_sz		= 8,
		.snsr_data_n		= 12,
		.ch_n			= 3,
		.ch_sz			= -4,
		.part			= "AK8963C",
		.vendor			= "Asahi Kasei Microdevices",
		.version		= 1,
		.max_range		= {
			.ival		= 2500,
			.fval		= 0,
		},
		.resolution		= {
			.ival		= 0,
			.fval		= 127929687,
		},
		.milliamp		= {
			.ival		= 0,
			.fval		= 600000,
		},
		.delay_us_min		= 10000,
		.delay_us_max		= 255000,
		.matrix			= { 1, 0, 0, 0, 1, 0, 0, 0, 1 },
		.scale			= {
		/* 0.127929687 (4192.0 / 32768.0) */
			.ival		= 0,
			.fval		= 127929687,
		},
	},
};
#define DEV_N				(ARRAY_SIZE(snsr_list))

struct dfsh_state {
	struct tty_struct *tty;		/* Refer to parent data structure */
	struct platform_device		*pdev;
	void *nvs_st[DEV_N];
	struct nvs_fn_if *nvs;
	struct sensor_cfg cfg[DEV_N];
	struct regulator_bulk_data vreg[ARRAY_SIZE(dfsh_vregs)];
	unsigned int sts;		/* status flags */
	unsigned int errs;		/* error count */
	unsigned int enabled_msk;	/* global enable status */
	unsigned int enabled[DEV_N];	/* dev enable status */
	int gpio_rst;			/* GPIO reset */
	int gpio_boot0;			/* GPIO boot0 */
	int gpio_rst_asrt_pol;		/* GPIO reset assert polarity */

	int pkt_byte_idx;
	int pyld_len;
	/* Write lock to single underlying tty device */
	struct mutex tty_write_lock;
	/* Read buffer - to hold one pkt before de-muxing */
	union {
		struct dfsh_pkt_t pkt;
		unsigned char pkt_buf[sizeof(struct dfsh_pkt_t)];
	};
};

static struct dfsh_state *st;

/* We are using crc32 to validate packets */
#define CRC_SIZE		4

#define PPYLD(pkt)		((uint8_t *)(pkt + \
				sizeof(struct dfsh_pkt_hdr_t)))

#define PYLD(pkt)		(*(uint32_t *)(pkt + \
				sizeof(struct dfsh_pkt_hdr_t)))

#define CRC(pkt, pyld_sz)	(*(uint32_t *)(pkt + \
				sizeof(struct dfsh_pkt_hdr_t) + \
				pyld_sz))

#define CRC_DATA_SZ(pyld_sz)	(sizeof(struct dfsh_pkt_hdr_t) \
				 + pyld_sz)

#define PKT_SZ(pyld_sz)		(sizeof(struct dfsh_pkt_hdr_t) \
				 + pyld_sz + CRC_SIZE)

static uint32_t pkt_crc(char *pkt, int len)
{
	uint32_t crc;

	crc = 0xCAFEBABA;
	/* FIX ME: Temp code until CRC module is coded up in fw */
	/* crc = crc32(0, pkt, len); */
	return crc;
}

static int dfsh_write_cmd(struct dfsh_state *st, uint8_t *buffer, int count)
{
	struct tty_struct *tty = st->tty;
	char pkt[sizeof(struct dfsh_pkt_t)];
	const char *b;
	int retval = 0;
	int remained;
	int c;

	if (mutex_lock_interruptible(&st->tty_write_lock))
		return -EINTR;

	/* Header */
	((struct dfsh_pkt_t *)pkt)->header.start = SENSOR_HUB_START;
	((struct dfsh_pkt_t *)pkt)->header.type = MSG_MCU;

	/* Payload */
	memcpy(PPYLD(pkt), buffer, count);

	/* CRC */
	CRC(pkt, sizeof(struct mcu_payload_t)) =
	    pkt_crc(pkt, CRC_DATA_SZ(sizeof(struct mcu_payload_t)));

	remained = PKT_SZ(sizeof(struct mcu_payload_t));
	b = pkt;

	while (remained > 0) {
		c = tty->ops->write(tty, b, remained);
		if (c < 0) {
			retval = c;
			goto break_out;
		}
		if (!c)
			break;
		b += c;
		remained -= c;
	}

break_out:
	if (tty->ops->flush_chars)
		tty->ops->flush_chars(tty);

	if (remained && tty->fasync)
		set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);

	mutex_unlock(&st->tty_write_lock);

	return (remained == 0) ? count : retval;
}

static int pkt_payload_len(unsigned char type)
{
	switch (type) {
	case MSG_CAMERA:
		return sizeof(struct camera_payload_t);
	case MSG_ACCEL:
		return sizeof(struct accel_payload_t);
	case MSG_GYRO:
		return sizeof(struct gyro_payload_t);
	case MSG_MAGN:
		return sizeof(struct magn_payload_t);
	case MSG_MCU:
		return sizeof(struct mcu_payload_t);
	default:
		return -1;
	}
}

static int dfsh_msg_id2snsr_id(u8 msg_id)
{
	int snsr_id;

	snsr_id = msg_id - SNSR_MSG_ID_START;
	if (snsr_id > (SNSR_MSG_ID_END - SNSR_MSG_ID_START))
		snsr_id = -EINVAL;
	return snsr_id;
}

static void dfsh_err(struct dfsh_state *st)
{
	st->errs++;
	if (!st->errs)
		st->errs--;
}

static int dfsh_reset(void *client,  int snsr_id)
{
	struct dfsh_state *st = (struct dfsh_state *)client;
	int ret = 0;

	if (st->gpio_rst >= 0) {
		ret = gpio_direction_output(st->gpio_rst,
					    st->gpio_rst_asrt_pol);
		mdelay(DFSH_RESET_DELAY_MS);
		ret |= gpio_direction_output(st->gpio_rst,
					     !st->gpio_rst_asrt_pol);
		if (ret < 0)
			dev_err(st->tty->dev, "%s GPIO=%d assert=%d ERR=%d\n",
				__func__, st->gpio_rst, st->gpio_rst_asrt_pol,
				ret);
		else if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(st->tty->dev, "%s\n", __func__);
	}
	return ret;
}

static int dfsh_pm(struct dfsh_state *st, bool enable)
{
	int ret = 0;

	if (enable) {
		ret = nvs_vregs_enable(st->tty->dev, st->vreg,
				       ARRAY_SIZE(dfsh_vregs));
		if (ret > 0) {
			mdelay(DFSH_POR_DELAY_MS);
			ret = dfsh_reset(st, -1);
		}
	} else {
		/* FIX ME */
		/* ret |= nvs_vregs_disable(st->tty->dev, st->vreg,
					ARRAY_SIZE(dfsh_vregs)); */
	}
	if (ret > 0)
		ret = 0;
	if (ret) {
		dev_err(st->tty->dev, "%s pwr=%x ERR=%d\n",
			__func__, enable, ret);
	} else {
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(st->tty->dev, "%s pwr=%x\n",
				 __func__, enable);
	}
	return ret;
}

static void dfsh_pm_exit(struct dfsh_state *st)
{
	dfsh_pm(st, false);
	nvs_vregs_exit(st->tty->dev, st->vreg, ARRAY_SIZE(dfsh_vregs));
}

static int dfsh_pm_init(struct dfsh_state *st)
{
	int ret;

	ret = nvs_vregs_init(&st->pdev->dev, st->vreg,
			      ARRAY_SIZE(dfsh_vregs), dfsh_vregs);

	gpio_set_value(st->gpio_rst, 0);
	gpio_set_value(st->gpio_boot0, 0);

	ret = nvs_vregs_disable(&st->pdev->dev, st->vreg,
			      ARRAY_SIZE(dfsh_vregs));

	ret = nvs_vregs_enable(&st->pdev->dev, st->vreg,
			      ARRAY_SIZE(dfsh_vregs));

	mdelay(DFSH_RESET_DELAY_MS);

	gpio_set_value(st->gpio_rst, 1);

	return ret;
}

static int dfsh_disable(struct dfsh_state *st, int snsr_id)
{
	bool disable = true;
	int ret = 0;
	unsigned int i;

	if (snsr_id >= 0) {
		if (st->enabled_msk & ~(1 << snsr_id)) {
			st->enabled_msk &= ~(1 << snsr_id);
			st->enabled[snsr_id] = 0;
			disable = false;
		}
	}
	if (disable) {
		ret = dfsh_pm(st, false);
		if (!ret) {
			st->enabled_msk = 0;
			for (i = 0; i < DEV_N; i++)
				st->enabled[i] = 0;
		}
	}
	return ret;
}

static int dfsh_enable(void *client, int snsr_id, int enable)
{
	struct dfsh_state *st = (struct dfsh_state *)client;
	int ret = 0;

	if (enable < 0)
		return st->enabled[snsr_id]; /* return enable status */

	if (enable) {
		ret = dfsh_pm(st, true);
		if (!ret) {
			/* if individual sensor enable is supported then here
			 * we want to send the sensor enable message to DFSH.
			 */
			/* ret = dfsh_en(st, snsr_id); */
			if (ret < 0) {
				if (!st->enabled[snsr_id])
					dfsh_disable(st, snsr_id);
			} else {
				st->enabled[snsr_id] = enable;
				st->enabled_msk |= (1 << snsr_id);
			}
		}
	} else {
		ret = dfsh_disable(st, snsr_id);
	}
	return ret;
}

static int dfsh_nvs_read(void *client, int snsr_id, char *buf)
{
	struct dfsh_state *st = (struct dfsh_state *)client;
	ssize_t t;

	t = sprintf(buf, "DFSH driver v.%u\n", DFSH_DRIVER_VERSION);
	/* device tree parameters */
	t += sprintf(buf + t, "gpio_reset=%d\n", st->gpio_rst);
	t += sprintf(buf + t, "gpio_reset_assert_polarity=%d\n",
		     st->gpio_rst_asrt_pol);
	return t;
}

static struct nvs_fn_dev dfsh_fn_dev = {
	.enable				= dfsh_enable,
	.reset				= dfsh_reset,
	.nvs_read			= dfsh_nvs_read,
};


static inline void dfsh_parse_pkt(struct tty_struct *tty, unsigned char c)
{
	struct dfsh_state *st = tty->disc_data;
	uint32_t crc;
	unsigned int data_i;
	unsigned int ts_i;
	int snsr_id;
	int64_t ts;

	/* sanity check index */
	if (st->pkt_byte_idx >= sizeof(st->pkt_buf))
		/* Reset if byte index longer than longest packet */
		st->pkt_byte_idx = 0;
	st->pkt_buf[st->pkt_byte_idx] = c;
	switch (st->pkt_byte_idx++) {
	case 0:
		/* expecting Magic value */
		if (c != SENSOR_HUB_START) {
			st->pkt_byte_idx = 0;
			pr_debug("sh_ldisc: msg start not recvd 0x%x\n", c);
		}
		break;

	case 1:
		/* Expecting message type */
		snsr_id = dfsh_msg_id2snsr_id(c);
		/* Calc payload len from msg type */
		if (snsr_id >= 0)
			st->pyld_len = pkt_payload_len(c);
		else if (c == MSG_MCU)
			st->pyld_len = sizeof(struct mcu_payload_t);
		else
			st->pkt_byte_idx = 0;
		break;

	default:
		/* Nothing to do until last byte has been received */
		if (st->pkt_byte_idx == (sizeof(struct dfsh_pkt_hdr_t) +
					 st->pyld_len + CRC_SIZE)) {
			/* validate packet crc */
			crc = pkt_crc(st->pkt_buf, CRC_DATA_SZ(st->pyld_len));
			if (crc == CRC(st->pkt_buf, st->pyld_len)) {
				snsr_id = dfsh_msg_id2snsr_id(st->pkt.
							      header.type);
				if (snsr_id >= 0) {
					/* sensor data */
					ts_i = sizeof(struct dfsh_pkt_hdr_t);
					data_i = ts_i + (snsr_id ?
					    st->cfg[snsr_id].timestamp_sz:0);
					/* sensor timestamp */
					memcpy(&ts, &st->pkt_buf[ts_i],
					       sizeof(ts));
					st->nvs->handler(st->nvs_st[snsr_id],
							 &st->pkt_buf[data_i],
							 ts);
				} else if (st->pkt.header.type == MSG_MCU) {
					/* message from MCU */
					dev_info(tty->dev, "received cmd:%x\n",
					      st->pkt.payload.mcu_payload.cmd);
				} else {
					dfsh_err(st);
				}
			} else {
				dfsh_err(st);
			}

			/* Packet de-muxed successfully or dropped.
			 * Clear to start over. */
			st->pkt_byte_idx = 0;
			st->pyld_len = 0;
		}
		break;
	}
}

static void dfsh_receive_buf(struct tty_struct *tty, const unsigned char *cp,
			     char *fp, int count)
{
	const unsigned char *p;
	char *f;
	char flags = TTY_NORMAL;
	char buf[64];
	int i;

	for (i = count, p = cp, f = fp; i; i--, p++) {
		if (f)
			flags = *f++;
		switch (flags) {
		case TTY_NORMAL:
			dfsh_parse_pkt(tty, *p);
			break;

		case TTY_BREAK:
		case TTY_PARITY:
		case TTY_FRAME:
		case TTY_OVERRUN:
			dfsh_err(tty->disc_data);
			pr_debug("sh_ldisc: tty ctrl\n");
			/* Skip errors */
			break;

		default:
			pr_err("sh_ldisc: %s: unknown flag %d\n",
			       tty_name(tty, buf), flags);
			break;
		}
	}
}

static int dfsh_ioctl(struct tty_struct *tty, struct file *file,
		      unsigned int cmd, unsigned long arg)
{
	struct dfsh_state *st = tty->disc_data;
	int ret = -EFAULT;

	switch (cmd) {
	case TCFLSH:
		/* flush our buffers and the serial port's buffer */
		if (arg == TCIOFLUSH || arg == TCOFLUSH)
			;
		ret = n_tty_ioctl_helper(tty, file, cmd, arg);
		break;

	default:
		ret = tty_mode_ioctl(tty, file, cmd, arg);
		break;
	}

	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(tty->dev, "%s cmd=%u arg=%lu ret=%d\n",
			 __func__, cmd, arg, ret);
	return ret;
}

static void dfsh_shutdown(struct tty_struct *tty)
{
	struct dfsh_state *st = tty->disc_data;
	unsigned int i;

	st->sts |= NVS_STS_SHUTDOWN;
	if (st->nvs) {
		for (i = 0; i < DEV_N; i++) {
			if (st->nvs_st[i])
				st->nvs->shutdown(st->nvs_st[i]);
		}
	}
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(tty->dev, "%s\n", __func__);
}

static void dfsh_close(struct tty_struct *tty)
{
	uint8_t cmd;
	struct dfsh_state *st = tty->disc_data;
	unsigned int i;

	cmd = CMD_STOP_TS;
	dfsh_write_cmd(st, &cmd, sizeof(cmd));
	cmd = CMD_CAM_FSIN_STOP;
	dfsh_write_cmd(st, &cmd, sizeof(cmd));

	if (st != NULL) {
		dfsh_shutdown(tty);
		if (st->nvs) {
			for (i = 0; i < DEV_N; i++) {
				if (st->nvs_st[i])
					st->nvs->remove(st->nvs_st[i]);
			}
		}

		dfsh_pm_exit(st);
	}

	dev_info(tty->dev, "%s\n", __func__);
}

static int dfsh_of_dt(struct dfsh_state *st, struct device_node *dn)
{
	if (!of_device_is_available(dn))
		return -ENODEV;

	/* default parameters */
	st->gpio_rst = -1;
	st->gpio_boot0 = -1;

	/* device tree parameters */
	if (dn) {
		if (!of_property_read_s32(dn, "gpio_reset_assert_polarity",
					  &st->gpio_rst_asrt_pol))
			st->gpio_rst_asrt_pol = !!st->gpio_rst_asrt_pol;

		st->gpio_rst = of_get_named_gpio(dn,
						 "dfsh,reset-gpio", 0);
		st->gpio_boot0 = of_get_named_gpio(dn,
						 "dfsh,boot0-gpio", 0);
	}

	/* initialize GPIO */
	if (gpio_is_valid(st->gpio_boot0) && gpio_is_valid(st->gpio_rst)) {
		if (gpio_request(st->gpio_boot0, "dfsh_boot0") ||
		    gpio_request(st->gpio_rst, "dfsh_reset")) {
			dev_err(&st->pdev->dev, "cannot request gpio\n");
			return -EPROBE_DEFER;
	}

		if (gpio_direction_output(st->gpio_boot0, 0) ||
		    gpio_direction_output(st->gpio_rst, 0)) {
			dev_err(&st->pdev->dev, "cannot set gpio\n");
			return -EPROBE_DEFER;
		}

		gpio_export(st->gpio_boot0, 1);
		gpio_export(st->gpio_rst, 1);

	}

	return 0;
}

static int dfsh_open(struct tty_struct *tty)
{
	uint8_t i, n, cmd;
	int ret;

	dev_info(tty->dev, "%s\n", __func__);

	memcpy(&st->cfg, &snsr_list, sizeof(st->cfg));

	for (i = 0; i < DEV_N; i++)
		nvs_of_dt(tty->dev->of_node, &st->cfg[i], NULL);

	dfsh_fn_dev.sts = &st->sts;
	dfsh_fn_dev.errs = &st->errs;
	st->nvs = nvs_iio();
	if (st->nvs == NULL) {
		ret = -ENODEV;
		goto dfsh_open_err;
	}

	n = 0;
	for (i = 0; i < DEV_N; i++) {
		ret = st->nvs->probe(&st->nvs_st[i], st, tty->dev,
				     &dfsh_fn_dev, &st->cfg[i]);
		if (!ret) {
			st->cfg[i].snsr_id = i;
			n++;
		}
	}
	if (!n) {
		dev_err(tty->dev, "%s nvs_probe ERR\n", __func__);
		ret = -ENODEV;
		goto dfsh_open_err;
	}

	tty->disc_data = st;
	tty->receive_room = N_TTY_BUF_SIZE;
	st->tty = tty;
	mutex_init(&st->tty_write_lock);

	cmd = CMD_START_TS;
	dfsh_write_cmd(st, &cmd, sizeof(cmd));
	cmd = CMD_CAM_PWR_ON;
	dfsh_write_cmd(st, &cmd, sizeof(cmd));
	cmd = CMD_CAM_FSIN_START;
	dfsh_write_cmd(st, &cmd, sizeof(cmd));

	dev_info(tty->dev, "%s done\n", __func__);
	return 0;

dfsh_open_err:
	dev_err(tty->dev, "%s ERR %d\n", __func__, ret);
	return ret;
}

static struct tty_ldisc_ops dfsh_tty_ldisc_ops = {
	.owner				= THIS_MODULE,
	.magic				= TTY_LDISC_MAGIC,
	.name				= DFSH_NAME,
	.open				= dfsh_open,
	.close				= dfsh_close,
	.ioctl				= dfsh_ioctl,
	.receive_buf		= dfsh_receive_buf,
};

static struct of_device_id dfsh_of_match[] = {
	{ .compatible = "nvidia,tegra186-dfsh" },
	{ },
};

static int dfsh_probe(struct platform_device *pdev)
{
	int ret;

	dev_info(&pdev->dev, "%s\n", __func__);

	st = devm_kzalloc(&pdev->dev, sizeof(struct dfsh_state), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	st->pdev = pdev;
	platform_set_drvdata(pdev, st);

	ret = dfsh_of_dt(st, pdev->dev.of_node);
	if (ret) {
		if (ret == -ENODEV) {
			dev_info(&pdev->dev, "%s DT disabled\n", __func__);
		} else {
			dev_err(&pdev->dev, "%s _of_dt ERR\n", __func__);
			ret = -ENODEV;
		}
		goto dfsh_open_err;
	}

	dfsh_pm_init(st);

	dev_info(&pdev->dev, "%s done\n", __func__);

	ret = tty_register_ldisc(N_NVS_DFSH, &dfsh_tty_ldisc_ops);

	return ret;

dfsh_open_err:
	dev_err(&pdev->dev, "%s ERR %d\n", __func__, ret);
	return ret;
}

static int dfsh_remove(struct platform_device *pdev)
{
	struct dfsh_state *st = platform_get_drvdata(pdev);
	int ret = tty_unregister_ldisc(N_NVS_DFSH);

	if (ret)
		pr_err("%s ERR=%d\n", __func__, ret);

	gpio_free(st->gpio_rst);
	gpio_free(st->gpio_boot0);

	kfree(st);

	return 0;
}

static struct platform_driver dfsh_driver = {
	.driver = {
		.name	= DFSH_NAME,
		.of_match_table = of_match_ptr(dfsh_of_match),
	},
	.probe		= dfsh_probe,
	.remove		= dfsh_remove,
};

module_platform_driver(dfsh_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("NVS line discipline driver for DFSH");
MODULE_AUTHOR("NVIDIA Corporation");
