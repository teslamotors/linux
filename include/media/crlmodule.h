/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2014 - 2018 Intel Corporation
 * Generic driver for common register list based camera sensor modules
 *
 */

#ifndef __CRLMODULE_H
#define __CRLMODULE_H

#include <media/v4l2-subdev.h>

#define CRLMODULE_NAME		"crlmodule"

#define CRL_MAX_CUSTOM_GPIO_AMOUNT 3

struct crl_custom_gpio {
	char name[16];
	int number;
	unsigned int val;
	unsigned int undo_val;
};

struct crlmodule_platform_data {
	unsigned short i2c_addr;
	unsigned short i2c_adapter;

	unsigned int ext_clk;		/* sensor external clk */

	unsigned int lanes;		/* Number of CSI-2 lanes */
	const s64 *op_sys_clock;

	int xshutdown;			/* gpio */
	struct crl_custom_gpio custom_gpio[CRL_MAX_CUSTOM_GPIO_AMOUNT];
	char module_name[16]; /* module name from ACPI */
	int crl_irq_pin;
	unsigned int irq_pin_flags;
	char irq_pin_name[16];
	const char *id_string;
	unsigned int high_framevalid_flags; /* high framevaild flags*/
};

#endif /* __CRLMODULE_H  */
