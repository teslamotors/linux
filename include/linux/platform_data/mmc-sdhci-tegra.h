/*
 * Copyright (C) 2009 Palm, Inc.
 * Author: Yvonne Yip <y@palm.com>
 *
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __PLATFORM_DATA_TEGRA_SDHCI_H
#define __PLATFORM_DATA_TEGRA_SDHCI_H

#include <linux/mmc/host.h>
#include <asm/mach/mmc.h>

/*
 * MMC_OCR_1V8_MASK will be used in board sdhci file
 * Example for cardhu it will be used in board-cardhu-sdhci.c
 * for built_in = 0 devices enabling ocr_mask to MMC_OCR_1V8_MASK
 * sets the voltage to 1.8V
 */
#define MMC_OCR_1V8_MASK    0x00000008
#define MMC_OCR_2V8_MASK    0x00010000
#define MMC_OCR_3V2_MASK    0x00100000
#define MMC_OCR_3V3_MASK    0x00200000

/* uhs mask can be used to mask any of the UHS modes support */
#define MMC_UHS_MASK_SDR12	0x1
#define MMC_UHS_MASK_SDR25	0x2
#define MMC_UHS_MASK_SDR50	0x4
#define MMC_UHS_MASK_DDR50	0x8
#define MMC_UHS_MASK_SDR104	0x10
#define MMC_MASK_HS200		0x20
#define MMC_MASK_HS400		0x40

/* runtime power management implementation type */
enum {
	RTPM_TYPE_DELAY_CG = 0,
	RTPM_TYPE_MMC,
	RTPM_TYPE_NONE
};

#define IS_MMC_RTPM(type)	(type == RTPM_TYPE_MMC)
#define IS_RTPM_DELAY_CG(type)	(type == RTPM_TYPE_DELAY_CG)
#define GET_RTPM_TYPE(type) \
		(IS_RTPM_DELAY_CG(type) ? "delayed clock gate rtpm" : \
		"mmc rtpm coupled with clock gate")

struct tegra_sdhci_platform_data {
	bool pwrdet_support;
	int cd_gpio;
	int wp_gpio;
	int power_gpio;
	int is_8bit;
	int pm_flags;
	int pm_caps;
	int nominal_vcore_mv;
	int min_vcore_override_mv;
	int boot_vcore_mv;
	u32 cpu_speedo;
	bool adma3_enable;
	unsigned int instance;
	unsigned int max_clk_limit;
	unsigned int ddr_clk_limit;
	unsigned int tap_delay;
	bool is_ddr_tap_delay;
	unsigned int ddr_tap_delay;
	unsigned int trim_delay;
	bool is_ddr_trim_delay;
	unsigned int ddr_trim_delay;
	unsigned int dqs_trim_delay;
	unsigned int dqs_trim_delay_hs533;
	unsigned int uhs_mask;
	struct mmc_platform_data mmc_data;
	bool power_off_rail;
	bool cd_wakeup_incapable;
	unsigned int calib_3v3_offsets;	/* Format to be filled: 0xXXXXPDPU */
	unsigned int calib_1v8_offsets;	/* Format to be filled: 0xXXXXPDPU */
	unsigned int compad_vref_3v3;
	unsigned int compad_vref_1v8;
	bool disable_clock_gate; /* no clock gate when true */
	bool update_pinctrl_settings;
	unsigned int default_drv_type;
	bool dll_calib_needed;
	bool pwr_off_during_lp0;
	bool disable_auto_cal;
	unsigned int auto_cal_step;
	bool en_io_trim_volt;
	bool is_emmc;
	bool limit_vddio_max_volt;
	bool enb_ext_loopback;
	bool enable_hs533_mode;
	bool dynamic_dma_pio_switch;
	bool is_sd_device;
	bool is_fix_clock_freq;
	/*Index 0 is fixed for ID mode. Rest according the MMC_TIMINGS modes*/
	bool enable_autocal_slew_override;
	unsigned int rtpm_type;
	bool enable_cq;
	bool enable_hw_cq;
	bool en_strobe; /* Enable enhance strobe mode for eMMC */
	bool enb_feedback_clock;
	bool en_periodic_calib;
	unsigned int fixed_clk_freq_table[MMC_TIMINGS_MAX_MODES + 1];
	bool disable_clk_gate;
	bool disable_runtime_pm;
	bool en_32bit_bc; /* Enable 32 bit block count for SD */
	bool en_periodic_cflush; /* Enable periodic cache flush for eMMC */
};

#endif
