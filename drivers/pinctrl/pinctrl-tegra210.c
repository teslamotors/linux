/*
 * Pinctrl data for the NVIDIA Tegra210 pinmux
 *
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/tegra-pmc.h>

#include "pinctrl-tegra.h"

/*
 * Most pins affected by the pinmux can also be GPIOs. Define these first.
 * These must match how the GPIO driver names/numbers its pins.
 */
#define _GPIO(offset)				(offset)

#define TEGRA_PIN_PEX_L0_RST_N_PA0		_GPIO(0)
#define TEGRA_PIN_PEX_L0_CLKREQ_N_PA1		_GPIO(1)
#define TEGRA_PIN_PEX_WAKE_N_PA2		_GPIO(2)
#define TEGRA_PIN_PEX_L1_RST_N_PA3		_GPIO(3)
#define TEGRA_PIN_PEX_L1_CLKREQ_N_PA4		_GPIO(4)
#define TEGRA_PIN_SATA_LED_ACTIVE_PA5		_GPIO(5)
#define TEGRA_PIN_PA6				_GPIO(6)
#define TEGRA_PIN_DAP1_FS_PB0			_GPIO(8)
#define TEGRA_PIN_DAP1_DIN_PB1			_GPIO(9)
#define TEGRA_PIN_DAP1_DOUT_PB2			_GPIO(10)
#define TEGRA_PIN_DAP1_SCLK_PB3			_GPIO(11)
#define TEGRA_PIN_SPI2_MOSI_PB4			_GPIO(12)
#define TEGRA_PIN_SPI2_MISO_PB5			_GPIO(13)
#define TEGRA_PIN_SPI2_SCK_PB6			_GPIO(14)
#define TEGRA_PIN_SPI2_CS0_PB7			_GPIO(15)
#define TEGRA_PIN_SPI1_MOSI_PC0			_GPIO(16)
#define TEGRA_PIN_SPI1_MISO_PC1			_GPIO(17)
#define TEGRA_PIN_SPI1_SCK_PC2			_GPIO(18)
#define TEGRA_PIN_SPI1_CS0_PC3			_GPIO(19)
#define TEGRA_PIN_SPI1_CS1_PC4			_GPIO(20)
#define TEGRA_PIN_SPI4_SCK_PC5			_GPIO(21)
#define TEGRA_PIN_SPI4_CS0_PC6			_GPIO(22)
#define TEGRA_PIN_SPI4_MOSI_PC7			_GPIO(23)
#define TEGRA_PIN_SPI4_MISO_PD0			_GPIO(24)
#define TEGRA_PIN_UART3_TX_PD1			_GPIO(25)
#define TEGRA_PIN_UART3_RX_PD2			_GPIO(26)
#define TEGRA_PIN_UART3_RTS_PD3			_GPIO(27)
#define TEGRA_PIN_UART3_CTS_PD4			_GPIO(28)
#define TEGRA_PIN_DMIC1_CLK_PE0			_GPIO(32)
#define TEGRA_PIN_DMIC1_DAT_PE1			_GPIO(33)
#define TEGRA_PIN_DMIC2_CLK_PE2			_GPIO(34)
#define TEGRA_PIN_DMIC2_DAT_PE3			_GPIO(35)
#define TEGRA_PIN_DMIC3_CLK_PE4			_GPIO(36)
#define TEGRA_PIN_DMIC3_DAT_PE5			_GPIO(37)
#define TEGRA_PIN_PE6				_GPIO(38)
#define TEGRA_PIN_PE7				_GPIO(39)
#define TEGRA_PIN_GEN3_I2C_SCL_PF0		_GPIO(40)
#define TEGRA_PIN_GEN3_I2C_SDA_PF1		_GPIO(41)
#define TEGRA_PIN_UART2_TX_PG0			_GPIO(48)
#define TEGRA_PIN_UART2_RX_PG1			_GPIO(49)
#define TEGRA_PIN_UART2_RTS_PG2			_GPIO(50)
#define TEGRA_PIN_UART2_CTS_PG3			_GPIO(51)
#define TEGRA_PIN_WIFI_EN_PH0			_GPIO(56)
#define TEGRA_PIN_WIFI_RST_PH1			_GPIO(57)
#define TEGRA_PIN_WIFI_WAKE_AP_PH2		_GPIO(58)
#define TEGRA_PIN_AP_WAKE_BT_PH3		_GPIO(59)
#define TEGRA_PIN_BT_RST_PH4			_GPIO(60)
#define TEGRA_PIN_BT_WAKE_AP_PH5		_GPIO(61)
#define TEGRA_PIN_PH6				_GPIO(62)
#define TEGRA_PIN_AP_WAKE_NFC_PH7		_GPIO(63)
#define TEGRA_PIN_NFC_EN_PI0			_GPIO(64)
#define TEGRA_PIN_NFC_INT_PI1			_GPIO(65)
#define TEGRA_PIN_GPS_EN_PI2			_GPIO(66)
#define TEGRA_PIN_GPS_RST_PI3			_GPIO(67)
#define TEGRA_PIN_UART4_TX_PI4			_GPIO(68)
#define TEGRA_PIN_UART4_RX_PI5			_GPIO(69)
#define TEGRA_PIN_UART4_RTS_PI6			_GPIO(70)
#define TEGRA_PIN_UART4_CTS_PI7			_GPIO(71)
#define TEGRA_PIN_GEN1_I2C_SDA_PJ0		_GPIO(72)
#define TEGRA_PIN_GEN1_I2C_SCL_PJ1		_GPIO(73)
#define TEGRA_PIN_GEN2_I2C_SCL_PJ2		_GPIO(74)
#define TEGRA_PIN_GEN2_I2C_SDA_PJ3		_GPIO(75)
#define TEGRA_PIN_DAP4_FS_PJ4			_GPIO(76)
#define TEGRA_PIN_DAP4_DIN_PJ5			_GPIO(77)
#define TEGRA_PIN_DAP4_DOUT_PJ6			_GPIO(78)
#define TEGRA_PIN_DAP4_SCLK_PJ7			_GPIO(79)
#define TEGRA_PIN_PK0				_GPIO(80)
#define TEGRA_PIN_PK1				_GPIO(81)
#define TEGRA_PIN_PK2				_GPIO(82)
#define TEGRA_PIN_PK3				_GPIO(83)
#define TEGRA_PIN_PK4				_GPIO(84)
#define TEGRA_PIN_PK5				_GPIO(85)
#define TEGRA_PIN_PK6				_GPIO(86)
#define TEGRA_PIN_PK7				_GPIO(87)
#define TEGRA_PIN_PL0				_GPIO(88)
#define TEGRA_PIN_PL1				_GPIO(89)
#define TEGRA_PIN_SDMMC1_CLK_PM0		_GPIO(96)
#define TEGRA_PIN_SDMMC1_CMD_PM1		_GPIO(97)
#define TEGRA_PIN_SDMMC1_DAT3_PM2		_GPIO(98)
#define TEGRA_PIN_SDMMC1_DAT2_PM3		_GPIO(99)
#define TEGRA_PIN_SDMMC1_DAT1_PM4		_GPIO(100)
#define TEGRA_PIN_SDMMC1_DAT0_PM5		_GPIO(101)
#define TEGRA_PIN_SDMMC3_CLK_PP0		_GPIO(120)
#define TEGRA_PIN_SDMMC3_CMD_PP1		_GPIO(121)
#define TEGRA_PIN_SDMMC3_DAT3_PP2		_GPIO(122)
#define TEGRA_PIN_SDMMC3_DAT2_PP3		_GPIO(123)
#define TEGRA_PIN_SDMMC3_DAT1_PP4		_GPIO(124)
#define TEGRA_PIN_SDMMC3_DAT0_PP5		_GPIO(125)
#define TEGRA_PIN_CAM1_MCLK_PS0			_GPIO(144)
#define TEGRA_PIN_CAM2_MCLK_PS1			_GPIO(145)
#define TEGRA_PIN_CAM_I2C_SCL_PS2		_GPIO(146)
#define TEGRA_PIN_CAM_I2C_SDA_PS3		_GPIO(147)
#define TEGRA_PIN_CAM_RST_PS4			_GPIO(148)
#define TEGRA_PIN_CAM_AF_EN_PS5			_GPIO(149)
#define TEGRA_PIN_CAM_FLASH_EN_PS6		_GPIO(150)
#define TEGRA_PIN_CAM1_PWDN_PS7			_GPIO(151)
#define TEGRA_PIN_CAM2_PWDN_PT0			_GPIO(152)
#define TEGRA_PIN_CAM1_STROBE_PT1		_GPIO(153)
#define TEGRA_PIN_UART1_TX_PU0			_GPIO(160)
#define TEGRA_PIN_UART1_RX_PU1			_GPIO(161)
#define TEGRA_PIN_UART1_RTS_PU2			_GPIO(162)
#define TEGRA_PIN_UART1_CTS_PU3			_GPIO(163)
#define TEGRA_PIN_LCD_BL_PWM_PV0		_GPIO(168)
#define TEGRA_PIN_LCD_BL_EN_PV1			_GPIO(169)
#define TEGRA_PIN_LCD_RST_PV2			_GPIO(170)
#define TEGRA_PIN_LCD_GPIO1_PV3			_GPIO(171)
#define TEGRA_PIN_LCD_GPIO2_PV4			_GPIO(172)
#define TEGRA_PIN_AP_READY_PV5			_GPIO(173)
#define TEGRA_PIN_TOUCH_RST_PV6			_GPIO(174)
#define TEGRA_PIN_TOUCH_CLK_PV7			_GPIO(175)
#define TEGRA_PIN_MODEM_WAKE_AP_PX0		_GPIO(184)
#define TEGRA_PIN_TOUCH_INT_PX1			_GPIO(185)
#define TEGRA_PIN_MOTION_INT_PX2		_GPIO(186)
#define TEGRA_PIN_ALS_PROX_INT_PX3		_GPIO(187)
#define TEGRA_PIN_TEMP_ALERT_PX4		_GPIO(188)
#define TEGRA_PIN_BUTTON_POWER_ON_PX5		_GPIO(189)
#define TEGRA_PIN_BUTTON_VOL_UP_PX6		_GPIO(190)
#define TEGRA_PIN_BUTTON_VOL_DOWN_PX7		_GPIO(191)
#define TEGRA_PIN_BUTTON_SLIDE_SW_PY0		_GPIO(192)
#define TEGRA_PIN_BUTTON_HOME_PY1		_GPIO(193)
#define TEGRA_PIN_LCD_TE_PY2			_GPIO(194)
#define TEGRA_PIN_PWR_I2C_SCL_PY3		_GPIO(195)
#define TEGRA_PIN_PWR_I2C_SDA_PY4		_GPIO(196)
#define TEGRA_PIN_CLK_32K_OUT_PY5		_GPIO(197)
#define TEGRA_PIN_PZ0				_GPIO(200)
#define TEGRA_PIN_PZ1				_GPIO(201)
#define TEGRA_PIN_PZ2				_GPIO(202)
#define TEGRA_PIN_PZ3				_GPIO(203)
#define TEGRA_PIN_PZ4				_GPIO(204)
#define TEGRA_PIN_PZ5				_GPIO(205)
#define TEGRA_PIN_DAP2_FS_PAA0			_GPIO(208)
#define TEGRA_PIN_DAP2_SCLK_PAA1		_GPIO(209)
#define TEGRA_PIN_DAP2_DIN_PAA2			_GPIO(210)
#define TEGRA_PIN_DAP2_DOUT_PAA3		_GPIO(211)
#define TEGRA_PIN_AUD_MCLK_PBB0			_GPIO(216)
#define TEGRA_PIN_DVFS_PWM_PBB1			_GPIO(217)
#define TEGRA_PIN_DVFS_CLK_PBB2			_GPIO(218)
#define TEGRA_PIN_GPIO_X1_AUD_PBB3		_GPIO(219)
#define TEGRA_PIN_GPIO_X3_AUD_PBB4		_GPIO(220)
#define TEGRA_PIN_HDMI_CEC_PCC0			_GPIO(224)
#define TEGRA_PIN_HDMI_INT_DP_HPD_PCC1		_GPIO(225)
#define TEGRA_PIN_SPDIF_OUT_PCC2		_GPIO(226)
#define TEGRA_PIN_SPDIF_IN_PCC3			_GPIO(227)
#define TEGRA_PIN_USB_VBUS_EN0_PCC4		_GPIO(228)
#define TEGRA_PIN_USB_VBUS_EN1_PCC5		_GPIO(229)
#define TEGRA_PIN_DP_HPD0_PCC6			_GPIO(230)
#define TEGRA_PIN_PCC7				_GPIO(231)
#define TEGRA_PIN_SPI2_CS1_PDD0			_GPIO(232)
#define TEGRA_PIN_QSPI_SCK_PEE0			_GPIO(240)
#define TEGRA_PIN_QSPI_CS_N_PEE1		_GPIO(241)
#define TEGRA_PIN_QSPI_IO0_PEE2			_GPIO(242)
#define TEGRA_PIN_QSPI_IO1_PEE3			_GPIO(243)
#define TEGRA_PIN_QSPI_IO2_PEE4			_GPIO(244)
#define TEGRA_PIN_QSPI_IO3_PEE5			_GPIO(245)

/* All non-GPIO pins follow */
#define NUM_GPIOS	(TEGRA_PIN_QSPI_IO3_PEE5 + 1)
#define _PIN(offset)	(NUM_GPIOS + (offset))

/* Non-GPIO pins */
#define TEGRA_PIN_CORE_PWR_REQ			_PIN(0)
#define TEGRA_PIN_CPU_PWR_REQ			_PIN(1)
#define TEGRA_PIN_PWR_INT_N			_PIN(2)
#define TEGRA_PIN_CLK_32K_IN			_PIN(3)
#define TEGRA_PIN_JTAG_RTCK			_PIN(4)
#define TEGRA_PIN_BATT_BCL			_PIN(5)
#define TEGRA_PIN_CLK_REQ			_PIN(6)
#define TEGRA_PIN_SHUTDOWN			_PIN(7)

#define TEGRA_DRV_PAD_QSPI_COMP			_PIN(8)
#define TEGRA_DRV_PAD_QSPI_COMP_CONTROL		_PIN(9)
#define TEGRA_DRV_PAD_QSPI_LPBK_CONTROL		_PIN(10)

#define SDMMC1_CLK_LPBK_CTRL_OFFSET	0
#define SDMMC3_CLK_LPBK_CTRL_OFFSET	4

#define EMMC2_PAD_CFGPADCTRL_OFFSET	0x1C8
#define EMMC4_PAD_CFGPADCTRL_OFFSET	0x1E0
#define DRV_BANK	0
#define MUX_BANK	1

#define EMMC_DPD_PARKING(x)	(x << EMMC_PARKING_BIT)
#define EMMC_PARKING_BIT	0xE
#define PARKING_SET	0x1FFF
#define PARKING_CLEAR	0x0

static const struct pinctrl_pin_desc  tegra210_pins[] = {
	PINCTRL_PIN(TEGRA_PIN_PEX_L0_RST_N_PA0, "PEX_L0_RST_N_PA0"),
	PINCTRL_PIN(TEGRA_PIN_PEX_L0_CLKREQ_N_PA1, "PEX_L0_CLKREQ_N_PA1"),
	PINCTRL_PIN(TEGRA_PIN_PEX_WAKE_N_PA2, "PEX_WAKE_N_PA2"),
	PINCTRL_PIN(TEGRA_PIN_PEX_L1_RST_N_PA3, "PEX_L1_RST_N_PA3"),
	PINCTRL_PIN(TEGRA_PIN_PEX_L1_CLKREQ_N_PA4, "PEX_L1_CLKREQ_N_PA4"),
	PINCTRL_PIN(TEGRA_PIN_SATA_LED_ACTIVE_PA5, "SATA_LED_ACTIVE_PA5"),
	PINCTRL_PIN(TEGRA_PIN_PA6, "PA6"),
	PINCTRL_PIN(TEGRA_PIN_DAP1_FS_PB0, "DAP1_FS_PB0"),
	PINCTRL_PIN(TEGRA_PIN_DAP1_DIN_PB1, "DAP1_DIN_PB1"),
	PINCTRL_PIN(TEGRA_PIN_DAP1_DOUT_PB2, "DAP1_DOUT_PB2"),
	PINCTRL_PIN(TEGRA_PIN_DAP1_SCLK_PB3, "DAP1_SCLK_PB3"),
	PINCTRL_PIN(TEGRA_PIN_SPI2_MOSI_PB4, "SPI2_MOSI_PB4"),
	PINCTRL_PIN(TEGRA_PIN_SPI2_MISO_PB5, "SPI2_MISO_PB5"),
	PINCTRL_PIN(TEGRA_PIN_SPI2_SCK_PB6, "SPI2_SCK_PB6"),
	PINCTRL_PIN(TEGRA_PIN_SPI2_CS0_PB7, "SPI2_CS0_PB7"),
	PINCTRL_PIN(TEGRA_PIN_SPI1_MOSI_PC0, "SPI1_MOSI_PC0"),
	PINCTRL_PIN(TEGRA_PIN_SPI1_MISO_PC1, "SPI1_MISO_PC1"),
	PINCTRL_PIN(TEGRA_PIN_SPI1_SCK_PC2, "SPI1_SCK_PC2"),
	PINCTRL_PIN(TEGRA_PIN_SPI1_CS0_PC3, "SPI1_CS0_PC3"),
	PINCTRL_PIN(TEGRA_PIN_SPI1_CS1_PC4, "SPI1_CS1_PC4"),
	PINCTRL_PIN(TEGRA_PIN_SPI4_SCK_PC5, "SPI4_SCK_PC5"),
	PINCTRL_PIN(TEGRA_PIN_SPI4_CS0_PC6, "SPI4_CS0_PC6"),
	PINCTRL_PIN(TEGRA_PIN_SPI4_MOSI_PC7, "SPI4_MOSI_PC7"),
	PINCTRL_PIN(TEGRA_PIN_SPI4_MISO_PD0, "SPI4_MISO_PD0"),
	PINCTRL_PIN(TEGRA_PIN_UART3_TX_PD1, "UART3_TX_PD1"),
	PINCTRL_PIN(TEGRA_PIN_UART3_RX_PD2, "UART3_RX_PD2"),
	PINCTRL_PIN(TEGRA_PIN_UART3_RTS_PD3, "UART3_RTS_PD3"),
	PINCTRL_PIN(TEGRA_PIN_UART3_CTS_PD4, "UART3_CTS_PD4"),
	PINCTRL_PIN(TEGRA_PIN_DMIC1_CLK_PE0, "DMIC1_CLK_PE0"),
	PINCTRL_PIN(TEGRA_PIN_DMIC1_DAT_PE1, "DMIC1_DAT_PE1"),
	PINCTRL_PIN(TEGRA_PIN_DMIC2_CLK_PE2, "DMIC2_CLK_PE2"),
	PINCTRL_PIN(TEGRA_PIN_DMIC2_DAT_PE3, "DMIC2_DAT_PE3"),
	PINCTRL_PIN(TEGRA_PIN_DMIC3_CLK_PE4, "DMIC3_CLK_PE4"),
	PINCTRL_PIN(TEGRA_PIN_DMIC3_DAT_PE5, "DMIC3_DAT_PE5"),
	PINCTRL_PIN(TEGRA_PIN_PE6, "PE6"),
	PINCTRL_PIN(TEGRA_PIN_PE7, "PE7"),
	PINCTRL_PIN(TEGRA_PIN_GEN3_I2C_SCL_PF0, "GEN3_I2C_SCL_PF0"),
	PINCTRL_PIN(TEGRA_PIN_GEN3_I2C_SDA_PF1, "GEN3_I2C_SDA_PF1"),
	PINCTRL_PIN(TEGRA_PIN_UART2_TX_PG0, "UART2_TX_PG0"),
	PINCTRL_PIN(TEGRA_PIN_UART2_RX_PG1, "UART2_RX_PG1"),
	PINCTRL_PIN(TEGRA_PIN_UART2_RTS_PG2, "UART2_RTS_PG2"),
	PINCTRL_PIN(TEGRA_PIN_UART2_CTS_PG3, "UART2_CTS_PG3"),
	PINCTRL_PIN(TEGRA_PIN_WIFI_EN_PH0, "WIFI_EN_PH0"),
	PINCTRL_PIN(TEGRA_PIN_WIFI_RST_PH1, "WIFI_RST_PH1"),
	PINCTRL_PIN(TEGRA_PIN_WIFI_WAKE_AP_PH2, "WIFI_WAKE_AP_PH2"),
	PINCTRL_PIN(TEGRA_PIN_AP_WAKE_BT_PH3, "AP_WAKE_BT_PH3"),
	PINCTRL_PIN(TEGRA_PIN_BT_RST_PH4, "BT_RST_PH4"),
	PINCTRL_PIN(TEGRA_PIN_BT_WAKE_AP_PH5, "BT_WAKE_AP_PH5"),
	PINCTRL_PIN(TEGRA_PIN_PH6, "PH6"),
	PINCTRL_PIN(TEGRA_PIN_AP_WAKE_NFC_PH7, "AP_WAKE_NFC_PH7"),
	PINCTRL_PIN(TEGRA_PIN_NFC_EN_PI0, "NFC_EN_PI0"),
	PINCTRL_PIN(TEGRA_PIN_NFC_INT_PI1, "NFC_INT_PI1"),
	PINCTRL_PIN(TEGRA_PIN_GPS_EN_PI2, "GPS_EN_PI2"),
	PINCTRL_PIN(TEGRA_PIN_GPS_RST_PI3, "GPS_RST_PI3"),
	PINCTRL_PIN(TEGRA_PIN_UART4_TX_PI4, "UART4_TX_PI4"),
	PINCTRL_PIN(TEGRA_PIN_UART4_RX_PI5, "UART4_RX_PI5"),
	PINCTRL_PIN(TEGRA_PIN_UART4_RTS_PI6, "UART4_RTS_PI6"),
	PINCTRL_PIN(TEGRA_PIN_UART4_CTS_PI7, "UART4_CTS_PI7"),
	PINCTRL_PIN(TEGRA_PIN_GEN1_I2C_SDA_PJ0, "GEN1_I2C_SDA_PJ0"),
	PINCTRL_PIN(TEGRA_PIN_GEN1_I2C_SCL_PJ1, "GEN1_I2C_SCL_PJ1"),
	PINCTRL_PIN(TEGRA_PIN_GEN2_I2C_SCL_PJ2, "GEN2_I2C_SCL_PJ2"),
	PINCTRL_PIN(TEGRA_PIN_GEN2_I2C_SDA_PJ3, "GEN2_I2C_SDA_PJ3"),
	PINCTRL_PIN(TEGRA_PIN_DAP4_FS_PJ4, "DAP4_FS_PJ4"),
	PINCTRL_PIN(TEGRA_PIN_DAP4_DIN_PJ5, "DAP4_DIN_PJ5"),
	PINCTRL_PIN(TEGRA_PIN_DAP4_DOUT_PJ6, "DAP4_DOUT_PJ6"),
	PINCTRL_PIN(TEGRA_PIN_DAP4_SCLK_PJ7, "DAP4_SCLK_PJ7"),
	PINCTRL_PIN(TEGRA_PIN_PK0, "PK0"),
	PINCTRL_PIN(TEGRA_PIN_PK1, "PK1"),
	PINCTRL_PIN(TEGRA_PIN_PK2, "PK2"),
	PINCTRL_PIN(TEGRA_PIN_PK3, "PK3"),
	PINCTRL_PIN(TEGRA_PIN_PK4, "PK4"),
	PINCTRL_PIN(TEGRA_PIN_PK5, "PK5"),
	PINCTRL_PIN(TEGRA_PIN_PK6, "PK6"),
	PINCTRL_PIN(TEGRA_PIN_PK7, "PK7"),
	PINCTRL_PIN(TEGRA_PIN_PL0, "PL0"),
	PINCTRL_PIN(TEGRA_PIN_PL1, "PL1"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC1_CLK_PM0, "SDMMC1_CLK_PM0"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC1_CMD_PM1, "SDMMC1_CMD_PM1"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC1_DAT3_PM2, "SDMMC1_DAT3_PM2"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC1_DAT2_PM3, "SDMMC1_DAT2_PM3"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC1_DAT1_PM4, "SDMMC1_DAT1_PM4"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC1_DAT0_PM5, "SDMMC1_DAT0_PM5"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC3_CLK_PP0, "SDMMC3_CLK_PP0"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC3_CMD_PP1, "SDMMC3_CMD_PP1"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC3_DAT3_PP2, "SDMMC3_DAT3_PP2"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC3_DAT2_PP3, "SDMMC3_DAT2_PP3"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC3_DAT1_PP4, "SDMMC3_DAT1_PP4"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC3_DAT0_PP5, "SDMMC3_DAT0_PP5"),
	PINCTRL_PIN(TEGRA_PIN_CAM1_MCLK_PS0, "CAM1_MCLK_PS0"),
	PINCTRL_PIN(TEGRA_PIN_CAM2_MCLK_PS1, "CAM2_MCLK_PS1"),
	PINCTRL_PIN(TEGRA_PIN_CAM_I2C_SCL_PS2, "CAM_I2C_SCL_PS2"),
	PINCTRL_PIN(TEGRA_PIN_CAM_I2C_SDA_PS3, "CAM_I2C_SDA_PS3"),
	PINCTRL_PIN(TEGRA_PIN_CAM_RST_PS4, "CAM_RST_PS4"),
	PINCTRL_PIN(TEGRA_PIN_CAM_AF_EN_PS5, "CAM_AF_EN_PS5"),
	PINCTRL_PIN(TEGRA_PIN_CAM_FLASH_EN_PS6, "CAM_FLASH_EN_PS6"),
	PINCTRL_PIN(TEGRA_PIN_CAM1_PWDN_PS7, "CAM1_PWDN_PS7"),
	PINCTRL_PIN(TEGRA_PIN_CAM2_PWDN_PT0, "CAM2_PWDN_PT0"),
	PINCTRL_PIN(TEGRA_PIN_CAM1_STROBE_PT1, "CAM1_STROBE_PT11"),
	PINCTRL_PIN(TEGRA_PIN_UART1_TX_PU0, "UART1_TX_PU0"),
	PINCTRL_PIN(TEGRA_PIN_UART1_RX_PU1, "UART1_RX_PU1"),
	PINCTRL_PIN(TEGRA_PIN_UART1_RTS_PU2, "UART1_RTS_PU2"),
	PINCTRL_PIN(TEGRA_PIN_UART1_CTS_PU3, "UART1_CTS_PU3"),
	PINCTRL_PIN(TEGRA_PIN_LCD_BL_PWM_PV0, "LCD_BL_PWM_PV0"),
	PINCTRL_PIN(TEGRA_PIN_LCD_BL_EN_PV1, "LCD_BL_EN_PV1"),
	PINCTRL_PIN(TEGRA_PIN_LCD_RST_PV2, "LCD_RST_PV2"),
	PINCTRL_PIN(TEGRA_PIN_LCD_GPIO1_PV3, "LCD_GPIO1_PV3"),
	PINCTRL_PIN(TEGRA_PIN_LCD_GPIO2_PV4, "LCD_GPIO2_PV4"),
	PINCTRL_PIN(TEGRA_PIN_AP_READY_PV5, "AP_READY_PV5"),
	PINCTRL_PIN(TEGRA_PIN_TOUCH_RST_PV6, "TOUCH_RST_PV6"),
	PINCTRL_PIN(TEGRA_PIN_TOUCH_CLK_PV7, "TOUCH_CLK_PV7"),
	PINCTRL_PIN(TEGRA_PIN_MODEM_WAKE_AP_PX0, "MODEM_WAKE_AP_PX0"),
	PINCTRL_PIN(TEGRA_PIN_TOUCH_INT_PX1, "TOUCH_INT_PX1"),
	PINCTRL_PIN(TEGRA_PIN_MOTION_INT_PX2, "MOTION_INT_PX2"),
	PINCTRL_PIN(TEGRA_PIN_ALS_PROX_INT_PX3, "ALS_PROX_INT_PX3"),
	PINCTRL_PIN(TEGRA_PIN_TEMP_ALERT_PX4, "TEMP_ALERT_PX4"),
	PINCTRL_PIN(TEGRA_PIN_BUTTON_POWER_ON_PX5, "BUTTON_POWER_ON_PX5"),
	PINCTRL_PIN(TEGRA_PIN_BUTTON_VOL_UP_PX6, "BUTTON_VOL_UP_PX6"),
	PINCTRL_PIN(TEGRA_PIN_BUTTON_VOL_DOWN_PX7, "BUTTON_VOL_DOWN_PX7"),
	PINCTRL_PIN(TEGRA_PIN_BUTTON_SLIDE_SW_PY0, "BUTTON_SLIDE_SW_PY0"),
	PINCTRL_PIN(TEGRA_PIN_BUTTON_HOME_PY1, "BUTTON_HOME_PY1"),
	PINCTRL_PIN(TEGRA_PIN_LCD_TE_PY2, "LCD_TE_PY2"),
	PINCTRL_PIN(TEGRA_PIN_PWR_I2C_SCL_PY3, "PWR_I2C_SCL_PY3"),
	PINCTRL_PIN(TEGRA_PIN_PWR_I2C_SDA_PY4, "PWR_I2C_SDA_PY4"),
	PINCTRL_PIN(TEGRA_PIN_CLK_32K_OUT_PY5, "CLK_32K_OUT_PY5"),
	PINCTRL_PIN(TEGRA_PIN_PZ0, "PZ0"),
	PINCTRL_PIN(TEGRA_PIN_PZ1, "PZ1"),
	PINCTRL_PIN(TEGRA_PIN_PZ2, "PZ2"),
	PINCTRL_PIN(TEGRA_PIN_PZ3, "PZ3"),
	PINCTRL_PIN(TEGRA_PIN_PZ4, "PZ4"),
	PINCTRL_PIN(TEGRA_PIN_PZ5, "PZ5"),
	PINCTRL_PIN(TEGRA_PIN_DAP2_FS_PAA0, "DAP2_FS_PAA0"),
	PINCTRL_PIN(TEGRA_PIN_DAP2_SCLK_PAA1, "DAP2_SCLK_PAA1"),
	PINCTRL_PIN(TEGRA_PIN_DAP2_DIN_PAA2, "DAP2_DIN_PAA2"),
	PINCTRL_PIN(TEGRA_PIN_DAP2_DOUT_PAA3, "DAP2_DOUT_PAA3"),
	PINCTRL_PIN(TEGRA_PIN_AUD_MCLK_PBB0, "AUD_MCLK_PBB0"),
	PINCTRL_PIN(TEGRA_PIN_DVFS_PWM_PBB1, "DVFS_PWM_PBB1"),
	PINCTRL_PIN(TEGRA_PIN_DVFS_CLK_PBB2, "DVFS_CLK_PBB2"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_X1_AUD_PBB3, "GPIO_X1_AUD_PBB3"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_X3_AUD_PBB4, "GPIO_X3_AUD_PBB4"),
	PINCTRL_PIN(TEGRA_PIN_HDMI_CEC_PCC0, "HDMI_CEC_PCC0"),
	PINCTRL_PIN(TEGRA_PIN_HDMI_INT_DP_HPD_PCC1, "HDMI_INT_DP_HPD_PCC1"),
	PINCTRL_PIN(TEGRA_PIN_SPDIF_OUT_PCC2, "SPDIF_OUT_PCC2"),
	PINCTRL_PIN(TEGRA_PIN_SPDIF_IN_PCC3, "SPDIF_IN_PCC3"),
	PINCTRL_PIN(TEGRA_PIN_USB_VBUS_EN0_PCC4, "USB_VBUS_EN0_PCC4"),
	PINCTRL_PIN(TEGRA_PIN_USB_VBUS_EN1_PCC5, "USB_VBUS_EN1_PCC5"),
	PINCTRL_PIN(TEGRA_PIN_DP_HPD0_PCC6, "DP_HPD0_PCC6"),
	PINCTRL_PIN(TEGRA_PIN_PCC7, "PCC7"),
	PINCTRL_PIN(TEGRA_PIN_SPI2_CS1_PDD0, "SPI2_CS1_PDD0"),
	PINCTRL_PIN(TEGRA_PIN_QSPI_SCK_PEE0, "QSPI_SCK_PEE0"),
	PINCTRL_PIN(TEGRA_PIN_QSPI_CS_N_PEE1, "QSPI_CS_N_PEE1"),
	PINCTRL_PIN(TEGRA_PIN_QSPI_IO0_PEE2, "QSPI_IO0_PEE2"),
	PINCTRL_PIN(TEGRA_PIN_QSPI_IO1_PEE3, "QSPI_IO1_PEE3"),
	PINCTRL_PIN(TEGRA_PIN_QSPI_IO2_PEE4, "QSPI_IO2_PEE4"),
	PINCTRL_PIN(TEGRA_PIN_QSPI_IO3_PEE5, "QSPI_IO3_PEE5"),
	PINCTRL_PIN(TEGRA_PIN_CORE_PWR_REQ, "CORE_PWR_REQ"),
	PINCTRL_PIN(TEGRA_PIN_CPU_PWR_REQ, "CPU_PWR_REQ"),
	PINCTRL_PIN(TEGRA_PIN_PWR_INT_N, "PWR_INT_N"),
	PINCTRL_PIN(TEGRA_PIN_CLK_32K_IN, "CLK_32K_IN"),
	PINCTRL_PIN(TEGRA_PIN_JTAG_RTCK, "JTAG_RTCK"),
	PINCTRL_PIN(TEGRA_PIN_BATT_BCL, "BATT_BCL"),
	PINCTRL_PIN(TEGRA_PIN_CLK_REQ, "CLK_REQ"),
	PINCTRL_PIN(TEGRA_PIN_SHUTDOWN, "SHUTDOWN"),
};

static const unsigned pex_l0_rst_n_pa0_pins[] = {
	TEGRA_PIN_PEX_L0_RST_N_PA0,
};

static const unsigned pex_l0_clkreq_n_pa1_pins[] = {
	TEGRA_PIN_PEX_L0_CLKREQ_N_PA1,
};

static const unsigned pex_wake_n_pa2_pins[] = {
	TEGRA_PIN_PEX_WAKE_N_PA2,
};

static const unsigned pex_l1_rst_n_pa3_pins[] = {
	TEGRA_PIN_PEX_L1_RST_N_PA3,
};

static const unsigned pex_l1_clkreq_n_pa4_pins[] = {
	TEGRA_PIN_PEX_L1_CLKREQ_N_PA4,
};

static const unsigned sata_led_active_pa5_pins[] = {
	TEGRA_PIN_SATA_LED_ACTIVE_PA5,
};

static const unsigned pa6_pins[] = {
	TEGRA_PIN_PA6,
};

static const unsigned dap1_fs_pb0_pins[] = {
	TEGRA_PIN_DAP1_FS_PB0,
};

static const unsigned dap1_din_pb1_pins[] = {
	TEGRA_PIN_DAP1_DIN_PB1,
};

static const unsigned dap1_dout_pb2_pins[] = {
	TEGRA_PIN_DAP1_DOUT_PB2,
};

static const unsigned dap1_sclk_pb3_pins[] = {
	TEGRA_PIN_DAP1_SCLK_PB3,
};

static const unsigned spi2_mosi_pb4_pins[] = {
	TEGRA_PIN_SPI2_MOSI_PB4,
};

static const unsigned spi2_miso_pb5_pins[] = {
	TEGRA_PIN_SPI2_MISO_PB5,
};

static const unsigned spi2_sck_pb6_pins[] = {
	TEGRA_PIN_SPI2_SCK_PB6,
};

static const unsigned spi2_cs0_pb7_pins[] = {
	TEGRA_PIN_SPI2_CS0_PB7,
};

static const unsigned spi1_mosi_pc0_pins[] = {
	TEGRA_PIN_SPI1_MOSI_PC0,
};

static const unsigned spi1_miso_pc1_pins[] = {
	TEGRA_PIN_SPI1_MISO_PC1,
};

static const unsigned spi1_sck_pc2_pins[] = {
	TEGRA_PIN_SPI1_SCK_PC2,
};

static const unsigned spi1_cs0_pc3_pins[] = {
	TEGRA_PIN_SPI1_CS0_PC3,
};

static const unsigned spi1_cs1_pc4_pins[] = {
	TEGRA_PIN_SPI1_CS1_PC4,
};

static const unsigned spi4_sck_pc5_pins[] = {
	TEGRA_PIN_SPI4_SCK_PC5,
};

static const unsigned spi4_cs0_pc6_pins[] = {
	TEGRA_PIN_SPI4_CS0_PC6,
};

static const unsigned spi4_mosi_pc7_pins[] = {
	TEGRA_PIN_SPI4_MOSI_PC7,
};

static const unsigned spi4_miso_pd0_pins[] = {
	TEGRA_PIN_SPI4_MISO_PD0,
};

static const unsigned uart3_tx_pd1_pins[] = {
	TEGRA_PIN_UART3_TX_PD1,
};

static const unsigned uart3_rx_pd2_pins[] = {
	TEGRA_PIN_UART3_RX_PD2,
};

static const unsigned uart3_rts_pd3_pins[] = {
	TEGRA_PIN_UART3_RTS_PD3,
};

static const unsigned uart3_cts_pd4_pins[] = {
	TEGRA_PIN_UART3_CTS_PD4,
};

static const unsigned dmic1_clk_pe0_pins[] = {
	TEGRA_PIN_DMIC1_CLK_PE0,
};

static const unsigned dmic1_dat_pe1_pins[] = {
	TEGRA_PIN_DMIC1_DAT_PE1,
};

static const unsigned dmic2_clk_pe2_pins[] = {
	TEGRA_PIN_DMIC2_CLK_PE2,
};

static const unsigned dmic2_dat_pe3_pins[] = {
	TEGRA_PIN_DMIC2_DAT_PE3,
};

static const unsigned dmic3_clk_pe4_pins[] = {
	TEGRA_PIN_DMIC3_CLK_PE4,
};

static const unsigned dmic3_dat_pe5_pins[] = {
	TEGRA_PIN_DMIC3_DAT_PE5,
};

static const unsigned pe6_pins[] = {
	TEGRA_PIN_PE6,
};

static const unsigned pe7_pins[] = {
	TEGRA_PIN_PE7,
};

static const unsigned gen3_i2c_scl_pf0_pins[] = {
	TEGRA_PIN_GEN3_I2C_SCL_PF0,
};

static const unsigned gen3_i2c_sda_pf1_pins[] = {
	TEGRA_PIN_GEN3_I2C_SDA_PF1,
};

static const unsigned uart2_tx_pg0_pins[] = {
	TEGRA_PIN_UART2_TX_PG0,
};

static const unsigned uart2_rx_pg1_pins[] = {
	TEGRA_PIN_UART2_RX_PG1,
};

static const unsigned uart2_rts_pg2_pins[] = {
	TEGRA_PIN_UART2_RTS_PG2,
};

static const unsigned uart2_cts_pg3_pins[] = {
	TEGRA_PIN_UART2_CTS_PG3,
};

static const unsigned wifi_en_ph0_pins[] = {
	TEGRA_PIN_WIFI_EN_PH0,
};

static const unsigned wifi_rst_ph1_pins[] = {
	TEGRA_PIN_WIFI_RST_PH1,
};

static const unsigned wifi_wake_ap_ph2_pins[] = {
	TEGRA_PIN_WIFI_WAKE_AP_PH2,
};

static const unsigned ap_wake_bt_ph3_pins[] = {
	TEGRA_PIN_AP_WAKE_BT_PH3,
};

static const unsigned bt_rst_ph4_pins[] = {
	TEGRA_PIN_BT_RST_PH4,
};

static const unsigned bt_wake_ap_ph5_pins[] = {
	TEGRA_PIN_BT_WAKE_AP_PH5,
};

static const unsigned ph6_pins[] = {
	TEGRA_PIN_PH6,
};

static const unsigned ap_wake_nfc_ph7_pins[] = {
	TEGRA_PIN_AP_WAKE_NFC_PH7,
};

static const unsigned nfc_en_pi0_pins[] = {
	TEGRA_PIN_NFC_EN_PI0,
};

static const unsigned nfc_int_pi1_pins[] = {
	TEGRA_PIN_NFC_INT_PI1,
};

static const unsigned gps_en_pi2_pins[] = {
	TEGRA_PIN_GPS_EN_PI2,
};

static const unsigned gps_rst_pi3_pins[] = {
	TEGRA_PIN_GPS_RST_PI3,
};

static const unsigned uart4_tx_pi4_pins[] = {
	TEGRA_PIN_UART4_TX_PI4,
};

static const unsigned uart4_rx_pi5_pins[] = {
	TEGRA_PIN_UART4_RX_PI5,
};

static const unsigned uart4_rts_pi6_pins[] = {
	TEGRA_PIN_UART4_RTS_PI6,
};

static const unsigned uart4_cts_pi7_pins[] = {
	TEGRA_PIN_UART4_CTS_PI7,
};

static const unsigned gen1_i2c_sda_pj0_pins[] = {
	TEGRA_PIN_GEN1_I2C_SDA_PJ0,
};

static const unsigned gen1_i2c_scl_pj1_pins[] = {
	TEGRA_PIN_GEN1_I2C_SCL_PJ1,
};

static const unsigned gen2_i2c_scl_pj2_pins[] = {
	TEGRA_PIN_GEN2_I2C_SCL_PJ2,
};

static const unsigned gen2_i2c_sda_pj3_pins[] = {
	TEGRA_PIN_GEN2_I2C_SDA_PJ3,
};

static const unsigned dap4_fs_pj4_pins[] = {
	TEGRA_PIN_DAP4_FS_PJ4,
};

static const unsigned dap4_din_pj5_pins[] = {
	TEGRA_PIN_DAP4_DIN_PJ5,
};

static const unsigned dap4_dout_pj6_pins[] = {
	TEGRA_PIN_DAP4_DOUT_PJ6,
};

static const unsigned dap4_sclk_pj7_pins[] = {
	TEGRA_PIN_DAP4_SCLK_PJ7,
};

static const unsigned pk0_pins[] = {
	TEGRA_PIN_PK0,
};

static const unsigned pk1_pins[] = {
	TEGRA_PIN_PK1,
};

static const unsigned pk2_pins[] = {
	TEGRA_PIN_PK2,
};

static const unsigned pk3_pins[] = {
	TEGRA_PIN_PK3,
};

static const unsigned pk4_pins[] = {
	TEGRA_PIN_PK4,
};

static const unsigned pk5_pins[] = {
	TEGRA_PIN_PK5,
};

static const unsigned pk6_pins[] = {
	TEGRA_PIN_PK6,
};

static const unsigned pk7_pins[] = {
	TEGRA_PIN_PK7,
};

static const unsigned pl0_pins[] = {
	TEGRA_PIN_PL0,
};

static const unsigned pl1_pins[] = {
	TEGRA_PIN_PL1,
};

static const unsigned sdmmc1_clk_pm0_pins[] = {
	TEGRA_PIN_SDMMC1_CLK_PM0,
};

static const unsigned sdmmc1_cmd_pm1_pins[] = {
	TEGRA_PIN_SDMMC1_CMD_PM1,
};

static const unsigned sdmmc1_dat3_pm2_pins[] = {
	TEGRA_PIN_SDMMC1_DAT3_PM2,
};

static const unsigned sdmmc1_dat2_pm3_pins[] = {
	TEGRA_PIN_SDMMC1_DAT2_PM3,
};

static const unsigned sdmmc1_dat1_pm4_pins[] = {
	TEGRA_PIN_SDMMC1_DAT1_PM4,
};

static const unsigned sdmmc1_dat0_pm5_pins[] = {
	TEGRA_PIN_SDMMC1_DAT0_PM5,
};

static const unsigned sdmmc3_clk_pp0_pins[] = {
	TEGRA_PIN_SDMMC3_CLK_PP0,
};

static const unsigned sdmmc3_cmd_pp1_pins[] = {
	TEGRA_PIN_SDMMC3_CMD_PP1,
};

static const unsigned sdmmc3_dat3_pp2_pins[] = {
	TEGRA_PIN_SDMMC3_DAT3_PP2,
};

static const unsigned sdmmc3_dat2_pp3_pins[] = {
	TEGRA_PIN_SDMMC3_DAT2_PP3,
};

static const unsigned sdmmc3_dat1_pp4_pins[] = {
	TEGRA_PIN_SDMMC3_DAT1_PP4,
};

static const unsigned sdmmc3_dat0_pp5_pins[] = {
	TEGRA_PIN_SDMMC3_DAT0_PP5,
};

static const unsigned cam1_mclk_ps0_pins[] = {
	TEGRA_PIN_CAM1_MCLK_PS0,
};

static const unsigned cam2_mclk_ps1_pins[] = {
	TEGRA_PIN_CAM2_MCLK_PS1,
};

static const unsigned cam_i2c_scl_ps2_pins[] = {
	TEGRA_PIN_CAM_I2C_SCL_PS2,
};

static const unsigned cam_i2c_sda_ps3_pins[] = {
	TEGRA_PIN_CAM_I2C_SDA_PS3,
};

static const unsigned cam_rst_ps4_pins[] = {
	TEGRA_PIN_CAM_RST_PS4,
};

static const unsigned cam_af_en_ps5_pins[] = {
	TEGRA_PIN_CAM_AF_EN_PS5,
};

static const unsigned cam_flash_en_ps6_pins[] = {
	TEGRA_PIN_CAM_FLASH_EN_PS6,
};

static const unsigned cam1_pwdn_ps7_pins[] = {
	TEGRA_PIN_CAM1_PWDN_PS7,
};

static const unsigned cam2_pwdn_pt0_pins[] = {
	TEGRA_PIN_CAM2_PWDN_PT0,
};

static const unsigned cam1_strobe_pt1_pins[] = {
	TEGRA_PIN_CAM1_STROBE_PT1,
};

static const unsigned uart1_tx_pu0_pins[] = {
	TEGRA_PIN_UART1_TX_PU0,
};

static const unsigned uart1_rx_pu1_pins[] = {
	TEGRA_PIN_UART1_RX_PU1,
};

static const unsigned uart1_rts_pu2_pins[] = {
	TEGRA_PIN_UART1_RTS_PU2,
};

static const unsigned uart1_cts_pu3_pins[] = {
	TEGRA_PIN_UART1_CTS_PU3,
};

static const unsigned lcd_bl_pwm_pv0_pins[] = {
	TEGRA_PIN_LCD_BL_PWM_PV0,
};

static const unsigned lcd_bl_en_pv1_pins[] = {
	TEGRA_PIN_LCD_BL_EN_PV1,
};

static const unsigned lcd_rst_pv2_pins[] = {
	TEGRA_PIN_LCD_RST_PV2,
};

static const unsigned lcd_gpio1_pv3_pins[] = {
	TEGRA_PIN_LCD_GPIO1_PV3,
};

static const unsigned lcd_gpio2_pv4_pins[] = {
	TEGRA_PIN_LCD_GPIO2_PV4,
};

static const unsigned ap_ready_pv5_pins[] = {
	TEGRA_PIN_AP_READY_PV5,
};

static const unsigned touch_rst_pv6_pins[] = {
	TEGRA_PIN_TOUCH_RST_PV6,
};

static const unsigned touch_clk_pv7_pins[] = {
	TEGRA_PIN_TOUCH_CLK_PV7,
};

static const unsigned modem_wake_ap_px0_pins[] = {
	TEGRA_PIN_MODEM_WAKE_AP_PX0,
};

static const unsigned touch_int_px1_pins[] = {
	TEGRA_PIN_TOUCH_INT_PX1,
};

static const unsigned motion_int_px2_pins[] = {
	TEGRA_PIN_MOTION_INT_PX2,
};

static const unsigned als_prox_int_px3_pins[] = {
	TEGRA_PIN_ALS_PROX_INT_PX3,
};

static const unsigned temp_alert_px4_pins[] = {
	TEGRA_PIN_TEMP_ALERT_PX4,
};

static const unsigned button_power_on_px5_pins[] = {
	TEGRA_PIN_BUTTON_POWER_ON_PX5,
};

static const unsigned button_vol_up_px6_pins[] = {
	TEGRA_PIN_BUTTON_VOL_UP_PX6,
};

static const unsigned button_vol_down_px7_pins[] = {
	TEGRA_PIN_BUTTON_VOL_DOWN_PX7,
};

static const unsigned button_slide_sw_py0_pins[] = {
	TEGRA_PIN_BUTTON_SLIDE_SW_PY0,
};

static const unsigned button_home_py1_pins[] = {
	TEGRA_PIN_BUTTON_HOME_PY1,
};

static const unsigned lcd_te_py2_pins[] = {
	TEGRA_PIN_LCD_TE_PY2,
};

static const unsigned pwr_i2c_scl_py3_pins[] = {
	TEGRA_PIN_PWR_I2C_SCL_PY3,
};

static const unsigned pwr_i2c_sda_py4_pins[] = {
	TEGRA_PIN_PWR_I2C_SDA_PY4,
};

static const unsigned clk_32k_out_py5_pins[] = {
	TEGRA_PIN_CLK_32K_OUT_PY5,
};

static const unsigned pz0_pins[] = {
	TEGRA_PIN_PZ0,
};

static const unsigned pz1_pins[] = {
	TEGRA_PIN_PZ1,
};

static const unsigned pz2_pins[] = {
	TEGRA_PIN_PZ2,
};

static const unsigned pz3_pins[] = {
	TEGRA_PIN_PZ3,
};

static const unsigned pz4_pins[] = {
	TEGRA_PIN_PZ4,
};

static const unsigned pz5_pins[] = {
	TEGRA_PIN_PZ5,
};

static const unsigned dap2_fs_paa0_pins[] = {
	TEGRA_PIN_DAP2_FS_PAA0,
};

static const unsigned dap2_sclk_paa1_pins[] = {
	TEGRA_PIN_DAP2_SCLK_PAA1,
};

static const unsigned dap2_din_paa2_pins[] = {
	TEGRA_PIN_DAP2_DIN_PAA2,
};

static const unsigned dap2_dout_paa3_pins[] = {
	TEGRA_PIN_DAP2_DOUT_PAA3,
};

static const unsigned aud_mclk_pbb0_pins[] = {
	TEGRA_PIN_AUD_MCLK_PBB0,
};

static const unsigned dvfs_pwm_pbb1_pins[] = {
	TEGRA_PIN_DVFS_PWM_PBB1,
};

static const unsigned dvfs_clk_pbb2_pins[] = {
	TEGRA_PIN_DVFS_CLK_PBB2,
};

static const unsigned gpio_x1_aud_pbb3_pins[] = {
	TEGRA_PIN_GPIO_X1_AUD_PBB3,
};

static const unsigned gpio_x3_aud_pbb4_pins[] = {
	TEGRA_PIN_GPIO_X3_AUD_PBB4,
};

static const unsigned hdmi_cec_pcc0_pins[] = {
	TEGRA_PIN_HDMI_CEC_PCC0,
};

static const unsigned hdmi_int_dp_hpd_pcc1_pins[] = {
	TEGRA_PIN_HDMI_INT_DP_HPD_PCC1,
};

static const unsigned spdif_out_pcc2_pins[] = {
	TEGRA_PIN_SPDIF_OUT_PCC2,
};

static const unsigned spdif_in_pcc3_pins[] = {
	TEGRA_PIN_SPDIF_IN_PCC3,
};

static const unsigned usb_vbus_en0_pcc4_pins[] = {
	TEGRA_PIN_USB_VBUS_EN0_PCC4,
};

static const unsigned usb_vbus_en1_pcc5_pins[] = {
	TEGRA_PIN_USB_VBUS_EN1_PCC5,
};

static const unsigned dp_hpd0_pcc6_pins[] = {
	TEGRA_PIN_DP_HPD0_PCC6,
};

static const unsigned pcc7_pins[] = {
	TEGRA_PIN_PCC7,
};

static const unsigned spi2_cs1_pdd0_pins[] = {
	TEGRA_PIN_SPI2_CS1_PDD0,
};

static const unsigned qspi_sck_pee0_pins[] = {
	TEGRA_PIN_QSPI_SCK_PEE0,
};

static const unsigned qspi_cs_n_pee1_pins[] = {
	TEGRA_PIN_QSPI_CS_N_PEE1,
};

static const unsigned qspi_io0_pee2_pins[] = {
	TEGRA_PIN_QSPI_IO0_PEE2,
};

static const unsigned qspi_io1_pee3_pins[] = {
	TEGRA_PIN_QSPI_IO1_PEE3,
};

static const unsigned qspi_io2_pee4_pins[] = {
	TEGRA_PIN_QSPI_IO2_PEE4,
};

static const unsigned qspi_io3_pee5_pins[] = {
	TEGRA_PIN_QSPI_IO3_PEE5,
};

static const unsigned core_pwr_req_pins[] = {
	TEGRA_PIN_CORE_PWR_REQ,
};

static const unsigned cpu_pwr_req_pins[] = {
	TEGRA_PIN_CPU_PWR_REQ,
};

static const unsigned pwr_int_n_pins[] = {
	TEGRA_PIN_PWR_INT_N,
};

static const unsigned clk_32k_in_pins[] = {
	TEGRA_PIN_CLK_32K_IN,
};

static const unsigned jtag_rtck_pins[] = {
	TEGRA_PIN_JTAG_RTCK,
};

static const unsigned batt_bcl_pins[] = {
	TEGRA_PIN_BATT_BCL,
};

static const unsigned clk_req_pins[] = {
	TEGRA_PIN_CLK_REQ,
};

static const unsigned shutdown_pins[] = {
	TEGRA_PIN_SHUTDOWN,
};

static const unsigned drive_als_prox_int_pins[] = {
	TEGRA_PIN_ALS_PROX_INT_PX3,
};

static const unsigned drive_ap_ready_pins[] = {
	TEGRA_PIN_AP_READY_PV5,
};

static const unsigned drive_ap_wake_bt_pins[] = {
	TEGRA_PIN_AP_WAKE_BT_PH3,
};

static const unsigned drive_ap_wake_nfc_pins[] = {
	TEGRA_PIN_AP_WAKE_NFC_PH7,
};

static const unsigned drive_aud_mclk_pins[] = {
	TEGRA_PIN_AUD_MCLK_PBB0,
};

static const unsigned drive_batt_bcl_pins[] = {
	TEGRA_PIN_BATT_BCL,
};

static const unsigned drive_bt_rst_pins[] = {
	TEGRA_PIN_BT_RST_PH4,
};

static const unsigned drive_bt_wake_ap_pins[] = {
	TEGRA_PIN_BT_WAKE_AP_PH5,
};

static const unsigned drive_button_home_pins[] = {
	TEGRA_PIN_BUTTON_HOME_PY1,
};

static const unsigned drive_button_power_on_pins[] = {
	TEGRA_PIN_BUTTON_POWER_ON_PX5,
};

static const unsigned drive_button_slide_sw_pins[] = {
	TEGRA_PIN_BUTTON_SLIDE_SW_PY0,
};

static const unsigned drive_button_vol_down_pins[] = {
	TEGRA_PIN_BUTTON_VOL_DOWN_PX7,
};

static const unsigned drive_button_vol_up_pins[] = {
	TEGRA_PIN_BUTTON_VOL_UP_PX6,
};

static const unsigned drive_cam1_mclk_pins[] = {
	TEGRA_PIN_CAM1_MCLK_PS0,
};

static const unsigned drive_cam1_pwdn_pins[] = {
	TEGRA_PIN_CAM1_PWDN_PS7,
};

static const unsigned drive_cam1_strobe_pins[] = {
	TEGRA_PIN_CAM1_STROBE_PT1,
};

static const unsigned drive_cam2_mclk_pins[] = {
	TEGRA_PIN_CAM2_MCLK_PS1,
};

static const unsigned drive_cam2_pwdn_pins[] = {
	TEGRA_PIN_CAM2_PWDN_PT0,
};

static const unsigned drive_cam_af_en_pins[] = {
	TEGRA_PIN_CAM_AF_EN_PS5,
};

static const unsigned drive_cam_flash_en_pins[] = {
	TEGRA_PIN_CAM_FLASH_EN_PS6,
};

static const unsigned drive_cam_i2c_scl_pins[] = {
	TEGRA_PIN_CAM_I2C_SCL_PS2,
};

static const unsigned drive_cam_i2c_sda_pins[] = {
	TEGRA_PIN_CAM_I2C_SDA_PS3,
};

static const unsigned drive_cam_rst_pins[] = {
	TEGRA_PIN_CAM_RST_PS4,
};

static const unsigned drive_clk_32k_in_pins[] = {
	TEGRA_PIN_CLK_32K_IN,
};

static const unsigned drive_clk_32k_out_pins[] = {
	TEGRA_PIN_CLK_32K_OUT_PY5,
};

static const unsigned drive_clk_req_pins[] = {
	TEGRA_PIN_CLK_REQ,
};

static const unsigned drive_core_pwr_req_pins[] = {
	TEGRA_PIN_CORE_PWR_REQ,
};

static const unsigned drive_cpu_pwr_req_pins[] = {
	TEGRA_PIN_CPU_PWR_REQ,
};

static const unsigned drive_dap1_din_pins[] = {
	TEGRA_PIN_DAP1_DIN_PB1,
};

static const unsigned drive_dap1_dout_pins[] = {
	TEGRA_PIN_DAP1_DOUT_PB2,
};

static const unsigned drive_dap1_fs_pins[] = {
	TEGRA_PIN_DAP1_FS_PB0,
};

static const unsigned drive_dap1_sclk_pins[] = {
	TEGRA_PIN_DAP1_SCLK_PB3,
};

static const unsigned drive_dap2_din_pins[] = {
	TEGRA_PIN_DAP2_DIN_PAA2
};

static const unsigned drive_dap2_dout_pins[] = {
	TEGRA_PIN_DAP2_DOUT_PAA3,
};

static const unsigned drive_dap2_fs_pins[] = {
	TEGRA_PIN_DAP2_FS_PAA0,
};

static const unsigned drive_dap2_sclk_pins[] = {
	TEGRA_PIN_DAP2_SCLK_PAA1,
};

static const unsigned drive_dap4_din_pins[] = {
	TEGRA_PIN_DAP4_DIN_PJ5,
};

static const unsigned drive_dap4_dout_pins[] = {
	TEGRA_PIN_DAP4_DOUT_PJ6,
};

static const unsigned drive_dap4_fs_pins[] = {
	TEGRA_PIN_DAP4_FS_PJ4,
};

static const unsigned drive_dap4_sclk_pins[] = {
	TEGRA_PIN_DAP4_SCLK_PJ7,
};

static const unsigned drive_sdmmc1_pins[] = {
	TEGRA_PIN_SDMMC1_CLK_PM0,
	TEGRA_PIN_SDMMC1_CMD_PM1,
	TEGRA_PIN_SDMMC1_DAT3_PM2,
	TEGRA_PIN_SDMMC1_DAT2_PM3,
	TEGRA_PIN_SDMMC1_DAT1_PM4,
	TEGRA_PIN_SDMMC1_DAT0_PM5,
};

static const unsigned drive_sdmmc3_pins[] = {
	TEGRA_PIN_SDMMC3_CLK_PP0,
	TEGRA_PIN_SDMMC3_CMD_PP1,
	TEGRA_PIN_SDMMC3_DAT3_PP2,
	TEGRA_PIN_SDMMC3_DAT2_PP3,
	TEGRA_PIN_SDMMC3_DAT1_PP4,
	TEGRA_PIN_SDMMC3_DAT0_PP5,
};

static const unsigned drive_sdmmc2_pins[] = {
};

static const unsigned drive_sdmmc4_pins[] = {
};

static const unsigned drive_dmic1_clk_pins[] = {
	TEGRA_PIN_DMIC1_CLK_PE0,
};

static const unsigned drive_dmic1_dat_pins[] = {
	TEGRA_PIN_DMIC1_DAT_PE1,
};

static const unsigned drive_dmic2_clk_pins[] = {
	TEGRA_PIN_DMIC2_CLK_PE2,
};

static const unsigned drive_dmic2_dat_pins[] = {
	TEGRA_PIN_DMIC2_DAT_PE3,
};

static const unsigned drive_dmic3_clk_pins[] = {
	TEGRA_PIN_DMIC3_CLK_PE4,
};

static const unsigned drive_dmic3_dat_pins[] = {
	TEGRA_PIN_DMIC3_DAT_PE5,
};

static const unsigned drive_dp_hpd0_pins[] = {
	TEGRA_PIN_DP_HPD0_PCC6,
};

static const unsigned drive_dvfs_clk_pins[] = {
	TEGRA_PIN_DVFS_CLK_PBB2,
};

static const unsigned drive_dvfs_pwm_pins[] = {
	TEGRA_PIN_DVFS_PWM_PBB1,
};

static const unsigned drive_gen1_i2c_scl_pins[] = {
	TEGRA_PIN_GEN1_I2C_SCL_PJ1,
};

static const unsigned drive_gen1_i2c_sda_pins[] = {
	TEGRA_PIN_GEN1_I2C_SDA_PJ0,
};

static const unsigned drive_gen2_i2c_scl_pins[] = {
	TEGRA_PIN_GEN2_I2C_SCL_PJ2,
};

static const unsigned drive_gen2_i2c_sda_pins[] = {
	TEGRA_PIN_GEN2_I2C_SDA_PJ3,
};

static const unsigned drive_gen3_i2c_scl_pins[] = {
	TEGRA_PIN_GEN3_I2C_SCL_PF0,
};

static const unsigned drive_gen3_i2c_sda_pins[] = {
	TEGRA_PIN_GEN3_I2C_SDA_PF1,
};

static const unsigned drive_pa6_pins[] = {
	TEGRA_PIN_PA6,
};

static const unsigned drive_pcc7_pins[] = {
	TEGRA_PIN_PCC7,
};

static const unsigned drive_pe6_pins[] = {
	TEGRA_PIN_PE6,
};

static const unsigned drive_pe7_pins[] = {
	TEGRA_PIN_PE7,
};

static const unsigned drive_ph6_pins[] = {
	TEGRA_PIN_PH6,
};

static const unsigned drive_pk0_pins[] = {
	TEGRA_PIN_PK0,
};

static const unsigned drive_pk1_pins[] = {
	TEGRA_PIN_PK1,
};

static const unsigned drive_pk2_pins[] = {
	TEGRA_PIN_PK2,
};

static const unsigned drive_pk3_pins[] = {
	TEGRA_PIN_PK3,
};

static const unsigned drive_pk4_pins[] = {
	TEGRA_PIN_PK4,
};

static const unsigned drive_pk5_pins[] = {
	TEGRA_PIN_PK5,
};

static const unsigned drive_pk6_pins[] = {
	TEGRA_PIN_PK6,
};

static const unsigned drive_pk7_pins[] = {
	TEGRA_PIN_PK7,
};

static const unsigned drive_pl0_pins[] = {
	TEGRA_PIN_PL0,
};

static const unsigned drive_pl1_pins[] = {
	TEGRA_PIN_PL1,
};

static const unsigned drive_pz0_pins[] = {
	TEGRA_PIN_PZ0,
};

static const unsigned drive_pz1_pins[] = {
	TEGRA_PIN_PZ1,
};

static const unsigned drive_pz2_pins[] = {
	TEGRA_PIN_PZ2,
};

static const unsigned drive_pz3_pins[] = {
	TEGRA_PIN_PZ3,
};

static const unsigned drive_pz4_pins[] = {
	TEGRA_PIN_PZ4,
};

static const unsigned drive_pz5_pins[] = {
	TEGRA_PIN_PZ5,
};

static const unsigned drive_gpio_x1_aud_pins[] = {
	TEGRA_PIN_GPIO_X1_AUD_PBB3,
};

static const unsigned drive_gpio_x3_aud_pins[] = {
	TEGRA_PIN_GPIO_X3_AUD_PBB4,
};

static const unsigned drive_gps_en_pins[] = {
	TEGRA_PIN_GPS_EN_PI2,
};

static const unsigned drive_gps_rst_pins[] = {
	TEGRA_PIN_GPS_RST_PI3,
};

static const unsigned drive_hdmi_cec_pins[] = {
	TEGRA_PIN_HDMI_CEC_PCC0,
};

static const unsigned drive_hdmi_int_dp_hpd_pins[] = {
	TEGRA_PIN_HDMI_INT_DP_HPD_PCC1,
};

static const unsigned drive_jtag_rtck_pins[] = {
	TEGRA_PIN_JTAG_RTCK,
};

static const unsigned drive_lcd_bl_en_pins[] = {
	TEGRA_PIN_LCD_BL_EN_PV1,
};

static const unsigned drive_lcd_bl_pwm_pins[] = {
	TEGRA_PIN_LCD_BL_PWM_PV0,
};

static const unsigned drive_lcd_gpio1_pins[] = {
	TEGRA_PIN_LCD_GPIO1_PV3,
};

static const unsigned drive_lcd_gpio2_pins[] = {
	TEGRA_PIN_LCD_GPIO2_PV4,
};

static const unsigned drive_lcd_rst_pins[] = {
	TEGRA_PIN_LCD_RST_PV2,
};

static const unsigned drive_lcd_te_pins[] = {
	TEGRA_PIN_LCD_TE_PY2,
};

static const unsigned drive_modem_wake_ap_pins[] = {
	TEGRA_PIN_MODEM_WAKE_AP_PX0,
};

static const unsigned drive_motion_int_pins[] = {
	TEGRA_PIN_MOTION_INT_PX2,
};

static const unsigned drive_nfc_en_pins[] = {
	TEGRA_PIN_NFC_EN_PI0,
};

static const unsigned drive_nfc_int_pins[] = {
	TEGRA_PIN_NFC_INT_PI1,
};

static const unsigned drive_pex_l0_clkreq_n_pins[] = {
	TEGRA_PIN_PEX_L0_CLKREQ_N_PA1,
};

static const unsigned drive_pex_l0_rst_n_pins[] = {
	TEGRA_PIN_PEX_L0_RST_N_PA0,
};

static const unsigned drive_pex_l1_clkreq_n_pins[] = {
	TEGRA_PIN_PEX_L1_CLKREQ_N_PA4,
};

static const unsigned drive_pex_l1_rst_n_pins[] = {
	TEGRA_PIN_PEX_L1_RST_N_PA3,
};

static const unsigned drive_pex_wake_n_pins[] = {
	TEGRA_PIN_PEX_WAKE_N_PA2,
};

static const unsigned drive_pwr_i2c_scl_pins[] = {
	TEGRA_PIN_PWR_I2C_SCL_PY3,
};

static const unsigned drive_pwr_i2c_sda_pins[] = {
	TEGRA_PIN_PWR_I2C_SDA_PY4,
};

static const unsigned drive_pwr_int_n_pins[] = {
	TEGRA_PIN_PWR_INT_N,
};

static const unsigned drive_qspi_sck_pins[] = {
	TEGRA_PIN_QSPI_SCK_PEE0,
};

static const unsigned drive_sata_led_active_pins[] = {
	TEGRA_PIN_SATA_LED_ACTIVE_PA5,
};

static const unsigned drive_shutdown_pins[] = {
	TEGRA_PIN_SHUTDOWN,
};

static const unsigned drive_spdif_in_pins[] = {
	TEGRA_PIN_SPDIF_IN_PCC3,
};

static const unsigned drive_spdif_out_pins[] = {
	TEGRA_PIN_SPDIF_OUT_PCC2,
};

static const unsigned drive_spi1_cs0_pins[] = {
	TEGRA_PIN_SPI1_CS0_PC3,
};

static const unsigned drive_spi1_cs1_pins[] = {
	TEGRA_PIN_SPI1_CS1_PC4,
};

static const unsigned drive_spi1_mosi_pins[] = {
	TEGRA_PIN_SPI1_MOSI_PC0,
};

static const unsigned drive_spi1_miso_pins[] = {
	TEGRA_PIN_SPI1_MISO_PC1,
};

static const unsigned drive_spi1_sck_pins[] = {
	TEGRA_PIN_SPI1_SCK_PC2,
};

static const unsigned drive_spi2_cs0_pins[] = {
	TEGRA_PIN_SPI2_CS0_PB7,
};

static const unsigned drive_spi2_cs1_pins[] = {
	TEGRA_PIN_SPI2_CS1_PDD0,
};

static const unsigned drive_spi2_mosi_pins[] = {
	TEGRA_PIN_SPI2_MOSI_PB4,
};

static const unsigned drive_spi2_miso_pins[] = {
	TEGRA_PIN_SPI2_MISO_PB5,
};

static const unsigned drive_spi2_sck_pins[] = {
	TEGRA_PIN_SPI2_SCK_PB6,
};

static const unsigned drive_spi4_cs0_pins[] = {
	TEGRA_PIN_SPI4_CS0_PC6,
};

static const unsigned drive_spi4_mosi_pins[] = {
	TEGRA_PIN_SPI4_MOSI_PC7,
};

static const unsigned drive_spi4_miso_pins[] = {
	TEGRA_PIN_SPI4_MISO_PD0,
};

static const unsigned drive_spi4_sck_pins[] = {
	TEGRA_PIN_SPI4_SCK_PC5,
};

static const unsigned drive_temp_alert_pins[] = {
	TEGRA_PIN_TEMP_ALERT_PX4,
};

static const unsigned drive_touch_clk_pins[] = {
	TEGRA_PIN_TOUCH_CLK_PV7,
};

static const unsigned drive_touch_int_pins[] = {
	TEGRA_PIN_TOUCH_INT_PX1,
};

static const unsigned drive_touch_rst_pins[] = {
	TEGRA_PIN_TOUCH_RST_PV6,
};

static const unsigned drive_uart1_cts_pins[] = {
	TEGRA_PIN_UART1_CTS_PU3,
};

static const unsigned drive_uart1_rts_pins[] = {
	TEGRA_PIN_UART1_RTS_PU2,
};

static const unsigned drive_uart1_rx_pins[] = {
	TEGRA_PIN_UART1_RX_PU1,
};

static const unsigned drive_uart1_tx_pins[] = {
	TEGRA_PIN_UART1_TX_PU0,
};

static const unsigned drive_uart2_cts_pins[] = {
	TEGRA_PIN_UART2_CTS_PG3,
};

static const unsigned drive_uart2_rts_pins[] = {
	TEGRA_PIN_UART2_RTS_PG2,
};

static const unsigned drive_uart2_rx_pins[] = {
	TEGRA_PIN_UART2_RX_PG1,
};

static const unsigned drive_uart2_tx_pins[] = {
	TEGRA_PIN_UART2_TX_PG0,
};

static const unsigned drive_uart3_cts_pins[] = {
	TEGRA_PIN_UART3_CTS_PD4,
};

static const unsigned drive_uart3_rts_pins[] = {
	TEGRA_PIN_UART3_RTS_PD3,
};

static const unsigned drive_uart3_rx_pins[] = {
	TEGRA_PIN_UART3_RX_PD2,
};

static const unsigned drive_uart3_tx_pins[] = {
	TEGRA_PIN_UART3_TX_PD1,
};

static const unsigned drive_uart4_cts_pins[] = {
	TEGRA_PIN_UART4_CTS_PI7,
};

static const unsigned drive_uart4_rts_pins[] = {
	TEGRA_PIN_UART4_RTS_PI6,
};

static const unsigned drive_uart4_rx_pins[] = {
	TEGRA_PIN_UART4_RX_PI5,
};

static const unsigned drive_uart4_tx_pins[] = {
	TEGRA_PIN_UART4_TX_PI4,
};

static const unsigned drive_usb_vbus_en0_pins[] = {
	TEGRA_PIN_USB_VBUS_EN0_PCC4,
};

static const unsigned drive_usb_vbus_en1_pins[] = {
	TEGRA_PIN_USB_VBUS_EN1_PCC5,
};

static const unsigned drive_wifi_en_pins[] = {
	TEGRA_PIN_WIFI_EN_PH0,
};

static const unsigned drive_wifi_rst_pins[] = {
	TEGRA_PIN_WIFI_RST_PH1,
};

static const unsigned drive_wifi_wake_ap_pins[] = {
	TEGRA_PIN_WIFI_WAKE_AP_PH2,
};

static const unsigned drive_qspi_comp_pins[] = {
	TEGRA_DRV_PAD_QSPI_COMP,
};

static const unsigned drive_qspi_comp_control_pins[] = {
	TEGRA_DRV_PAD_QSPI_COMP_CONTROL,
};

static const unsigned drive_qspi_lpbk_control_pins[] = {
	TEGRA_DRV_PAD_QSPI_LPBK_CONTROL,
};

enum tegra_mux_dt {
	TEGRA_MUX_GPIO = TEGRA_PINMUX_SPECIAL_GPIO,
	TEGRA_MUX_UNUSED = TEGRA_PINMUX_SPECIAL_UNUSED,
	TEGRA_MUX_BLINK = TEGRA_PINMUX_SPECIAL_MAX,
	TEGRA_MUX_CEC,
	TEGRA_MUX_CLDVFS,
	TEGRA_MUX_CPU,
	TEGRA_MUX_DTV,
	TEGRA_MUX_EXTPERIPH3,
	TEGRA_MUX_I2C1,
	TEGRA_MUX_I2C2,
	TEGRA_MUX_I2C3,
	TEGRA_MUX_I2CVI,
	TEGRA_MUX_I2CPMU,
	TEGRA_MUX_I2S1,
	TEGRA_MUX_I2S2,
	TEGRA_MUX_I2S3,
	TEGRA_MUX_I2S4A,
	TEGRA_MUX_I2S4B,
	TEGRA_MUX_I2S5A,
	TEGRA_MUX_I2S5B,
	TEGRA_MUX_IQC0,
	TEGRA_MUX_IQC1,
	TEGRA_MUX_PWM0,
	TEGRA_MUX_PWM1,
	TEGRA_MUX_PWM2,
	TEGRA_MUX_PWM3,
	TEGRA_MUX_PMI,
	TEGRA_MUX_UARTA,
	TEGRA_MUX_UARTB,
	TEGRA_MUX_UARTC,
	TEGRA_MUX_UARTD,
	TEGRA_MUX_UART,
	TEGRA_MUX_DMIC1,
	TEGRA_MUX_DMIC2,
	TEGRA_MUX_DMIC3,
	TEGRA_MUX_DISPLAYA,
	TEGRA_MUX_DISPLAYB,
	TEGRA_MUX_RSVD0,
	TEGRA_MUX_RSVD1,
	TEGRA_MUX_RSVD2,
	TEGRA_MUX_RSVD3,
	TEGRA_MUX_SDMMC1,
	TEGRA_MUX_SDMMC3,
	TEGRA_MUX_SOC,
	TEGRA_MUX_CORE,
	TEGRA_MUX_AUD,
	TEGRA_MUX_SOR0,
	TEGRA_MUX_SOR1,
	TEGRA_MUX_TOUCH,
	TEGRA_MUX_SPDIF,
	TEGRA_MUX_QSPI,
	TEGRA_MUX_SPI1,
	TEGRA_MUX_SPI2,
	TEGRA_MUX_SPI3,
	TEGRA_MUX_SPI4,
	TEGRA_MUX_USB,
	TEGRA_MUX_VGP1,
	TEGRA_MUX_VGP2,
	TEGRA_MUX_VGP3,
	TEGRA_MUX_VGP4,
	TEGRA_MUX_VGP5,
	TEGRA_MUX_VGP6,
	TEGRA_MUX_VIMCLK,
	TEGRA_MUX_VIMCLK2,
	TEGRA_MUX_SATA,
	TEGRA_MUX_CCLA,
	TEGRA_MUX_JTAG,
	TEGRA_MUX_SYS,
	TEGRA_MUX_PE,
	TEGRA_MUX_PE0,
	TEGRA_MUX_PE1,
	TEGRA_MUX_DP,
	TEGRA_MUX_CLK,
	TEGRA_MUX_SHUTDOWN,
	TEGRA_MUX_BCL,
};

static const char * const blink_groups[] = {
	"clk_32k_out_py5",
};

static const char * const cec_groups[] = {
	"hdmi_cec_pcc0",
};

static const char * const cldvfs_groups[] = {
	"dvfs_pwm_pbb1",
	"dvfs_clk_pbb2",
};

static const char * const cpu_groups[] = {
	"cpu_pwr_req",
};

static const char * const dtv_groups[] = {
	"spi2_mosi_pb4",
	"spi2_miso_pb5",
	"spi2_sck_pb6",
	"spi2_cs0_pb7",
};

static const char * const extperiph3_groups[] = {
	"cam1_mclk_ps0",
	"cam2_mclk_ps1",
};

static const char * const i2c1_groups[] = {
	"gen1_i2c_sda_pj0",
	"gen1_i2c_scl_pj1",
};

static const char * const i2c2_groups[] = {
	"gen2_i2c_scl_pj2",
	"gen2_i2c_sda_pj3",
};

static const char * const i2c3_groups[] = {
	"gen3_i2c_scl_pf0",
	"gen3_i2c_sda_pf1",
	"cam_i2c_scl_ps2",
	"cam_i2c_sda_ps3",
};

static const char * const i2cvi_groups[] = {
	"cam_i2c_scl_ps2",
	"cam_i2c_sda_ps3",
};

static const char * const i2cpmu_groups[] = {
	"pwr_i2c_scl_py3",
	"pwr_i2c_sda_py4",
};

static const char * const i2s1_groups[] = {
	"dap1_fs_pb0",
	"dap1_din_pb1",
	"dap1_dout_pb2",
	"dap1_sclk_pb3",
};

static const char * const i2s2_groups[] = {
	"dap2_fs_paa0",
	"dap2_sclk_paa1",
	"dap2_din_paa2",
	"dap2_dout_paa3",
};

static const char * const i2s3_groups[] = {
	"dmic1_clk_pe0",
	"dmic1_dat_pe1",
	"dmic2_clk_pe2",
	"dmic2_dat_pe3",
};

static const char * const i2s4a_groups[] = {
	"uart2_tx_pg0",
	"uart2_rx_pg1",
	"uart2_rts_pg2",
	"uart2_cts_pg3",
};

static const char * const i2s4b_groups[] = {
	"dap4_fs_pj4",
	"dap4_din_pj5",
	"dap4_dout_pj6",
	"dap4_sclk_pj7",
};

static const char * const i2s5a_groups[] = {
	"dmic3_clk_pe4",
	"dmic3_dat_pe5",
	"pe6",
	"pe7",
};

static const char * const i2s5b_groups[] = {
	"pk0",
	"pk1",
	"pk2",
	"pk3",
};

static const char * const iqc0_groups[] = {
	"pk0",
	"pk1",
	"pk2",
	"pk3",
};

static const char * const iqc1_groups[] = {
	"pk4",
	"pk5",
	"pk6",
	"pk7",
};

static const char * const pwm0_groups[] = {
	"lcd_bl_pwm_pv0",
};

static const char * const pwm1_groups[] = {
	"lcd_gpio2_pv4",
};

static const char * const pwm2_groups[] = {
	"pe6",
};

static const char * const pwm3_groups[] = {
	"pe7",
};

static const char * const pmi_groups[] = {
	"pwr_int_n",
};

static const char * const uartc_groups[] = {
	"uart3_tx_pd1",
	"uart3_rx_pd2",
	"uart3_rts_pd3",
	"uart3_cts_pd4",
};

static const char * const uartd_groups[] = {
	"uart4_tx_pi4",
	"uart4_rx_pi5",
	"uart4_rts_pi6",
	"uart4_cts_pi7",
};

static const char * const uarta_groups[] = {
	"uart1_tx_pu0",
	"uart1_rx_pu1",
	"uart1_rts_pu2",
	"uart1_cts_pu3",
};

static const char * const uartb_groups[] = {
	"uart2_tx_pg0",
	"uart2_rx_pg1",
	"uart2_rts_pg2",
	"uart2_cts_pg3",
	"ap_wake_bt_ph3",
	"bt_rst_ph4",
};

static const char * const uart_groups[] = {
	"uart2_tx_pg0",
	"uart2_rx_pg1",
	"uart2_rts_pg2",
	"uart2_cts_pg3",
	"uart4_tx_pi4",
	"uart4_rx_pi5",
	"uart4_rts_pi6",
	"uart4_cts_pi7",
};

static const char * const dmic1_groups[] = {
	"dmic1_clk_pe0",
	"dmic1_dat_pe1",
};

static const char * const dmic2_groups[] = {
	"dmic2_clk_pe2",
	"dmic2_dat_pe3",
};

static const char * const dmic3_groups[] = {
	"dmic3_clk_pe4",
	"dmic3_dat_pe5",
};

static const char * const displaya_groups[] = {
	"lcd_bl_pwm_pv0",
	"lcd_te_py2",
};

static const char * const displayb_groups[] = {
	"lcd_gpio1_pv3",
	"lcd_gpio2_pv4",
};

static const char * const rsvd0_groups[] = {
	"pe6",
	"pe7",
	"wifi_en_ph0",
	"wifi_rst_ph1",
	"wifi_wake_ap_ph2",
	"ap_wake_bt_ph3",
	"bt_rst_ph4",
	"bt_wake_ap_ph5",
	"ph6",
	"ap_wake_nfc_ph7",
	"nfc_en_pi0",
	"nfc_int_pi1",
	"gps_en_pi2",
	"gps_rst_pi3",
	"pl0",
	"lcd_bl_en_pv1",
	"lcd_rst_pv2",
	"ap_ready_pv5",
	"touch_rst_pv6",
	"modem_wake_ap_px0",
	"touch_int_px1",
	"motion_int_px2",
	"als_prox_int_px3",
	"temp_alert_px4",
	"button_power_on_px5",
	"button_vol_up_px6",
	"button_vol_down_px7",
	"button_slide_sw_py0",
	"button_home_py1",
	"dvfs_pwm_pbb1",
	"dvfs_clk_pbb2",
	"gpio_x1_aud_pbb3",
	"gpio_x3_aud_pbb4",
	"pcc7",
};

static const char * const rsvd1_groups[] = {
	"pex_l0_rst_n_pa0",
	"pex_l0_clkreq_n_pa1",
	"pex_wake_n_pa2",
	"pex_l1_rst_n_pa3",
	"pex_l1_clkreq_n_pa4",
	"sata_led_active_pa5",
	"pa6",
	"dap1_fs_pb0",
	"dap1_din_pb1",
	"dap1_dout_pb2",
	"dap1_sclk_pb3",
	"spi1_mosi_pc0",
	"spi1_miso_pc1",
	"spi1_sck_pc2",
	"spi1_cs0_pc3",
	"spi1_cs1_pc4",
	"spi4_sck_pc5",
	"spi4_cs0_pc6",
	"spi4_mosi_pc7",
	"spi4_miso_pd0",
	"gen3_i2c_scl_pf0",
	"gen3_i2c_sda_pf1",
	"wifi_en_ph0",
	"wifi_rst_ph1",
	"wifi_wake_ap_ph2",
	"bt_wake_ap_ph5",
	"ph6",
	"ap_wake_nfc_ph7",
	"nfc_en_pi0",
	"nfc_int_pi1",
	"gps_en_pi2",
	"gps_rst_pi3",
	"gen1_i2c_sda_pj0",
	"gen1_i2c_scl_pj1",
	"gen2_i2c_scl_pj2",
	"gen2_i2c_sda_pj3",
	"dap4_fs_pj4",
	"dap4_din_pj5",
	"dap4_dout_pj6",
	"dap4_sclk_pj7",
	"pk4",
	"pk5",
	"pk6",
	"pk7",
	"pl0",
	"pl1",
	"sdmmc1_clk_pm0",
	"sdmmc1_dat0_pm5",
	"sdmmc3_clk_pp0",
	"sdmmc3_cmd_pp1",
	"sdmmc3_dat3_pp2",
	"sdmmc3_dat2_pp3",
	"sdmmc3_dat1_pp4",
	"sdmmc3_dat0_pp5",
	"cam1_mclk_ps0",
	"cam2_mclk_ps1",
	"cam_rst_ps4",
	"cam1_pwdn_ps7",
	"cam2_pwdn_pt0",
	"cam1_strobe_pt1",
	"uart1_tx_pu0",
	"uart1_rx_pu1",
	"uart1_rts_pu2",
	"uart1_cts_pu3",
	"lcd_bl_en_pv1",
	"lcd_rst_pv2",
	"lcd_gpio1_pv3",
	"ap_ready_pv5",
	"touch_rst_pv6",
	"touch_clk_pv7",
	"modem_wake_ap_px0",
	"touch_int_px1",
	"motion_int_px2",
	"als_prox_int_px3",
	"temp_alert_px4",
	"button_power_on_px5",
	"button_vol_up_px6",
	"button_vol_down_px7",
	"button_slide_sw_py0",
	"button_home_py1",
	"lcd_te_py2",
	"pwr_i2c_scl_py3",
	"pwr_i2c_sda_py4",
	"pz0",
	"pz3",
	"pz4",
	"pz5",
	"dap2_fs_paa0",
	"dap2_sclk_paa1",
	"dap2_din_paa2",
	"dap2_dout_paa3",
	"aud_mclk_pbb0",
	"gpio_x1_aud_pbb3",
	"gpio_x3_aud_pbb4",
	"hdmi_cec_pcc0",
	"hdmi_int_dp_hpd_pcc1",
	"spdif_out_pcc2",
	"spdif_in_pcc3",
	"usb_vbus_en0_pcc4",
	"usb_vbus_en1_pcc5",
	"dp_hpd0_pcc6",
	"pcc7",
	"spi2_cs1_pdd0",
	"qspi_sck_pee0",
	"qspi_cs_n_pee1",
	"qspi_io0_pee2",
	"qspi_io1_pee3",
	"qspi_io2_pee4",
	"qspi_io3_pee5",
	"jtag_rtck",
	"core_pwr_req",
	"cpu_pwr_req",
	"pwr_int_n",
	"clk_32k_in",
	"batt_bcl",
	"clk_req",
	"shutdown",
};

static const char * const rsvd2_groups[] = {
	"pex_l0_rst_n_pa0",
	"pex_l0_clkreq_n_pa1",
	"pex_wake_n_pa2",
	"pex_l1_rst_n_pa3",
	"pex_l1_clkreq_n_pa4",
	"sata_led_active_pa5",
	"pa6",
	"dap1_fs_pb0",
	"dap1_din_pb1",
	"dap1_dout_pb2",
	"dap1_sclk_pb3",
	"spi2_mosi_pb4",
	"spi2_miso_pb5",
	"spi2_sck_pb6",
	"spi2_cs0_pb7",
	"spi1_mosi_pc0",
	"spi1_miso_pc1",
	"spi1_sck_pc2",
	"spi1_cs0_pc3",
	"spi1_cs1_pc4",
	"spi4_sck_pc5",
	"spi4_cs0_pc6",
	"spi4_mosi_pc7",
	"spi4_miso_pd0",
	"uart3_tx_pd1",
	"uart3_rx_pd2",
	"uart3_rts_pd3",
	"uart3_cts_pd4",
	"dmic1_clk_pe0",
	"dmic1_dat_pe1",
	"dmic2_clk_pe2",
	"dmic2_dat_pe3",
	"dmic3_clk_pe4",
	"dmic3_dat_pe5",
	"gen3_i2c_scl_pf0",
	"gen3_i2c_sda_pf1",
	"uart2_rts_pg2",
	"uart2_cts_pg3",
	"wifi_en_ph0",
	"wifi_rst_ph1",
	"wifi_wake_ap_ph2",
	"bt_wake_ap_ph5",
	"ph6",
	"ap_wake_nfc_ph7",
	"nfc_en_pi0",
	"nfc_int_pi1",
	"gps_en_pi2",
	"gps_rst_pi3",
	"uart4_tx_pi4",
	"uart4_rx_pi5",
	"uart4_rts_pi6",
	"uart4_cts_pi7",
	"gen1_i2c_sda_pj0",
	"gen1_i2c_scl_pj1",
	"gen2_i2c_scl_pj2",
	"gen2_i2c_sda_pj3",
	"dap4_fs_pj4",
	"dap4_din_pj5",
	"dap4_dout_pj6",
	"dap4_sclk_pj7",
	"pk0",
	"pk1",
	"pk2",
	"pk3",
	"pk4",
	"pk5",
	"pk6",
	"pk7",
	"pl0",
	"pl1",
	"sdmmc1_clk_pm0",
	"sdmmc1_cmd_pm1",
	"sdmmc1_dat3_pm2",
	"sdmmc1_dat2_pm3",
	"sdmmc1_dat1_pm4",
	"sdmmc1_dat0_pm5",
	"sdmmc3_clk_pp0",
	"sdmmc3_cmd_pp1",
	"sdmmc3_dat3_pp2",
	"sdmmc3_dat2_pp3",
	"sdmmc3_dat1_pp4",
	"sdmmc3_dat0_pp5",
	"cam1_mclk_ps0",
	"cam2_mclk_ps1",
	"cam_i2c_scl_ps2",
	"cam_i2c_sda_ps3",
	"cam_rst_ps4",
	"cam_af_en_ps5",
	"cam_flash_en_ps6",
	"cam1_pwdn_ps7",
	"cam2_pwdn_pt0",
	"cam1_strobe_pt1",
	"uart1_tx_pu0",
	"uart1_rx_pu1",
	"uart1_rts_pu2",
	"uart1_cts_pu3",
	"lcd_bl_en_pv1",
	"lcd_rst_pv2",
	"lcd_gpio1_pv3",
	"lcd_gpio2_pv4",
	"ap_ready_pv5",
	"touch_rst_pv6",
	"touch_clk_pv7",
	"modem_wake_ap_px0",
	"touch_int_px1",
	"motion_int_px2",
	"als_prox_int_px3",
	"temp_alert_px4",
	"button_power_on_px5",
	"button_vol_up_px6",
	"button_vol_down_px7",
	"button_slide_sw_py0",
	"button_home_py1",
	"lcd_te_py2",
	"pwr_i2c_scl_py3",
	"pwr_i2c_sda_py4",
	"clk_32k_out_py5",
	"pz0",
	"pz1",
	"pz2",
	"pz3",
	"pz4",
	"pz5",
	"dap2_fs_paa0",
	"dap2_sclk_paa1",
	"dap2_din_paa2",
	"dap2_dout_paa3",
	"aud_mclk_pbb0",
	"hdmi_cec_pcc0",
	"hdmi_int_dp_hpd_pcc1",
	"spdif_out_pcc2",
	"spdif_in_pcc3",
	"usb_vbus_en0_pcc4",
	"usb_vbus_en1_pcc5",
	"dp_hpd0_pcc6",
	"pcc7",
	"spi2_cs1_pdd0",
	"qspi_sck_pee0",
	"qspi_cs_n_pee1",
	"qspi_io0_pee2",
	"qspi_io1_pee3",
	"qspi_io2_pee4",
	"qspi_io3_pee5",
	"jtag_rtck",
	"core_pwr_req",
	"cpu_pwr_req",
	"pwr_int_n",
	"clk_32k_in",
	"batt_bcl",
	"clk_req",
	"shutdown",
};

static const char * const rsvd3_groups[] = {
	"pex_l0_rst_n_pa0",
	"pex_l0_clkreq_n_pa1",
	"pex_wake_n_pa2",
	"pex_l1_rst_n_pa3",
	"pex_l1_clkreq_n_pa4",
	"sata_led_active_pa5",
	"pa6",
	"dap1_fs_pb0",
	"dap1_din_pb1",
	"dap1_dout_pb2",
	"dap1_sclk_pb3",
	"spi2_mosi_pb4",
	"spi2_miso_pb5",
	"spi2_sck_pb6",
	"spi2_cs0_pb7",
	"spi1_mosi_pc0",
	"spi1_miso_pc1",
	"spi1_sck_pc2",
	"spi1_cs0_pc3",
	"spi1_cs1_pc4",
	"spi4_sck_pc5",
	"spi4_cs0_pc6",
	"spi4_mosi_pc7",
	"spi4_miso_pd0",
	"uart3_tx_pd1",
	"uart3_rx_pd2",
	"uart3_rts_pd3",
	"uart3_cts_pd4",
	"dmic1_clk_pe0",
	"dmic1_dat_pe1",
	"dmic2_clk_pe2",
	"dmic2_dat_pe3",
	"dmic3_clk_pe4",
	"dmic3_dat_pe5",
	"pe6",
	"pe7",
	"gen3_i2c_scl_pf0",
	"gen3_i2c_sda_pf1",
	"wifi_en_ph0",
	"wifi_rst_ph1",
	"wifi_wake_ap_ph2",
	"ap_wake_bt_ph3",
	"bt_rst_ph4",
	"bt_wake_ap_ph5",
	"ph6",
	"ap_wake_nfc_ph7",
	"nfc_en_pi0",
	"nfc_int_pi1",
	"gps_en_pi2",
	"gps_rst_pi3",
	"uart4_tx_pi4",
	"uart4_rx_pi5",
	"uart4_rts_pi6",
	"uart4_cts_pi7",
	"gen1_i2c_sda_pj0",
	"gen1_i2c_scl_pj1",
	"gen2_i2c_scl_pj2",
	"gen2_i2c_sda_pj3",
	"dap4_fs_pj4",
	"dap4_din_pj5",
	"dap4_dout_pj6",
	"dap4_sclk_pj7",
	"pk0",
	"pk1",
	"pk2",
	"pk3",
	"pk4",
	"pk5",
	"pk6",
	"pk7",
	"pl0",
	"pl1",
	"sdmmc1_clk_pm0",
	"sdmmc1_cmd_pm1",
	"sdmmc1_dat3_pm2",
	"sdmmc1_dat2_pm3",
	"sdmmc1_dat1_pm4",
	"sdmmc1_dat0_pm5",
	"sdmmc3_clk_pp0",
	"sdmmc3_cmd_pp1",
	"sdmmc3_dat3_pp2",
	"sdmmc3_dat2_pp3",
	"sdmmc3_dat1_pp4",
	"sdmmc3_dat0_pp5",
	"cam1_mclk_ps0",
	"cam2_mclk_ps1",
	"cam_i2c_scl_ps2",
	"cam_i2c_sda_ps3",
	"cam_rst_ps4",
	"cam_af_en_ps5",
	"cam_flash_en_ps6",
	"cam1_pwdn_ps7",
	"cam2_pwdn_pt0",
	"cam1_strobe_pt1",
	"uart1_tx_pu0",
	"uart1_rx_pu1",
	"uart1_rts_pu2",
	"uart1_cts_pu3",
	"lcd_bl_pwm_pv0",
	"lcd_bl_en_pv1",
	"lcd_rst_pv2",
	"lcd_gpio1_pv3",
	"ap_ready_pv5",
	"touch_rst_pv6",
	"touch_clk_pv7",
	"modem_wake_ap_px0",
	"touch_int_px1",
	"motion_int_px2",
	"als_prox_int_px3",
	"temp_alert_px4",
	"button_power_on_px5",
	"button_vol_up_px6",
	"button_vol_down_px7",
	"button_slide_sw_py0",
	"button_home_py1",
	"lcd_te_py2",
	"pwr_i2c_scl_py3",
	"pwr_i2c_sda_py4",
	"clk_32k_out_py5",
	"pz0",
	"pz1",
	"pz2",
	"pz3",
	"pz4",
	"pz5",
	"dap2_fs_paa0",
	"dap2_sclk_paa1",
	"dap2_din_paa2",
	"dap2_dout_paa3",
	"aud_mclk_pbb0",
	"dvfs_pwm_pbb1",
	"dvfs_clk_pbb2",
	"gpio_x1_aud_pbb3",
	"gpio_x3_aud_pbb4",
	"hdmi_cec_pcc0",
	"hdmi_int_dp_hpd_pcc1",
	"spdif_out_pcc2",
	"spdif_in_pcc3",
	"usb_vbus_en0_pcc4",
	"usb_vbus_en1_pcc5",
	"dp_hpd0_pcc6",
	"pcc7",
	"spi2_cs1_pdd0",
	"qspi_sck_pee0",
	"qspi_cs_n_pee1",
	"qspi_io0_pee2",
	"qspi_io1_pee3",
	"qspi_io2_pee4",
	"qspi_io3_pee5",
	"jtag_rtck",
	"core_pwr_req",
	"cpu_pwr_req",
	"pwr_int_n",
	"clk_32k_in",
	"batt_bcl",
	"clk_req",
	"shutdown",
};

static const char * const sdmmc1_groups[] = {
	"sdmmc1_clk_pm0",
	"sdmmc1_cmd_pm1",
	"sdmmc1_dat3_pm2",
	"sdmmc1_dat2_pm3",
	"sdmmc1_dat1_pm4",
	"sdmmc1_dat0_pm5",
	"pz1",
	"pz4",
};

static const char * const sdmmc3_groups[] = {
	"sdmmc3_clk_pp0",
	"sdmmc3_cmd_pp1",
	"sdmmc3_dat3_pp2",
	"sdmmc3_dat2_pp3",
	"sdmmc3_dat1_pp4",
	"sdmmc3_dat0_pp5",
	"pz2",
	"pz3",
};

static const char * const soc_groups[] = {
	"pl1",
	"clk_32k_out_py5",
	"pz5",
};

static const char * const core_groups[] = {
	"core_pwr_req",
};

static const char * const aud_groups[] = {
	"aud_mclk_pbb0",
};

static const char * const sor0_groups[] = {
	"lcd_bl_pwm_pv0",
};

static const char * const sor1_groups[] = {
	"lcd_gpio2_pv4",
};

static const char * const touch_groups[] = {
	"touch_clk_pv7",
};

static const char * const spdif_groups[] = {
	"uart2_tx_pg0",
	"uart2_rx_pg1",
	"ap_wake_bt_ph3",
	"bt_rst_ph4",
	"spdif_out_pcc2",
	"spdif_in_pcc3",
};

static const char * const qspi_groups[] = {
	"qspi_sck_pee0",
	"qspi_cs_n_pee1",
	"qspi_io0_pee2",
	"qspi_io1_pee3",
	"qspi_io2_pee4",
	"qspi_io3_pee5",
};

static const char * const spi1_groups[] = {
	"spi1_mosi_pc0",
	"spi1_miso_pc1",
	"spi1_sck_pc2",
	"spi1_cs0_pc3",
	"spi1_cs1_pc4",
};

static const char * const spi2_groups[] = {
	"spi2_mosi_pb4",
	"spi2_miso_pb5",
	"spi2_sck_pb6",
	"spi2_cs0_pb7",
	"spi2_cs1_pdd0",
};

static const char * const spi3_groups[] = {
	"sdmmc1_cmd_pm1",
	"sdmmc1_dat3_pm2",
	"sdmmc1_dat2_pm3",
	"sdmmc1_dat1_pm4",
	"dvfs_pwm_pbb1",
	"dvfs_clk_pbb2",
	"gpio_x1_aud_pbb3",
	"gpio_x3_aud_pbb4",
};

static const char * const spi4_groups[] = {
	"spi4_sck_pc5",
	"spi4_cs0_pc6",
	"spi4_mosi_pc7",
	"spi4_miso_pd0",
	"uart3_tx_pd1",
	"uart3_rx_pd2",
	"uart3_rts_pd3",
	"uart3_cts_pd4",
};

static const char * const trace_groups[] = {
	"pi2",
	"pi4",
	"pi7",
	"ph0",
	"ph6",
	"ph7",
	"pg2",
	"pg3",
	"pk1",
	"pk3",
};

static const char * const usb_groups[] = {
	"usb_vbus_en0_pcc4",
	"usb_vbus_en1_pcc5",
};

static const char * const vgp1_groups[] = {
	"cam_rst_ps4",
};

static const char * const vgp2_groups[] = {
	"cam_af_en_ps5",
};

static const char * const vgp3_groups[] = {
	"cam_flash_en_ps6",
};

static const char * const vgp4_groups[] = {
	"cam1_pwdn_ps7",
};

static const char * const vgp5_groups[] = {
	"cam2_pwdn_pt0",
};

static const char * const vgp6_groups[] = {
	"cam1_strobe_pt1",
};

static const char * const vimclk_groups[] = {
	"cam_af_en_ps5",
	"cam_flash_en_ps6",
};

static const char * const vimclk2_groups[] = {
	"pz0",
	"pz1",
};

static const char * const sata_groups[] = {
	"sata_led_active_pa5",
	"pa6",
};

static const char * const ccla_groups[] = {
	"pz2",
};

static const char * const jtag_groups[] = {
	"jtag_rtck",
};

static const char * const sys_groups[] = {
	"clk_req",
};

static const char * const pe0_groups[] = {
	"pex_l0_rst_n_pa0",
	"pex_l0_clkreq_n_pa1",
};

static const char * const pe_groups[] = {
	"pex_wake_n_pa2",
};

static const char * const pe1_groups[] = {
	"pex_l1_rst_n_pa3",
	"pex_l1_clkreq_n_pa4",
};

static const char * const dp_groups[] = {
	"hdmi_int_dp_hpd_pcc1",
	"dp_hpd0_pcc6",
};

static const char * const clk_groups[] = {
	"clk_32k_in",
};

static const char * const shutdown_groups[] = {
	"shutdown",
};

static const char * const bcl_groups[] = {
	"batt_bcl",
};

static const char * const safe_groups[] = {
	"sdmmc1_cmd_pm1",
	"sdmmc1_dat3_pm2",
	"sdmmc1_dat2_pm3",
	"sdmmc1_dat1_pm4",
	"sdmmc1_dat0_pm5",
	"uart2_rx_pg1",
	"uart2_tx_pg0",
	"uart2_rts_pg2",
	"uart2_cts_pg3",
	"uart3_tx_pd1",
	"uart3_rx_pd2",
	"uart3_cts_pd4",
	"uart3_rts_pd3",
	"gen1_i2c_scl_pj1",
	"gen1_i2c_sda_pj0",
	"dap4_fs_pj4",
	"dap4_din_pj5",
	"dap4_dout_pj6",
	"dap4_sclk_pj7",
	"pk0",
	"pk1",
	"pk3",
	"pk4",
	"pk2",
	"pk7",
	"gen2_i2c_scl_pj2",
	"gen2_i2c_sda_pj3",
	"cam1_mclk_ps0",
	"cam2_mclk_ps1",
	"cam_i2c_scl_ps2",
	"cam_i2c_sda_ps3",
	"jtag_rtck",
	"pwr_i2c_scl_py3",
	"pwr_i2c_sda_py4",
	"clk_32k_out_py5",
	"core_pwr_req",
	"cpu_pwr_req",
	"pwr_int_n",
	"clk_32k_in",
	"dap1_fs_pb0",
	"dap1_din_pb1",
	"dap1_dout_pb2",
	"dap1_sclk_pb3",
	"spdif_in_pcc3",
	"spdif_out_pcc2",
	"dap2_fs_paa0",
	"dap2_din_paa2",
	"dap2_dout_paa3",
	"dap2_sclk_paa1",
	"dvfs_pwm_pbb1",
	"gpio_x1_aud_pbb3",
	"gpio_x3_aud_pbb4",
	"dvfs_clk_pbb2",
	"sdmmc3_clk_pp0",
	"sdmmc3_cmd_pp1",
	"sdmmc3_dat0_pp5",
	"sdmmc3_dat1_pp4",
	"sdmmc3_dat2_pp3",
	"sdmmc3_dat3_pp2",
	"pex_l0_rst_n_pa0",
	"pex_l0_clkreq_n_pa1",
	"pex_wake_n_pa2",
	"pex_l1_rst_n_pa3",
	"pex_l1_clkreq_n_pa4",
	"hdmi_cec_pcc0",
	"sata_led_active_pa5",
	"spi1_mosi_pc0",
	"spi1_miso_pc1",
	"spi1_sck_pc2",
	"spi1_cs0_pc3",
	"spi1_cs1_pc4",
	"spi2_mosi_pb4",
	"spi2_miso_pb5",
	"spi2_sck_pb6",
	"spi2_cs0_pb7",
	"spi2_cs1_pdd0",
	"spi4_mosi_pc7",
	"spi4_miso_pd0",
	"spi4_sck_pc5",
	"spi4_cs0_pc6",
	"qspi_sck_pee0",
	"qspi_cs_n_pee1",
	"qspi_io0_pee2",
	"qspi_io1_pee3",
	"qspi_io2_pee4",
	"qspi_io3_pee5",
	"dmic1_clk_pe0",
	"dmic1_dat_pe1",
	"dmic2_clk_pe2",
	"dmic2_dat_pe3",
	"dmic3_clk_pe4",
	"dmic3_dat_pe5",
	"gen3_i2c_scl_pf0",
	"gen3_i2c_sda_pf1",
	"uart1_tx_pu0",
	"uart1_rx_pu1",
	"uart1_rts_pu2",
	"uart1_cts_pu3",
	"uart4_tx_pi4",
	"uart4_rx_pi5",
	"uart4_rts_pi6",
	"uart4_cts_pi7",
	"batt_bcl",
	"clk_req",
	"shutdown",
	"aud_mclk_pbb0",
	"gpio_pcc7",
	"hdmi_int_dp_hpd_pcc1",
	"wifi_en_ph0",
	"wifi_rst_ph1",
	"wifi_wake_ap_ph2",
	"ap_wake_bt_ph3",
	"bt_rst_ph4",
	"bt_wake_ap_ph5",
	"ap_wake_nfc_ph7",
	"nfc_en_pi0",
	"nfc_int_pi1",
	"gps_en_pi2",
	"gps_rst_pi3",
	"cam_rst_ps4",
	"cam_af_en_ps5",
	"cam_flash_en_ps6",
	"cam1_pwdn_ps7",
	"cam2_pwdn_pt0",
	"cam1_strobe_pt1",
	"lcd_te_py2",
	"lcd_bl_pwm_pv0",
	"lcd_bl_en_pv1",
	"lcd_rst_pv2",
	"lcd_gpio1_pv3",
	"lcd_gpio2_pv4",
	"ap_ready_pv5",
	"touch_rst_pv6",
	"touch_clk_pv7",
	"modem_wake_ap_px0",
	"touch_int_px1",
	"motion_int_px2",
	"als_prox_int_px3",
	"temp_alert_px4",
	"button_power_on_px5",
	"button_vol_up_px6",
	"button_vol_down_px7",
	"button_slide_sw_py0",
	"button_home_py1",
	"pa6",
	"pe6",
	"pe7",
	"ph6",
	"pk5",
	"pk6",
	"pl0",
	"pl1",
	"pz0",
	"pz1",
	"pz2",
	"pz3",
	"pz4",
	"pz5",
	"usb_vbus_en0_pcc4",
	"usb_vbus_en1_pcc5",
	"dp_hpd0_pcc6",
};

#define FUNCTION_SPECIAL(fname)			\
	{						\
		.name = #fname,				\
		.groups = safe_groups,			\
		.ngroups = ARRAY_SIZE(safe_groups),	\
	}						\

#define FUNCTION(fname)					\
	{						\
		.name = #fname,				\
		.groups = fname##_groups,		\
		.ngroups = ARRAY_SIZE(fname##_groups),	\
	}						\

static const struct tegra_function tegra210_functions[] = {
	FUNCTION_SPECIAL(gpio),
	FUNCTION_SPECIAL(unused),
	FUNCTION(blink),
	FUNCTION(cec),
	FUNCTION(cldvfs),
	FUNCTION(cpu),
	FUNCTION(dtv),
	FUNCTION(extperiph3),
	FUNCTION(i2c1),
	FUNCTION(i2c2),
	FUNCTION(i2c3),
	FUNCTION(i2cvi),
	FUNCTION(i2cpmu),
	FUNCTION(i2s1),
	FUNCTION(i2s2),
	FUNCTION(i2s3),
	FUNCTION(i2s4a),
	FUNCTION(i2s4b),
	FUNCTION(i2s5a),
	FUNCTION(i2s5b),
	FUNCTION(iqc0),
	FUNCTION(iqc1),
	FUNCTION(pwm0),
	FUNCTION(pwm1),
	FUNCTION(pwm2),
	FUNCTION(pwm3),
	FUNCTION(pmi),
	FUNCTION(uarta),
	FUNCTION(uartb),
	FUNCTION(uartc),
	FUNCTION(uartd),
	FUNCTION(uart),
	FUNCTION(dmic1),
	FUNCTION(dmic2),
	FUNCTION(dmic3),
	FUNCTION(displaya),
	FUNCTION(displayb),
	FUNCTION(rsvd0),
	FUNCTION(rsvd1),
	FUNCTION(rsvd2),
	FUNCTION(rsvd3),
	FUNCTION(sdmmc1),
	FUNCTION(sdmmc3),
	FUNCTION(soc),
	FUNCTION(core),
	FUNCTION(aud),
	FUNCTION(sor0),
	FUNCTION(sor1),
	FUNCTION(touch),
	FUNCTION(spdif),
	FUNCTION(qspi),
	FUNCTION(spi1),
	FUNCTION(spi2),
	FUNCTION(spi3),
	FUNCTION(spi4),
	FUNCTION(usb),
	FUNCTION(vgp1),
	FUNCTION(vgp2),
	FUNCTION(vgp3),
	FUNCTION(vgp4),
	FUNCTION(vgp5),
	FUNCTION(vgp6),
	FUNCTION(vimclk),
	FUNCTION(vimclk2),
	FUNCTION(sata),
	FUNCTION(ccla),
	FUNCTION(jtag),
	FUNCTION(sys),
	FUNCTION(pe),
	FUNCTION(pe0),
	FUNCTION(pe1),
	FUNCTION(dp),
	FUNCTION(clk),
	FUNCTION(shutdown),
	FUNCTION(bcl),
};

#define DRV_PINGROUP_REG_A	0x8d4	/* bank 0 */
#define PINGROUP_REG_A		0x3000	/* bank 1 */

#define PINGROUP_REG_Y(r) ((r) - PINGROUP_REG_A)
#define PINGROUP_REG_N(r) -1

#define PINGROUP(pg_name, f0, f1, f2, f3, r, od, schmitt_b, hsm_b, drvtype, e_io_hv)	\
	{							\
		.name = #pg_name,				\
		.pins = pg_name##_pins,				\
		.npins = ARRAY_SIZE(pg_name##_pins),		\
		.funcs = {					\
			TEGRA_MUX_ ## f0,			\
			TEGRA_MUX_ ## f1,			\
			TEGRA_MUX_ ## f2,			\
			TEGRA_MUX_ ## f3,			\
		},						\
		.mux_reg = PINGROUP_REG_Y(r),			\
		.drvdn_bit = -1,				\
		.drvup_bit = -1,				\
		.slwr_bit = -1,					\
		.slwf_bit = -1,					\
		.lpmd_bit = -1,					\
		.mux_bank = 1,					\
		.mux_bit = 0,					\
		.pupd_reg = PINGROUP_REG_Y(r),			\
		.pupd_bank = 1,					\
		.pupd_bit = 2,					\
		.tri_reg = PINGROUP_REG_Y(r),			\
		.tri_bank = 1,					\
		.tri_bit = 4,					\
		.einput_bit = 6,				\
		.e_io_hv_bit = 10,				\
		.odrain_bit = 11,				\
		.drvtype_bit = 13,				\
		.lock_bit = 7,					\
		.parked_bit = 5,				\
		.schmitt_bit = schmitt_b,			\
		.hsm_bit = hsm_b,				\
		.drv_reg = -1,					\
		.ioreset_bit = -1,				\
		.rcv_sel_bit = -1,				\
		.lpdr_bit = -1,					\
		.pbias_buf_bit = -1,			\
		.preemp_bit = -1,				\
		.rfu_in_bit = -1,				\
	}

#define DRV_PINGROUP_Y(r) ((r) - DRV_PINGROUP_REG_A)
#define DRV_PINGROUP_N(r) -1

#define DRV_PINGROUP(pg_name, r, drvdn_b, drvdn_w, drvup_b, drvup_w, slwr_b, slwr_w, slwf_b, slwf_w)	\
	{							\
		.name = "drive_" #pg_name,			\
		.pins = drive_##pg_name##_pins,			\
		.npins = ARRAY_SIZE(drive_##pg_name##_pins),	\
		.mux_reg = -1,					\
		.pupd_reg = -1,					\
		.tri_reg = -1,					\
		.einput_bit = -1,					\
		.odrain_bit = -1,					\
		.lock_bit = -1,						\
		.ioreset_bit = -1,					\
		.rcv_sel_bit = -1,					\
		.parked_bit = -1,					\
		.hsm_bit = -1,					\
		.schmitt_bit = -1,				\
		.lpdr_bit = -1,				\
		.pbias_buf_bit = -1,			\
		.preemp_bit = -1,				\
		.rfu_in_bit = -1,				\
		.drv_reg = DRV_PINGROUP_Y(r),		\
		.drv_bank = 0,					\
		.lpmd_bit = -1,						\
		.drvdn_bit = drvdn_b,				\
		.drvdn_width = drvdn_w,				\
		.drvup_bit = drvup_b,				\
		.drvup_width = drvup_w,				\
		.slwr_bit = slwr_b,				\
		.slwr_width = slwr_w,				\
		.slwf_bit = slwf_b,				\
		.slwf_width = slwf_w,				\
		.drvtype_bit = -1,					\
	}

static const struct tegra_pingroup tegra210_groups[] = {
	/*      pg_name,	f0,	f1,	f2,	f3,	r,	od,	schmitt_b,	hsm_b,	drvtype,	e_io_hv */

	PINGROUP(sdmmc1_clk_pm0,	SDMMC1,		RSVD1,		RSVD2,		RSVD3,		0x3000,  N,	12,	 9,	Y,	N),
	PINGROUP(sdmmc1_cmd_pm1,	SDMMC1,		SPI3,		RSVD2,		RSVD3,		0x3004,  N,	12,	 9,	Y,	N),
	PINGROUP(sdmmc1_dat3_pm2,	SDMMC1,		SPI3,		RSVD2,		RSVD3,		0x3008,  N,	12,	 9,	Y,	N),
	PINGROUP(sdmmc1_dat2_pm3,	SDMMC1,		SPI3,		RSVD2,		RSVD3,		0x300c,  N,	12,	 9,	Y,	N),
	PINGROUP(sdmmc1_dat1_pm4,	SDMMC1,		SPI3,		RSVD2,		RSVD3,		0x3010,  N,	12,	 9,	Y,	N),
	PINGROUP(sdmmc1_dat0_pm5,	SDMMC1,		RSVD1,		RSVD2,		RSVD3,		0x3014,  N,	12,	 9,	Y,	N),
	PINGROUP(uart2_rx_pg1,		UARTB,		I2S4A,		SPDIF,		UART,		0x30f8,  Y,	12,	-1,	N,	N),
	PINGROUP(uart2_tx_pg0,		UARTB,		I2S4A,		SPDIF,		UART,		0x30f4,  Y,	12,	-1,	N,	N),
	PINGROUP(uart2_rts_pg2,		UARTB,		I2S4A,		RSVD2,		UART,		0x30fc,  Y,	12,	-1,	N,	N),
	PINGROUP(uart2_cts_pg3,		UARTB,		I2S4A,		RSVD2,		UART,		0x3100,  Y,	12,	-1,	N,	N),
	PINGROUP(uart3_tx_pd1,		UARTC,		SPI4,		RSVD2,		RSVD3,		0x3104,  Y,	12,	-1,	N,	N),
	PINGROUP(uart3_rx_pd2,		UARTC,		SPI4,		RSVD2,		RSVD3,		0x3108,  Y,	12,	-1,	N,	N),
	PINGROUP(uart3_cts_pd4,		UARTC,		SPI4,		RSVD2,		RSVD3,		0x3110,  Y,	12,	-1,	N,	N),
	PINGROUP(uart3_rts_pd3,		UARTC,		SPI4,		RSVD2,		RSVD3,		0x310c,  Y,	12,	-1,	N,	N),
	PINGROUP(gen1_i2c_scl_pj1,	I2C1,		RSVD1,		RSVD2,		RSVD3,		0x30bc,  Y,	12,	-1,	N,	Y),
	PINGROUP(gen1_i2c_sda_pj0,	I2C1,		RSVD1,		RSVD2,		RSVD3,		0x30c0,  Y,	12,	-1,	N,	Y),
	PINGROUP(dap4_fs_pj4,		I2S4B,		RSVD1,		RSVD2,		RSVD3,		0x3144,  Y,	12,	-1,	N,	N),
	PINGROUP(dap4_din_pj5,		I2S4B,		RSVD1,		RSVD2,		RSVD3,		0x3148,  Y,	12,	-1,	N,	N),
	PINGROUP(dap4_dout_pj6,		I2S4B,		RSVD1,		RSVD2,		RSVD3,		0x314c,  Y,	12,	-1,	N,	N),
	PINGROUP(dap4_sclk_pj7,		I2S4B,		RSVD1,		RSVD2,		RSVD3,		0x3150,  Y,	12,	-1,	N,	N),
	PINGROUP(pk0,			IQC0,		I2S5B,		RSVD2,		RSVD3,		0x3254,  N,	12,	 9,	Y,	N),
	PINGROUP(pk1,			IQC0,		I2S5B,		RSVD2,		RSVD3,		0x3258,  N,	12,	 9,	Y,	N),
	PINGROUP(pk3,			IQC0,		I2S5B,		RSVD2,		RSVD3,		0x3260,  N,	12,	 9,	Y,	N),
	PINGROUP(pk4,			IQC1,		RSVD1,		RSVD2,		RSVD3,		0x3264,  N,	12,	 9,	Y,	N),
	PINGROUP(pk2,			IQC0,		I2S5B,		RSVD2,		RSVD3,		0x325c,  N,	12,	 9,	Y,	N),
	PINGROUP(pk7,			IQC1,		RSVD1,		RSVD2,		RSVD3,		0x3270,  N,	12,	 9,	Y,	N),
	PINGROUP(gen2_i2c_scl_pj2,	I2C2,		RSVD1,		RSVD2,		RSVD3,		0x30c4,  Y,	12,	-1,	N,	Y),
	PINGROUP(gen2_i2c_sda_pj3,	I2C2,		RSVD1,		RSVD2,		RSVD3,		0x30c8,  Y,	12,	-1,	N,	Y),
	PINGROUP(cam1_mclk_ps0,		EXTPERIPH3,	RSVD1,		RSVD2,		RSVD3,		0x3154,  Y,	12,	-1,	N,	N),
	PINGROUP(cam2_mclk_ps1,		EXTPERIPH3,	RSVD1,		RSVD2,		RSVD3,		0x3158,  Y,	12,	-1,	N,	N),
	PINGROUP(cam_i2c_scl_ps2,	I2C3,		I2CVI,		RSVD2,		RSVD3,		0x30d4,  Y,	12,	-1,	N,	Y),
	PINGROUP(cam_i2c_sda_ps3,	I2C3,		I2CVI,		RSVD2,		RSVD3,		0x30d8,  Y,	12,	-1,	N,	Y),
	PINGROUP(jtag_rtck,		JTAG,		RSVD1,		RSVD2,		RSVD3,		0x315c,  Y,	12,	-1,	N,	N),
	PINGROUP(pwr_i2c_scl_py3,	I2CPMU,		RSVD1,		RSVD2,		RSVD3,		0x30dc,  Y,	12,	-1,	N,	Y),
	PINGROUP(pwr_i2c_sda_py4,	I2CPMU,		RSVD1,		RSVD2,		RSVD3,		0x30e0,  Y,	12,	-1,	N,	Y),
	PINGROUP(clk_32k_out_py5,	SOC,		BLINK,		RSVD2,		RSVD3,		0x3164,  Y,	12,	-1,	N,	N),
	PINGROUP(core_pwr_req,		CORE,		RSVD1,		RSVD2,		RSVD3,		0x317c,  N,	12,	-1,	N,	N),
	PINGROUP(cpu_pwr_req,		CPU,		RSVD1,		RSVD2,		RSVD3,		0x3170,  N,	12,	-1,	N,	N),
	PINGROUP(pwr_int_n,		PMI,		RSVD1,		RSVD2,		RSVD3,		0x3174,  N,	12,	-1,	N,	N),
	PINGROUP(clk_32k_in,		CLK,		RSVD1,		RSVD2,		RSVD3,		0x3160,  N,	12,	-1,	N,	N),
	PINGROUP(dap1_fs_pb0,		I2S1,		RSVD1,		RSVD2,		RSVD3,		0x3124,  N,	12,	 9,	Y,	N),
	PINGROUP(dap1_din_pb1,		I2S1,		RSVD1,		RSVD2,		RSVD3,		0x3128,  N,	12,	 9,	Y,	N),
	PINGROUP(dap1_dout_pb2,		I2S1,		RSVD1,		RSVD2,		RSVD3,		0x312c,  N,	12,	 9,	Y,	N),
	PINGROUP(dap1_sclk_pb3,		I2S1,		RSVD1,		RSVD2,		RSVD3,		0x3130,  N,	12,	 9,	Y,	N),
	PINGROUP(spdif_in_pcc3,		SPDIF,		RSVD1,		RSVD2,		RSVD3,		0x31a4,  Y,	12,	-1,	N,	N),
	PINGROUP(spdif_out_pcc2,	SPDIF,		RSVD1,		RSVD2,		RSVD3,		0x31a0,  Y,	12,	-1,	N,	N),
	PINGROUP(dap2_fs_paa0,		I2S2,		RSVD1,		RSVD2,		RSVD3,		0x3134,  N,	12,	 9,	Y,	N),
	PINGROUP(dap2_din_paa2,		I2S2,		RSVD1,		RSVD2,		RSVD3,		0x3138,  N,	12,	 9,	Y,	N),
	PINGROUP(dap2_dout_paa3,	I2S2,		RSVD1,		RSVD2,		RSVD3,		0x313c,  N,	12,	 9,	Y,	N),
	PINGROUP(dap2_sclk_paa1,	I2S2,		RSVD1,		RSVD2,		RSVD3,		0x3140,  N,	12,	 9,	Y,	N),
	PINGROUP(dvfs_pwm_pbb1,		RSVD0,		CLDVFS,		SPI3,		RSVD3,		0x3184,  Y,	12,	-1,	N,	N),
	PINGROUP(gpio_x1_aud_pbb3,	RSVD0,		RSVD1,		SPI3,		RSVD3,		0x318c,  Y,	12,	-1,	N,	N),
	PINGROUP(gpio_x3_aud_pbb4,	RSVD0,		RSVD1,		SPI3,		RSVD3,		0x3190,  Y,	12,	-1,	N,	N),
	PINGROUP(dvfs_clk_pbb2,		RSVD0,		CLDVFS,		SPI3,		RSVD3,		0x3188,  Y,	12,	-1,	N,	N),
	PINGROUP(sdmmc3_clk_pp0,	SDMMC3,		RSVD1,		RSVD2,		RSVD3,		0x301c,  N,	12,	 9,	Y,	N),
	PINGROUP(sdmmc3_cmd_pp1,	SDMMC3,		RSVD1,		RSVD2,		RSVD3,		0x3020,  N,	12,	 9,	Y,	N),
	PINGROUP(sdmmc3_dat0_pp5,	SDMMC3,		RSVD1,		RSVD2,		RSVD3,		0x3024,  N,	12,	 9,	Y,	N),
	PINGROUP(sdmmc3_dat1_pp4,	SDMMC3,		RSVD1,		RSVD2,		RSVD3,		0x3028,  N,	12,	 9,	Y,	N),
	PINGROUP(sdmmc3_dat2_pp3,	SDMMC3,		RSVD1,		RSVD2,		RSVD3,		0x302c,  N,	12,	 9,	Y,	N),
	PINGROUP(sdmmc3_dat3_pp2,	SDMMC3,		RSVD1,		RSVD2,		RSVD3,		0x3030,  N,	12,	 9,	Y,	N),
	PINGROUP(pex_l0_rst_n_pa0,	PE0,		RSVD1,		RSVD2,		RSVD3,		0x3038,  Y,	12,	-1,	N,	Y),
	PINGROUP(pex_l0_clkreq_n_pa1,	PE0,		RSVD1,		RSVD2,		RSVD3,		0x303c,  Y,	12,	-1,	N,	Y),
	PINGROUP(pex_wake_n_pa2,	PE,		RSVD1,		RSVD2,		RSVD3,		0x3040,  Y,	12,	-1,	N,	Y),
	PINGROUP(pex_l1_rst_n_pa3,	PE1,		RSVD1,		RSVD2,		RSVD3,		0x3044,  Y,	12,	-1,	N,	Y),
	PINGROUP(pex_l1_clkreq_n_pa4,	PE1,		RSVD1,		RSVD2,		RSVD3,		0x3048,  Y,	12,	-1,	N,	Y),
	PINGROUP(hdmi_cec_pcc0,		CEC,		RSVD1,		RSVD2,		RSVD3,		0x3198,  Y,	12,	-1,	N,	Y),
	PINGROUP(sata_led_active_pa5,	SATA,		RSVD1,		RSVD2,		RSVD3,		0x304c,  Y,	12,	-1,	N,	N),
	PINGROUP(spi1_mosi_pc0,		SPI1,		RSVD1,		RSVD2,		RSVD3,		0x3050,  N,	12,	 9,	Y,	N),
	PINGROUP(spi1_miso_pc1,		SPI1,		RSVD1,		RSVD2,		RSVD3,		0x3054,  N,	12,	 9,	Y,	N),
	PINGROUP(spi1_sck_pc2,		SPI1,		RSVD1,		RSVD2,		RSVD3,		0x3058,  N,	12,	 9,	Y,	N),
	PINGROUP(spi1_cs0_pc3,		SPI1,		RSVD1,		RSVD2,		RSVD3,		0x305c,  N,	12,	 9,	Y,	N),
	PINGROUP(spi1_cs1_pc4,		SPI1,		RSVD1,		RSVD2,		RSVD3,		0x3060,  N,	12,	 9,	Y,	N),
	PINGROUP(spi2_mosi_pb4,		SPI2,		DTV,		RSVD2,		RSVD3,		0x3064,  N,	12,	 9,	Y,	N),
	PINGROUP(spi2_miso_pb5,		SPI2,		DTV,		RSVD2,		RSVD3,		0x3068,  N,	12,	 9,	Y,	N),
	PINGROUP(spi2_sck_pb6,		SPI2,		DTV,		RSVD2,		RSVD3,		0x306c,  N,	12,	 9,	Y,	N),
	PINGROUP(spi2_cs0_pb7,		SPI2,		DTV,		RSVD2,		RSVD3,		0x3070,  N,	12,	 9,	Y,	N),
	PINGROUP(spi2_cs1_pdd0,		SPI2,		RSVD1,		RSVD2,		RSVD3,		0x3074,  N,	12,	 9,	Y,	N),
	PINGROUP(spi4_mosi_pc7,		SPI4,		RSVD1,		RSVD2,		RSVD3,		0x3078,  N,	12,	 9,	Y,	N),
	PINGROUP(spi4_miso_pd0,		SPI4,		RSVD1,		RSVD2,		RSVD3,		0x307c,  N,	12,	 9,	Y,	N),
	PINGROUP(spi4_sck_pc5,		SPI4,		RSVD1,		RSVD2,		RSVD3,		0x3080,  N,	12,	 9,	Y,	N),
	PINGROUP(spi4_cs0_pc6,		SPI4,		RSVD1,		RSVD2,		RSVD3,		0x3084,  N,	12,	 9,	Y,	N),
	PINGROUP(qspi_sck_pee0,		QSPI,		RSVD1,		RSVD2,		RSVD3,		0x3088,  N,	12,	 9,	Y,	N),
	PINGROUP(qspi_cs_n_pee1,	QSPI,		RSVD1,		RSVD2,		RSVD3,		0x308c,  N,	12,	 9,	Y,	N),
	PINGROUP(qspi_io0_pee2,		QSPI,		RSVD1,		RSVD2,		RSVD3,		0x3090,  N,	12,	 9,	Y,	N),
	PINGROUP(qspi_io1_pee3,		QSPI,		RSVD1,		RSVD2,		RSVD3,		0x3094,  N,	12,	 9,	Y,	N),
	PINGROUP(qspi_io2_pee4,		QSPI,		RSVD1,		RSVD2,		RSVD3,		0x3098,  N,	12,	 9,	Y,	N),
	PINGROUP(qspi_io3_pee5,		QSPI,		RSVD1,		RSVD2,		RSVD3,		0x309c,  N,	12,	 9,	Y,	N),
	PINGROUP(dmic1_clk_pe0,		DMIC1,		I2S3,		RSVD2,		RSVD3,		0x30a4,  Y,	12,	-1,	N,	N),
	PINGROUP(dmic1_dat_pe1,		DMIC1,		I2S3,		RSVD2,		RSVD3,		0x30a8,  Y,	12,	-1,	N,	N),
	PINGROUP(dmic2_clk_pe2,		DMIC2,		I2S3,		RSVD2,		RSVD3,		0x30ac,  Y,	12,	-1,	N,	N),
	PINGROUP(dmic2_dat_pe3,		DMIC2,		I2S3,		RSVD2,		RSVD3,		0x30b0,  Y,	12,	-1,	N,	N),
	PINGROUP(dmic3_clk_pe4,		DMIC3,		I2S5A,		RSVD2,		RSVD3,		0x30b4,  Y,	12,	-1,	N,	N),
	PINGROUP(dmic3_dat_pe5,		DMIC3,		I2S5A,		RSVD2,		RSVD3,		0x30b8,  Y,	12,	-1,	N,	N),
	PINGROUP(gen3_i2c_scl_pf0,	I2C3,		RSVD1,		RSVD2,		RSVD3,		0x30cc,  Y,	12,	-1,	N,	Y),
	PINGROUP(gen3_i2c_sda_pf1,	I2C3,		RSVD1,		RSVD2,		RSVD3,		0x30d0,  Y,	12,	-1,	N,	Y),
	PINGROUP(uart1_tx_pu0,		UARTA,		RSVD1,		RSVD2,		RSVD3,		0x30e4,  Y,	12,	-1,	N,	N),
	PINGROUP(uart1_rx_pu1,		UARTA,		RSVD1,		RSVD2,		RSVD3,		0x30e8,  Y,	12,	-1,	N,	N),
	PINGROUP(uart1_rts_pu2,		UARTA,		RSVD1,		RSVD2,		RSVD3,		0x30ec,  Y,	12,	-1,	N,	N),
	PINGROUP(uart1_cts_pu3,		UARTA,		RSVD1,		RSVD2,		RSVD3,		0x30f0,  Y,	12,	-1,	N,	N),
	PINGROUP(uart4_tx_pi4,		UARTD,		UART,		RSVD2,		RSVD3,		0x3114,  Y,	12,	-1,	N,	N),
	PINGROUP(uart4_rx_pi5,		UARTD,		UART,		RSVD2,		RSVD3,		0x3118,  Y,	12,	-1,	N,	N),
	PINGROUP(uart4_rts_pi6,		UARTD,		UART,		RSVD2,		RSVD3,		0x311c,  Y,	12,	-1,	N,	N),
	PINGROUP(uart4_cts_pi7,		UARTD,		UART,		RSVD2,		RSVD3,		0x3120,  Y,	12,	-1,	N,	N),
	PINGROUP(batt_bcl,		BCL,		RSVD1,		RSVD2,		RSVD3,		0x3168,  Y,	12,	-1,	N,	Y),
	PINGROUP(clk_req,		SYS,		RSVD1,		RSVD2,		RSVD3,		0x316c,  N,	12,	-1,	N,	N),
	PINGROUP(shutdown,		SHUTDOWN,	RSVD1,		RSVD2,		RSVD3,		0x3178,  N,	12,	-1,	N,	N),
	PINGROUP(aud_mclk_pbb0,		AUD,		RSVD1,		RSVD2,		RSVD3,		0x3180,  Y,	12,	-1,	N,	N),
	PINGROUP(pcc7,			RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x3194,  Y,	12,	-1,	N,	Y),
	PINGROUP(hdmi_int_dp_hpd_pcc1,	DP,		RSVD1,		RSVD2,		RSVD3,		0x319c,  Y,	12,	-1,	N,	Y),
	PINGROUP(wifi_en_ph0,		RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x31b4,  Y,	12,	-1,	N,	N),
	PINGROUP(wifi_rst_ph1,		RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x31b8,  Y,	12,	-1,	N,	N),
	PINGROUP(wifi_wake_ap_ph2,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x31bc,  Y,	12,	-1,	N,	N),
	PINGROUP(ap_wake_bt_ph3,	RSVD0,		UARTB,		SPDIF,		RSVD3,		0x31c0,  Y,	12,	-1,	N,	N),
	PINGROUP(bt_rst_ph4,		RSVD0,		UARTB,		SPDIF,		RSVD3,		0x31c4,  Y,	12,	-1,	N,	N),
	PINGROUP(bt_wake_ap_ph5,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x31c8,  Y,	12,	-1,	N,	N),
	PINGROUP(ap_wake_nfc_ph7,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x31cc,  Y,	12,	-1,	N,	N),
	PINGROUP(nfc_en_pi0,		RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x31d0,  Y,	12,	-1,	N,	N),
	PINGROUP(nfc_int_pi1,		RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x31d4,  Y,	12,	-1,	N,	N),
	PINGROUP(gps_en_pi2,		RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x31d8,  Y,	12,	-1,	N,	N),
	PINGROUP(gps_rst_pi3,		RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x31dc,  Y,	12,	-1,	N,	N),
	PINGROUP(cam_rst_ps4,		VGP1,		RSVD1,		RSVD2,		RSVD3,		0x31e0,  Y,	12,	-1,	N,	N),
	PINGROUP(cam_af_en_ps5,	 	VIMCLK,		VGP2,		RSVD2,		RSVD3,		0x31e4,  Y,	12,	-1,	N,	N),
	PINGROUP(cam_flash_en_ps6,	VIMCLK,		VGP3,		RSVD2,		RSVD3,		0x31e8,  Y,	12,	-1,	N,	N),
	PINGROUP(cam1_pwdn_ps7,		VGP4,		RSVD1,		RSVD2,		RSVD3,		0x31ec,  Y,	12,	-1,	N,	N),
	PINGROUP(cam2_pwdn_pt0,		VGP5,		RSVD1,		RSVD2,		RSVD3,		0x31f0,  Y,	12,	-1,	N,	N),
	PINGROUP(cam1_strobe_pt1,	VGP6,		RSVD1,		RSVD2,		RSVD3,		0x31f4,  Y,	12,	-1,	N,	N),
	PINGROUP(lcd_te_py2,		DISPLAYA,	RSVD1,		RSVD2,		RSVD3,		0x31f8,  Y,	12,	-1,	N,	N),
	PINGROUP(lcd_bl_pwm_pv0,	DISPLAYA,	PWM0,		SOR0,		RSVD3,		0x31fc,  Y,	12,	-1,	N,	N),
	PINGROUP(lcd_bl_en_pv1,		RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x3200,  Y,	12,	-1,	N,	N),
	PINGROUP(lcd_rst_pv2,		RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x3204,  Y,	12,	-1,	N,	N),
	PINGROUP(lcd_gpio1_pv3,		DISPLAYB,	RSVD1,		RSVD2,		RSVD3,		0x3208,  Y,	12,	-1,	N,	N),
	PINGROUP(lcd_gpio2_pv4,		DISPLAYB,	PWM1,		RSVD2,		SOR1,		0x320c,  Y,	12,	-1,	N,	N),
	PINGROUP(ap_ready_pv5,		RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x3210,  Y,	12,	-1,	N,	N),
	PINGROUP(touch_rst_pv6,		RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x3214,  Y,	12,	-1,	N,	N),
	PINGROUP(touch_clk_pv7,		TOUCH,		RSVD1,		RSVD2,		RSVD3,		0x3218,  Y,	12,	-1,	N,	N),
	PINGROUP(modem_wake_ap_px0,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x321c,  Y,	12,	-1,	N,	N),
	PINGROUP(touch_int_px1,		RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x3220,  Y,	12,	-1,	N,	N),
	PINGROUP(motion_int_px2,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x3224,  Y,	12,	-1,	N,	N),
	PINGROUP(als_prox_int_px3,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x3228,  Y,	12,	-1,	N,	N),
	PINGROUP(temp_alert_px4,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x322c,  Y,	12,	-1,	N,	N),
	PINGROUP(button_power_on_px5,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x3230,  Y,	12,	-1,	N,	N),
	PINGROUP(button_vol_up_px6,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x3234,  Y,	12,	-1,	N,	N),
	PINGROUP(button_vol_down_px7,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x3238,  Y,	12,	-1,	N,	N),
	PINGROUP(button_slide_sw_py0,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x323c,  Y,	12,	-1,	N,	N),
	PINGROUP(button_home_py1,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x3240,  Y,	12,	-1,	N,	N),
	PINGROUP(pa6,			SATA,		RSVD1,		RSVD2,		RSVD3,		0x3244,  Y,	12,	-1,	N,	N),
	PINGROUP(pe6,			RSVD0,		I2S5A,		PWM2,		RSVD3,		0x3248,  Y,	12,	-1,	N,	N),
	PINGROUP(pe7,			RSVD0,		I2S5A,		PWM3,		RSVD3,		0x324c,  Y,	12,	-1,	N,	N),
	PINGROUP(ph6,			RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x3250,  Y,	12,	-1,	N,	N),
	PINGROUP(pk5,			IQC1,		RSVD1,		RSVD2,		RSVD3,		0x3268,  N,	12,	 9,	Y,	N),
	PINGROUP(pk6,			IQC1,		RSVD1,		RSVD2,		RSVD3,		0x326c,  N,	12,	 9,	Y,	N),
	PINGROUP(pl0,			RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x3274,  N,	12,	 9,	Y,	N),
	PINGROUP(pl1,			SOC,		RSVD1,		RSVD2,		RSVD3,		0x3278,  N,	12,	 9,	Y,	N),
	PINGROUP(pz0,			VIMCLK2,	RSVD1,		RSVD2,		RSVD3,		0x327c,  Y,	12,	-1,	N,	N),
	PINGROUP(pz1,			VIMCLK2,	SDMMC1,		RSVD2,		RSVD3,		0x3280,  Y,	12,	-1,	N,	N),
	PINGROUP(pz2,			SDMMC3,		CCLA,		RSVD2,		RSVD3,		0x3284,  Y,	12,	-1,	N,	N),
	PINGROUP(pz3,			SDMMC3,		RSVD1,		RSVD2,		RSVD3,		0x3288,  Y,	12,	-1,	N,	N),
	PINGROUP(pz4,			SDMMC1,		RSVD1,		RSVD2,		RSVD3,		0x328c,  Y,	12,	-1,	N,	N),
	PINGROUP(pz5,			SOC,		RSVD1,		RSVD2,		RSVD3,		0x3290,  Y,	12,	-1,	N,	N),
	PINGROUP(usb_vbus_en0_pcc4,	USB,		RSVD1,		RSVD2,		RSVD3,		0x31a8,  Y,	12,	-1,	N,	Y),
	PINGROUP(usb_vbus_en1_pcc5,	USB,		RSVD1,		RSVD2,		RSVD3,		0x31ac,  Y,	12,	-1,	N,	Y),
	PINGROUP(dp_hpd0_pcc6,		DP,		RSVD1,		RSVD2,		RSVD3,		0x31b0,  Y,	12,	-1,	N,	N),

	/* pg_name,	r,	drvdn_b,	drvdn_w,	drvup_b,	drvup_w,	slwr_b,	slwr_w,	slwf_b,	slwf_w */

	DRV_PINGROUP(als_prox_int,	0x8e4,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(ap_ready,		0x8e8,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(ap_wake_bt,	0x8ec,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(ap_wake_nfc,	0x8f0,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(aud_mclk,		0x8f4,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(batt_bcl,		0x8f8,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(bt_rst,		0x8fc,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(bt_wake_ap,	0x900,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(button_home,	0x904,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(button_power_on,	0x908,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(button_slide_sw,	0x90c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(button_vol_down,	0x910,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(button_vol_up,	0x914,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(cam1_mclk,		0x918,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(cam1_pwdn,		0x91c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(cam1_strobe,	0x920,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(cam2_mclk,		0x924,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(cam2_pwdn,		0x928,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(cam_af_en,		0x92c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(cam_flash_en,	0x930,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(cam_i2c_scl,	0x934,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(cam_i2c_sda,	0x938,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(cam_rst,		0x93c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(clk_32k_in,	0x940,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(clk_32k_out,	0x944,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(clk_req,		0x948,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(core_pwr_req,	0x94c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(cpu_pwr_req,	0x950,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(dap1_din,		0x954,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(dap1_dout,		0x958,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(dap1_fs,		0x95c,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(dap1_sclk,		0x960,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(dap2_din,		0x964,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(dap2_dout,		0x968,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(dap2_fs,		0x96c,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(dap2_sclk,		0x970,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(dap4_din,		0x974,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(dap4_dout,		0x978,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(dap4_fs,		0x97c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(dap4_sclk,		0x980,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(dmic1_clk,		0x984,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(dmic1_dat,		0x988,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(dmic2_clk,		0x98c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(dmic2_dat,		0x990,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(dmic3_clk,		0x994,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(dmic3_dat,		0x998,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(dp_hpd0,		0x99c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(dvfs_clk,		0x9a0,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(dvfs_pwm,		0x9a4,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(gen1_i2c_scl,	0x9a8,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(gen1_i2c_sda,	0x9ac,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(gen2_i2c_scl,	0x9b0,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(gen2_i2c_sda,	0x9b4,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(gen3_i2c_scl,	0x9b8,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(gen3_i2c_sda,	0x9bc,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pa6,		0x9c0,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pcc7,		0x9c4,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pe6,		0x9c8,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pe7,		0x9cc,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(ph6,		0x9d0,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pk0,		0x9d4,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(pk1,		0x9d8,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(pk2,		0x9dc,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(pk3,		0x9e0,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(pk4,		0x9e4,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(pk5,		0x9e8,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(pk6,		0x9ec,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(pk7,		0x9f0,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(pl0,		0x9f4,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(pl1,		0x9f8,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(pz0,		0x9fc,	12,	7,	20,	7,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pz1,		0xa00,	12,	7,	20,	7,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pz2,		0xa04,	12,	7,	20,	7,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pz3,		0xa08,	12,	7,	20,	7,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pz4,		0xa0c,	12,	7,	20,	7,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pz5,		0xa10,	12,	7,	20,	7,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(gpio_x1_aud,	0xa14,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(gpio_x3_aud,	0xa18,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(gps_en,		0xa1c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(gps_rst,		0xa20,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(hdmi_cec,		0xa24,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(hdmi_int_dp_hpd,	0xa28,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(jtag_rtck,		0xa2c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(lcd_bl_en,		0xa30,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(lcd_bl_pwm,	0xa34,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(lcd_gpio1,		0xa38,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(lcd_gpio2,		0xa3c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(lcd_rst,		0xa40,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(lcd_te,		0xa44,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(modem_wake_ap,	0xa48,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(motion_int,	0xa4c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(nfc_en,		0xa50,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(nfc_int,		0xa54,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pex_l0_clkreq_n,	0xa58,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pex_l0_rst_n,	0xa5c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pex_l1_clkreq_n,	0xa60,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pex_l1_rst_n,	0xa64,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pex_wake_n,	0xa68,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pwr_i2c_scl,	0xa6c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pwr_i2c_sda,	0xa70,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(pwr_int_n,		0xa74,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(qspi_comp,		0xa78,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(qspi_sck,		0xa90,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(sata_led_active,	0xa94,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(sdmmc1,		0xa98,	12,	7,	20,	7,	28,	2,	30,	2),
	DRV_PINGROUP(sdmmc2,		0xa9c,	2,	6,	8,	6,	28,	2,	30,	2),
	DRV_PINGROUP(sdmmc3,		0xab0,	12,	7,	20,	7,	28,	2,	30,	2),
	DRV_PINGROUP(sdmmc4,		0xab4,	2,	6,	8,	6,	28,	2,	30,	2),
	DRV_PINGROUP(shutdown,		0xac8,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(spdif_in,		0xacc,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(spdif_out,		0xad0,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(spi1_cs0,		0xad4,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(spi1_cs1,		0xad8,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(spi1_miso,		0xadc,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(spi1_mosi,		0xae0,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(spi1_sck,		0xae4,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(spi2_cs0,		0xae8,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(spi2_cs1,		0xaec,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(spi2_miso,		0xaf0,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(spi2_mosi,		0xaf4,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(spi2_sck,		0xaf8,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(spi4_cs0,		0xafc,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(spi4_miso,		0xb00,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(spi4_mosi,		0xb04,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(spi4_sck,		0xb08,	-1,	-1,	-1,	-1,	28,	2,	30,	2),
	DRV_PINGROUP(temp_alert,	0xb0c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(touch_clk,		0xb10,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(touch_int,		0xb14,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(touch_rst,		0xb18,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(uart1_cts,		0xb1c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(uart1_rts,		0xb20,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(uart1_rx,		0xb24,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(uart1_tx,		0xb28,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(uart2_cts,		0xb2c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(uart2_rts,		0xb30,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(uart2_rx,		0xb34,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(uart2_tx,		0xb38,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(uart3_cts,		0xb3c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(uart3_rts,		0xb40,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(uart3_rx,		0xb44,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(uart3_tx,		0xb48,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(uart4_cts,		0xb4c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(uart4_rts,		0xb50,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(uart4_rx,		0xb54,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(uart4_tx,		0xb58,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(usb_vbus_en0,	0xb5c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(usb_vbus_en1,	0xb60,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(wifi_en,		0xb64,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(wifi_rst,		0xb68,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(wifi_wake_ap,	0xb6c,	12,	5,	20,	5,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(qspi_comp_control,	0xb70,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1),
	DRV_PINGROUP(qspi_lpbk_control,	0xb78,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1),
};

static int tegra210_pinctrl_suspend(u32 *pg_data)
{
	int i;
	u32 *ctx = pg_data;

	/* Save current Pinmux register data */
	for (i = 0; i <  ARRAY_SIZE(tegra210_groups); i++) {
		if (tegra210_groups[i].drv_reg < 0)
			*ctx++ =
				tegra_pinctrl_readl(tegra210_groups[i].mux_bank,
					tegra210_groups[i].mux_reg);
		else
			*ctx++ =
				tegra_pinctrl_readl(tegra210_groups[i].drv_bank,
					tegra210_groups[i].drv_reg);
	}
	return 0;
}

static void tegra210_pinctrl_resume(u32 *pg_data)
{
	int i;
	u32 *ctx = pg_data;
	u32 reg_value;

	/* Restore Pinmux register data.
	 * PARK bit cleared as part of this.
	 */
	for (i = 0; i <  ARRAY_SIZE(tegra210_groups); i++) {
		if (tegra210_groups[i].drv_reg < 0)
			tegra_pinctrl_writel(*ctx++,
				tegra210_groups[i].mux_bank,
				tegra210_groups[i].mux_reg);
		else
			tegra_pinctrl_writel(*ctx++,
				tegra210_groups[i].drv_bank,
				tegra210_groups[i].drv_reg);
	}

	/* Clear parking bit for EMMC IO brick pads */
	reg_value = tegra_pinctrl_readl(DRV_BANK, EMMC2_PAD_CFGPADCTRL_OFFSET);
	reg_value &= ~(EMMC_DPD_PARKING(PARKING_SET));
	tegra_pinctrl_writel(reg_value, DRV_BANK, EMMC2_PAD_CFGPADCTRL_OFFSET);

	reg_value = tegra_pinctrl_readl(DRV_BANK, EMMC4_PAD_CFGPADCTRL_OFFSET);
	reg_value &= ~(EMMC_DPD_PARKING(PARKING_SET));
	tegra_pinctrl_writel(reg_value, DRV_BANK, EMMC4_PAD_CFGPADCTRL_OFFSET);

	tegra_pmc_enable_wake_det(false);
}

static int tegra210_gpio_request_enable(unsigned pin)
{
	/* Clear E_LPBK for SDMMC1_CLK and SDMMC3_CLK pads
	 * before using them as GPIOs
	 */
	switch (pin) {
	case TEGRA_PIN_SDMMC1_CLK_PM0:
		tegra_pinctrl_writel(0, 0, SDMMC1_CLK_LPBK_CTRL_OFFSET);
		break;
	case TEGRA_PIN_SDMMC3_CLK_PP0:
		tegra_pinctrl_writel(0, 0, SDMMC3_CLK_LPBK_CTRL_OFFSET);
		break;
	default:
		return 0;
	}

	return 0;
}

static const struct tegra_pinctrl_soc_data tegra210_pinctrl = {
	.ngpios = NUM_GPIOS,
	.pins = tegra210_pins,
	.npins = ARRAY_SIZE(tegra210_pins),
	.functions = tegra210_functions,
	.nfunctions = ARRAY_SIZE(tegra210_functions),
	.groups = tegra210_groups,
	.ngroups = ARRAY_SIZE(tegra210_groups),
	.suspend = tegra210_pinctrl_suspend,
	.resume = tegra210_pinctrl_resume,
	.gpio_request_enable = tegra210_gpio_request_enable,
	.hsm_in_mux = true,
	.schmitt_in_mux = true,
	.drvtype_in_mux = true,
};

static int tegra210_pinctrl_probe(struct platform_device *pdev)
{
	return tegra_pinctrl_probe(pdev, &tegra210_pinctrl);
}

static struct of_device_id tegra210_pinctrl_of_match[] = {
	{ .compatible = "nvidia,tegra210-pinmux", },
	{ },
};
MODULE_DEVICE_TABLE(of, tegra210_pinctrl_of_match);

static struct platform_driver tegra210_pinctrl_driver = {
	.driver = {
		.name = "tegra210-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = tegra210_pinctrl_of_match,
	},
	.probe = tegra210_pinctrl_probe,
	.remove = tegra_pinctrl_remove,
};

static int __init tegra210_pinctrl_init(void)
{
	return platform_driver_register(&tegra210_pinctrl_driver);
}
postcore_initcall_sync(tegra210_pinctrl_init);

static void __exit tegra210_pinctrl_exit(void)
{
	platform_driver_unregister(&tegra210_pinctrl_driver);
}
module_exit(tegra210_pinctrl_exit);

MODULE_AUTHOR("Shravani Dingari <shravanid@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra210 pinctrl driver");
MODULE_LICENSE("GPL v2");
