/*
 * Copyright (c) 2012-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irqchip/tegra.h>

#include <mach/irqs.h>

#include "gpio-names.h"
#include "iomap.h"

static int tegra_gpio_wakes[] = {
	TEGRA_GPIO_PA2,				/* wake0 */
	TEGRA_GPIO_PA6,				/* wake1 */
	TEGRA_GPIO_PEE1,			/* wake2 */
	TEGRA_GPIO_PB4,				/* wake3 */
	TEGRA_GPIO_PE6,				/* wake4 */
	TEGRA_GPIO_PE7,				/* wake5 */
	TEGRA_GPIO_PG3,				/* wake6 */
	TEGRA_GPIO_PD4,				/* wake7 */
	TEGRA_GPIO_PH2,				/* wake8 */
	-EINVAL,				/* wake9 */
	TEGRA_GPIO_PH6,				/* wake10 */
	TEGRA_GPIO_PI1,				/* wake11 */
	TEGRA_GPIO_PJ0,				/* wake12 */
	TEGRA_GPIO_PJ3,				/* wake13 */
	TEGRA_GPIO_PK4,				/* wake14 */
	TEGRA_GPIO_PK6,				/* wake15 */
	-EINVAL,				/* wake16 */
	TEGRA_GPIO_PM4,				/* wake17 */
	-EINVAL,				/* wake18 */
	TEGRA_GPIO_PCC0,			/* wake19 */
	TEGRA_GPIO_PF1,				/* wake20 */
	TEGRA_GPIO_PL1,				/* wake21 */
	TEGRA_GPIO_PY5,				/* wake22 */
	TEGRA_GPIO_PY4,				/* wake23 */
	TEGRA_GPIO_PX5,				/* wake24 */
	TEGRA_GPIO_PX6,				/* wake25 */
	TEGRA_GPIO_PX7,				/* wake26 */
	TEGRA_GPIO_PY0,				/* wake27 */
	TEGRA_GPIO_PY1,				/* wake28 */
	-EINVAL,				/* wake29 */
	-EINVAL,				/* wake30 */
	-EINVAL,				/* wake31 */
	TEGRA_GPIO_PX3,				/* wake32 */
	TEGRA_GPIO_PX4,				/* wake33 */
	TEGRA_GPIO_PZ0,				/* wake34 */
	TEGRA_GPIO_PZ1,				/* wake35 */
	TEGRA_GPIO_PZ2,				/* wake36 */
	-EINVAL,				/* wake37 */
	-EINVAL,				/* wake38 */
	-EINVAL,				/* wake39 */
	-EINVAL,				/* wake40 */
	-EINVAL,				/* wake41 */
	-EINVAL,				/* wake42 */
	-EINVAL,				/* wake43 */
	-EINVAL,				/* wake44 */
	TEGRA_GPIO_PP4,				/* wake45 */
	-EINVAL,				/* wake46 */
	TEGRA_GPIO_PS2,				/* wake47 */
	TEGRA_GPIO_PS3,				/* wake48 */
	TEGRA_GPIO_PZ5,				/* wake49 */
	TEGRA_GPIO_PCC6,			/* wake50 */
	-EINVAL,				/* wake51 */
	TEGRA_GPIO_PH5,				/* wake52 */
	TEGRA_GPIO_PCC1,			/* wake53 */
	TEGRA_GPIO_PCC4,			/* wake54 */
	TEGRA_GPIO_PCC5,			/* wake55 */
	TEGRA_GPIO_PV2,				/* wake56 */
	TEGRA_GPIO_PV3,				/* wake57 */
	TEGRA_GPIO_PV4,				/* wake58 */
	TEGRA_GPIO_PI7,				/* wake59 */
	-EINVAL,				/* wake60 */
	-EINVAL,				/* wake61 */
	TEGRA_GPIO_PX1,				/* wake62 */
	TEGRA_GPIO_PX2,				/* wake63 */
};

static int tegra_wake_event_irq[] = {
	-EAGAIN, /* PEX_WAKE_N */		/* wake0 */
	-EAGAIN,				/* wake1 */
	-EAGAIN, /* QSPI_CS_N */		/* wake2 */
	-EAGAIN, /* SPI2_MOSI */		/* wake3 */
	-EAGAIN,				/* wake4 */
	-EAGAIN,				/* wake5 */
	-EAGAIN, /* UART2_CTS */		/* wake6 */
	-EAGAIN, /* UART3_CTS */		/* wake7 */
	-EAGAIN, /* WIFI_WAKE_AP */		/* wake8 */
	-EINVAL,				/* wake9 */
	-EAGAIN,				/* wake10 */
	-EAGAIN, /* NFC_INT */			/* wake11 */
	-EAGAIN, /* GEN1_I2C_SDA */		/* wake12 */
	-EAGAIN, /* GEN2_I2C_SDA */		/* wake13 */
	-EAGAIN,				/* wake14 */
	-EAGAIN,				/* wake15 */
	INT_RTC,				/* wake16 */
	-EAGAIN, /* SDMMC1_DAT1 */		/* wake17 */
	-EINVAL,				/* wake18 */
	-EAGAIN, /* HDMI_CEC */			/* wake19 */
	-EAGAIN, /* GEN3_I2C_SDA */		/* wake20 */
	-EAGAIN,				/* wake21 */
	-EAGAIN, /* CLK_32K_OUT */		/* wake22 */
	-EAGAIN, /* PWR_I2C_SDA */		/* wake23 */
	-EAGAIN, /* BUTTON_POWER_ON */		/* wake24 */
	-EAGAIN, /* BUTTON_VOL_UP */		/* wake25 */
	-EAGAIN, /* BUTTON_VOL_DOWN */		/* wake26 */
	-EAGAIN, /* BUTTON_SLIDE_SW */		/* wake27 */
	-EAGAIN, /* BUTTON_HOME */		/* wake28 */
	-EINVAL,				/* wake29 */
	-EINVAL,				/* wake30 */
	-EINVAL,				/* wake31 */
	-EAGAIN, /* ALS_PROX_INT */		/* wake32 */
	-EAGAIN, /* TEMP_ALERT */		/* wake33 */
	-EAGAIN,				/* wake34 */
	-EAGAIN,				/* wake35 */
	-EAGAIN,				/* wake36 */
	-EINVAL,				/* wake37 */
	-EINVAL,				/* wake38 */
	INT_USB,				/* wake39 */
	INT_USB2,				/* wake40 */
	INT_USB2,				/* wake41 */
	INT_USB2,				/* wake42 */
	INT_USB2,				/* wake43 */
	INT_XUSB_PADCTL,			/* wake44 */
	-EAGAIN,				/* wake45 */
	-EINVAL,				/* wake46 */
	-EAGAIN, /* CAM_I2C_SCL */		/* wake47 */
	-EAGAIN, /* CAM_I2C_SDA */		/* wake48 */
	-EAGAIN,				/* wake49 */
	-EAGAIN, /* DP_HPD0 */			/* wake50 */
	INT_EXTERNAL_PMU,			/* wake51 */
	-EAGAIN, /* BT_WAKE_AP */		/* wake52 */
	-EAGAIN, /* HDMI_INT_DP_HPD */		/* wake53 */
	-EAGAIN, /* USB_VBUS_EN0 */		/* wake54 */
	-EAGAIN, /* USB_VBUS_EN1 */		/* wake55 */
	-EAGAIN, /* LCD_RST */			/* wake56 */
	-EAGAIN, /* LCD_GPIO1 */		/* wake57 */
	-EAGAIN, /* LCD_GPIO2 */		/* wake58 */
	-EAGAIN, /* UART4_CTS */		/* wake59 */
	-EINVAL,				/* wake60 */
	-EINVAL,				/* wake61 */
	-EAGAIN, /* TOUCH_INT */		/* wake62 */
	-EAGAIN, /* MOTION_INT */		/* wake63 */
};

static int __init tegra21x_wakeup_table_init(void)
{
	tegra_gpio_wake_table = tegra_gpio_wakes;
	tegra_irq_wake_table = tegra_wake_event_irq;
	tegra_wake_table_len = ARRAY_SIZE(tegra_gpio_wakes);
	return 0;
}

int __init tegra_wakeup_table_init(void)
{
	return tegra21x_wakeup_table_init();
}
