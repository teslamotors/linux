/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015 - 2018 Intel Corporation
 *
 * Based on ATOMISP dw9714 implementation by
 * Huang Shenbo <shenbo.huang@intel.com.
 *
 */

#ifndef __DW9714_H__
#define __DW9714_H__

#include <linux/types.h>

#define DW9714_NAME		"dw9714"

#define DW9714_MAX_FOCUS_POS	1023
#define DW9714_CTRL_STEPS	16 /* Keep this value power of 2 */
#define DW9714_CTRL_DELAY_US	1000

#define VCM_DEFAULT_S 0x0
#define VCM_VAL(data, s) (u16)((data) << 4 | (s))

struct dw9714_platform_data {
	struct device *sensor_dev;
	int gpio_xsd; /* Should be < 0 if not used */
};

#ifdef CONFIG_INTEL_IPU4_OV13858
extern bool vcm_in_use;
extern void crlmodule_vcm_gpio_set_value(unsigned int gpio, int value);
#endif
#endif
