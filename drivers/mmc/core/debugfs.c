/*
 * Debugfs support for hosts and cards
 *
 * Copyright (C) 2008 Atmel Corporation
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/moduleparam.h>
#include <linux/export.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/fault-inject.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>

#include "core.h"
#include "mmc_ops.h"

#ifdef CONFIG_FAIL_MMC_REQUEST

static DECLARE_FAULT_ATTR(fail_default_attr);
static char *fail_request;
module_param(fail_request, charp, 0);

#endif /* CONFIG_FAIL_MMC_REQUEST */

/* The debugfs functions are optimized away when CONFIG_DEBUG_FS isn't set. */
static int mmc_ios_show(struct seq_file *s, void *data)
{
	static const char *vdd_str[] = {
		[8]	= "2.0",
		[9]	= "2.1",
		[10]	= "2.2",
		[11]	= "2.3",
		[12]	= "2.4",
		[13]	= "2.5",
		[14]	= "2.6",
		[15]	= "2.7",
		[16]	= "2.8",
		[17]	= "2.9",
		[18]	= "3.0",
		[19]	= "3.1",
		[20]	= "3.2",
		[21]	= "3.3",
		[22]	= "3.4",
		[23]	= "3.5",
		[24]	= "3.6",
	};
	struct mmc_host	*host = s->private;
	struct mmc_ios	*ios = &host->ios;
	const char *str;

	seq_printf(s, "clock:\t\t%u Hz\n", ios->clock);
	if (host->actual_clock)
		seq_printf(s, "actual clock:\t%u Hz\n", host->actual_clock);
	seq_printf(s, "vdd:\t\t%u ", ios->vdd);
	if ((1 << ios->vdd) & MMC_VDD_165_195)
		seq_printf(s, "(1.65 - 1.95 V)\n");
	else if (ios->vdd < (ARRAY_SIZE(vdd_str) - 1)
			&& vdd_str[ios->vdd] && vdd_str[ios->vdd + 1])
		seq_printf(s, "(%s ~ %s V)\n", vdd_str[ios->vdd],
				vdd_str[ios->vdd + 1]);
	else
		seq_printf(s, "(invalid)\n");

	switch (ios->bus_mode) {
	case MMC_BUSMODE_OPENDRAIN:
		str = "open drain";
		break;
	case MMC_BUSMODE_PUSHPULL:
		str = "push-pull";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "bus mode:\t%u (%s)\n", ios->bus_mode, str);

	switch (ios->chip_select) {
	case MMC_CS_DONTCARE:
		str = "don't care";
		break;
	case MMC_CS_HIGH:
		str = "active high";
		break;
	case MMC_CS_LOW:
		str = "active low";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "chip select:\t%u (%s)\n", ios->chip_select, str);

	switch (ios->power_mode) {
	case MMC_POWER_OFF:
		str = "off";
		break;
	case MMC_POWER_UP:
		str = "up";
		break;
	case MMC_POWER_ON:
		str = "on";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "power mode:\t%u (%s)\n", ios->power_mode, str);
	seq_printf(s, "bus width:\t%u (%u bits)\n",
			ios->bus_width, 1 << ios->bus_width);

	switch (ios->timing) {
	case MMC_TIMING_LEGACY:
		str = "legacy";
		break;
	case MMC_TIMING_MMC_HS:
		str = "mmc high-speed";
		break;
	case MMC_TIMING_SD_HS:
		str = "sd high-speed";
		break;
	case MMC_TIMING_UHS_SDR12:
		str = "sd uhs SDR12";
		break;
	case MMC_TIMING_UHS_SDR25:
		str = "sd uhs SDR25";
		break;
	case MMC_TIMING_UHS_SDR50:
		str = "sd uhs SDR50";
		break;
	case MMC_TIMING_UHS_SDR104:
		str = "sd uhs SDR104";
		break;
	case MMC_TIMING_UHS_DDR50:
		str = "sd uhs DDR50";
		break;
	case MMC_TIMING_MMC_DDR52:
		str = "mmc DDR52";
		break;
	case MMC_TIMING_MMC_HS200:
		str = "mmc HS200";
		break;
	case MMC_TIMING_MMC_HS400:
		str = "mmc HS400";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "timing spec:\t%u (%s)\n", ios->timing, str);

	switch (ios->signal_voltage) {
	case MMC_SIGNAL_VOLTAGE_330:
		str = "3.30 V";
		break;
	case MMC_SIGNAL_VOLTAGE_180:
		str = "1.80 V";
		break;
	case MMC_SIGNAL_VOLTAGE_120:
		str = "1.20 V";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "signal voltage:\t%u (%s)\n", ios->chip_select, str);

	return 0;
}

static int mmc_ios_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmc_ios_show, inode->i_private);
}

static const struct file_operations mmc_ios_fops = {
	.open		= mmc_ios_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int mmc_clock_opt_get(void *data, u64 *val)
{
	struct mmc_host *host = data;

	*val = host->ios.clock;

	return 0;
}

static int mmc_clock_opt_set(void *data, u64 val)
{
	struct mmc_host *host = data;

	/* We need this check due to input value is u64 */
	if (val > host->f_max)
		return -EINVAL;

	mmc_claim_host(host);
	mmc_set_clock(host, (unsigned int) val);
	mmc_release_host(host);

	return 0;
}

static int mmc_speed_opt_get(void *data, u64 *val)
{
	struct mmc_host *host = data;
	static const char *const uhs_speeds[] = {
		[UHS_SDR12_BUS_SPEED] = "SDR12 ",
		[UHS_SDR25_BUS_SPEED] = "SDR25 ",
		[UHS_SDR50_BUS_SPEED] = "SDR50 ",
		[UHS_SDR104_BUS_SPEED] = "SDR104 ",
		[UHS_DDR50_BUS_SPEED] = "DDR50 ",
	};
	const char *str = "";
	*val = 0;

	if (!host || !host->card)
		return 0;
	if (mmc_sd_card_uhs(host->card) &&
		(host->card->sd_bus_speed < ARRAY_SIZE(uhs_speeds))) {
		str = uhs_speeds[host->card->sd_bus_speed];
		*val = host->card->sd_bus_speed;
	} else if ((host->ios.timing == MMC_TIMING_MMC_HS)
			|| (host->ios.timing == MMC_TIMING_SD_HS)) {
		str = "high speed";
		*val = HIGH_SPEED_BUS_SPEED;
	} else if (host->ios.timing == MMC_TIMING_MMC_HS200) {
		str = "HS200";
		*val = UHS_SDR104_BUS_SPEED;
	} else if (host->ios.timing == MMC_TIMING_MMC_DDR52) {
		str = "DDR50";
		*val = UHS_DDR50_BUS_SPEED;
	} else if (host->ios.timing == MMC_TIMING_MMC_HS400) {
		str = "HS400";
		*val = UHS_HS400_BUS_SPEED;
	}

	pr_info("%s: current speed %s\n",
			mmc_hostname(host->card->host),
			str);
	return 0;
}

static int mmc_set_bus_speed_mode(struct mmc_host *host, u32 speed)
{
	struct mmc_card *card = host->card;
	int err = 0;
	u8 card_type = card->ext_csd.raw_card_type & EXT_CSD_CARD_TYPE_MASK;
	u32 caps = card->host->caps, caps2 = card->host->caps2;
	u64 current_mode;

	mmc_speed_opt_get(host, &current_mode);
	if (speed == UHS_DDR50_BUS_SPEED) {
		if ((current_mode == UHS_SDR104_BUS_SPEED)
			|| (current_mode == UHS_HS400_BUS_SPEED)) {
		/* check card and host capability for DDR50 to proceed */
			if (!(((caps & MMC_CAP_1_8V_DDR) &&
				(card_type & EXT_CSD_CARD_TYPE_DDR_1_8V)) ||
				((caps & MMC_CAP_1_2V_DDR) &&
				(card_type & EXT_CSD_CARD_TYPE_DDR_1_2V)))) {
				pr_info("%s: DDR mode is not supported \n",
				mmc_hostname(card->host));
				return  -EINVAL;
			}

			if (current_mode == UHS_HS400_BUS_SPEED)
				err = mmc_hs400_to_ddr(card);
			else if (current_mode == UHS_SDR104_BUS_SPEED)
				err = mmc_hs200_to_ddr(card);
			if (err) {
				pr_err("%s: switch to DDR failed with err %d\n",
				mmc_hostname(card->host), err);
				return  -EINVAL;
			} else {
				pr_err("%s: switch to DDR mode successful\n",
				mmc_hostname(card->host));
			}
		} else {
			pr_info("%s: Usage info, switch to DDR is allowed from HS200 & HS400 mode \n",
				mmc_hostname(card->host));
			return  -EINVAL;
		}
	} else if (speed == UHS_SDR104_BUS_SPEED) {
		if (current_mode == UHS_DDR50_BUS_SPEED) {
			if (!(((caps2 & MMC_CAP2_HS200_1_8V_SDR) &&
				(card_type & EXT_CSD_CARD_TYPE_HS200_1_8V)) ||
				((caps2 & MMC_CAP2_HS200_1_2V_SDR) &&
				(card_type & EXT_CSD_CARD_TYPE_HS200_1_2V)))) {
				pr_info("%s: HS200 mode is not supported \n",
				mmc_hostname(card->host));
				return  -EINVAL;
			}

			err = mmc_ddr_to_hs200(card);
			if (err) {
				pr_err("%s: switch to HS200 failed with error %d\n",
				mmc_hostname(card->host), err);
				return -EINVAL;
			} else {
				pr_err("%s: switch to HS200 mode successful\n",
				mmc_hostname(card->host));
			}
		} else {
			pr_info("%s: Usage info, switch to HS200 is allowed from DDR mode\n",
				mmc_hostname(card->host));
			return -EINVAL;

		}
	} else if (speed == UHS_HS400_BUS_SPEED) {
		if (current_mode == UHS_DDR50_BUS_SPEED) {
			if (!(((caps2 & MMC_CAP2_HS400_1_8V) &&
				(card_type & EXT_CSD_CARD_TYPE_HS400_1_8V)) ||
				((caps2 & MMC_CAP2_HS400_1_2V) &&
				(card_type & EXT_CSD_CARD_TYPE_HS400_1_2V)))) {
				pr_info("%s: HS400 mode is not supported \n",
				mmc_hostname(card->host));
				return -EINVAL;
			}

			err = mmc_ddr_to_hs400(card);
			if (err) {
				pr_err("%s: switch to HS400 failed with error %d\n",
				mmc_hostname(card->host), err);
				return -EINVAL;
			} else {
				pr_err("%s: switch to HS400 mode successful\n",
				mmc_hostname(card->host));
			}
		} else {
			pr_info("%s: Usage info, switch to HS400 is allowed from DDR mode\n",
				mmc_hostname(card->host));
			return -EINVAL;
		}
	}
	return err;
}

static int mmc_speed_opt_set(void *data, u64 val)
{
	int err = 0;
	struct mmc_host *host = data;
	u32 prev_timing, prev_maxdtr;
	u64 current_mode;

	mmc_speed_opt_get(host, &current_mode);

	if (host->card->type != MMC_TYPE_MMC) {
		pr_warn("%s: usage error, only MMC device is supported\n",
			mmc_hostname(host));
		return 0;
	}

	if ((val != UHS_DDR50_BUS_SPEED) && (val != UHS_HS400_BUS_SPEED)
		&& (val !=UHS_SDR104_BUS_SPEED)) {
		pr_warn("%s: usage error, set only 3 for HS200, 4 for DDR and 5 for HS400\n",
			mmc_hostname(host));
		return -EINVAL;
	}

	if (val == current_mode) {
		pr_warn("%s: usage info: Current eMMC mode is same as Switch Mode\n",
			mmc_hostname(host));
		return 0;
	}

	prev_timing = host->ios.timing;
	prev_maxdtr = host->card->sw_caps.uhs_max_dtr;

	/* Set bus speed mode of the card */
	mmc_claim_host(host);

	err = mmc_set_bus_speed_mode(host, val);
	if (err) {
		/* Restore to previous values in case of err*/
		host->ios.timing = prev_timing;
		host->card->sw_caps.uhs_max_dtr = prev_maxdtr;
		pr_err("%s: could not set bus speed error = %d\n",
			mmc_hostname(host), err);
	}
	mmc_release_host(host);
	return err;
}

DEFINE_SIMPLE_ATTRIBUTE(mmc_clock_fops, mmc_clock_opt_get, mmc_clock_opt_set,
	"%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(mmc_speed_fops, mmc_speed_opt_get, mmc_speed_opt_set,
	"%llu\n");

void mmc_add_host_debugfs(struct mmc_host *host)
{
	struct dentry *root;

	root = debugfs_create_dir(mmc_hostname(host), NULL);
	if (IS_ERR(root))
		/* Don't complain -- debugfs just isn't enabled */
		return;
	if (!root)
		/* Complain -- debugfs is enabled, but it failed to
		 * create the directory. */
		goto err_root;

	host->debugfs_root = root;

	if (!debugfs_create_file("ios", S_IRUSR, root, host, &mmc_ios_fops))
		goto err_node;

	if (!debugfs_create_file("clock", S_IRUSR | S_IWUSR, root, host,
			&mmc_clock_fops))
		goto err_node;

	if (!debugfs_create_file("speed", S_IRUSR | S_IWUSR, root, host,
			&mmc_speed_fops))
		goto err_node;

#ifdef CONFIG_MMC_CLKGATE
	if (!debugfs_create_u32("clk_delay", (S_IRUSR | S_IWUSR),
				root, &host->clk_delay))
		goto err_node;
#endif
#ifdef CONFIG_FAIL_MMC_REQUEST
	if (fail_request)
		setup_fault_attr(&fail_default_attr, fail_request);
	host->fail_mmc_request = fail_default_attr;
	if (IS_ERR(fault_create_debugfs_attr("fail_mmc_request",
					     root,
					     &host->fail_mmc_request)))
		goto err_node;
#endif
	return;

err_node:
	debugfs_remove_recursive(root);
	host->debugfs_root = NULL;
err_root:
	dev_err(&host->class_dev, "failed to initialize debugfs\n");
}

void mmc_remove_host_debugfs(struct mmc_host *host)
{
	debugfs_remove_recursive(host->debugfs_root);
}

static int mmc_dbg_card_status_get(void *data, u64 *val)
{
	struct mmc_card	*card = data;
	u32		status;
	int		ret;

	mmc_get_card(card);

	ret = mmc_send_status(data, &status);
	if (!ret)
		*val = status;

	mmc_put_card(card);

	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(mmc_dbg_card_status_fops, mmc_dbg_card_status_get,
		NULL, "%08llx\n");

static int mmc_dbg_card_speed_class_get(void *data, u64 *val)
{
	struct mmc_card	*card = data;

	*val = card->speed_class;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(mmc_dbg_card_speed_class_fops,
		mmc_dbg_card_speed_class_get, NULL, "%llu\n");

static int mmc_get_ext_csd_byte_val(struct mmc_card *card, u64 *val,
		unsigned int ext_csd_byte)
{
	u8 *ext_csd;
	int err = 0;

	ext_csd = kmalloc(512, GFP_KERNEL);
	if (!ext_csd) {
		err = -ENOMEM;
		return err;
	}

	mmc_claim_host(card->host);
	err = mmc_send_ext_csd(card, ext_csd);
	mmc_release_host(card->host);

	if (!err)
		*val = ext_csd[ext_csd_byte];

	kfree(ext_csd);
	return err;
}

static int mmc_dbg_ext_csd_eol_get(void *data, u64 *val)
{
	struct mmc_card *card = data;
	return mmc_get_ext_csd_byte_val(card, val, EXT_CSD_PRE_EOL_INFO);
}

DEFINE_SIMPLE_ATTRIBUTE(mmc_dbg_ext_csd_eol_fops,
			mmc_dbg_ext_csd_eol_get, NULL, "%llu\n");


static int mmc_dbg_ext_csd_life_time_type_a(void *data, u64 *val)
{
	struct mmc_card *card = data;
	return mmc_get_ext_csd_byte_val(card, val,
			EXT_CSD_DEVICE_LIFE_EST_TYP_A);
}

DEFINE_SIMPLE_ATTRIBUTE(mmc_dbg_ext_csd_life_time_type_a_fops,
		mmc_dbg_ext_csd_life_time_type_a, NULL, "%llu\n");


static int mmc_dbg_ext_csd_life_time_type_b(void *data, u64 *val)
{
	struct mmc_card *card = data;
	return mmc_get_ext_csd_byte_val(card, val,
			EXT_CSD_DEVICE_LIFE_EST_TYP_B);
}

DEFINE_SIMPLE_ATTRIBUTE(mmc_dbg_ext_csd_life_time_type_b_fops,
		mmc_dbg_ext_csd_life_time_type_b, NULL, "%llu\n");

static int mmc_dbg_ext_csd_device_type(void *data, u64 *val)
{
	struct mmc_card *card = data;
	return mmc_get_ext_csd_byte_val(card, val, EXT_CSD_CARD_TYPE);
}

DEFINE_SIMPLE_ATTRIBUTE(mmc_dbg_ext_csd_device_type_fops,
		mmc_dbg_ext_csd_device_type, NULL, "%llu\n");

/* Here index starts with zero*/
static char *mmc_ext_csd_read_by_index(int start, int end,
		int strlen, struct inode *inode)
{
	struct mmc_card *card = inode->i_private;
	char *buf;
	ssize_t n = 0;
	u8 *ext_csd;
	int err, i;

	buf = kmalloc(strlen + 1, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	ext_csd = kmalloc(512, GFP_KERNEL);
	if (!ext_csd) {
		kfree(buf);
		return ERR_PTR(-ENOMEM);
	}

	mmc_get_card(card);
	err = mmc_send_ext_csd(card, ext_csd);
	mmc_put_card(card);
	if (err)
		goto out_free;

	for (i = start; i <= end; i++)
		n += sprintf(buf + n, "%02x", ext_csd[i]);
	n += sprintf(buf + n, "\n");

	kfree(ext_csd);
	return buf;

out_free:
	kfree(ext_csd);
	kfree(buf);
	return NULL;
}

#define EXT_CSD_FW_V_STR_LEN 17
static int mmc_ext_csd_fw_v_open(struct inode *inode, struct file *filp)
{
	/*Firmware Version ext_csd 254:261 */
	filp->private_data = mmc_ext_csd_read_by_index(254, 261,
				EXT_CSD_FW_V_STR_LEN, inode);
	return 0;
}

static ssize_t mmc_ext_csd_fw_v_read(struct file *filp, char __user *ubuf,
				size_t cnt, loff_t *ppos)
{
	char *buf = filp->private_data;

	return simple_read_from_buffer(ubuf, cnt, ppos,
				       buf, EXT_CSD_FW_V_STR_LEN);
}

static int mmc_ext_csd_fw_v_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations mmc_dbg_ext_csd_fw_v_fops = {
	.open		= mmc_ext_csd_fw_v_open,
	.read		= mmc_ext_csd_fw_v_read,
	.release	= mmc_ext_csd_fw_v_release,
	.llseek		= default_llseek,
};

#define EXT_CSD_STR_LEN 1025

static int mmc_ext_csd_open(struct inode *inode, struct file *filp)
{
	filp->private_data = mmc_ext_csd_read_by_index(0, 511,
				EXT_CSD_STR_LEN, inode);
	return 0;
}

static ssize_t mmc_ext_csd_read(struct file *filp, char __user *ubuf,
				size_t cnt, loff_t *ppos)
{
	char *buf = filp->private_data;

	return simple_read_from_buffer(ubuf, cnt, ppos,
				       buf, EXT_CSD_STR_LEN);
}

static int mmc_ext_csd_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations mmc_dbg_ext_csd_fops = {
	.open		= mmc_ext_csd_open,
	.read		= mmc_ext_csd_read,
	.release	= mmc_ext_csd_release,
	.llseek		= default_llseek,
};

void mmc_add_card_debugfs(struct mmc_card *card)
{
	struct mmc_host	*host = card->host;
	struct dentry	*root;

	if (!host->debugfs_root)
		return;

	root = debugfs_create_dir(mmc_card_id(card), host->debugfs_root);
	if (IS_ERR(root))
		/* Don't complain -- debugfs just isn't enabled */
		return;
	if (!root)
		/* Complain -- debugfs is enabled, but it failed to
		 * create the directory. */
		goto err;

	card->debugfs_root = root;

	if (!debugfs_create_x32("state", S_IRUSR, root, &card->state))
		goto err;

	if (mmc_card_mmc(card) || mmc_card_sd(card))
		if (!debugfs_create_file("status", S_IRUSR, root, card,
					&mmc_dbg_card_status_fops))
			goto err;

	if (mmc_card_mmc(card)) {
		if (!debugfs_create_file("ext_csd", S_IRUSR, root, card,
					&mmc_dbg_ext_csd_fops))
			goto err;
		if (!debugfs_create_file("eol_status", S_IRUSR, root, card,
					&mmc_dbg_ext_csd_eol_fops))
			goto err;
		if (!debugfs_create_file("dhs_type_a", S_IRUSR, root, card,
					&mmc_dbg_ext_csd_life_time_type_a_fops))
			goto err;
		if (!debugfs_create_file("dhs_type_b", S_IRUSR, root, card,
					&mmc_dbg_ext_csd_life_time_type_b_fops))
			goto err;
		if (!debugfs_create_file("emmc_device_type", S_IRUSR, root,
					card, &mmc_dbg_ext_csd_device_type_fops))
			goto err;
		if (!debugfs_create_file("firmware_version", S_IRUSR, root,
					card, &mmc_dbg_ext_csd_fw_v_fops))
			goto err;
	}

	if (mmc_card_sd(card))
		if (!debugfs_create_file("speed_class", S_IROTH, root, card,
					&mmc_dbg_card_speed_class_fops))
			goto err;

	return;

err:
	debugfs_remove_recursive(root);
	card->debugfs_root = NULL;
	dev_err(&card->dev, "failed to initialize debugfs\n");
}

void mmc_remove_card_debugfs(struct mmc_card *card)
{
	debugfs_remove_recursive(card->debugfs_root);
}
