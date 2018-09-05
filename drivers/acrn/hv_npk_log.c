/*
 * ACRN Hypervisor NPK Log
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2018 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * BSD LICENSE
 *
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * The hv_npk_log driver is to enable/disable/configure the ACRN hypervisor
 * NPK log. It communicates with the hypervisor via the HC_SETUP_HV_NPK_LOG
 * hypercall, and exposes the interface to usperspace via sysfs.
 * With this driver, the user could:
 * 1. Configure the Master/Channel used for the hypervisor NPK log.
 * 2. Configure the log level of the hypervisor NPK log.
 * 3. Enable/Disable the hypervisor NPK log.
 *
 *           +-------------------------------------+
 *           |    Interfaces exposed by the driver |
 *       SOS | U:   and used by the applications   |
 *           | +----------------^----------------+ |
 *           |                  |                  |
 *           | K:     +---------v---------+        |
 *           |        | hv_npk_log driver |        |
 *           |        +---------^---------+        |
 *           +------------------|------------------+
 *                              | HC_SETUP_HV_NPK_LOG
 *           +------------------|------------------+
 *       HV  |          +-------v-------+          |
 *           |          |    npk_log    |          |
 *           |          +-------+-------+          |
 *           +------------------|------------------+
 *           |
 *           +-------------+----v----+-------------+
 *  NPK MMIO |             | /////// |             |
 *           |             | /////// |             |
 *           +-------------+----+----+-------------+
 *                              |
 *                              +---+ The Master/Channel reserved
 *                                    for the hypervisor NPK logs
 */

#define pr_fmt(fmt) "ACRN HV_NPK_LOG: " fmt

#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/pci.h>
#include <linux/vhm/acrn_hv_defs.h>
#include <linux/vhm/vhm_hypercall.h>
#include "hv_npk_log.h"

#define HV_NPK_LOG_USAGE \
	"echo \"E[nable] [#M #C] [#L]\" to enable    the ACRN HV NPK Log\n" \
	"echo \"D[isable]\"             to disable   the ACRN HV NPK Log\n" \
	"echo \"C[onfig] [#M #C] [#L]\" to configure the ACRN HV NPK Log\n"

static struct hv_npk_log_conf *hnl_conf;

/* Try to get the master/channel based on the given address */
static int addr2mc(phys_addr_t addr, int *master, int *channel)
{
	phys_addr_t offset, base;
	unsigned int start, end;
	int c, m;

	if (!hnl_conf || !master || !channel || !hnl_conf->nchan)
		return -EINVAL;

	/* check if the addr belongs to SW_BAR or FW_BAR */
	if (addr >= hnl_conf->stmr_base && addr < hnl_conf->stmr_end) {
		base = hnl_conf->stmr_base;
		start = hnl_conf->sw_start;
		end = hnl_conf->sw_end;
	} else if (addr >= hnl_conf->ftmr_base && addr < hnl_conf->ftmr_end) {
		base = hnl_conf->ftmr_base;
		start = hnl_conf->fw_start;
		end = hnl_conf->fw_end;
	} else {
		return -EINVAL;
	}

	offset = addr - base;
	if (offset % NPK_CHAN_SIZE)
		return -EINVAL;

	c = offset / NPK_CHAN_SIZE;
	m = c / hnl_conf->nchan;
	c = c % hnl_conf->nchan;
	if (start + m > end)
		return -EINVAL;

	*channel = c;
	*master = m + start;
	return 0;
}

/* Try to get the MMIO address based on the given master/channel */
static int mc2addr(phys_addr_t *addr, unsigned int master, unsigned int channel)
{
	phys_addr_t base;
	unsigned int start;

	if (!hnl_conf || !addr || !hnl_conf->nchan
			|| channel >= hnl_conf->nchan)
		return -EINVAL;

	/* check if the master belongs to SW_BAR or FW_BAR */
	if (master >= hnl_conf->sw_start && master <= hnl_conf->sw_end) {
		base = hnl_conf->stmr_base;
		start = hnl_conf->sw_start;
	} else if (master >= hnl_conf->fw_start && master <= hnl_conf->fw_end) {
		base = hnl_conf->ftmr_base;
		start = hnl_conf->fw_start;
	} else {
		return -EINVAL;
	}

	*addr = base + ((master - start) * hnl_conf->nchan + channel)
		* NPK_CHAN_SIZE;
	return 0;
}

static int npk_dev_match(struct device *dev, void *data)
{
	return 1;
}

/* Check if the NPK device/driver exists, and get info from them */
static int load_npk_conf(void)
{
	u32 reg;
	int err;
	void __iomem *base;
	struct device *dev;
	struct pci_dev *pdev;
	struct device_driver *drv;

	/* check if the NPK device and driver exists */
	drv = driver_find(NPK_DRV_NAME, &pci_bus_type);
	if (!drv) {
		pr_err("Cannot find the %s driver\n", NPK_DRV_NAME);
		return -ENODEV;
	}

	dev = driver_find_device(drv, NULL, NULL, npk_dev_match);
	if (!dev) {
		pr_err("Cannot find the NPK device\n");
		return -ENODEV;
	}

	hnl_conf = kzalloc(sizeof(struct hv_npk_log_conf), GFP_KERNEL);
	if (!hnl_conf)
		return -ENOMEM;

	/* get the base address of FW_BAR */
	pdev = to_pci_dev(dev);
	err = pci_read_config_dword(pdev, PCI_REG_FW_LBAR, &reg);
	if (err)
		return err;
	hnl_conf->ftmr_base = reg & 0xfffc0000U;
	err = pci_read_config_dword(pdev, PCI_REG_FW_UBAR, &reg);
	if (err)
		return err;
	hnl_conf->ftmr_base |= ((phys_addr_t)reg << 32);

	/* read out some configurations of NPK */
	base = devm_ioremap(dev, pdev->resource[TH_MMIO_CONFIG].start,
			resource_size(&(pdev->resource[TH_MMIO_CONFIG])));
	if (!base) {
		pr_err("Cannot map the NPK configuration address\n");
		goto error;
	}

	reg = ioread32(base + REG_STH_STHCAP0);
	hnl_conf->sw_start = reg & 0xffffU;
	hnl_conf->sw_end = reg >> 16;
	reg = ioread32(base + REG_STH_STHCAP1);
	hnl_conf->nchan = reg & 0xffU;
	hnl_conf->fw_end = reg >> 24;
	reg = ioread32(base + REG_STH_STHCAP2);
	hnl_conf->fw_start = reg & 0xffffU;
	devm_iounmap(dev, base);

	hnl_conf->status = HV_NPK_LOG_UNKNOWN;
	hnl_conf->master = HV_NPK_LOG_UNKNOWN;
	hnl_conf->channel = HV_NPK_LOG_UNKNOWN;
	hnl_conf->loglevel = HV_NPK_LOG_UNKNOWN;
	hnl_conf->stmr_base = pdev->resource[TH_MMIO_SW].start;

	if (hnl_conf->sw_end < hnl_conf->sw_start
			|| hnl_conf->fw_end < hnl_conf->fw_start
			|| hnl_conf->nchan == 0)
		goto error;

	hnl_conf->stmr_end = hnl_conf->stmr_base + (hnl_conf->sw_end -
			hnl_conf->sw_start) * hnl_conf->nchan * NPK_CHAN_SIZE;
	hnl_conf->ftmr_end = hnl_conf->ftmr_base + (hnl_conf->fw_end -
			hnl_conf->fw_start) * hnl_conf->nchan * NPK_CHAN_SIZE;

	return 0;

error:
	kfree(hnl_conf);
	hnl_conf = NULL;
	return -EINVAL;
}

/* User interface to set the configuration */
static int hv_npk_log_conf_set(const char *val, const struct kernel_param *kp)
{
	char **argv;
	int i, argc, ret = -EINVAL;
	struct hv_npk_log_param cmd;
	unsigned int args[HV_NPK_LOG_MAX_PARAM];

	if (!hnl_conf && load_npk_conf() < 0)
		return -EINVAL;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv)
		return -ENOMEM;
	if (!argc || argc > HV_NPK_LOG_MAX_PARAM)
		goto out;

	for (i = 1; i < argc; i++)
		if (kstrtouint(argv[i], 10, &args[i]) < 0)
			goto out;

	memset(&cmd, 0, sizeof(struct hv_npk_log_param));
	cmd.loglevel = 0xffffU;
	cmd.cmd = HV_NPK_LOG_CMD_INVALID;
	switch (tolower(argv[0][0])) {
	case 'e': /* enable */
	case 'c': /* configure */
		if (!strncasecmp(argv[0], "enable", strlen(argv[0]))) {
			cmd.cmd = HV_NPK_LOG_CMD_ENABLE;
		} else if (!strncasecmp(argv[0], "configure", strlen(argv[0]))
				&& argc != 1) {
			cmd.cmd = HV_NPK_LOG_CMD_CONF;
		} else
			break;

		if (argc <= 2) {
			cmd.loglevel = argc == 2 ? args[1] : 0xffffU;
			if (hnl_conf->master == HV_NPK_LOG_UNKNOWN)
				mc2addr(&cmd.mmio_addr, HV_NPK_LOG_DFT_MASTER,
						HV_NPK_LOG_DFT_CHANNEL);
		} else if (argc > 2 && !mc2addr(&cmd.mmio_addr,
					args[1], args[2])) {
			cmd.loglevel = argc == 4 ? args[3] : 0xffffU;
		}
		break;
	case 'd': /* disable */
		if (!strncasecmp(argv[0], "disable", strlen(argv[0]))
				&& argc == 1)
			cmd.cmd = HV_NPK_LOG_CMD_DISABLE;
		break;
	default:
		pr_err("Unsupported command : %s\n", argv[0]);
		break;
	}

	if (cmd.cmd != HV_NPK_LOG_CMD_INVALID) {
		ret = hcall_setup_hv_npk_log(virt_to_phys(&cmd));
		ret = (ret < 0 || cmd.res == HV_NPK_LOG_RES_KO) ? -EINVAL : 0;
	}

out:
	argv_free(argv);
	if (ret < 0)
		pr_err("Unsupported configuration : %s\n", val);
	return ret;
}

/* User interface to query the configuration */
static int hv_npk_log_conf_get(char *buffer, const struct kernel_param *kp)
{
	long ret;
	struct hv_npk_log_param query;

	if (!hnl_conf && load_npk_conf() < 0)
		return sprintf(buffer, "%s\n",
				"Failed to init the configuration.");

	memset(&query, 0, sizeof(struct hv_npk_log_param));
	query.cmd = HV_NPK_LOG_CMD_QUERY;
	ret = hcall_setup_hv_npk_log(virt_to_phys(&query));
	if (ret < 0 || query.res == HV_NPK_LOG_RES_KO)
		return sprintf(buffer, "%s\n", "Failed to invoke the hcall.");

	if (!addr2mc(query.mmio_addr, &hnl_conf->master, &hnl_conf->channel)) {
		hnl_conf->status = query.res == HV_NPK_LOG_RES_ENABLED ?
			HV_NPK_LOG_ENABLED : HV_NPK_LOG_DISABLED;
	} else {
		hnl_conf->status = HV_NPK_LOG_UNKNOWN;
		hnl_conf->master = HV_NPK_LOG_UNKNOWN;
		hnl_conf->channel = HV_NPK_LOG_UNKNOWN;
	}
	hnl_conf->loglevel = query.loglevel;

	return scnprintf(buffer, PAGE_SIZE, "Master(SW:%d~%d FW:%d~%d):%d "
			"Channel(0~%d):%d Status:%d Log Level: %d\n%s\n",
			hnl_conf->sw_start, hnl_conf->sw_end,
			hnl_conf->fw_start, hnl_conf->fw_end,
			hnl_conf->master, hnl_conf->nchan - 1,
			hnl_conf->channel, hnl_conf->status,
			hnl_conf->loglevel, HV_NPK_LOG_USAGE);
}

/* /sys/module/hv_npk_log/parameters/hv_npk_log_conf */
static struct kernel_param_ops hv_npk_log_conf_param_ops = {
	.set = hv_npk_log_conf_set,
	.get = hv_npk_log_conf_get,
};
module_param_cb(hv_npk_log_conf, &hv_npk_log_conf_param_ops, NULL, 0644);

static struct miscdevice hv_npk_log_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hv_npk_log",
};

static int __init hv_npk_log_init(void)
{
	return misc_register(&hv_npk_log_misc);
}

static void __exit hv_npk_log_exit(void)
{
	kfree(hnl_conf);

	misc_deregister(&hv_npk_log_misc);
}

module_init(hv_npk_log_init);
module_exit(hv_npk_log_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Intel Corp., http://www.intel.com");
MODULE_DESCRIPTION("Driver for the Intel ACRN Hypervisor NPK Log");
MODULE_VERSION("0.1");
