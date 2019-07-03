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
#define CRL_MAX_GPIO_POWERUP_SEQ	4

/* set this flag if this module needs serializer initialization */
#define CRL_MODULE_FL_INIT_SER	BIT(0)
/* set this flag if this module has extra powerup sequence */
#define CRL_MODULE_FL_POWERUP	BIT(1)
/* set this flag if this module needs reset signal */
#define CRL_MODULE_FL_RESET	BIT(2)

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

	/* specify gpio pins of Deser for PWDN, FSIN, RESET. */
	int xshutdown;
	int fsin;
	int reset;

	/* specify gpio pins boot timing. */
	/* Bit 3 write 0/1 on GPIO3
	 * Bit 2 write 0/1 on GPIO2
	 * Bit 1 write 0/1 on GPIO1
	 * Bit 0 write 0/1 on GPIO0
	 */
	char gpio_powerup_seq[CRL_MAX_GPIO_POWERUP_SEQ];

	/* module_flags can be:
	 * CRL_MODULE_FL_INIT_SER
	 * CRL_MODULE_FL_POWERUP
	 * CRL_MODULE_FL_RESET
	 */
	unsigned int module_flags;

	struct crl_custom_gpio custom_gpio[CRL_MAX_CUSTOM_GPIO_AMOUNT];
	char module_name[16]; /* module name from ACPI */
	int crl_irq_pin;
	unsigned int irq_pin_flags;
	char irq_pin_name[16];
	const char *id_string;
	char suffix; /* suffix to identify multi sensors, abcd.. */
	unsigned int high_framevalid_flags; /* high framevaild flags*/
};

#endif /* __CRLMODULE_H  */
