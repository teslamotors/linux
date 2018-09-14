/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef __CRLMODULE_LITE_H
#define __CRLMODULE_LITE_H

#define CRLMODULE_LITE_NAME		"crlmodule-lite"

struct crlmodule_lite_platform_data {
	unsigned short i2c_addr;
	unsigned short i2c_adapter;

	unsigned int ext_clk;		/* sensor external clk */

	unsigned int lanes;		/* Number of CSI-2 lanes */
	const s64 *op_sys_clock;

	int xshutdown;			/* gpio */
	char module_name[16]; /* module name from ACPI */
	char suffix; /* suffix to identify multi sensors, abcd.. */
};

#endif /* __CRLMODULE_LITE_H  */
