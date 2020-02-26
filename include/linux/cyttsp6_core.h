/*
 * cyttsp6_core.h
 * Cypress TrueTouch(TM) Standard Product V4 Core Module.
 * For use with Cypress touchscreen controllers.
 * Supported parts include:
 * CY8CTMA46X
 * CY8CTMA1036/768
 * CYTMA445
 *
 * Copyright (C) 2012-2015 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#ifndef _LINUX_CYTTSP6_CORE_H
#define _LINUX_CYTTSP6_CORE_H

#include <linux/stringify.h>

#define CYTTSP6_I2C_NAME "cyttsp6_i2c_adapter"
#define CYTTSP6_SPI_NAME "cyttsp6_spi_adapter"

#define CYTTSP6_CORE_NAME "cyttsp6_core"
#define CYTTSP6_MT_NAME "cyttsp6_mt"
#define CYTTSP6_BTN_NAME "cyttsp6_btn"
#define CYTTSP6_PROXIMITY_NAME "cyttsp6_proximity"

#define CY_DRIVER_NAME TTDA
#define CY_DRIVER_MAJOR 02
#define CY_DRIVER_MINOR 05

#define CY_DRIVER_REVCTRL 776586

#define CY_DRIVER_VERSION		    \
__stringify(CY_DRIVER_NAME)		    \
"." __stringify(CY_DRIVER_MAJOR)	    \
"." __stringify(CY_DRIVER_MINOR)	    \
"." __stringify(CY_DRIVER_REVCTRL)

#define CY_DRIVER_DATE "20150206"	/* YYYYMMDD */

/* abs settings */
#define CY_IGNORE_VALUE             -1

enum cyttsp6_core_platform_flags {
	CY_CORE_FLAG_NONE		= 0,
	CY_CORE_FLAG_WAKE_ON_GESTURE	= (1 << 0),
	CY_CORE_FLAG_POWEROFF_ON_SLEEP	= (1 << 1),
	CY_CORE_FLAG_BOOTLOADER_FAST_EXIT = (1 << 2),
	/* choose SCAN_TYPE or TOUCH_MODE RAM ID to alter scan type */
	CY_CORE_FLAG_SCAN_MODE_USES_RAM_ID_SCAN_TYPE = (1 << 3),
};

enum cyttsp6_core_platform_easy_wakeup_gesture {
	CY_CORE_EWG_NONE,
	CY_CORE_EWG_TAP_TAP,
	CY_CORE_EWG_TWO_FINGER_SLIDE,
	CY_CORE_EWG_RESERVED,
	CY_CORE_EWG_WAKE_ON_INT_FROM_HOST = 0xFF,
};

enum cyttsp6_loader_platform_flags {
	CY_LOADER_FLAG_NONE,
	CY_LOADER_FLAG_CALIBRATE_AFTER_FW_UPGRADE,
	/* Use CONFIG_VER field in TT_CFG to decide TT_CFG update */
	CY_LOADER_FLAG_CHECK_TTCONFIG_VERSION,
	CY_LOADER_FLAG_CALIBRATE_AFTER_TTCONFIG_UPGRADE,
};

struct touch_settings {
	const uint8_t *data;
	uint32_t size;
	uint8_t tag;
};

struct cyttsp6_touch_firmware {
	const uint8_t *img;
	uint32_t size;
	const uint8_t *ver;
	uint8_t vsize;
};

struct cyttsp6_touch_config {
	struct touch_settings *param_regs;
	struct touch_settings *param_size;
	const uint8_t *fw_ver;
	uint8_t fw_vsize;
};

struct cyttsp6_loader_platform_data {
	struct cyttsp6_touch_firmware *fw;
	struct cyttsp6_touch_config *ttconfig;
	u32 flags;
};

typedef int (*cyttsp6_platform_read) (struct device *dev, u16 addr,
	void *buf, int size);

#define CY_TOUCH_SETTINGS_MAX 32

struct cyttsp6_core_platform_data {
	int irq_gpio;
	int rst_gpio;
	int err_gpio;
	int level_irq_udelay;
	int max_xfer_len;
	int (*xres)(struct cyttsp6_core_platform_data *pdata,
		struct device *dev);
	int (*init)(struct cyttsp6_core_platform_data *pdata,
		int on, struct device *dev);
	int (*power)(struct cyttsp6_core_platform_data *pdata,
		int on, struct device *dev, atomic_t *ignore_irq);
	int (*detect)(struct cyttsp6_core_platform_data *pdata,
		struct device *dev, cyttsp6_platform_read read);
	int (*irq_stat)(struct cyttsp6_core_platform_data *pdata,
		struct device *dev);
	int (*error_stat)(struct cyttsp6_core_platform_data *pdata,
		struct device *dev);
	struct touch_settings *sett[CY_TOUCH_SETTINGS_MAX];
	u32 flags;
	u8 easy_wakeup_gesture;
};

struct touch_framework {
	const int16_t  *abs;
	uint8_t         size;
	uint8_t         enable_vkeys;
} __packed;

enum cyttsp6_mt_platform_flags {
	CY_MT_FLAG_NONE = 0x00,
	CY_MT_FLAG_HOVER = 0x04,
	CY_MT_FLAG_FLIP = 0x08,
	CY_MT_FLAG_INV_X = 0x10,
	CY_MT_FLAG_INV_Y = 0x20,
	CY_MT_FLAG_VKEYS = 0x40,
	CY_MT_FLAG_NO_TOUCH_ON_LO = 0x80,
};

struct cyttsp6_mt_platform_data {
	struct touch_framework *frmwrk;
	unsigned short flags;
	char const *inp_dev_name;
	int vkeys_x;
	int vkeys_y;
};

struct cyttsp6_btn_platform_data {
	char const *inp_dev_name;
};

struct cyttsp6_proximity_platform_data {
	struct touch_framework *frmwrk;
	char const *inp_dev_name;
};

struct cyttsp6_platform_data {
	struct cyttsp6_core_platform_data *core_pdata;
	struct cyttsp6_mt_platform_data *mt_pdata;
	struct cyttsp6_btn_platform_data *btn_pdata;
	struct cyttsp6_proximity_platform_data *prox_pdata;
	struct cyttsp6_loader_platform_data *loader_pdata;
};

#endif /* _LINUX_CYTTSP6_CORE_H */
