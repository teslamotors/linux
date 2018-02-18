/*
 * Pinctrl data for the NVIDIA Tegra186 pinmux
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include "pinctrl-tegra.h"

/*
 * Most pins affected by the pinmux can also be GPIOs. Define these first.
 * These must match how the GPIO driver names/numbers its pins.
 */
#define _GPIO(offset)				(offset)

#define TEGRA_PIN_PEX_L0_RST_N_PA0                    _GPIO(0)
#define TEGRA_PIN_PEX_L0_CLKREQ_N_PA1                    _GPIO(1)
#define TEGRA_PIN_PEX_WAKE_N_PA2                    _GPIO(2)
#define TEGRA_PIN_PEX_L1_RST_N_PA3                    _GPIO(3)
#define TEGRA_PIN_PEX_L1_CLKREQ_N_PA4                    _GPIO(4)
#define TEGRA_PIN_PEX_L2_RST_N_PA5                    _GPIO(5)
#define TEGRA_PIN_PEX_L2_CLKREQ_N_PA6                    _GPIO(6)
#define TEGRA_PIN_UART4_TX_PB0                    _GPIO(8)
#define TEGRA_PIN_UART4_RX_PB1                    _GPIO(9)
#define TEGRA_PIN_UART4_RTS_PB2                    _GPIO(10)
#define TEGRA_PIN_UART4_CTS_PB3                    _GPIO(11)
#define TEGRA_PIN_GPIO_WAN1_PB4                    _GPIO(12)
#define TEGRA_PIN_GPIO_WAN2_PB5                    _GPIO(13)
#define TEGRA_PIN_GPIO_WAN3_PB6                    _GPIO(14)
#define TEGRA_PIN_GPIO_WAN4_PC0                    _GPIO(16)
#define TEGRA_PIN_DAP2_SCLK_PC1                    _GPIO(17)
#define TEGRA_PIN_DAP2_DOUT_PC2                    _GPIO(18)
#define TEGRA_PIN_DAP2_DIN_PC3                    _GPIO(19)
#define TEGRA_PIN_DAP2_FS_PC4                    _GPIO(20)
#define TEGRA_PIN_GEN1_I2C_SCL_PC5                    _GPIO(21)
#define TEGRA_PIN_GEN1_I2C_SDA_PC6                    _GPIO(22)
#define TEGRA_PIN_SDMMC1_CLK_PD0                    _GPIO(24)
#define TEGRA_PIN_SDMMC1_CMD_PD1                    _GPIO(25)
#define TEGRA_PIN_SDMMC1_DAT0_PD2                    _GPIO(26)
#define TEGRA_PIN_SDMMC1_DAT1_PD3                    _GPIO(27)
#define TEGRA_PIN_SDMMC1_DAT2_PD4                    _GPIO(28)
#define TEGRA_PIN_SDMMC1_DAT3_PD5                    _GPIO(29)
#define TEGRA_PIN_EQOS_TXC_PE0                    _GPIO(32)
#define TEGRA_PIN_EQOS_TD0_PE1                    _GPIO(33)
#define TEGRA_PIN_EQOS_TD1_PE2                    _GPIO(34)
#define TEGRA_PIN_EQOS_TD2_PE3                    _GPIO(35)
#define TEGRA_PIN_EQOS_TD3_PE4                    _GPIO(36)
#define TEGRA_PIN_EQOS_TX_CTL_PE5                    _GPIO(37)
#define TEGRA_PIN_EQOS_RD0_PE6                    _GPIO(38)
#define TEGRA_PIN_EQOS_RD1_PE7                    _GPIO(39)
#define TEGRA_PIN_EQOS_RD2_PF0                    _GPIO(40)
#define TEGRA_PIN_EQOS_RD3_PF1                    _GPIO(41)
#define TEGRA_PIN_EQOS_RX_CTL_PF2                    _GPIO(42)
#define TEGRA_PIN_EQOS_RXC_PF3                    _GPIO(43)
#define TEGRA_PIN_EQOS_MDIO_PF4                    _GPIO(44)
#define TEGRA_PIN_EQOS_MDC_PF5                    _GPIO(45)
#define TEGRA_PIN_SDMMC3_CLK_PG0                    _GPIO(48)
#define TEGRA_PIN_SDMMC3_CMD_PG1                    _GPIO(49)
#define TEGRA_PIN_SDMMC3_DAT0_PG2                    _GPIO(50)
#define TEGRA_PIN_SDMMC3_DAT1_PG3                    _GPIO(51)
#define TEGRA_PIN_SDMMC3_DAT2_PG4                    _GPIO(52)
#define TEGRA_PIN_SDMMC3_DAT3_PG5                    _GPIO(53)
#define TEGRA_PIN_GPIO_WAN5_PH0                    _GPIO(56)
#define TEGRA_PIN_GPIO_WAN6_PH1                    _GPIO(57)
#define TEGRA_PIN_GPIO_WAN7_PH2                    _GPIO(58)
#define TEGRA_PIN_GPIO_WAN8_PH3                    _GPIO(59)
#define TEGRA_PIN_BCPU_PWR_REQ_PH4                    _GPIO(60)
#define TEGRA_PIN_MCPU_PWR_REQ_PH5                    _GPIO(61)
#define TEGRA_PIN_GPU_PWR_REQ_PH6                    _GPIO(62)
#define TEGRA_PIN_GPIO_PQ0_PI0                    _GPIO(64)
#define TEGRA_PIN_GPIO_PQ1_PI1                    _GPIO(65)
#define TEGRA_PIN_GPIO_PQ2_PI2                    _GPIO(66)
#define TEGRA_PIN_GPIO_PQ3_PI3                    _GPIO(67)
#define TEGRA_PIN_GPIO_PQ4_PI4                    _GPIO(68)
#define TEGRA_PIN_GPIO_PQ5_PI5                    _GPIO(69)
#define TEGRA_PIN_GPIO_PQ6_PI6                    _GPIO(70)
#define TEGRA_PIN_GPIO_PQ7_PI7                    _GPIO(71)
#define TEGRA_PIN_DAP1_SCLK_PJ0                    _GPIO(72)
#define TEGRA_PIN_DAP1_DOUT_PJ1                    _GPIO(73)
#define TEGRA_PIN_DAP1_DIN_PJ2                    _GPIO(74)
#define TEGRA_PIN_DAP1_FS_PJ3                    _GPIO(75)
#define TEGRA_PIN_AUD_MCLK_PJ4                    _GPIO(76)
#define TEGRA_PIN_GPIO_AUD0_PJ5                    _GPIO(77)
#define TEGRA_PIN_GPIO_AUD1_PJ6                    _GPIO(78)
#define TEGRA_PIN_GPIO_AUD2_PJ7                    _GPIO(79)
#define TEGRA_PIN_GPIO_AUD3_PK0                    _GPIO(80)
#define TEGRA_PIN_GEN7_I2C_SCL_PL0                    _GPIO(88)
#define TEGRA_PIN_GEN7_I2C_SDA_PL1                    _GPIO(89)
#define TEGRA_PIN_GEN9_I2C_SCL_PL2                    _GPIO(90)
#define TEGRA_PIN_GEN9_I2C_SDA_PL3                    _GPIO(91)
#define TEGRA_PIN_USB_VBUS_EN0_PL4                    _GPIO(92)
#define TEGRA_PIN_USB_VBUS_EN1_PL5                    _GPIO(93)
#define TEGRA_PIN_GP_PWM6_PL6                    _GPIO(94)
#define TEGRA_PIN_GP_PWM7_PL7                    _GPIO(95)
#define TEGRA_PIN_DMIC1_DAT_PM0                    _GPIO(96)
#define TEGRA_PIN_DMIC1_CLK_PM1                    _GPIO(97)
#define TEGRA_PIN_DMIC2_DAT_PM2                    _GPIO(98)
#define TEGRA_PIN_DMIC2_CLK_PM3                    _GPIO(99)
#define TEGRA_PIN_DMIC4_DAT_PM4                    _GPIO(100)
#define TEGRA_PIN_DMIC4_CLK_PM5                    _GPIO(101)
#define TEGRA_PIN_GPIO_CAM1_PN0                    _GPIO(104)
#define TEGRA_PIN_GPIO_CAM2_PN1                    _GPIO(105)
#define TEGRA_PIN_GPIO_CAM3_PN2                    _GPIO(106)
#define TEGRA_PIN_GPIO_CAM4_PN3                    _GPIO(107)
#define TEGRA_PIN_GPIO_CAM5_PN4                    _GPIO(108)
#define TEGRA_PIN_GPIO_CAM6_PN5                    _GPIO(109)
#define TEGRA_PIN_GPIO_CAM7_PN6                    _GPIO(110)
#define TEGRA_PIN_EXTPERIPH1_CLK_PO0                    _GPIO(112)
#define TEGRA_PIN_EXTPERIPH2_CLK_PO1                    _GPIO(113)
#define TEGRA_PIN_CAM_I2C_SCL_PO2                    _GPIO(114)
#define TEGRA_PIN_CAM_I2C_SDA_PO3                    _GPIO(115)
#define TEGRA_PIN_DP_AUX_CH0_HPD_PP0                    _GPIO(120)
#define TEGRA_PIN_DP_AUX_CH1_HPD_PP1                    _GPIO(121)
#define TEGRA_PIN_HDMI_CEC_PP2                    _GPIO(122)
#define TEGRA_PIN_GPIO_EDP0_PP3                    _GPIO(123)
#define TEGRA_PIN_GPIO_EDP1_PP4                    _GPIO(124)
#define TEGRA_PIN_GPIO_EDP2_PP5                    _GPIO(125)
#define TEGRA_PIN_GPIO_EDP3_PP6                    _GPIO(126)
#define TEGRA_PIN_DIRECTDC1_CLK_PQ0                    _GPIO(128)
#define TEGRA_PIN_DIRECTDC1_IN_PQ1                    _GPIO(129)
#define TEGRA_PIN_DIRECTDC1_OUT0_PQ2                    _GPIO(130)
#define TEGRA_PIN_DIRECTDC1_OUT1_PQ3                    _GPIO(131)
#define TEGRA_PIN_DIRECTDC1_OUT2_PQ4                    _GPIO(132)
#define TEGRA_PIN_DIRECTDC1_OUT3_PQ5                    _GPIO(133)
#define TEGRA_PIN_QSPI_SCK_PR0                    _GPIO(136)
#define TEGRA_PIN_QSPI_IO0_PR1                    _GPIO(137)
#define TEGRA_PIN_QSPI_IO1_PR2                    _GPIO(138)
#define TEGRA_PIN_QSPI_IO2_PR3                    _GPIO(139)
#define TEGRA_PIN_QSPI_IO3_PR4                    _GPIO(140)
#define TEGRA_PIN_QSPI_CS_N_PR5                    _GPIO(141)
#define TEGRA_PIN_PWR_I2C_SCL_PS0                    _GPIO(144)
#define TEGRA_PIN_PWR_I2C_SDA_PS1                    _GPIO(145)
#define TEGRA_PIN_BATT_OC_PS2                    _GPIO(146)
#define TEGRA_PIN_SAFE_STATE_PS3                    _GPIO(147)
#define TEGRA_PIN_VCOMP_ALERT_PS4                    _GPIO(148)
#define TEGRA_PIN_UART1_TX_PT0                    _GPIO(152)
#define TEGRA_PIN_UART1_RX_PT1                    _GPIO(153)
#define TEGRA_PIN_UART1_RTS_PT2                    _GPIO(154)
#define TEGRA_PIN_UART1_CTS_PT3                    _GPIO(155)
#define TEGRA_PIN_GPIO_DIS0_PU0                    _GPIO(160)
#define TEGRA_PIN_GPIO_DIS1_PU1                    _GPIO(161)
#define TEGRA_PIN_GPIO_DIS2_PU2                    _GPIO(162)
#define TEGRA_PIN_GPIO_DIS3_PU3                    _GPIO(163)
#define TEGRA_PIN_GPIO_DIS4_PU4                    _GPIO(164)
#define TEGRA_PIN_GPIO_DIS5_PU5                    _GPIO(165)
#define TEGRA_PIN_GPIO_SEN0_PV0                    _GPIO(168)
#define TEGRA_PIN_GPIO_SEN1_PV1                    _GPIO(169)
#define TEGRA_PIN_GPIO_SEN2_PV2                    _GPIO(170)
#define TEGRA_PIN_GPIO_SEN3_PV3                    _GPIO(171)
#define TEGRA_PIN_GPIO_SEN4_PV4                    _GPIO(172)
#define TEGRA_PIN_GPIO_SEN5_PV5                    _GPIO(173)
#define TEGRA_PIN_GPIO_SEN6_PV6                    _GPIO(174)
#define TEGRA_PIN_GPIO_SEN7_PV7                    _GPIO(175)
#define TEGRA_PIN_GEN8_I2C_SCL_PW0                    _GPIO(176)
#define TEGRA_PIN_GEN8_I2C_SDA_PW1                    _GPIO(177)
#define TEGRA_PIN_UART3_TX_PW2                    _GPIO(178)
#define TEGRA_PIN_UART3_RX_PW3                    _GPIO(179)
#define TEGRA_PIN_UART3_RTS_PW4                    _GPIO(180)
#define TEGRA_PIN_UART3_CTS_PW5                    _GPIO(181)
#define TEGRA_PIN_UART7_TX_PW6                    _GPIO(182)
#define TEGRA_PIN_UART7_RX_PW7                    _GPIO(183)
#define TEGRA_PIN_UART2_TX_PX0                    _GPIO(184)
#define TEGRA_PIN_UART2_RX_PX1                    _GPIO(185)
#define TEGRA_PIN_UART2_RTS_PX2                    _GPIO(186)
#define TEGRA_PIN_UART2_CTS_PX3                    _GPIO(187)
#define TEGRA_PIN_UART5_TX_PX4                    _GPIO(188)
#define TEGRA_PIN_UART5_RX_PX5                    _GPIO(189)
#define TEGRA_PIN_UART5_RTS_PX6                    _GPIO(190)
#define TEGRA_PIN_UART5_CTS_PX7                    _GPIO(191)
#define TEGRA_PIN_GPIO_MDM1_PY0                    _GPIO(192)
#define TEGRA_PIN_GPIO_MDM2_PY1                    _GPIO(193)
#define TEGRA_PIN_GPIO_MDM3_PY2                    _GPIO(194)
#define TEGRA_PIN_GPIO_MDM4_PY3                    _GPIO(195)
#define TEGRA_PIN_GPIO_MDM5_PY4                    _GPIO(196)
#define TEGRA_PIN_GPIO_MDM6_PY5                    _GPIO(197)
#define TEGRA_PIN_GPIO_MDM7_PY6                    _GPIO(198)
#define TEGRA_PIN_CAN1_DOUT_PZ0                    _GPIO(200)
#define TEGRA_PIN_CAN1_DIN_PZ1                    _GPIO(201)
#define TEGRA_PIN_CAN0_DOUT_PZ2                    _GPIO(202)
#define TEGRA_PIN_CAN0_DIN_PZ3                    _GPIO(203)
#define TEGRA_PIN_CAN_GPIO0_PAA0                    _GPIO(208)
#define TEGRA_PIN_CAN_GPIO1_PAA1                    _GPIO(209)
#define TEGRA_PIN_CAN_GPIO2_PAA2                    _GPIO(210)
#define TEGRA_PIN_CAN_GPIO3_PAA3                    _GPIO(211)
#define TEGRA_PIN_CAN_GPIO4_PAA4                    _GPIO(212)
#define TEGRA_PIN_CAN_GPIO5_PAA5                    _GPIO(213)
#define TEGRA_PIN_CAN_GPIO6_PAA6                    _GPIO(214)
#define TEGRA_PIN_CAN_GPIO7_PAA7                    _GPIO(215)
#define TEGRA_PIN_UFS0_REF_CLK_PBB0                    _GPIO(216)
#define TEGRA_PIN_UFS0_RST_PBB1                    _GPIO(217)
#define TEGRA_PIN_DAP4_SCLK_PCC0                    _GPIO(224)
#define TEGRA_PIN_DAP4_DOUT_PCC1                    _GPIO(225)
#define TEGRA_PIN_DAP4_DIN_PCC2                    _GPIO(226)
#define TEGRA_PIN_DAP4_FS_PCC3                    _GPIO(227)
#define TEGRA_PIN_GPIO_SEN8_PEE0                    _GPIO(240)
#define TEGRA_PIN_GPIO_SEN9_PEE1                    _GPIO(241)
#define TEGRA_PIN_TOUCH_CLK_PEE2                    _GPIO(242)
#define TEGRA_PIN_POWER_ON_PFF0                    _GPIO(248)
#define TEGRA_PIN_GPIO_SW1_PFF1                    _GPIO(249)
#define TEGRA_PIN_GPIO_SW2_PFF2                    _GPIO(250)
#define TEGRA_PIN_GPIO_SW3_PFF3                    _GPIO(251)
#define TEGRA_PIN_GPIO_SW4_PFF4                    _GPIO(252)

/* All non-GPIO pins follow */
#define NUM_GPIOS	(TEGRA_PIN_GPIO_SW4_PFF4 + 1)
#define _PIN(offset)	(NUM_GPIOS + (offset))

/* Non-GPIO pins */
#define TEGRA_PIN_DIRECTDC_COMP 		_PIN(0)
#define TEGRA_PIN_SDMMC1_COMP		_PIN(1)
#define TEGRA_PIN_EQOS_COMP			_PIN(2)
#define TEGRA_PIN_SDMMC3_COMP		_PIN(3)
#define TEGRA_PIN_QSPI_COMP			_PIN(4)
#define TEGRA_PIN_SHUTDOWN			_PIN(5)
#define TEGRA_PIN_PMU_INT				_PIN(6)
#define TEGRA_PIN_SOC_PWR_REQ			_PIN(7)
#define TEGRA_PIN_CLK_32K_IN			_PIN(8)

#define DRV_BANK	0
#define MUX_BANK	1

static const struct pinctrl_pin_desc  tegra186_pins[] = {
	PINCTRL_PIN(TEGRA_PIN_PEX_L0_RST_N_PA0, "PEX_L0_RST_N_PA0"),
	PINCTRL_PIN(TEGRA_PIN_PEX_L0_CLKREQ_N_PA1, "PEX_L0_CLKREQ_N_PA1"),
	PINCTRL_PIN(TEGRA_PIN_PEX_WAKE_N_PA2, "PEX_WAKE_N_PA2"),
	PINCTRL_PIN(TEGRA_PIN_PEX_L1_RST_N_PA3, "PEX_L1_RST_N_PA3"),
	PINCTRL_PIN(TEGRA_PIN_PEX_L1_CLKREQ_N_PA4, "PEX_L1_CLKREQ_N_PA4"),
	PINCTRL_PIN(TEGRA_PIN_PEX_L2_RST_N_PA5, "PEX_L2_RST_N_PA5"),
	PINCTRL_PIN(TEGRA_PIN_PEX_L2_CLKREQ_N_PA6, "PEX_L2_CLKREQ_N_PA6"),
	PINCTRL_PIN(TEGRA_PIN_UART4_TX_PB0, "UART4_TX_PB0"),
	PINCTRL_PIN(TEGRA_PIN_UART4_RX_PB1, "UART4_RX_PB1"),
	PINCTRL_PIN(TEGRA_PIN_UART4_RTS_PB2, "UART4_RTS_PB2"),
	PINCTRL_PIN(TEGRA_PIN_UART4_CTS_PB3, "UART4_CTS_PB3"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_WAN1_PB4, "GPIO_WAN1_PB4"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_WAN2_PB5, "GPIO_WAN2_PB5"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_WAN3_PB6, "GPIO_WAN3_PB6"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_WAN4_PC0, "GPIO_WAN4_PC0"),
	PINCTRL_PIN(TEGRA_PIN_DAP2_SCLK_PC1, "DAP2_SCLK_PC1"),
	PINCTRL_PIN(TEGRA_PIN_DAP2_DOUT_PC2, "DAP2_DOUT_PC2"),
	PINCTRL_PIN(TEGRA_PIN_DAP2_DIN_PC3, "DAP2_DIN_PC3"),
	PINCTRL_PIN(TEGRA_PIN_DAP2_FS_PC4, "DAP2_FS_PC4"),
	PINCTRL_PIN(TEGRA_PIN_GEN1_I2C_SCL_PC5, "GEN1_I2C_SCL_PC5"),
	PINCTRL_PIN(TEGRA_PIN_GEN1_I2C_SDA_PC6, "GEN1_I2C_SDA_PC6"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC1_CLK_PD0, "SDMMC1_CLK_PD0"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC1_CMD_PD1, "SDMMC1_CMD_PD1"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC1_DAT0_PD2, "SDMMC1_DAT0_PD2"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC1_DAT1_PD3, "SDMMC1_DAT1_PD3"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC1_DAT2_PD4, "SDMMC1_DAT2_PD4"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC1_DAT3_PD5, "SDMMC1_DAT3_PD5"),
	PINCTRL_PIN(TEGRA_PIN_EQOS_TXC_PE0, "EQOS_TXC_PE0"),
	PINCTRL_PIN(TEGRA_PIN_EQOS_TD0_PE1, "EQOS_TD0_PE1"),
	PINCTRL_PIN(TEGRA_PIN_EQOS_TD1_PE2, "EQOS_TD1_PE2"),
	PINCTRL_PIN(TEGRA_PIN_EQOS_TD2_PE3, "EQOS_TD2_PE3"),
	PINCTRL_PIN(TEGRA_PIN_EQOS_TD3_PE4, "EQOS_TD3_PE4"),
	PINCTRL_PIN(TEGRA_PIN_EQOS_TX_CTL_PE5, "EQOS_TX_CTL_PE5"),
	PINCTRL_PIN(TEGRA_PIN_EQOS_RD0_PE6, "EQOS_RD0_PE6"),
	PINCTRL_PIN(TEGRA_PIN_EQOS_RD1_PE7, "EQOS_RD1_PE7"),
	PINCTRL_PIN(TEGRA_PIN_EQOS_RD2_PF0, "EQOS_RD2_PF0"),
	PINCTRL_PIN(TEGRA_PIN_EQOS_RD3_PF1, "EQOS_RD3_PF1"),
	PINCTRL_PIN(TEGRA_PIN_EQOS_RX_CTL_PF2, "EQOS_RX_CTL_PF2"),
	PINCTRL_PIN(TEGRA_PIN_EQOS_RXC_PF3, "EQOS_RXC_PF3"),
	PINCTRL_PIN(TEGRA_PIN_EQOS_MDIO_PF4, "EQOS_MDIO_PF4"),
	PINCTRL_PIN(TEGRA_PIN_EQOS_MDC_PF5, "EQOS_MDC_PF5"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC3_CLK_PG0, "SDMMC3_CLK_PG0"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC3_CMD_PG1, "SDMMC3_CMD_PG1"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC3_DAT0_PG2, "SDMMC3_DAT0_PG2"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC3_DAT1_PG3, "SDMMC3_DAT1_PG3"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC3_DAT2_PG4, "SDMMC3_DAT2_PG4"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC3_DAT3_PG5, "SDMMC3_DAT3_PG5"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_WAN5_PH0, "GPIO_WAN5_PH0"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_WAN6_PH1, "GPIO_WAN6_PH1"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_WAN7_PH2, "GPIO_WAN7_PH2"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_WAN8_PH3, "GPIO_WAN8_PH3"),
	PINCTRL_PIN(TEGRA_PIN_BCPU_PWR_REQ_PH4, "BCPU_PWR_REQ_PH4"),
	PINCTRL_PIN(TEGRA_PIN_MCPU_PWR_REQ_PH5, "MCPU_PWR_REQ_PH5"),
	PINCTRL_PIN(TEGRA_PIN_GPU_PWR_REQ_PH6, "GPU_PWR_REQ_PH6"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_PQ0_PI0, "GPIO_PQ0_PI0"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_PQ1_PI1, "GPIO_PQ1_PI1"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_PQ2_PI2, "GPIO_PQ2_PI2"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_PQ3_PI3, "GPIO_PQ3_PI3"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_PQ4_PI4, "GPIO_PQ4_PI4"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_PQ5_PI5, "GPIO_PQ5_PI5"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_PQ6_PI6, "GPIO_PQ6_PI6"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_PQ7_PI7, "GPIO_PQ7_PI7"),
	PINCTRL_PIN(TEGRA_PIN_DAP1_SCLK_PJ0, "DAP1_SCLK_PJ0"),
	PINCTRL_PIN(TEGRA_PIN_DAP1_DOUT_PJ1, "DAP1_DOUT_PJ1"),
	PINCTRL_PIN(TEGRA_PIN_DAP1_DIN_PJ2, "DAP1_DIN_PJ2"),
	PINCTRL_PIN(TEGRA_PIN_DAP1_FS_PJ3, "DAP1_FS_PJ3"),
	PINCTRL_PIN(TEGRA_PIN_AUD_MCLK_PJ4, "AUD_MCLK_PJ4"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_AUD0_PJ5, "GPIO_AUD0_PJ5"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_AUD1_PJ6, "GPIO_AUD1_PJ6"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_AUD2_PJ7, "GPIO_AUD2_PJ7"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_AUD3_PK0, "GPIO_AUD3_PK0"),
	PINCTRL_PIN(TEGRA_PIN_GEN7_I2C_SCL_PL0, "GEN7_I2C_SCL_PL0"),
	PINCTRL_PIN(TEGRA_PIN_GEN7_I2C_SDA_PL1, "GEN7_I2C_SDA_PL1"),
	PINCTRL_PIN(TEGRA_PIN_GEN9_I2C_SCL_PL2, "GEN9_I2C_SCL_PL2"),
	PINCTRL_PIN(TEGRA_PIN_GEN9_I2C_SDA_PL3, "GEN9_I2C_SDA_PL3"),
	PINCTRL_PIN(TEGRA_PIN_USB_VBUS_EN0_PL4, "USB_VBUS_EN0_PL4"),
	PINCTRL_PIN(TEGRA_PIN_USB_VBUS_EN1_PL5, "USB_VBUS_EN1_PL5"),
	PINCTRL_PIN(TEGRA_PIN_GP_PWM6_PL6, "GP_PWM6_PL6"),
	PINCTRL_PIN(TEGRA_PIN_GP_PWM7_PL7, "GP_PWM7_PL7"),
	PINCTRL_PIN(TEGRA_PIN_DMIC1_DAT_PM0, "DMIC1_DAT_PM0"),
	PINCTRL_PIN(TEGRA_PIN_DMIC1_CLK_PM1, "DMIC1_CLK_PM1"),
	PINCTRL_PIN(TEGRA_PIN_DMIC2_DAT_PM2, "DMIC2_DAT_PM2"),
	PINCTRL_PIN(TEGRA_PIN_DMIC2_CLK_PM3, "DMIC2_CLK_PM3"),
	PINCTRL_PIN(TEGRA_PIN_DMIC4_DAT_PM4, "DMIC4_DAT_PM4"),
	PINCTRL_PIN(TEGRA_PIN_DMIC4_CLK_PM5, "DMIC4_CLK_PM5"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_CAM1_PN0, "GPIO_CAM1_PN0"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_CAM2_PN1, "GPIO_CAM2_PN1"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_CAM3_PN2, "GPIO_CAM3_PN2"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_CAM4_PN3, "GPIO_CAM4_PN3"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_CAM5_PN4, "GPIO_CAM5_PN4"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_CAM6_PN5, "GPIO_CAM6_PN5"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_CAM7_PN6, "GPIO_CAM7_PN6"),
	PINCTRL_PIN(TEGRA_PIN_EXTPERIPH1_CLK_PO0, "EXTPERIPH1_CLK_PO0"),
	PINCTRL_PIN(TEGRA_PIN_EXTPERIPH2_CLK_PO1, "EXTPERIPH2_CLK_PO1"),
	PINCTRL_PIN(TEGRA_PIN_CAM_I2C_SCL_PO2, "CAM_I2C_SCL_PO2"),
	PINCTRL_PIN(TEGRA_PIN_CAM_I2C_SDA_PO3, "CAM_I2C_SDA_PO3"),
	PINCTRL_PIN(TEGRA_PIN_DP_AUX_CH0_HPD_PP0, "DP_AUX_CH0_HPD_PP0"),
	PINCTRL_PIN(TEGRA_PIN_DP_AUX_CH1_HPD_PP1, "DP_AUX_CH1_HPD_PP1"),
	PINCTRL_PIN(TEGRA_PIN_HDMI_CEC_PP2, "HDMI_CEC_PP2"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_EDP0_PP3, "GPIO_EDP0_PP3"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_EDP1_PP4, "GPIO_EDP1_PP4"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_EDP2_PP5, "GPIO_EDP2_PP5"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_EDP3_PP6, "GPIO_EDP3_PP6"),
	PINCTRL_PIN(TEGRA_PIN_DIRECTDC1_CLK_PQ0, "DIRECTDC1_CLK_PQ0"),
	PINCTRL_PIN(TEGRA_PIN_DIRECTDC1_IN_PQ1, "DIRECTDC1_IN_PQ1"),
	PINCTRL_PIN(TEGRA_PIN_DIRECTDC1_OUT0_PQ2, "DIRECTDC1_OUT0_PQ2"),
	PINCTRL_PIN(TEGRA_PIN_DIRECTDC1_OUT1_PQ3, "DIRECTDC1_OUT1_PQ3"),
	PINCTRL_PIN(TEGRA_PIN_DIRECTDC1_OUT2_PQ4, "DIRECTDC1_OUT2_PQ4"),
	PINCTRL_PIN(TEGRA_PIN_DIRECTDC1_OUT3_PQ5, "DIRECTDC1_OUT3_PQ5"),
	PINCTRL_PIN(TEGRA_PIN_QSPI_SCK_PR0, "QSPI_SCK_PR0"),
	PINCTRL_PIN(TEGRA_PIN_QSPI_IO0_PR1, "QSPI_IO0_PR1"),
	PINCTRL_PIN(TEGRA_PIN_QSPI_IO1_PR2, "QSPI_IO1_PR2"),
	PINCTRL_PIN(TEGRA_PIN_QSPI_IO2_PR3, "QSPI_IO2_PR3"),
	PINCTRL_PIN(TEGRA_PIN_QSPI_IO3_PR4, "QSPI_IO3_PR4"),
	PINCTRL_PIN(TEGRA_PIN_QSPI_CS_N_PR5, "QSPI_CS_N_PR5"),
	PINCTRL_PIN(TEGRA_PIN_PWR_I2C_SCL_PS0, "PWR_I2C_SCL_PS0"),
	PINCTRL_PIN(TEGRA_PIN_PWR_I2C_SDA_PS1, "PWR_I2C_SDA_PS1"),
	PINCTRL_PIN(TEGRA_PIN_BATT_OC_PS2, "BATT_OC_PS2"),
	PINCTRL_PIN(TEGRA_PIN_SAFE_STATE_PS3, "SAFE_STATE_PS3"),
	PINCTRL_PIN(TEGRA_PIN_VCOMP_ALERT_PS4, "VCOMP_ALERT_PS4"),
	PINCTRL_PIN(TEGRA_PIN_UART1_TX_PT0, "UART1_TX_PT0"),
	PINCTRL_PIN(TEGRA_PIN_UART1_RX_PT1, "UART1_RX_PT1"),
	PINCTRL_PIN(TEGRA_PIN_UART1_RTS_PT2, "UART1_RTS_PT2"),
	PINCTRL_PIN(TEGRA_PIN_UART1_CTS_PT3, "UART1_CTS_PT3"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_DIS0_PU0, "GPIO_DIS0_PU0"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_DIS1_PU1, "GPIO_DIS1_PU1"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_DIS2_PU2, "GPIO_DIS2_PU2"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_DIS3_PU3, "GPIO_DIS3_PU3"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_DIS4_PU4, "GPIO_DIS4_PU4"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_DIS5_PU5, "GPIO_DIS5_PU5"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_SEN0_PV0, "GPIO_SEN0_PV0"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_SEN1_PV1, "GPIO_SEN1_PV1"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_SEN2_PV2, "GPIO_SEN2_PV2"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_SEN3_PV3, "GPIO_SEN3_PV3"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_SEN4_PV4, "GPIO_SEN4_PV4"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_SEN5_PV5, "GPIO_SEN5_PV5"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_SEN6_PV6, "GPIO_SEN6_PV6"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_SEN7_PV7, "GPIO_SEN7_PV7"),
	PINCTRL_PIN(TEGRA_PIN_GEN8_I2C_SCL_PW0, "GEN8_I2C_SCL_PW0"),
	PINCTRL_PIN(TEGRA_PIN_GEN8_I2C_SDA_PW1, "GEN8_I2C_SDA_PW1"),
	PINCTRL_PIN(TEGRA_PIN_UART3_TX_PW2, "UART3_TX_PW2"),
	PINCTRL_PIN(TEGRA_PIN_UART3_RX_PW3, "UART3_RX_PW3"),
	PINCTRL_PIN(TEGRA_PIN_UART3_RTS_PW4, "UART3_RTS_PW4"),
	PINCTRL_PIN(TEGRA_PIN_UART3_CTS_PW5, "UART3_CTS_PW5"),
	PINCTRL_PIN(TEGRA_PIN_UART7_TX_PW6, "UART7_TX_PW6"),
	PINCTRL_PIN(TEGRA_PIN_UART7_RX_PW7, "UART7_RX_PW7"),
	PINCTRL_PIN(TEGRA_PIN_UART2_TX_PX0, "UART2_TX_PX0"),
	PINCTRL_PIN(TEGRA_PIN_UART2_RX_PX1, "UART2_RX_PX1"),
	PINCTRL_PIN(TEGRA_PIN_UART2_RTS_PX2, "UART2_RTS_PX2"),
	PINCTRL_PIN(TEGRA_PIN_UART2_CTS_PX3, "UART2_CTS_PX3"),
	PINCTRL_PIN(TEGRA_PIN_UART5_TX_PX4, "UART5_TX_PX4"),
	PINCTRL_PIN(TEGRA_PIN_UART5_RX_PX5, "UART5_RX_PX5"),
	PINCTRL_PIN(TEGRA_PIN_UART5_RTS_PX6, "UART5_RTS_PX6"),
	PINCTRL_PIN(TEGRA_PIN_UART5_CTS_PX7, "UART5_CTS_PX7"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_MDM1_PY0, "GPIO_MDM1_PY0"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_MDM2_PY1, "GPIO_MDM2_PY1"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_MDM3_PY2, "GPIO_MDM3_PY2"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_MDM4_PY3, "GPIO_MDM4_PY3"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_MDM5_PY4, "GPIO_MDM5_PY4"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_MDM6_PY5, "GPIO_MDM6_PY5"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_MDM7_PY6, "GPIO_MDM7_PY6"),
	PINCTRL_PIN(TEGRA_PIN_CAN1_DOUT_PZ0, "CAN1_DOUT_PZ0"),
	PINCTRL_PIN(TEGRA_PIN_CAN1_DIN_PZ1, "CAN1_DIN_PZ1"),
	PINCTRL_PIN(TEGRA_PIN_CAN0_DOUT_PZ2, "CAN0_DOUT_PZ2"),
	PINCTRL_PIN(TEGRA_PIN_CAN0_DIN_PZ3, "CAN0_DIN_PZ3"),
	PINCTRL_PIN(TEGRA_PIN_CAN_GPIO0_PAA0, "CAN_GPIO0_PAA0"),
	PINCTRL_PIN(TEGRA_PIN_CAN_GPIO1_PAA1, "CAN_GPIO1_PAA1"),
	PINCTRL_PIN(TEGRA_PIN_CAN_GPIO2_PAA2, "CAN_GPIO2_PAA2"),
	PINCTRL_PIN(TEGRA_PIN_CAN_GPIO3_PAA3, "CAN_GPIO3_PAA3"),
	PINCTRL_PIN(TEGRA_PIN_CAN_GPIO4_PAA4, "CAN_GPIO4_PAA4"),
	PINCTRL_PIN(TEGRA_PIN_CAN_GPIO5_PAA5, "CAN_GPIO5_PAA5"),
	PINCTRL_PIN(TEGRA_PIN_CAN_GPIO6_PAA6, "CAN_GPIO6_PAA6"),
	PINCTRL_PIN(TEGRA_PIN_CAN_GPIO7_PAA7, "CAN_GPIO7_PAA7"),
	PINCTRL_PIN(TEGRA_PIN_UFS0_REF_CLK_PBB0, "UFS0_REF_CLK_PBB0"),
	PINCTRL_PIN(TEGRA_PIN_UFS0_RST_PBB1, "UFS0_RST_PBB1"),
	PINCTRL_PIN(TEGRA_PIN_DAP4_SCLK_PCC0, "DAP4_SCLK_PCC0"),
	PINCTRL_PIN(TEGRA_PIN_DAP4_DOUT_PCC1, "DAP4_DOUT_PCC1"),
	PINCTRL_PIN(TEGRA_PIN_DAP4_DIN_PCC2, "DAP4_DIN_PCC2"),
	PINCTRL_PIN(TEGRA_PIN_DAP4_FS_PCC3, "DAP4_FS_PCC3"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_SEN8_PEE0, "GPIO_SEN8_PEE0"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_SEN9_PEE1, "GPIO_SEN9_PEE1"),
	PINCTRL_PIN(TEGRA_PIN_TOUCH_CLK_PEE2, "TOUCH_CLK_PEE2"),
	PINCTRL_PIN(TEGRA_PIN_POWER_ON_PFF0, "POWER_ON_PFF0"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_SW1_PFF1, "GPIO_SW1_PFF1"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_SW2_PFF2, "GPIO_SW2_PFF2"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_SW3_PFF3, "GPIO_SW3_PFF3"),
	PINCTRL_PIN(TEGRA_PIN_GPIO_SW4_PFF4, "GPIO_SW4_PFF4"),
	PINCTRL_PIN(TEGRA_PIN_DIRECTDC_COMP, "DIRECTDC_COMP"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC1_COMP,	"SDMMC1_COMP"),
	PINCTRL_PIN(TEGRA_PIN_EQOS_COMP, "EQOS_COMP"),
	PINCTRL_PIN(TEGRA_PIN_SDMMC3_COMP, "SDMMC3_COMP"),
	PINCTRL_PIN(TEGRA_PIN_QSPI_COMP, "QSPI_COMP"),
	PINCTRL_PIN(TEGRA_PIN_SHUTDOWN, "SHUTDOWN"),
	PINCTRL_PIN(TEGRA_PIN_PMU_INT, "PMU_INT"),
	PINCTRL_PIN(TEGRA_PIN_SOC_PWR_REQ, "SOC_PWR_REQ"),
	PINCTRL_PIN(TEGRA_PIN_CLK_32K_IN, "CLK_32K_IN"),

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

static const unsigned pex_l2_rst_n_pa5_pins[] = {
	TEGRA_PIN_PEX_L2_RST_N_PA5,
};

static const unsigned pex_l2_clkreq_n_pa6_pins[] = {
	TEGRA_PIN_PEX_L2_CLKREQ_N_PA6,
};

static const unsigned uart4_tx_pb0_pins[] = {
	TEGRA_PIN_UART4_TX_PB0,
};

static const unsigned uart4_rx_pb1_pins[] = {
	TEGRA_PIN_UART4_RX_PB1,
};

static const unsigned uart4_rts_pb2_pins[] = {
	TEGRA_PIN_UART4_RTS_PB2,
};

static const unsigned uart4_cts_pb3_pins[] = {
	TEGRA_PIN_UART4_CTS_PB3,
};

static const unsigned gpio_wan1_pb4_pins[] = {
	TEGRA_PIN_GPIO_WAN1_PB4,
};

static const unsigned gpio_wan2_pb5_pins[] = {
	TEGRA_PIN_GPIO_WAN2_PB5,
};

static const unsigned gpio_wan3_pb6_pins[] = {
	TEGRA_PIN_GPIO_WAN3_PB6,
};

static const unsigned gpio_wan4_pc0_pins[] = {
	TEGRA_PIN_GPIO_WAN4_PC0,
};

static const unsigned dap2_sclk_pc1_pins[] = {
	TEGRA_PIN_DAP2_SCLK_PC1,
};

static const unsigned dap2_dout_pc2_pins[] = {
	TEGRA_PIN_DAP2_DOUT_PC2,
};

static const unsigned dap2_din_pc3_pins[] = {
	TEGRA_PIN_DAP2_DIN_PC3,
};

static const unsigned dap2_fs_pc4_pins[] = {
	TEGRA_PIN_DAP2_FS_PC4,
};

static const unsigned gen1_i2c_scl_pc5_pins[] = {
	TEGRA_PIN_GEN1_I2C_SCL_PC5,
};

static const unsigned gen1_i2c_sda_pc6_pins[] = {
	TEGRA_PIN_GEN1_I2C_SDA_PC6,
};

static const unsigned sdmmc1_clk_pd0_pins[] = {
	TEGRA_PIN_SDMMC1_CLK_PD0,
};

static const unsigned sdmmc1_cmd_pd1_pins[] = {
	TEGRA_PIN_SDMMC1_CMD_PD1,
};

static const unsigned sdmmc1_comp_pins[] = {
	TEGRA_PIN_SDMMC1_COMP,
};

static const unsigned sdmmc1_dat0_pd2_pins[] = {
	TEGRA_PIN_SDMMC1_DAT0_PD2,
};

static const unsigned sdmmc1_dat1_pd3_pins[] = {
	TEGRA_PIN_SDMMC1_DAT1_PD3,
};

static const unsigned sdmmc1_dat2_pd4_pins[] = {
	TEGRA_PIN_SDMMC1_DAT2_PD4,
};

static const unsigned sdmmc1_dat3_pd5_pins[] = {
TEGRA_PIN_SDMMC1_DAT3_PD5,
};

static const unsigned eqos_txc_pe0_pins[] = {
	TEGRA_PIN_EQOS_TXC_PE0,
};

static const unsigned eqos_td0_pe1_pins[] = {
	TEGRA_PIN_EQOS_TD0_PE1,
};

static const unsigned eqos_td1_pe2_pins[] = {
	TEGRA_PIN_EQOS_TD1_PE2,
};

static const unsigned eqos_td2_pe3_pins[] = {
	TEGRA_PIN_EQOS_TD2_PE3,
};

static const unsigned eqos_td3_pe4_pins[] = {
	TEGRA_PIN_EQOS_TD3_PE4,
};

static const unsigned eqos_tx_ctl_pe5_pins[] = {
	TEGRA_PIN_EQOS_TX_CTL_PE5,
};

static const unsigned eqos_rd0_pe6_pins[] = {
	TEGRA_PIN_EQOS_RD0_PE6,
};

static const unsigned eqos_rd1_pe7_pins[] = {
	TEGRA_PIN_EQOS_RD1_PE7,
};

static const unsigned eqos_rd2_pf0_pins[] = {
	TEGRA_PIN_EQOS_RD2_PF0,
};

static const unsigned eqos_rd3_pf1_pins[] = {
	TEGRA_PIN_EQOS_RD3_PF1,
};

static const unsigned eqos_rx_ctl_pf2_pins[] = {
	TEGRA_PIN_EQOS_RX_CTL_PF2,
};

static const unsigned eqos_rxc_pf3_pins[] = {
	TEGRA_PIN_EQOS_RXC_PF3,
};

static const unsigned eqos_mdio_pf4_pins[] = {
	TEGRA_PIN_EQOS_MDIO_PF4,
};

static const unsigned eqos_mdc_pf5_pins[] = {
	TEGRA_PIN_EQOS_MDC_PF5,
};

static const unsigned eqos_comp_pins[] = {
	TEGRA_PIN_EQOS_COMP,
};

static const unsigned sdmmc4_clk_pins[] = {};

static const unsigned sdmmc4_cmd_pins[] = {};

static const unsigned sdmmc4_dqs_pins[] = {};

static const unsigned sdmmc4_dat7_pins[] = {};

static const unsigned sdmmc4_dat6_pins[] = {};

static const unsigned sdmmc4_dat5_pins[] = {};

static const unsigned sdmmc4_dat4_pins[] = {};

static const unsigned sdmmc4_dat3_pins[] = {};

static const unsigned sdmmc4_dat2_pins[] = {};

static const unsigned sdmmc4_dat1_pins[] = {};

static const unsigned sdmmc4_dat0_pins[] = {};

static const unsigned sdmmc3_clk_pg0_pins[] = {
	TEGRA_PIN_SDMMC3_CLK_PG0,
};

static const unsigned sdmmc3_cmd_pg1_pins[] = {
	TEGRA_PIN_SDMMC3_CMD_PG1,
};

static const unsigned sdmmc3_dat0_pg2_pins[] = {
	TEGRA_PIN_SDMMC3_DAT0_PG2,
};

static const unsigned sdmmc3_comp_pins[] = {
	TEGRA_PIN_SDMMC3_COMP,
};

static const unsigned sdmmc3_dat1_pg3_pins[] = {
	TEGRA_PIN_SDMMC3_DAT1_PG3,
};

static const unsigned sdmmc3_dat2_pg4_pins[] = {
	TEGRA_PIN_SDMMC3_DAT2_PG4,
};

static const unsigned sdmmc3_dat3_pg5_pins[] = {
	TEGRA_PIN_SDMMC3_DAT3_PG5,
};

static const unsigned gpio_wan5_ph0_pins[] = {
	TEGRA_PIN_GPIO_WAN5_PH0,
};

static const unsigned gpio_wan6_ph1_pins[] = {
	TEGRA_PIN_GPIO_WAN6_PH1,
};

static const unsigned gpio_wan7_ph2_pins[] = {
	TEGRA_PIN_GPIO_WAN7_PH2,
};

static const unsigned gpio_wan8_ph3_pins[] = {
	TEGRA_PIN_GPIO_WAN8_PH3,
};

static const unsigned bcpu_pwr_req_ph4_pins[] = {
	TEGRA_PIN_BCPU_PWR_REQ_PH4,
};

static const unsigned mcpu_pwr_req_ph5_pins[] = {
	TEGRA_PIN_MCPU_PWR_REQ_PH5,
};

static const unsigned gpu_pwr_req_ph6_pins[] = {
	TEGRA_PIN_GPU_PWR_REQ_PH6,
};

static const unsigned gpio_pq0_pi0_pins[] = {
	TEGRA_PIN_GPIO_PQ0_PI0,
};

static const unsigned gpio_pq1_pi1_pins[] = {
	TEGRA_PIN_GPIO_PQ1_PI1,
};

static const unsigned gpio_pq2_pi2_pins[] = {
	TEGRA_PIN_GPIO_PQ2_PI2,
};

static const unsigned gpio_pq3_pi3_pins[] = {
	TEGRA_PIN_GPIO_PQ3_PI3,
};

static const unsigned gpio_pq4_pi4_pins[] = {
	TEGRA_PIN_GPIO_PQ4_PI4,
};

static const unsigned gpio_pq5_pi5_pins[] = {
	TEGRA_PIN_GPIO_PQ5_PI5,
};

static const unsigned gpio_pq6_pi6_pins[] = {
	TEGRA_PIN_GPIO_PQ6_PI6,
};

static const unsigned gpio_pq7_pi7_pins[] = {
	TEGRA_PIN_GPIO_PQ7_PI7,
};

static const unsigned dap1_sclk_pj0_pins[] = {
	TEGRA_PIN_DAP1_SCLK_PJ0,
};

static const unsigned dap1_dout_pj1_pins[] = {
	TEGRA_PIN_DAP1_DOUT_PJ1,
};

static const unsigned dap1_din_pj2_pins[] = {
	TEGRA_PIN_DAP1_DIN_PJ2,
};

static const unsigned dap1_fs_pj3_pins[] = {
	TEGRA_PIN_DAP1_FS_PJ3,
};

static const unsigned aud_mclk_pj4_pins[] = {
	TEGRA_PIN_AUD_MCLK_PJ4,
};

static const unsigned gpio_aud0_pj5_pins[] = {
	TEGRA_PIN_GPIO_AUD0_PJ5,
};

static const unsigned gpio_aud1_pj6_pins[] = {
	TEGRA_PIN_GPIO_AUD1_PJ6,
};

static const unsigned gpio_aud2_pj7_pins[] = {
	TEGRA_PIN_GPIO_AUD2_PJ7,
};

static const unsigned gpio_aud3_pk0_pins[] = {
	TEGRA_PIN_GPIO_AUD3_PK0,
};

static const unsigned gen7_i2c_scl_pl0_pins[] = {
	TEGRA_PIN_GEN7_I2C_SCL_PL0,
};

static const unsigned gen7_i2c_sda_pl1_pins[] = {
	TEGRA_PIN_GEN7_I2C_SDA_PL1,
};

static const unsigned gen9_i2c_scl_pl2_pins[] = {
	TEGRA_PIN_GEN9_I2C_SCL_PL2,
};

static const unsigned gen9_i2c_sda_pl3_pins[] = {
	TEGRA_PIN_GEN9_I2C_SDA_PL3,
};

static const unsigned usb_vbus_en0_pl4_pins[] = {
	TEGRA_PIN_USB_VBUS_EN0_PL4,
};

static const unsigned usb_vbus_en1_pl5_pins[] = {
	TEGRA_PIN_USB_VBUS_EN1_PL5,
};

static const unsigned gp_pwm6_pl6_pins[] = {
	TEGRA_PIN_GP_PWM6_PL6,
};

static const unsigned gp_pwm7_pl7_pins[] = {
	TEGRA_PIN_GP_PWM7_PL7,
};

static const unsigned dmic1_dat_pm0_pins[] = {
	TEGRA_PIN_DMIC1_DAT_PM0,
};

static const unsigned dmic1_clk_pm1_pins[] = {
	TEGRA_PIN_DMIC1_CLK_PM1,
};

static const unsigned dmic2_dat_pm2_pins[] = {
	TEGRA_PIN_DMIC2_DAT_PM2,
};

static const unsigned dmic2_clk_pm3_pins[] = {
	TEGRA_PIN_DMIC2_CLK_PM3,
};

static const unsigned dmic4_dat_pm4_pins[] = {
	TEGRA_PIN_DMIC4_DAT_PM4,
};

static const unsigned dmic4_clk_pm5_pins[] = {
	TEGRA_PIN_DMIC4_CLK_PM5,
};

static const unsigned gpio_cam1_pn0_pins[] = {
	TEGRA_PIN_GPIO_CAM1_PN0,
};

static const unsigned gpio_cam2_pn1_pins[] = {
	TEGRA_PIN_GPIO_CAM2_PN1,
};

static const unsigned gpio_cam3_pn2_pins[] = {
	TEGRA_PIN_GPIO_CAM3_PN2,
};

static const unsigned gpio_cam4_pn3_pins[] = {
	TEGRA_PIN_GPIO_CAM4_PN3,
};

static const unsigned gpio_cam5_pn4_pins[] = {
	TEGRA_PIN_GPIO_CAM5_PN4,
};

static const unsigned gpio_cam6_pn5_pins[] = {
	TEGRA_PIN_GPIO_CAM6_PN5,
};

static const unsigned gpio_cam7_pn6_pins[] = {
	TEGRA_PIN_GPIO_CAM7_PN6,
};

static const unsigned extperiph1_clk_po0_pins[] = {
	TEGRA_PIN_EXTPERIPH1_CLK_PO0,
};

static const unsigned extperiph2_clk_po1_pins[] = {
	TEGRA_PIN_EXTPERIPH2_CLK_PO1,
};

static const unsigned cam_i2c_scl_po2_pins[] = {
	TEGRA_PIN_CAM_I2C_SCL_PO2,
};

static const unsigned cam_i2c_sda_po3_pins[] = {
	TEGRA_PIN_CAM_I2C_SDA_PO3,
};

static const unsigned dp_aux_ch0_hpd_pp0_pins[] = {
	TEGRA_PIN_DP_AUX_CH0_HPD_PP0,
};

static const unsigned dp_aux_ch1_hpd_pp1_pins[] = {
	TEGRA_PIN_DP_AUX_CH1_HPD_PP1,
};

static const unsigned hdmi_cec_pp2_pins[] = {
	TEGRA_PIN_HDMI_CEC_PP2,
};

static const unsigned gpio_edp0_pp3_pins[] = {
	TEGRA_PIN_GPIO_EDP0_PP3,
};

static const unsigned gpio_edp1_pp4_pins[] = {
	TEGRA_PIN_GPIO_EDP1_PP4,
};

static const unsigned gpio_edp2_pp5_pins[] = {
	TEGRA_PIN_GPIO_EDP2_PP5,
};

static const unsigned gpio_edp3_pp6_pins[] = {
	TEGRA_PIN_GPIO_EDP3_PP6,
};

static const unsigned directdc1_clk_pq0_pins[] = {
	TEGRA_PIN_DIRECTDC1_CLK_PQ0,
};

static const unsigned directdc_comp_pins[] = {
	TEGRA_PIN_DIRECTDC_COMP,
};

static const unsigned directdc1_in_pq1_pins[] = {
	TEGRA_PIN_DIRECTDC1_IN_PQ1,
};

static const unsigned directdc1_out0_pq2_pins[] = {
	TEGRA_PIN_DIRECTDC1_OUT0_PQ2,
};

static const unsigned directdc1_out1_pq3_pins[] = {
	TEGRA_PIN_DIRECTDC1_OUT1_PQ3,
};

static const unsigned directdc1_out2_pq4_pins[] = {
	TEGRA_PIN_DIRECTDC1_OUT2_PQ4,
};

static const unsigned directdc1_out3_pq5_pins[] = {
	TEGRA_PIN_DIRECTDC1_OUT3_PQ5,
};

static const unsigned qspi_sck_pr0_pins[] = {
	TEGRA_PIN_QSPI_SCK_PR0,
};

static const unsigned qspi_io0_pr1_pins[] = {
	TEGRA_PIN_QSPI_IO0_PR1,
};

static const unsigned qspi_io1_pr2_pins[] = {
	TEGRA_PIN_QSPI_IO1_PR2,
};

static const unsigned qspi_io2_pr3_pins[] = {
	TEGRA_PIN_QSPI_IO2_PR3,
};

static const unsigned qspi_io3_pr4_pins[] = {
	TEGRA_PIN_QSPI_IO3_PR4,
};

static const unsigned qspi_cs_n_pr5_pins[] = {
	TEGRA_PIN_QSPI_CS_N_PR5,
};

static const unsigned qspi_comp_pins[] = {
	TEGRA_PIN_QSPI_COMP,
};

static const unsigned pwr_i2c_scl_ps0_pins[] = {
	TEGRA_PIN_PWR_I2C_SCL_PS0,
};

static const unsigned pwr_i2c_sda_ps1_pins[] = {
	TEGRA_PIN_PWR_I2C_SDA_PS1,
};

static const unsigned batt_oc_ps2_pins[] = {
	TEGRA_PIN_BATT_OC_PS2,
};

static const unsigned safe_state_ps3_pins[] = {
	TEGRA_PIN_SAFE_STATE_PS3,
};

static const unsigned vcomp_alert_ps4_pins[] = {
	TEGRA_PIN_VCOMP_ALERT_PS4,
};

static const unsigned soc_pwr_req_pins[] = {
	TEGRA_PIN_SOC_PWR_REQ,
};

static const unsigned uart1_tx_pt0_pins[] = {
	TEGRA_PIN_UART1_TX_PT0,
};

static const unsigned uart1_rx_pt1_pins[] = {
	TEGRA_PIN_UART1_RX_PT1,
};

static const unsigned uart1_rts_pt2_pins[] = {
	TEGRA_PIN_UART1_RTS_PT2,
};

static const unsigned uart1_cts_pt3_pins[] = {
	TEGRA_PIN_UART1_CTS_PT3,
};

static const unsigned gpio_dis0_pu0_pins[] = {
	TEGRA_PIN_GPIO_DIS0_PU0,
};

static const unsigned gpio_dis1_pu1_pins[] = {
	TEGRA_PIN_GPIO_DIS1_PU1,
};

static const unsigned gpio_dis2_pu2_pins[] = {
	TEGRA_PIN_GPIO_DIS2_PU2,
};

static const unsigned gpio_dis3_pu3_pins[] = {
	TEGRA_PIN_GPIO_DIS3_PU3,
};

static const unsigned gpio_dis4_pu4_pins[] = {
	TEGRA_PIN_GPIO_DIS4_PU4,
};

static const unsigned gpio_dis5_pu5_pins[] = {
	TEGRA_PIN_GPIO_DIS5_PU5,
};

static const unsigned gpio_sen0_pv0_pins[] = {
	TEGRA_PIN_GPIO_SEN0_PV0,
};

static const unsigned gpio_sen1_pv1_pins[] = {
	TEGRA_PIN_GPIO_SEN1_PV1,
};

static const unsigned gpio_sen2_pv2_pins[] = {
	TEGRA_PIN_GPIO_SEN2_PV2,
};

static const unsigned gpio_sen3_pv3_pins[] = {
	TEGRA_PIN_GPIO_SEN3_PV3,
};

static const unsigned gpio_sen4_pv4_pins[] = {
	TEGRA_PIN_GPIO_SEN4_PV4,
};

static const unsigned gpio_sen5_pv5_pins[] = {
	TEGRA_PIN_GPIO_SEN5_PV5,
};

static const unsigned gpio_sen6_pv6_pins[] = {
	TEGRA_PIN_GPIO_SEN6_PV6,
};

static const unsigned gpio_sen7_pv7_pins[] = {
	TEGRA_PIN_GPIO_SEN7_PV7,
};

static const unsigned gen8_i2c_scl_pw0_pins[] = {
	TEGRA_PIN_GEN8_I2C_SCL_PW0,
};

static const unsigned gen8_i2c_sda_pw1_pins[] = {
	TEGRA_PIN_GEN8_I2C_SDA_PW1,
};

static const unsigned uart3_tx_pw2_pins[] = {
	TEGRA_PIN_UART3_TX_PW2,
};

static const unsigned uart3_rx_pw3_pins[] = {
	TEGRA_PIN_UART3_RX_PW3,
};

static const unsigned uart3_rts_pw4_pins[] = {
	TEGRA_PIN_UART3_RTS_PW4,
};

static const unsigned uart3_cts_pw5_pins[] = {
	TEGRA_PIN_UART3_CTS_PW5,
};

static const unsigned uart7_tx_pw6_pins[] = {
	TEGRA_PIN_UART7_TX_PW6,
};

static const unsigned uart7_rx_pw7_pins[] = {
	TEGRA_PIN_UART7_RX_PW7,
};

static const unsigned uart2_tx_px0_pins[] = {
	TEGRA_PIN_UART2_TX_PX0,
};

static const unsigned uart2_rx_px1_pins[] = {
	TEGRA_PIN_UART2_RX_PX1,
};

static const unsigned uart2_rts_px2_pins[] = {
	TEGRA_PIN_UART2_RTS_PX2,
};

static const unsigned uart2_cts_px3_pins[] = {
	TEGRA_PIN_UART2_CTS_PX3,
};

static const unsigned uart5_tx_px4_pins[] = {
	TEGRA_PIN_UART5_TX_PX4,
};

static const unsigned uart5_rx_px5_pins[] = {
	TEGRA_PIN_UART5_RX_PX5,
};

static const unsigned uart5_rts_px6_pins[] = {
	TEGRA_PIN_UART5_RTS_PX6,
};

static const unsigned uart5_cts_px7_pins[] = {
	TEGRA_PIN_UART5_CTS_PX7,
};

static const unsigned gpio_mdm1_py0_pins[] = {
	TEGRA_PIN_GPIO_MDM1_PY0,
};

static const unsigned gpio_mdm2_py1_pins[] = {
	TEGRA_PIN_GPIO_MDM2_PY1,
};

static const unsigned gpio_mdm3_py2_pins[] = {
	TEGRA_PIN_GPIO_MDM3_PY2,
};

static const unsigned gpio_mdm4_py3_pins[] = {
	TEGRA_PIN_GPIO_MDM4_PY3,
};

static const unsigned gpio_mdm5_py4_pins[] = {
	TEGRA_PIN_GPIO_MDM5_PY4,
};

static const unsigned gpio_mdm6_py5_pins[] = {
	TEGRA_PIN_GPIO_MDM6_PY5,
};

static const unsigned gpio_mdm7_py6_pins[] = {
	TEGRA_PIN_GPIO_MDM7_PY6,
};

static const unsigned can1_dout_pz0_pins[] = {
	TEGRA_PIN_CAN1_DOUT_PZ0,
};

static const unsigned can1_din_pz1_pins[] = {
	TEGRA_PIN_CAN1_DIN_PZ1,
};

static const unsigned can0_dout_pz2_pins[] = {
	TEGRA_PIN_CAN0_DOUT_PZ2,
};

static const unsigned can0_din_pz3_pins[] = {
	TEGRA_PIN_CAN0_DIN_PZ3,
};

static const unsigned can_gpio0_paa0_pins[] = {
	TEGRA_PIN_CAN_GPIO0_PAA0,
};

static const unsigned can_gpio1_paa1_pins[] = {
	TEGRA_PIN_CAN_GPIO1_PAA1,
};

static const unsigned can_gpio2_paa2_pins[] = {
	TEGRA_PIN_CAN_GPIO2_PAA2,
};

static const unsigned can_gpio3_paa3_pins[] = {
	TEGRA_PIN_CAN_GPIO3_PAA3,
};

static const unsigned can_gpio4_paa4_pins[] = {
	TEGRA_PIN_CAN_GPIO4_PAA4,
};

static const unsigned can_gpio5_paa5_pins[] = {
	TEGRA_PIN_CAN_GPIO5_PAA5,
};

static const unsigned can_gpio6_paa6_pins[] = {
	TEGRA_PIN_CAN_GPIO6_PAA6,
};

static const unsigned can_gpio7_paa7_pins[] = {
	TEGRA_PIN_CAN_GPIO7_PAA7,
};

static const unsigned ufs0_ref_clk_pbb0_pins[] = {
	TEGRA_PIN_UFS0_REF_CLK_PBB0,
};

static const unsigned ufs0_rst_pbb1_pins[] = {
	TEGRA_PIN_UFS0_RST_PBB1,
};

static const unsigned dap4_sclk_pcc0_pins[] = {
	TEGRA_PIN_DAP4_SCLK_PCC0,
};

static const unsigned dap4_dout_pcc1_pins[] = {
	TEGRA_PIN_DAP4_DOUT_PCC1,
};

static const unsigned dap4_din_pcc2_pins[] = {
	TEGRA_PIN_DAP4_DIN_PCC2,
};

static const unsigned dap4_fs_pcc3_pins[] = {
	TEGRA_PIN_DAP4_FS_PCC3,
};

static const unsigned gpio_sen8_pee0_pins[] = {
	TEGRA_PIN_GPIO_SEN8_PEE0,
};

static const unsigned gpio_sen9_pee1_pins[] = {
	TEGRA_PIN_GPIO_SEN9_PEE1,
};


static const unsigned touch_clk_pee2_pins[] = {
	TEGRA_PIN_TOUCH_CLK_PEE2,
};

static const unsigned power_on_pff0_pins[] = {
	TEGRA_PIN_POWER_ON_PFF0,
};

static const unsigned gpio_sw1_pff1_pins[] = {
	TEGRA_PIN_GPIO_SW1_PFF1,
};

static const unsigned gpio_sw2_pff2_pins[] = {
	TEGRA_PIN_GPIO_SW2_PFF2,
};

static const unsigned gpio_sw3_pff3_pins[] = {
	TEGRA_PIN_GPIO_SW3_PFF3,
};

static const unsigned gpio_sw4_pff4_pins[] = {
	TEGRA_PIN_GPIO_SW4_PFF4,
};

static const unsigned clk_32k_in_pins[] = {
	TEGRA_PIN_CLK_32K_IN,
};

static const unsigned shutdown_pins[] = {
	TEGRA_PIN_SHUTDOWN,
};

static const unsigned pmu_int_pins[] = {
	TEGRA_PIN_PMU_INT,
};

static const unsigned drive_ufs0_rst_pins[] = {
	TEGRA_PIN_UFS0_RST_PBB1,
};

static const unsigned drive_ufs0_ref_clk_pins[] = {
	TEGRA_PIN_UFS0_REF_CLK_PBB0,
};

static const unsigned drive_gpio_wan8_pins[] = {
	TEGRA_PIN_GPIO_WAN8_PH3,
};

static const unsigned drive_gpio_wan7_pins[] = {
	TEGRA_PIN_GPIO_WAN7_PH2,
};

static const unsigned drive_gpio_wan6_pins[] = {
	TEGRA_PIN_GPIO_WAN6_PH1,
};

static const unsigned drive_gpio_wan5_pins[] = {
	TEGRA_PIN_GPIO_WAN5_PH0,
};

static const unsigned drive_uart2_tx_pins[] = {
	TEGRA_PIN_UART2_TX_PX0,
};

static const unsigned drive_uart2_rx_pins[] = {
	TEGRA_PIN_UART2_RX_PX1,
};

static const unsigned drive_uart2_rts_pins[] = {
	TEGRA_PIN_UART2_RTS_PX2,
};

static const unsigned drive_uart2_cts_pins[] = {
	TEGRA_PIN_UART2_CTS_PX3,
};

static const unsigned drive_uart5_rx_pins[] = {
	TEGRA_PIN_UART5_RX_PX5,
};

static const unsigned drive_uart5_tx_pins[] = {
	TEGRA_PIN_UART5_TX_PX4,
};

static const unsigned drive_uart5_rts_pins[] = {
	TEGRA_PIN_UART5_RTS_PX6,
};

static const unsigned drive_uart5_cts_pins[] = {
	TEGRA_PIN_UART5_CTS_PX7,
};

static const unsigned drive_gpio_mdm1_pins[] = {
	TEGRA_PIN_GPIO_MDM1_PY0,
};

static const unsigned drive_gpio_mdm2_pins[] = {
	TEGRA_PIN_GPIO_MDM2_PY1,
};

static const unsigned drive_gpio_mdm3_pins[] = {
	TEGRA_PIN_GPIO_MDM3_PY2,
};

static const unsigned drive_gpio_mdm4_pins[] = {
	TEGRA_PIN_GPIO_MDM4_PY3,
};

static const unsigned drive_gpio_mdm5_pins[] = {
	TEGRA_PIN_GPIO_MDM5_PY4,
};

static const unsigned drive_gpio_mdm6_pins[] = {
	TEGRA_PIN_GPIO_MDM6_PY5,
};

static const unsigned drive_gpio_mdm7_pins[] = {
	TEGRA_PIN_GPIO_MDM7_PY6,
};

static const unsigned drive_bcpu_pwr_req_pins[] = {
	TEGRA_PIN_BCPU_PWR_REQ_PH4,
};

static const unsigned drive_mcpu_pwr_req_pins[] = {
	TEGRA_PIN_MCPU_PWR_REQ_PH5,
};

static const unsigned drive_gpu_pwr_req_pins[] = {
	TEGRA_PIN_GPU_PWR_REQ_PH6,
};

static const unsigned drive_gen7_i2c_scl_pins[] = {
	TEGRA_PIN_GEN7_I2C_SCL_PL0,
};

static const unsigned drive_gen7_i2c_sda_pins[] = {
	TEGRA_PIN_GEN7_I2C_SDA_PL1,
};

static const unsigned drive_gen9_i2c_sda_pins[] = {
	TEGRA_PIN_GEN9_I2C_SDA_PL3,
};

static const unsigned drive_gen9_i2c_scl_pins[] = {
	TEGRA_PIN_GEN9_I2C_SCL_PL2,
};

static const unsigned drive_usb_vbus_en0_pins[] = {
	TEGRA_PIN_USB_VBUS_EN0_PL4,
};

static const unsigned drive_usb_vbus_en1_pins[] = {
	TEGRA_PIN_USB_VBUS_EN1_PL5,
};

static const unsigned drive_gp_pwm7_pins[] = {
	TEGRA_PIN_GP_PWM7_PL7,
};

static const unsigned drive_gp_pwm6_pins[] = {
	TEGRA_PIN_GP_PWM6_PL6,
};

static const unsigned drive_gpio_sw1_pins[] = {
	TEGRA_PIN_GPIO_SW1_PFF1,
};

static const unsigned drive_gpio_sw2_pins[] = {
	TEGRA_PIN_GPIO_SW2_PFF2,
};

static const unsigned drive_gpio_sw3_pins[] = {
	TEGRA_PIN_GPIO_SW3_PFF3,
};

static const unsigned drive_gpio_sw4_pins[] = {
	TEGRA_PIN_GPIO_SW4_PFF4,
};

static const unsigned drive_shutdown_pins[] = {
	TEGRA_PIN_SHUTDOWN,
};

static const unsigned drive_pmu_int_pins[] = {
};

static const unsigned drive_safe_state_pins[] = {
	TEGRA_PIN_SAFE_STATE_PS3,
};

static const unsigned drive_vcomp_alert_pins[] = {
	TEGRA_PIN_VCOMP_ALERT_PS4,
};

static const unsigned drive_soc_pwr_req_pins[] = {
	TEGRA_PIN_SOC_PWR_REQ,
};

static const unsigned drive_batt_oc_pins[] = {
	TEGRA_PIN_BATT_OC_PS2,
};

static const unsigned drive_clk_32k_in_pins[] = {
	TEGRA_PIN_CLK_32K_IN,
};

static const unsigned drive_power_on_pins[] = {
	TEGRA_PIN_POWER_ON_PFF0,
};

static const unsigned drive_pwr_i2c_scl_pins[] = {
	TEGRA_PIN_PWR_I2C_SCL_PS0,
};

static const unsigned drive_pwr_i2c_sda_pins[] = {
	TEGRA_PIN_PWR_I2C_SDA_PS1,
};

static const unsigned drive_gpio_dis0_pins[] = {
	TEGRA_PIN_GPIO_DIS0_PU0,
};

static const unsigned drive_gpio_dis1_pins[] = {
	TEGRA_PIN_GPIO_DIS1_PU1,
};

static const unsigned drive_gpio_dis2_pins[] = {
	TEGRA_PIN_GPIO_DIS2_PU2,
};

static const unsigned drive_gpio_dis3_pins[] = {
	TEGRA_PIN_GPIO_DIS3_PU3,
};

static const unsigned drive_gpio_dis4_pins[] = {
	TEGRA_PIN_GPIO_DIS4_PU4,
};

static const unsigned drive_gpio_dis5_pins[] = {
	TEGRA_PIN_GPIO_DIS5_PU5,
};

static const unsigned drive_qspi_io3_pins[] = {
	TEGRA_PIN_QSPI_IO3_PR4,
};

static const unsigned drive_qspi_io2_pins[] = {
	TEGRA_PIN_QSPI_IO2_PR3,
};

static const unsigned drive_qspi_io1_pins[] = {
	TEGRA_PIN_QSPI_IO1_PR2,
};

static const unsigned drive_qspi_io0_pins[] = {
	TEGRA_PIN_QSPI_IO0_PR1,
};

static const unsigned drive_qspi_sck_pins[] = {
	TEGRA_PIN_QSPI_SCK_PR0,
};

static const unsigned drive_qspi_cs_n_pins[] = {
	TEGRA_PIN_QSPI_CS_N_PR5,
};

static const unsigned drive_sdmmc3_dat3_pins[] = {
	TEGRA_PIN_SDMMC3_DAT3_PG5,
};

static const unsigned drive_sdmmc3_dat2_pins[] = {
	TEGRA_PIN_SDMMC3_DAT2_PG4,
};

static const unsigned drive_sdmmc3_dat1_pins[] = {
	TEGRA_PIN_SDMMC3_DAT1_PG3,
};

static const unsigned drive_sdmmc3_dat0_pins[] = {
	TEGRA_PIN_SDMMC3_DAT0_PG2,
};

static const unsigned drive_sdmmc3_cmd_pins[] = {
	TEGRA_PIN_SDMMC3_CMD_PG1,
};

static const unsigned drive_sdmmc3_clk_pins[] = {
	TEGRA_PIN_SDMMC3_CLK_PG0,
};

static const unsigned drive_eqos_td3_pins[] = {
	TEGRA_PIN_EQOS_TD3_PE4,
};

static const unsigned drive_eqos_td2_pins[] = {
	TEGRA_PIN_EQOS_TD2_PE3,
};

static const unsigned drive_eqos_td1_pins[] = {
	TEGRA_PIN_EQOS_TD1_PE2,
};

static const unsigned drive_eqos_td0_pins[] = {
	TEGRA_PIN_EQOS_TD0_PE1,
};

static const unsigned drive_eqos_rd3_pins[] = {
	TEGRA_PIN_EQOS_RD3_PF1,
};

static const unsigned drive_eqos_rd2_pins[] = {
	TEGRA_PIN_EQOS_RD2_PF0,
};

static const unsigned drive_eqos_rd1_pins[] = {
	TEGRA_PIN_EQOS_RD1_PE7,
};

static const unsigned drive_eqos_mdio_pins[] = {
	TEGRA_PIN_EQOS_MDIO_PF4,
};

static const unsigned drive_eqos_rd0_pins[] = {
	TEGRA_PIN_EQOS_RD0_PE6,
};

static const unsigned drive_eqos_mdc_pins[] = {
	TEGRA_PIN_EQOS_MDC_PF5,
};

static const unsigned drive_eqos_txc_pins[] = {
	TEGRA_PIN_EQOS_TXC_PE0,
};

static const unsigned drive_eqos_rxc_pins[] = {
	TEGRA_PIN_EQOS_RXC_PF3,
};

static const unsigned drive_eqos_tx_ctl_pins[] = {
	TEGRA_PIN_EQOS_TX_CTL_PE5,
};

static const unsigned drive_eqos_rx_ctl_pins[] = {
	TEGRA_PIN_EQOS_RX_CTL_PF2,
};

static const unsigned drive_sdmmc1_clk_pins[] = {
	TEGRA_PIN_SDMMC1_CLK_PD0,
};

static const unsigned drive_sdmmc1_cmd_pins[] = {
	TEGRA_PIN_SDMMC1_CMD_PD1,
};

static const unsigned drive_sdmmc1_dat3_pins[] = {
	TEGRA_PIN_SDMMC1_DAT3_PD5,
};

static const unsigned drive_sdmmc1_dat2_pins[] = {
	TEGRA_PIN_SDMMC1_DAT2_PD4,
};

static const unsigned drive_sdmmc1_dat1_pins[] = {
	TEGRA_PIN_SDMMC1_DAT1_PD3,
};

static const unsigned drive_sdmmc1_dat0_pins[] = {
	TEGRA_PIN_SDMMC1_DAT0_PD2,
};

static const unsigned drive_pex_l2_clkreq_n_pins[] = {
	TEGRA_PIN_PEX_L2_CLKREQ_N_PA6,
};

static const unsigned drive_pex_wake_n_pins[] = {
	TEGRA_PIN_PEX_WAKE_N_PA2,
};

static const unsigned drive_pex_l1_clkreq_n_pins[] = {
	TEGRA_PIN_PEX_L1_CLKREQ_N_PA4,
};

static const unsigned drive_pex_l1_rst_n_pins[] = {
	TEGRA_PIN_PEX_L1_RST_N_PA3,
};

static const unsigned drive_pex_l0_clkreq_n_pins[] = {
	TEGRA_PIN_PEX_L0_CLKREQ_N_PA1,
};

static const unsigned drive_pex_l0_rst_n_pins[] = {
	TEGRA_PIN_PEX_L0_RST_N_PA0,
};

static const unsigned drive_pex_l2_rst_n_pins[] = {
	TEGRA_PIN_PEX_L2_RST_N_PA5,
};

static const unsigned drive_gpio_edp2_pins[] = {
	TEGRA_PIN_GPIO_EDP2_PP5,
};

static const unsigned drive_gpio_edp3_pins[] = {
	TEGRA_PIN_GPIO_EDP3_PP6,
};

static const unsigned drive_gpio_edp0_pins[] = {
	TEGRA_PIN_GPIO_EDP0_PP3,
};

static const unsigned drive_gpio_edp1_pins[] = {
	TEGRA_PIN_GPIO_EDP1_PP4,
};

static const unsigned drive_dp_aux_ch0_hpd_pins[] = {
	TEGRA_PIN_DP_AUX_CH0_HPD_PP0,
};

static const unsigned drive_dp_aux_ch1_hpd_pins[] = {
	TEGRA_PIN_DP_AUX_CH1_HPD_PP1,
};

static const unsigned drive_hdmi_cec_pins[] = {
	TEGRA_PIN_HDMI_CEC_PP2,
};

static const unsigned drive_gpio_pq0_pins[] = {
	TEGRA_PIN_GPIO_PQ0_PI0,
};

static const unsigned drive_gpio_pq1_pins[] = {
	TEGRA_PIN_GPIO_PQ1_PI1,
};

static const unsigned drive_gpio_pq2_pins[] = {
	TEGRA_PIN_GPIO_PQ2_PI2,
};

static const unsigned drive_gpio_pq3_pins[] = {
	TEGRA_PIN_GPIO_PQ3_PI3,
};

static const unsigned drive_gpio_pq4_pins[] = {
	TEGRA_PIN_GPIO_PQ4_PI4,
};

static const unsigned drive_gpio_pq5_pins[] = {
	TEGRA_PIN_GPIO_PQ5_PI5,
};

static const unsigned drive_gpio_pq6_pins[] = {
	TEGRA_PIN_GPIO_PQ6_PI6,
};

static const unsigned drive_gpio_pq7_pins[] = {
	TEGRA_PIN_GPIO_PQ7_PI7,
};

static const unsigned drive_dap2_din_pins[] = {
	TEGRA_PIN_DAP2_DIN_PC3,
};

static const unsigned drive_dap2_dout_pins[] = {
	TEGRA_PIN_DAP2_DOUT_PC2,
};

static const unsigned drive_dap2_fs_pins[] = {
	TEGRA_PIN_DAP2_FS_PC4,
};

static const unsigned drive_dap2_sclk_pins[] = {
	TEGRA_PIN_DAP2_SCLK_PC1,
};

static const unsigned drive_uart4_cts_pins[] = {
	TEGRA_PIN_UART4_CTS_PB3,
};

static const unsigned drive_uart4_rts_pins[] = {
	TEGRA_PIN_UART4_RTS_PB2,
};

static const unsigned drive_uart4_rx_pins[] = {
	TEGRA_PIN_UART4_RX_PB1,
};

static const unsigned drive_uart4_tx_pins[] = {
	TEGRA_PIN_UART4_TX_PB0,
};

static const unsigned drive_gpio_wan4_pins[] = {
	TEGRA_PIN_GPIO_WAN4_PC0,
};

static const unsigned drive_gpio_wan3_pins[] = {
	TEGRA_PIN_GPIO_WAN3_PB6,
};

static const unsigned drive_gpio_wan2_pins[] = {
	TEGRA_PIN_GPIO_WAN2_PB5,
};

static const unsigned drive_gpio_wan1_pins[] = {
	TEGRA_PIN_GPIO_WAN1_PB4,
};

static const unsigned drive_gen1_i2c_scl_pins[] = {
	TEGRA_PIN_GEN1_I2C_SCL_PC5,
};

static const unsigned drive_gen1_i2c_sda_pins[] = {
	TEGRA_PIN_GEN1_I2C_SDA_PC6,
};

static const unsigned drive_extperiph2_clk_pins[] = {
	TEGRA_PIN_EXTPERIPH1_CLK_PO0,
};

static const unsigned drive_extperiph1_clk_pins[] = {
	TEGRA_PIN_EXTPERIPH2_CLK_PO1,
};

static const unsigned drive_cam_i2c_sda_pins[] = {
	TEGRA_PIN_CAM_I2C_SDA_PO3,
};

static const unsigned drive_cam_i2c_scl_pins[] = {
	TEGRA_PIN_CAM_I2C_SCL_PO2,
};

static const unsigned drive_gpio_cam1_pins[] = {
	TEGRA_PIN_GPIO_CAM1_PN0,
};

static const unsigned drive_gpio_cam2_pins[] = {
	TEGRA_PIN_GPIO_CAM2_PN1,
};

static const unsigned drive_gpio_cam3_pins[] = {
	TEGRA_PIN_GPIO_CAM3_PN2,
};

static const unsigned drive_gpio_cam4_pins[] = {
	TEGRA_PIN_GPIO_CAM4_PN3,
};

static const unsigned drive_gpio_cam5_pins[] = {
	TEGRA_PIN_GPIO_CAM5_PN4,
};

static const unsigned drive_gpio_cam6_pins[] = {
	TEGRA_PIN_GPIO_CAM6_PN5,
};

static const unsigned drive_gpio_cam7_pins[] = {
	TEGRA_PIN_GPIO_CAM7_PN6,
};

static const unsigned drive_dmic1_clk_pins[] = {
	TEGRA_PIN_DMIC1_CLK_PM1,
};

static const unsigned drive_dmic1_dat_pins[] = {
	TEGRA_PIN_DMIC1_DAT_PM0,
};

static const unsigned drive_dmic2_dat_pins[] = {
	TEGRA_PIN_DMIC2_DAT_PM2,
};

static const unsigned drive_dmic2_clk_pins[] = {
	TEGRA_PIN_DMIC2_CLK_PM3,
};

static const unsigned drive_dmic4_dat_pins[] = {
	TEGRA_PIN_DMIC4_DAT_PM4,
};

static const unsigned drive_dmic4_clk_pins[] = {
	TEGRA_PIN_DMIC4_CLK_PM5,
};

static const unsigned drive_dap4_fs_pins[] = {
	TEGRA_PIN_DAP4_FS_PCC3,
};

static const unsigned drive_dap4_din_pins[] = {
	TEGRA_PIN_DAP4_DIN_PCC2,
};

static const unsigned drive_dap4_dout_pins[] = {
	TEGRA_PIN_DAP4_DOUT_PCC1,
};

static const unsigned drive_dap4_sclk_pins[] = {
	TEGRA_PIN_DAP4_SCLK_PCC0,
};

static const unsigned drive_gpio_aud3_pins[] = {
	TEGRA_PIN_GPIO_AUD3_PK0,
};

static const unsigned drive_gpio_aud2_pins[] = {
	TEGRA_PIN_GPIO_AUD2_PJ7,
};

static const unsigned drive_gpio_aud1_pins[] = {
	TEGRA_PIN_GPIO_AUD1_PJ6,
};

static const unsigned drive_gpio_aud0_pins[] = {
	TEGRA_PIN_GPIO_AUD0_PJ5,
};

static const unsigned drive_aud_mclk_pins[] = {
	TEGRA_PIN_AUD_MCLK_PJ4,
};

static const unsigned drive_dap1_fs_pins[] = {
	TEGRA_PIN_DAP1_FS_PJ3,
};

static const unsigned drive_dap1_din_pins[] = {
	TEGRA_PIN_DAP1_DIN_PJ2,
};

static const unsigned drive_dap1_dout_pins[] = {
	TEGRA_PIN_DAP1_DOUT_PJ1,
};

static const unsigned drive_dap1_sclk_pins[] = {
	TEGRA_PIN_DAP1_SCLK_PJ0,
};

static const unsigned drive_touch_clk_pins[] = {
	TEGRA_PIN_TOUCH_CLK_PEE2,
};

static const unsigned drive_uart3_cts_pins[] = {
	TEGRA_PIN_UART3_CTS_PW5,
};

static const unsigned drive_uart3_rts_pins[] = {
	TEGRA_PIN_UART3_RTS_PW4,
};

static const unsigned drive_uart3_rx_pins[] = {
	TEGRA_PIN_UART3_RX_PW3,
};

static const unsigned drive_uart3_tx_pins[] = {
	TEGRA_PIN_UART3_TX_PW2,
};

static const unsigned drive_gen8_i2c_sda_pins[] = {
	TEGRA_PIN_GEN8_I2C_SDA_PW1,
};

static const unsigned drive_gen8_i2c_scl_pins[] = {
	TEGRA_PIN_GEN8_I2C_SCL_PW0,
};

static const unsigned drive_uart7_rx_pins[] = {
	TEGRA_PIN_UART7_RX_PW7,
};

static const unsigned drive_uart7_tx_pins[] = {
	TEGRA_PIN_UART7_TX_PW6,
};

static const unsigned drive_gpio_sen0_pins[] = {
	TEGRA_PIN_GPIO_SEN0_PV0,
};

static const unsigned drive_gpio_sen1_pins[] = {
	TEGRA_PIN_GPIO_SEN1_PV1,
};

static const unsigned drive_gpio_sen2_pins[] = {
	TEGRA_PIN_GPIO_SEN2_PV2,
};

static const unsigned drive_gpio_sen3_pins[] = {
	TEGRA_PIN_GPIO_SEN3_PV3,
};

static const unsigned drive_gpio_sen4_pins[] = {
	TEGRA_PIN_GPIO_SEN4_PV4,
};

static const unsigned drive_gpio_sen5_pins[] = {
	TEGRA_PIN_GPIO_SEN5_PV5,
};

static const unsigned drive_gpio_sen6_pins[] = {
	TEGRA_PIN_GPIO_SEN6_PV6,
};

static const unsigned drive_gpio_sen7_pins[] = {
	TEGRA_PIN_GPIO_SEN7_PV7,
};

static const unsigned drive_gpio_sen8_pins[] = {
	TEGRA_PIN_GPIO_SEN8_PEE0,
};

static const unsigned drive_gpio_sen9_pins[] = {
	TEGRA_PIN_GPIO_SEN9_PEE1,
};

static const unsigned drive_can_gpio7_pins[] = {
	TEGRA_PIN_CAN_GPIO7_PAA7,
};

static const unsigned drive_can1_dout_pins[] = {
	TEGRA_PIN_CAN1_DOUT_PZ0,
};

static const unsigned drive_can1_din_pins[] = {
	TEGRA_PIN_CAN1_DIN_PZ1,
};

static const unsigned drive_can0_dout_pins[] = {
	TEGRA_PIN_CAN0_DOUT_PZ2,
};

static const unsigned drive_can0_din_pins[] = {
	TEGRA_PIN_CAN0_DIN_PZ3,
};

static const unsigned drive_can_gpio0_pins[] = {
	TEGRA_PIN_CAN_GPIO0_PAA0,
};

static const unsigned drive_can_gpio1_pins[] = {
	TEGRA_PIN_CAN_GPIO1_PAA1,
};

static const unsigned drive_can_gpio2_pins[] = {
	TEGRA_PIN_CAN_GPIO2_PAA2,
};

static const unsigned drive_can_gpio3_pins[] = {
	TEGRA_PIN_CAN_GPIO3_PAA3,
};

static const unsigned drive_can_gpio4_pins[] = {
	TEGRA_PIN_CAN_GPIO4_PAA4,
};

static const unsigned drive_can_gpio5_pins[] = {
	TEGRA_PIN_CAN_GPIO5_PAA5,
};

static const unsigned drive_can_gpio6_pins[] = {
	TEGRA_PIN_CAN_GPIO6_PAA6,
};

static const unsigned drive_uart1_cts_pins[] = {
	TEGRA_PIN_UART1_CTS_PT3,
};

static const unsigned drive_uart1_rts_pins[] = {
	TEGRA_PIN_UART1_RTS_PT2,
};

static const unsigned drive_uart1_rx_pins[] = {
	TEGRA_PIN_UART1_RX_PT1,
};

static const unsigned drive_uart1_tx_pins[] = {
	TEGRA_PIN_UART1_TX_PT0,
};

static const unsigned drive_directdc1_out3_pins[] = {
	TEGRA_PIN_DIRECTDC1_OUT3_PQ5,
};

static const unsigned drive_directdc1_out2_pins[] = {
	TEGRA_PIN_DIRECTDC1_OUT2_PQ4,
};

static const unsigned drive_directdc1_out1_pins[] = {
	TEGRA_PIN_DIRECTDC1_OUT1_PQ3,
};

static const unsigned drive_directdc1_out0_pins[] = {
	TEGRA_PIN_DIRECTDC1_OUT0_PQ2,
};

static const unsigned drive_directdc1_clk_pins[] = {
	TEGRA_PIN_DIRECTDC1_CLK_PQ0,
};

static const unsigned drive_directdc1_in_pins[] = {
	TEGRA_PIN_DIRECTDC1_IN_PQ1,
};

enum tegra_mux_dt {
	TEGRA_MUX_RSVD0,
	TEGRA_MUX_RSVD1,
	TEGRA_MUX_RSVD2,
	TEGRA_MUX_RSVD3,
	TEGRA_MUX_TOUCH,
	TEGRA_MUX_UARTC,
	TEGRA_MUX_I2C8,
	TEGRA_MUX_UARTG,
	TEGRA_MUX_SPI2,
	TEGRA_MUX_GP,
	TEGRA_MUX_DCA,
	TEGRA_MUX_WDT,
	TEGRA_MUX_I2C2,
	TEGRA_MUX_CAN1,
	TEGRA_MUX_CAN0,
	TEGRA_MUX_DMIC3,
	TEGRA_MUX_DMIC5,
	TEGRA_MUX_GPIO,
	TEGRA_MUX_DSPK1,
	TEGRA_MUX_DSPK0,
	TEGRA_MUX_SPDIF,
	TEGRA_MUX_AUD,
	TEGRA_MUX_I2S1,
	TEGRA_MUX_DMIC1,
	TEGRA_MUX_DMIC2,
	TEGRA_MUX_I2S3,
	TEGRA_MUX_DMIC4,
	TEGRA_MUX_I2S4,
	TEGRA_MUX_EXTPERIPH2,
	TEGRA_MUX_EXTPERIPH1,
	TEGRA_MUX_I2C3,
	TEGRA_MUX_VGP1,
	TEGRA_MUX_VGP2,
	TEGRA_MUX_VGP3,
	TEGRA_MUX_VGP4,
	TEGRA_MUX_VGP5,
	TEGRA_MUX_VGP6,
	TEGRA_MUX_EXTPERIPH3,
	TEGRA_MUX_EXTPERIPH4,
	TEGRA_MUX_SPI4,
	TEGRA_MUX_I2S2,
	TEGRA_MUX_UARTD,
	TEGRA_MUX_I2C1,
	TEGRA_MUX_UARTA,
	TEGRA_MUX_DIRECTDC1,
	TEGRA_MUX_DIRECTDC,
	TEGRA_MUX_IQC0,
	TEGRA_MUX_IQC1,
	TEGRA_MUX_I2S6,
	TEGRA_MUX_DTV,
	TEGRA_MUX_UARTF,
	TEGRA_MUX_SDMMC3,
	TEGRA_MUX_SDMMC4,
	TEGRA_MUX_SDMMC1,
	TEGRA_MUX_DP,
	TEGRA_MUX_HDMI,
	TEGRA_MUX_PE2,
	TEGRA_MUX_SATA,
	TEGRA_MUX_PE,
	TEGRA_MUX_PE1,
	TEGRA_MUX_PE0,
	TEGRA_MUX_SOC,
	TEGRA_MUX_EQOS,
	TEGRA_MUX_SDMMC2,
	TEGRA_MUX_QSPI,
	TEGRA_MUX_SCE,
	TEGRA_MUX_I2C5,
	TEGRA_MUX_DISPLAYA,
	TEGRA_MUX_DISPLAYB,
	TEGRA_MUX_DCC,
	TEGRA_MUX_SPI1,
	TEGRA_MUX_UARTB,
	TEGRA_MUX_UARTE,
	TEGRA_MUX_SPI3,
	TEGRA_MUX_NV,
	TEGRA_MUX_CCLA,
	TEGRA_MUX_I2C7,
	TEGRA_MUX_I2C9,
	TEGRA_MUX_I2S5,
	TEGRA_MUX_USB,
	TEGRA_MUX_UFS0,
};

static const char * const rsvd0_groups[] = {
	"gpio_sen0_pv0",
	"gpio_sen5_pv5",
	"gpio_sen6_pv6",
	"gpio_sen7_pv7",
	"gpio_sen8_pee0",
	"gpio_sen9_pee1",
	"can_gpio7_paa7",
	"can_gpio0_paa0",
	"can_gpio1_paa1",
	"can_gpio3_paa3",
	"can_gpio4_paa4",
	"can_gpio5_paa5",
	"can_gpio6_paa6",
	"gpio_aud3_pk0",
	"gpio_aud2_pj7",
	"gpio_aud1_pj6",
	"gpio_aud0_pj5",
	"gpio_cam7_pn6",
	"gpio_wan4_pc0",
	"gpio_wan3_pb6",
	"gpio_wan2_pb5",
	"gpio_wan1_pb4",
	"gpio_pq0_pi0",
	"gpio_pq1_pi1",
	"gpio_pq2_pi2",
	"gpio_pq3_pi3",
	"gpio_pq4_pi4",
	"gpio_pq5_pi5",
	"gpio_pq6_pi6",
	"gpio_pq7_pi7",
	"gpio_edp2_pp5",
	"gpio_edp3_pp6",
	"gpio_edp0_pp3",
	"gpio_edp1_pp4",
	"gpio_sw1_pff1",
	"gpio_sw2_pff2",
	"gpio_sw3_pff3",
	"gpio_sw4_pff4",
	"shutdown",
	"power_on_pff0",
	"gpio_dis0_pu0",
	"gpio_dis1_pu1",
	"gpio_dis2_pu2",
	"gpio_dis3_pu3",
	"gpio_dis4_pu4",
	"gpio_dis5_pu5",
	"gpio_wan8_ph3",
	"gpio_wan7_ph2",
	"gpio_wan6_ph1",
	"gpio_wan5_ph0",
	"gpio_mdm1_py0",
	"gpio_mdm2_py1",
	"gpio_mdm3_py2",
	"gpio_mdm4_py3",
	"gpio_mdm5_py4",
	"gpio_mdm7_py6",
	"bcpu_pwr_req_ph4",
	"mcpu_pwr_req_ph5",
	"gpu_pwr_req_ph6",
};

static const char * const rsvd1_groups[] = {
	"touch_clk_pee2",
	"uart3_cts_pw5",
	"uart3_rts_pw4",
	"uart3_rx_pw3",
	"uart3_tx_pw2",
	"gen8_i2c_sda_pw1",
	"gen8_i2c_scl_pw0",
	"uart7_rx_pw7",
	"uart7_tx_pw6",
	"gpio_sen0_pv0",
	"gpio_sen1_pv1",
	"gpio_sen2_pv2",
	"gpio_sen3_pv3",
	"gpio_sen4_pv4",
	"gpio_sen5_pv5",
	"can1_dout_pz0",
	"can1_din_pz1",
	"can0_dout_pz2",
	"can0_din_pz3",
	"can_gpio2_paa2",
	"can_gpio3_paa3",
	"can_gpio4_paa4",
	"can_gpio5_paa5",
	"can_gpio6_paa6",
	"gpio_aud1_pj6",
	"gpio_aud0_pj5",
	"aud_mclk_pj4",
	"dap1_fs_pj3",
	"dap1_din_pj2",
	"dap1_dout_pj1",
	"dap1_sclk_pj0",
	"dap4_fs_pcc3",
	"dap4_din_pcc2",
	"dap4_dout_pcc1",
	"dap4_sclk_pcc0",
	"extperiph2_clk_po1",
	"extperiph1_clk_po0",
	"cam_i2c_sda_po3",
	"cam_i2c_scl_po2",
	"gpio_cam1_pn0",
	"dap2_din_pc3",
	"dap2_dout_pc2",
	"dap2_fs_pc4",
	"dap2_sclk_pc1",
	"uart4_cts_pb3",
	"uart4_rts_pb2",
	"uart4_rx_pb1",
	"uart4_tx_pb0",
	"gpio_wan4_pc0",
	"gpio_wan3_pb6",
	"gpio_wan2_pb5",
	"gpio_wan1_pb4",
	"gen1_i2c_scl_pc5",
	"gen1_i2c_sda_pc6",
	"uart1_cts_pt3",
	"uart1_rts_pt2",
	"uart1_rx_pt1",
	"uart1_tx_pt0",
	"directdc1_out3_pq5",
	"directdc1_out2_pq4",
	"directdc1_out1_pq3",
	"directdc1_out0_pq2",
	"directdc1_in_pq1",
	"directdc1_clk_pq0",
	"dp_aux_ch0_hpd_pp0",
	"dp_aux_ch1_hpd_pp1",
	"hdmi_cec_pp2",
	"pex_wake_n_pa2",
	"pex_l1_clkreq_n_pa4",
	"pex_l1_rst_n_pa3",
	"pex_l0_clkreq_n_pa1",
	"pex_l0_rst_n_pa0",
	"sdmmc1_clk_pd0",
	"sdmmc1_cmd_pd1",
	"sdmmc1_dat3_pd5",
	"sdmmc1_dat2_pd4",
	"sdmmc1_dat1_pd3",
	"sdmmc1_dat0_pd2",
	"eqos_mdc_pf5",
	"sdmmc3_dat3_pg5",
	"sdmmc3_dat2_pg4",
	"sdmmc3_dat1_pg3",
	"sdmmc3_dat0_pg2",
	"sdmmc3_comp",
	"sdmmc3_cmd_pg1",
	"sdmmc3_clk_pg0",
	"sdmmc4_clk",
	"sdmmc4_cmd",
	"sdmmc4_dqs",
	"sdmmc4_dat7",
	"sdmmc4_dat6",
	"sdmmc4_dat5",
	"sdmmc4_dat4",
	"sdmmc4_dat3",
	"sdmmc4_dat2",
	"sdmmc4_dat1",
	"sdmmc4_dat0",
	"qspi_io3_pr4",
	"qspi_io2_pr3",
	"qspi_io1_pr2",
	"qspi_io0_pr1",
	"qspi_sck_pr0",
	"qspi_cs_n_pr5",
	"qspi_comp",
	"gpio_sw1_pff1",
	"gpio_sw2_pff2",
	"gpio_sw3_pff3",
	"gpio_sw4_pff4",
	"shutdown",
	"safe_state_ps3",
	"vcomp_alert_ps4",
	"batt_oc_ps2",
	"power_on_pff0",
	"pwr_i2c_scl_ps0",
	"pwr_i2c_sda_ps1",
	"gpio_dis1_pu1",
	"gpio_dis3_pu3",
	"gpio_wan8_ph3",
	"gpio_wan7_ph2",
	"gpio_wan6_ph1",
	"gpio_wan5_ph0",
	"uart2_tx_px0",
	"uart2_rx_px1",
	"uart2_rts_px2",
	"uart2_cts_px3",
	"gpio_mdm1_py0",
	"gpio_mdm2_py1",
	"gpio_mdm3_py2",
	"gpio_mdm6_py5",
	"gpio_mdm7_py6",
	"bcpu_pwr_req_ph4",
	"mcpu_pwr_req_ph5",
	"gpu_pwr_req_ph6",
	"usb_vbus_en0_pl4",
	"usb_vbus_en1_pl5",
	"gp_pwm7_pl7",
	"gp_pwm6_pl6",
	"ufs0_rst_pbb1",
	"ufs0_ref_clk_pbb0",
};

static const char * const rsvd2_groups[] = {
	"touch_clk_pee2",
	"uart3_cts_pw5",
	"uart3_rts_pw4",
	"uart3_rx_pw3",
	"uart3_tx_pw2",
	"gen8_i2c_sda_pw1",
	"gen8_i2c_scl_pw0",
	"uart7_rx_pw7",
	"uart7_tx_pw6",
	"gpio_sen0_pv0",
	"gpio_sen1_pv1",
	"gpio_sen2_pv2",
	"gpio_sen3_pv3",
	"gpio_sen4_pv4",
	"gpio_sen5_pv5",
	"gpio_sen6_pv6",
	"gpio_sen7_pv7",
	"gpio_sen8_pee0",
	"gpio_sen9_pee1",
	"can_gpio7_paa7",
	"can1_dout_pz0",
	"can1_din_pz1",
	"can0_dout_pz2",
	"can0_din_pz3",
	"can_gpio2_paa2",
	"can_gpio3_paa3",
	"can_gpio4_paa4",
	"can_gpio5_paa5",
	"can_gpio6_paa6",
	"gpio_aud1_pj6",
	"gpio_aud0_pj5",
	"aud_mclk_pj4",
	"dap1_fs_pj3",
	"dap1_din_pj2",
	"dap1_dout_pj1",
	"dap1_sclk_pj0",
	"dmic1_clk_pm1",
	"dmic1_dat_pm0",
	"dmic2_dat_pm2",
	"dmic2_clk_pm3",
	"dmic4_dat_pm4",
	"dmic4_clk_pm5",
	"dap4_fs_pcc3",
	"dap4_din_pcc2",
	"dap4_dout_pcc1",
	"dap4_sclk_pcc0",
	"extperiph2_clk_po1",
	"extperiph1_clk_po0",
	"cam_i2c_sda_po3",
	"cam_i2c_scl_po2",
	"gpio_cam1_pn0",
	"gpio_cam2_pn1",
	"gpio_cam3_pn2",
	"gpio_cam4_pn3",
	"gpio_cam5_pn4",
	"gpio_cam6_pn5",
	"gpio_cam7_pn6",
	"dap2_din_pc3",
	"dap2_dout_pc2",
	"dap2_fs_pc4",
	"dap2_sclk_pc1",
	"uart4_cts_pb3",
	"uart4_rts_pb2",
	"uart4_rx_pb1",
	"uart4_tx_pb0",
	"gpio_wan4_pc0",
	"gpio_wan3_pb6",
	"gpio_wan2_pb5",
	"gpio_wan1_pb4",
	"gen1_i2c_scl_pc5",
	"gen1_i2c_sda_pc6",
	"uart1_cts_pt3",
	"uart1_rts_pt2",
	"uart1_rx_pt1",
	"uart1_tx_pt0",
	"directdc1_out3_pq5",
	"directdc1_out2_pq4",
	"directdc1_out1_pq3",
	"directdc1_out0_pq2",
	"directdc1_in_pq1",
	"directdc1_clk_pq0",
	"dp_aux_ch0_hpd_pp0",
	"dp_aux_ch1_hpd_pp1",
	"hdmi_cec_pp2",
	"pex_wake_n_pa2",
	"pex_l1_clkreq_n_pa4",
	"pex_l1_rst_n_pa3",
	"pex_l0_clkreq_n_pa1",
	"pex_l0_rst_n_pa0",
	"sdmmc1_clk_pd0",
	"sdmmc1_cmd_pd1",
	"sdmmc1_dat3_pd5",
	"sdmmc1_dat2_pd4",
	"sdmmc1_dat1_pd3",
	"sdmmc1_dat0_pd2",
	"eqos_td3_pe4",
	"eqos_td2_pe3",
	"eqos_td1_pe2",
	"eqos_td0_pe1",
	"eqos_rd3_pf1",
	"eqos_rd2_pf0",
	"eqos_rd1_pe7",
	"eqos_mdio_pf4",
	"eqos_rd0_pe6",
	"eqos_mdc_pf5",
	"eqos_comp",
	"eqos_txc_pe0",
	"eqos_rxc_pf3",
	"eqos_tx_ctl_pe5",
	"eqos_rx_ctl_pf2",
	"sdmmc3_dat3_pg5",
	"sdmmc3_dat2_pg4",
	"sdmmc3_dat1_pg3",
	"sdmmc3_dat0_pg2",
	"sdmmc3_comp",
	"sdmmc3_cmd_pg1",
	"sdmmc3_clk_pg0",
	"sdmmc4_clk",
	"sdmmc4_cmd",
	"sdmmc4_dqs",
	"sdmmc4_dat7",
	"sdmmc4_dat6",
	"sdmmc4_dat5",
	"sdmmc4_dat4",
	"sdmmc4_dat3",
	"sdmmc4_dat2",
	"sdmmc4_dat1",
	"sdmmc4_dat0",
	"qspi_io3_pr4",
	"qspi_io2_pr3",
	"qspi_io1_pr2",
	"qspi_io0_pr1",
	"qspi_sck_pr0",
	"qspi_cs_n_pr5",
	"qspi_comp",
	"gpio_sw1_pff1",
	"gpio_sw2_pff2",
	"gpio_sw3_pff3",
	"gpio_sw4_pff4",
	"shutdown",
	"safe_state_ps3",
	"vcomp_alert_ps4",
	"batt_oc_ps2",
	"power_on_pff0",
	"pwr_i2c_scl_ps0",
	"pwr_i2c_sda_ps1",
	"gpio_dis0_pu0",
	"gpio_dis2_pu2",
	"gpio_dis4_pu4",
	"uart2_tx_px0",
	"uart2_rx_px1",
	"uart2_rts_px2",
	"uart2_cts_px3",
	"uart5_rts_px6",
	"uart5_cts_px7",
	"gpio_mdm1_py0",
	"gpio_mdm2_py1",
	"gpio_mdm3_py2",
	"gpio_mdm5_py4",
	"gpio_mdm6_py5",
	"gpio_mdm7_py6",
	"bcpu_pwr_req_ph4",
	"mcpu_pwr_req_ph5",
	"gpu_pwr_req_ph6",
	"gen7_i2c_scl_pl0",
	"gen7_i2c_sda_pl1",
	"gen9_i2c_sda_pl3",
	"gen9_i2c_scl_pl2",
	"usb_vbus_en0_pl4",
	"usb_vbus_en1_pl5",
	"gp_pwm7_pl7",
	"gp_pwm6_pl6",
	"ufs0_rst_pbb1",
	"ufs0_ref_clk_pbb0",
};

static const char * const rsvd3_groups[] = {
	"touch_clk_pee2",
	"uart3_cts_pw5",
	"uart3_rts_pw4",
	"uart3_rx_pw3",
	"uart3_tx_pw2",
	"gen8_i2c_sda_pw1",
	"gen8_i2c_scl_pw0",
	"uart7_rx_pw7",
	"uart7_tx_pw6",
	"gpio_sen0_pv0",
	"gpio_sen1_pv1",
	"gpio_sen2_pv2",
	"gpio_sen3_pv3",
	"gpio_sen4_pv4",
	"gpio_sen5_pv5",
	"gpio_sen6_pv6",
	"gpio_sen7_pv7",
	"gpio_sen8_pee0",
	"gpio_sen9_pee1",
	"can_gpio7_paa7",
	"can1_dout_pz0",
	"can1_din_pz1",
	"can0_dout_pz2",
	"can0_din_pz3",
	"can_gpio0_paa0",
	"can_gpio1_paa1",
	"can_gpio2_paa2",
	"can_gpio3_paa3",
	"can_gpio4_paa4",
	"can_gpio5_paa5",
	"can_gpio6_paa6",
	"gpio_aud3_pk0",
	"gpio_aud2_pj7",
	"gpio_aud1_pj6",
	"gpio_aud0_pj5",
	"aud_mclk_pj4",
	"dap1_fs_pj3",
	"dap1_din_pj2",
	"dap1_dout_pj1",
	"dap1_sclk_pj0",
	"dmic1_clk_pm1",
	"dmic1_dat_pm0",
	"dmic2_dat_pm2",
	"dmic2_clk_pm3",
	"dmic4_dat_pm4",
	"dmic4_clk_pm5",
	"dap4_fs_pcc3",
	"dap4_din_pcc2",
	"dap4_dout_pcc1",
	"dap4_sclk_pcc0",
	"extperiph2_clk_po1",
	"extperiph1_clk_po0",
	"cam_i2c_sda_po3",
	"cam_i2c_scl_po2",
	"gpio_cam1_pn0",
	"gpio_cam2_pn1",
	"gpio_cam3_pn2",
	"gpio_cam4_pn3",
	"gpio_cam5_pn4",
	"gpio_cam6_pn5",
	"gpio_cam7_pn6",
	"dap2_din_pc3",
	"dap2_dout_pc2",
	"dap2_fs_pc4",
	"dap2_sclk_pc1",
	"uart4_cts_pb3",
	"uart4_rts_pb2",
	"uart4_rx_pb1",
	"uart4_tx_pb0",
	"gpio_wan4_pc0",
	"gpio_wan3_pb6",
	"gpio_wan2_pb5",
	"gpio_wan1_pb4",
	"gen1_i2c_scl_pc5",
	"gen1_i2c_sda_pc6",
	"uart1_cts_pt3",
	"uart1_rts_pt2",
	"uart1_rx_pt1",
	"uart1_tx_pt0",
	"directdc1_out3_pq5",
	"directdc1_out2_pq4",
	"directdc1_out1_pq3",
	"directdc1_out0_pq2",
	"directdc1_in_pq1",
	"directdc1_clk_pq0",
	"gpio_pq0_pi0",
	"gpio_pq1_pi1",
	"gpio_pq2_pi2",
	"gpio_pq3_pi3",
	"gpio_pq4_pi4",
	"gpio_pq5_pi5",
	"gpio_pq6_pi6",
	"gpio_pq7_pi7",
	"gpio_edp2_pp5",
	"gpio_edp3_pp6",
	"gpio_edp0_pp3",
	"gpio_edp1_pp4",
	"dp_aux_ch0_hpd_pp0",
	"dp_aux_ch1_hpd_pp1",
	"hdmi_cec_pp2",
	"pex_l2_clkreq_n",
	"pex_wake_n_pa2",
	"pex_l1_clkreq_n_pa4",
	"pex_l1_rst_n_pa3",
	"pex_l0_clkreq_n_pa1",
	"pex_l0_rst_n_pa0",
	"pex_l2_rst_n_pa5",
	"sdmmc1_clk_pd0",
	"sdmmc1_cmd_pd1",
	"sdmmc1_dat3_pd5",
	"sdmmc1_dat2_pd4",
	"sdmmc1_dat1_pd3",
	"sdmmc1_dat0_pd2",
	"eqos_td3_pe4",
	"eqos_td2_pe3",
	"eqos_td1_pe2",
	"eqos_td0_pe1",
	"eqos_rd3_pf1",
	"eqos_rd2_pf0",
	"eqos_rd1_pe7",
	"eqos_mdio_pf4",
	"eqos_rd0_pe6",
	"eqos_mdc_pf5",
	"eqos_comp",
	"eqos_txc_pe0",
	"eqos_rxc_pf3",
	"eqos_tx_ctl_pe5",
	"eqos_rx_ctl_pf2",
	"sdmmc3_dat3_pg5",
	"sdmmc3_dat2_pg4",
	"sdmmc3_dat1_pg3",
	"sdmmc3_dat0_pg2",
	"sdmmc3_comp",
	"sdmmc3_cmd_pg1",
	"sdmmc3_clk_pg0",
	"sdmmc4_clk",
	"sdmmc4_cmd",
	"sdmmc4_dqs",
	"sdmmc4_dat7",
	"sdmmc4_dat6",
	"sdmmc4_dat5",
	"sdmmc4_dat4",
	"sdmmc4_dat3",
	"sdmmc4_dat2",
	"sdmmc4_dat1",
	"sdmmc4_dat0",
	"qspi_io3_pr4",
	"qspi_io2_pr3",
	"qspi_io1_pr2",
	"qspi_io0_pr1",
	"qspi_sck_pr0",
	"qspi_cs_n_pr5",
	"qspi_comp",
	"gpio_sw1_pff1",
	"gpio_sw2_pff2",
	"gpio_sw3_pff3",
	"gpio_sw4_pff4",
	"shutdown",
	"safe_state_ps3",
	"vcomp_alert_ps4",
	"batt_oc_ps2",
	"power_on_pff0",
	"pwr_i2c_scl_ps0",
	"pwr_i2c_sda_ps1",
	"gpio_dis0_pu0",
	"gpio_dis1_pu1",
	"gpio_dis2_pu2",
	"gpio_dis3_pu3",
	"gpio_dis4_pu4",
	"gpio_dis5_pu5",
	"gpio_wan8_ph3",
	"gpio_wan7_ph2",
	"gpio_wan6_ph1",
	"gpio_wan5_ph0",
	"uart2_tx_px0",
	"uart2_rx_px1",
	"uart2_rts_px2",
	"uart2_cts_px3",
	"uart5_rx_px5",
	"uart5_tx_px4",
	"uart5_rts_px6",
	"uart5_cts_px7",
	"gpio_mdm1_py0",
	"gpio_mdm2_py1",
	"gpio_mdm3_py2",
	"gpio_mdm4_py3",
	"gpio_mdm5_py4",
	"gpio_mdm6_py5",
	"gpio_mdm7_py6",
	"bcpu_pwr_req_ph4",
	"mcpu_pwr_req_ph5",
	"gpu_pwr_req_ph6",
	"gen7_i2c_scl_pl0",
	"gen7_i2c_sda_pl1",
	"gen9_i2c_sda_pl3",
	"gen9_i2c_scl_pl2",
	"usb_vbus_en0_pl4",
	"usb_vbus_en1_pl5",
	"gp_pwm7_pl7",
	"gp_pwm6_pl6",
	"ufs0_rst_pbb1",
	"ufs0_ref_clk_pbb0",
};

static const char * const touch_groups[] = {
	"touch_clk_pee2",
};

static const char * const uartc_groups[] = {
	"uart3_cts_pw5",
	"uart3_rts_pw4",
	"uart3_rx_pw3",
	"uart3_tx_pw2",
};

static const char * const i2c8_groups[] = {
	"gen8_i2c_sda_pw1",
	"gen8_i2c_scl_pw0",
};

static const char * const uartg_groups[] = {
	"uart7_rx_pw7",
	"uart7_tx_pw6",
};

static const char * const spi2_groups[] = {
	"gpio_sen1_pv1",
	"gpio_sen2_pv2",
	"gpio_sen3_pv3",
	"gpio_sen4_pv4",
};

static const char * const gp_groups[] = {
	"gpio_sen6_pv6",
	"pex_l2_clkreq_n",
	"gpio_dis0_pu0",
	"gpio_dis2_pu2",
	"gpio_dis5_pu5",
	"uart5_rx_px5",
	"gp_pwm7_pl7",
	"gp_pwm6_pl6",
};

static const char * const dca_groups[] = {
	"gpio_dis2_pu2",
	"gpio_dis4_pu4",
};

static const char * const wdt_groups[] = {
	"gpio_sen7_pv7",
	"can_gpio7_paa7",
};

static const char * const i2c2_groups[] = {
	"gpio_sen8_pee0",
	"gpio_sen9_pee1",
};

static const char * const can1_groups[] = {
	"can1_dout_pz0",
	"can1_din_pz1",
};

static const char * const can0_groups[] = {
	"can0_dout_pz2",
	"can0_din_pz3",
};

static const char * const dmic3_groups[] = {
	"can_gpio0_paa0",
	"can_gpio1_paa1",
};

static const char * const dmic5_groups[] = {
	"can_gpio0_paa0",
	"can_gpio1_paa1",
};

static const char * const gpio_groups[] = {
	"can_gpio2_paa2",
};

static const char * const dspk1_groups[] = {
	"gpio_aud3_pk0",
	"gpio_aud2_pj7",
};

static const char * const spdif_groups[] = {
	"gpio_aud3_pk0",
	"gpio_aud2_pj7",
};

static const char * const aud_groups[] = {
	"aud_mclk_pj4",
};

static const char * const i2s1_groups[] = {
	"dap1_fs_pj3",
	"dap1_din_pj2",
	"dap1_dout_pj1",
	"dap1_sclk_pj0",
};

static const char * const dmic1_groups[] = {
	"dmic1_clk_pm1",
	"dmic1_dat_pm0",
};

static const char * const dmic2_groups[] = {
	"dmic2_dat_pm2",
	"dmic2_clk_pm3",
};

static const char * const i2s3_groups[] = {
	"dmic1_clk_pm1",
	"dmic1_dat_pm0",
	"dmic2_dat_pm2",
	"dmic2_clk_pm3",
};

static const char * const dmic4_groups[] = {
	"dmic4_dat_pm4",
	"dmic4_clk_pm5",
};

static const char * const dspk0_groups[] = {
	"dmic4_dat_pm4",
	"dmic4_clk_pm5",
};

static const char * const i2s4_groups[] = {
	"dap4_fs_pcc3",
	"dap4_din_pcc2",
	"dap4_dout_pcc1",
	"dap4_sclk_pcc0",
};

static const char * const extperiph2_groups[] = {
	"extperiph2_clk_po1"
};

static const char * const extperiph1_groups[] = {
	"extperiph1_clk_po0"
};

static const char * const i2c3_groups[] = {
	"cam_i2c_sda_po3",
	"cam_i2c_scl_po2",
};

static const char * const vgp1_groups[] = {
	"gpio_cam1_pn0",
};

static const char * const vgp2_groups[] = {
	"gpio_cam2_pn1",
};

static const char * const vgp3_groups[] = {
	"gpio_cam3_pn2",
};

static const char * const vgp4_groups[] = {
	"gpio_cam4_pn3",
};

static const char * const vgp5_groups[] = {
	"gpio_cam5_pn4",
};

static const char * const vgp6_groups[] = {
	"gpio_cam6_pn5",
};


static const char * const extperiph3_groups[] = {
	"gpio_cam2_pn1",
};

static const char * const extperiph4_groups[] = {
	"gpio_cam3_pn2",
};

static const char * const spi4_groups[] = {
	"gpio_cam4_pn3",
	"gpio_cam5_pn4",
	"gpio_cam6_pn5",
	"gpio_cam7_pn6",
};

static const char * const i2s2_groups[] = {
	"dap2_din_pc3",
	"dap2_dout_pc2",
	"dap2_fs_pc4",
	"dap2_sclk_pc1",
};

static const char * const uartd_groups[] = {
	"uart4_cts_pb3",
	"uart4_rts_pb2",
	"uart4_rx_pb1",
	"uart4_tx_pb0",
};

static const char * const i2c1_groups[] = {
	"gen1_i2c_scl_pc5",
	"gen1_i2c_sda_pc6",
};

static const char * const uarta_groups[] = {
	"uart1_cts_pt3",
	"uart1_rts_pt2",
	"uart1_rx_pt1",
	"uart1_tx_pt0",
};

static const char * const directdc1_groups[] = {
	"directdc1_out3_pq5",
	"directdc1_out2_pq4",
	"directdc1_out1_pq3",
	"directdc1_out0_pq2",
	"directdc1_in_pq1",
	"directdc1_clk_pq0",
};

static const char * const directdc_groups[] = {
	"directdc_comp",
};

static const char * const iqc0_groups[] = {
	"gpio_pq0_pi0",
	"gpio_pq1_pi1",
	"gpio_pq2_pi2",
	"gpio_pq3_pi3",
};


static const char * const iqc1_groups[] = {
	"gpio_pq4_pi4",
	"gpio_pq5_pi5",
	"gpio_pq6_pi6",
	"gpio_pq7_pi7",
};

static const char * const i2s6_groups[] = {
	"gpio_pq0_pi0",
	"gpio_pq1_pi1",
	"gpio_pq2_pi2",
	"gpio_pq3_pi3",
};

static const char * const dtv_groups[] = {
	"gpio_pq4_pi4",
	"gpio_pq5_pi5",
	"gpio_pq6_pi6",
	"gpio_pq7_pi7",
};

static const char * const uartf_groups[] = {
	"gpio_edp2_pp5",
	"gpio_edp3_pp6",
	"gpio_edp0_pp3",
	"gpio_edp1_pp4",
};

static const char * const sdmmc3_groups[] = {
	"gpio_edp2_pp5",
	"gpio_edp0_pp3",
	"sdmmc3_dat3_pg5",
	"sdmmc3_dat2_pg4",
	"sdmmc3_dat1_pg3",
	"sdmmc3_dat0_pg2",
	"sdmmc3_comp",
	"sdmmc3_cmd_pg1",
	"sdmmc3_clk_pg0",
};

static const char * const sdmmc4_groups[] = {
	"sdmmc4_clk",
	"sdmmc4_cmd",
	"sdmmc4_dqs",
	"sdmmc4_dat7",
	"sdmmc4_dat6",
	"sdmmc4_dat5",
	"sdmmc4_dat4",
	"sdmmc4_dat3",
	"sdmmc4_dat2",
	"sdmmc4_dat1",
	"sdmmc4_dat0",
};

static const char * const sdmmc1_groups[] = {
	"gpio_edp3_pp6",
	"gpio_edp1_pp4",
	"sdmmc1_dat3_pd5",
	"sdmmc1_dat2_pd4",
	"sdmmc1_dat1_pd3",
	"sdmmc1_dat0_pd2",
	"sdmmc1_comp",
	"sdmmc1_cmd_pd1",
	"sdmmc1_clk_pd0",
};

static const char * const dp_groups[] = {
	"dp_aux_ch0_hpd_pp0",
	"dp_aux_ch1_hpd_pp1",
};


static const char * const hdmi_groups[] = {
	"hdmi_cec_pp2",
};

static const char * const pe2_groups[] = {
	"pex_l2_rst_n_pa5",
	"pex_l2_clkreq_n_pa6",
};

static const char * const sata_groups[] = {
	"pex_l2_clkreq_n_pa6",
	"pex_l2_rst_n_pa5",
};

static const char * const pe_groups[] = {
	"pex_wake_n_pa2",
};

static const char * const pe1_groups[] = {
	"pex_l1_clkreq_n_pa4",
	"pex_l1_rst_n_pa3",
};

static const char * const pe0_groups[] = {
	"pex_l0_clkreq_n_pa1",
	"pex_l0_rst_n_pa0",
};

static const char * const soc_groups[] = {
	"pex_l2_rst_n_pa5",
	"eqos_mdio_pf4",
	"vcomp_alert_ps4",
	"batt_oc_ps2",
	"gpio_dis4_pu4",
	"gpio_mdm6_py5",
};

static const char * const eqos_groups[] = {
	"eqos_td3_pe4",
	"eqos_td2_pe3",
	"eqos_td1_pe2",
	"eqos_td0_pe1",
	"eqos_rd3_pf1",
	"eqos_rd2_pf0",
	"eqos_rd1_pe7",
	"eqos_mdio_pf4",
	"eqos_rd0_pe6",
	"eqos_mdc_pf5",
	"eqos_comp",
	"eqos_txc_pe0",
	"eqos_rxc_pf3",
	"eqos_tx_ctl_pe5",
	"eqos_rx_ctl_pf2",
};

static const char * const sdmmc2_groups[] = {
	"eqos_td3_pe4",
	"eqos_td2_pe3",
	"eqos_td1_pe2",
	"eqos_td0_pe1",
	"eqos_rd3_pf1",
	"eqos_rd2_pf0",
	"eqos_rd1_pe7",
	"eqos_rd0_pe6",
	"eqos_comp",
	"eqos_txc_pe0",
	"eqos_rxc_pf3",
	"eqos_tx_ctl_pe5",
	"eqos_rx_ctl_pf2",
};

static const char * const qspi_groups[] = {
	"qspi_io3_pr4",
	"qspi_io2_pr3",
	"qspi_io1_pr2",
	"qspi_io0_pr1",
	"qspi_sck_pr0",
	"qspi_cs_n_pr5",
	"qspi_comp",
};

static const char * const sce_groups[] = {
	"safe_state_ps3",
};

static const char * const i2c5_groups[] = {
	"pwr_i2c_scl_ps0",
	"pwr_i2c_sda_ps1",
};

static const char * const displaya_groups[] = {
	"gpio_dis1_pu1",
};

static const char * const displayb_groups[] = {
	"gpio_dis3_pu3",
};

static const char * const dcc_groups[] = {
	"gpio_dis5_pu5",
};

static const char * const spi1_groups[] = {
	"gpio_wan8_ph3",
	"gpio_wan7_ph2",
	"gpio_wan6_ph1",
	"gpio_wan5_ph0",
	"gpio_mdm4_py3",
	"gpio_mdm5_py4",
};

static const char * const uartb_groups[] = {
	"uart2_tx_px0",
	"uart2_rx_px1",
	"uart2_cts_px3",
	"uart2_rts_px2",
};

static const char * const uarte_groups[] = {
	"uart5_tx_px4",
	"uart5_rx_px5",
	"uart5_cts_px7",
	"uart5_rts_px6",
};

static const char * const spi3_groups[] = {
	"uart5_tx_px4",
	"uart5_rx_px5",
	"uart5_cts_px7",
	"uart5_rts_px6",
};

static const char * const nv_groups[] = {
	"uart5_tx_px4",
};

static const char * const ccla_groups[] = {
	"gpio_mdm4_py3",
};

static const char * const i2c7_groups[] = {
	"gen7_i2c_scl_pl0",
	"gen7_i2c_sda_pl1",
};

static const char * const i2c9_groups[] = {
	"gen9_i2c_sda_pl3",
	"gen9_i2c_scl_pl2",
};

static const char * const i2s5_groups[] = {
	"gen7_i2c_scl_pl0",
	"gen7_i2c_sda_pl1",
	"gen9_i2c_sda_pl3",
	"gen9_i2c_scl_pl2",
};

static const char * const usb_groups[] = {
	"usb_vbus_en0_pl4",
	"usb_vbus_en1_pl5",
};

static const char * const ufs0_groups[] = {
	"ufs0_rst_pbb1",
	"ufs0_ref_clk_pbb0",
};

#define FUNCTION(fname)					\
	{						\
		.name = #fname,				\
		.groups = fname##_groups,		\
		.ngroups = ARRAY_SIZE(fname##_groups),	\
	}						\

static struct tegra_function tegra186_functions[] = {
	FUNCTION(rsvd0),
	FUNCTION(rsvd1),
	FUNCTION(rsvd2),
	FUNCTION(rsvd3),
	FUNCTION(touch),
	FUNCTION(uartc),
	FUNCTION(i2c8),
	FUNCTION(uartg),
	FUNCTION(spi2),
	FUNCTION(gp),
	FUNCTION(dca),
	FUNCTION(wdt),
	FUNCTION(i2c2),
	FUNCTION(can1),
	FUNCTION(can0),
	FUNCTION(dmic3),
	FUNCTION(dmic5),
	FUNCTION(gpio),
	FUNCTION(dspk1),
	FUNCTION(dspk0),
	FUNCTION(spdif),
	FUNCTION(aud),
	FUNCTION(i2s1),
	FUNCTION(dmic1),
	FUNCTION(dmic2),
	FUNCTION(i2s3),
	FUNCTION(dmic4),
	FUNCTION(i2s4),
	FUNCTION(extperiph2),
	FUNCTION(extperiph1),
	FUNCTION(i2c3),
	FUNCTION(vgp1),
	FUNCTION(vgp2),
	FUNCTION(vgp3),
	FUNCTION(vgp4),
	FUNCTION(vgp5),
	FUNCTION(vgp6),
	FUNCTION(extperiph3),
	FUNCTION(extperiph4),
	FUNCTION(spi4),
	FUNCTION(i2s2),
	FUNCTION(uartd),
	FUNCTION(i2c1),
	FUNCTION(uarta),
	FUNCTION(directdc1),
	FUNCTION(directdc),
	FUNCTION(iqc0),
	FUNCTION(iqc1),
	FUNCTION(i2s6),
	FUNCTION(dtv),
	FUNCTION(uartf),
	FUNCTION(sdmmc3),
	FUNCTION(sdmmc4),
	FUNCTION(sdmmc1),
	FUNCTION(dp),
	FUNCTION(hdmi),
	FUNCTION(pe2),
	FUNCTION(sata),
	FUNCTION(pe),
	FUNCTION(pe1),
	FUNCTION(pe0),
	FUNCTION(soc),
	FUNCTION(eqos),
	FUNCTION(sdmmc2),
	FUNCTION(qspi),
	FUNCTION(sce),
	FUNCTION(i2c5),
	FUNCTION(displaya),
	FUNCTION(displayb),
	FUNCTION(dcc),
	FUNCTION(spi1),
	FUNCTION(uartb),
	FUNCTION(uarte),
	FUNCTION(spi3),
	FUNCTION(nv),
	FUNCTION(ccla),
	FUNCTION(i2c7),
	FUNCTION(i2c9),
	FUNCTION(i2s5),
	FUNCTION(usb),
	FUNCTION(ufs0),
};

#define PINGROUP_REG_Y(r) ((r))
#define PINGROUP_REG_N(r) -1

#define PINGROUP(pg_name, f0, f1, f2, f3, r, bank, pupd, e_io_hv, e_input, e_lpdr, e_pbias_buf, \
			gpio_sfio_sel, e_od, schmitt_b, drvtype, epreemp, io_reset, rfu_in)	\
	{	\
		.name = #pg_name,			\
		.pins = pg_name##_pins,		\
		.npins = ARRAY_SIZE(pg_name##_pins),		\
			.funcs = {					\
				TEGRA_MUX_ ## f0,			\
				TEGRA_MUX_ ## f1,			\
				TEGRA_MUX_ ## f2,			\
				TEGRA_MUX_ ## f3,			\
			},								\
		.mux_reg = PINGROUP_REG_Y(r), \
		.drvdn_bit = -1,				\
		.drvup_bit = -1,				\
		.slwr_bit = -1,					\
		.slwf_bit = -1,					\
		.lpmd_bit = -1,					\
		.lock_bit = -1,					\
		.hsm_bit = -1,					\
		.parked_bit = -1,				\
		.mux_bank = bank,					\
		.mux_bit = 0,					\
		.pupd_reg = PINGROUP_REG_##pupd(r),	\
		.pupd_bank = bank,					\
		.pupd_bit = 2,					\
		.tri_reg = PINGROUP_REG_Y(r),			\
		.tri_bank = bank,					\
		.tri_bit = 4,				\
		.e_io_hv_bit = e_io_hv,				\
		.einput_bit = e_input,				\
		.gpio_bit = gpio_sfio_sel,			\
		.odrain_bit = e_od,				\
		.schmitt_bit = schmitt_b,			\
		.drvtype_bit = 13,				\
		.lpdr_bit = e_lpdr,				\
		.pbias_buf_bit = e_io_hv,				\
		.preemp_bit = e_io_hv,				\
		.rfu_in_bit = 20,				\
		.drv_reg = -1,					\
	}

#define DRV_PINGROUP_Y(r) ((r))
#define DRV_PINGROUP_N(r) -1

#define DRV_PINGROUP(pg_name, r, drvdn_b, drvdn_w, drvup_b, drvup_w, slwr_b, slwr_w, slwf_b, slwf_w, bank)	\
	{							\
		.name = "drive_" #pg_name,			\
		.pins = drive_##pg_name##_pins,			\
		.npins = ARRAY_SIZE(drive_##pg_name##_pins),	\
		.mux_reg = -1,					\
		.pupd_reg = -1,					\
		.tri_reg = -1,					\
		.einput_bit = -1,				\
		.e_io_hv_bit = -1,				\
		.odrain_bit = -1,				\
		.lock_bit = -1,					\
		.parked_bit = -1,				\
		.lpmd_bit = -1,					\
		.drvtype_bit = -1,				\
		.lpdr_bit = -1,				\
		.pbias_buf_bit = -1,				\
		.preemp_bit = -1,				\
		.rfu_in_bit = -1,				\
		.drv_reg = DRV_PINGROUP_Y(r),			\
		.drv_bank = bank,				\
		.drvdn_bit = drvdn_b,				\
		.drvdn_width = drvdn_w,				\
		.drvup_bit = drvup_b,				\
		.drvup_width = drvup_w,				\
		.slwr_bit = slwr_b,				\
		.slwr_width = slwr_w,				\
		.slwf_bit = slwf_b,				\
		.slwf_width = slwf_w,				\
	}

static const struct tegra_pingroup tegra186_groups[] = {
		/*      pg_name,	f0,	f1,	f2,	f3,	r,	od,	schmitt_b,	hsm_b,	drvtype,	e_io_hv */
	PINGROUP(touch_clk_pee2,        TOUCH,        RSVD1,        RSVD2,        RSVD3,         0x2000,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart3_cts_pw5,        UARTC,        RSVD1,        RSVD2,        RSVD3,         0x2008,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart3_rts_pw4,        UARTC,        RSVD1,        RSVD2,        RSVD3,        0x2010,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart3_rx_pw3,        UARTC,        RSVD1,        RSVD2,        RSVD3,        0x2018,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart3_tx_pw2,        UARTC,        RSVD1,        RSVD2,        RSVD3,        0x2020,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gen8_i2c_sda_pw1,        I2C8,        RSVD1,        RSVD2,        RSVD3,        0x2028,        1,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gen8_i2c_scl_pw0,        I2C8,        RSVD1,        RSVD2,        RSVD3,        0x2030,        1,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart7_rx_pw7,        UARTG,        RSVD1,        RSVD2,        RSVD3,        0x2038,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart7_tx_pw6,        UARTG,        RSVD1,        RSVD2,        RSVD3,        0x2040,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_sen0_pv0,        RSVD0,        RSVD1,        RSVD2,        RSVD3,        0x2048,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_sen1_pv1,        SPI2,        RSVD1,        RSVD2,        RSVD3,        0x2050,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_sen2_pv2,        SPI2,        RSVD1,        RSVD2,        RSVD3,        0x2058,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_sen3_pv3,        SPI2,        RSVD1,        RSVD2,        RSVD3,        0x2060,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_sen4_pv4,        SPI2,        RSVD1,        RSVD2,        RSVD3,        0x2068,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_sen5_pv5,        RSVD0,        RSVD1,        RSVD2,        RSVD3,        0x2070,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_sen6_pv6,        RSVD0,        GP,        RSVD2,        RSVD3,        0x2078,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_sen7_pv7,        RSVD0,        WDT,        RSVD2,        RSVD3,        0x2080,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_sen8_pee0,        RSVD0,        I2C2,        RSVD2,        RSVD3,        0x2088,        1,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_sen9_pee1,        RSVD0,        I2C2,        RSVD2,        RSVD3,        0x2090,        1,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(can_gpio7_paa7,        RSVD0,        WDT,        RSVD2,        RSVD3,         0x3000,        1,        Y,	-1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(can1_dout_pz0,        CAN1,        RSVD1,        RSVD2,        RSVD3,         0x3008,       1,        Y,	-1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(can1_din_pz1,        CAN1,        RSVD1,        RSVD2,        RSVD3,        0x3010,        1,        Y,	-1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(can0_dout_pz2,        CAN0,        RSVD1,        RSVD2,        RSVD3,        0x3018,        1,        Y,	-1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(can0_din_pz3,        CAN0,        RSVD1,        RSVD2,        RSVD3,        0x3020,        1,        Y,	-1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(can_gpio0_paa0,        RSVD0,        DMIC3,        DMIC5,        RSVD3,        0x3028,        1,        Y,	-1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(can_gpio1_paa1,        RSVD0,        DMIC3,        DMIC5,        RSVD3,        0x3030,        1,        Y,	-1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(can_gpio2_paa2,        GPIO,        RSVD1,        RSVD2,        RSVD3,        0x3038,        1,        Y,	-1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(can_gpio3_paa3,        RSVD0,        RSVD1,        RSVD2,        RSVD3,        0x3040,        1,        Y,	-1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(can_gpio4_paa4,        RSVD0,        RSVD1,        RSVD2,        RSVD3,        0x3048,        1,        Y,	-1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(can_gpio5_paa5,        RSVD0,        RSVD1,        RSVD2,        RSVD3,        0x3050,        1,        Y,	-1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(can_gpio6_paa6,        RSVD0,        RSVD1,        RSVD2,        RSVD3,        0x3058,        1,        Y,	-1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(gpio_aud3_pk0,        RSVD0,        DSPK1,        SPDIF,        RSVD3,          0x1000,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_aud2_pj7,        RSVD0,        DSPK1,        SPDIF,        RSVD3,          0x1008,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_aud1_pj6,        RSVD0,        RSVD1,        RSVD2,        RSVD3,         0x1010,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_aud0_pj5,        RSVD0,        RSVD1,        RSVD2,        RSVD3,         0x1018,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(aud_mclk_pj4,        AUD,        RSVD1,        RSVD2,        RSVD3,         0x1020,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(dap1_fs_pj3,        I2S1,        RSVD1,        RSVD2,        RSVD3,         0x1028,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(dap1_din_pj2,        I2S1,        RSVD1,        RSVD2,        RSVD3,         0x1030,       0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(dap1_dout_pj1,        I2S1,        RSVD1,        RSVD2,        RSVD3,         0x1038,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(dap1_sclk_pj0,        I2S1,        RSVD1,        RSVD2,        RSVD3,         0x1040,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(dmic1_clk_pm1,        DMIC1,        I2S3,        RSVD2,        RSVD3,          0x2000,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(dmic1_dat_pm0,        DMIC1,        I2S3,        RSVD2,        RSVD3,          0x2008,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(dmic2_dat_pm2,        DMIC2,        I2S3,        RSVD2,        RSVD3,         0x2010,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(dmic2_clk_pm3,        DMIC2,        I2S3,        RSVD2,        RSVD3,         0x2018,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(dmic4_dat_pm4,        DMIC4,        DSPK0,        RSVD2,        RSVD3,         0x2020,       0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(dmic4_clk_pm5,        DMIC4,        DSPK0,        RSVD2,        RSVD3,         0x2028,       0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(dap4_fs_pcc3,        I2S4,        RSVD1,        RSVD2,        RSVD3,         0x2030,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(dap4_din_pcc2,        I2S4,        RSVD1,        RSVD2,        RSVD3,         0x2038,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(dap4_dout_pcc1,        I2S4,        RSVD1,        RSVD2,        RSVD3,         0x2040,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(dap4_sclk_pcc0,        I2S4,        RSVD1,        RSVD2,        RSVD3,         0x2048,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(extperiph2_clk_po1,        EXTPERIPH2,        RSVD1,        RSVD2,        RSVD3,          0x0000,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(extperiph1_clk_po0,        EXTPERIPH1,        RSVD1,        RSVD2,        RSVD3,          0x0008,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(cam_i2c_sda_po3,        I2C3,        RSVD1,        RSVD2,        RSVD3,         0x0010,        0,        Y,    5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(cam_i2c_scl_po2,        I2C3,        RSVD1,        RSVD2,        RSVD3,         0x0018,        0,        Y,    5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_cam1_pn0,        VGP1,        RSVD1,        RSVD2,        RSVD3,         0x0020,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_cam2_pn1,        VGP2,        EXTPERIPH3,        RSVD2,        RSVD3,         0x0028,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_cam3_pn2,        VGP3,        EXTPERIPH4,        RSVD2,        RSVD3,         0x0030,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_cam4_pn3,        VGP4,        SPI4,        RSVD2,        RSVD3,         0x0038,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_cam5_pn4,        VGP5,        SPI4,        RSVD2,        RSVD3,         0x0040,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_cam6_pn5,        VGP6,        SPI4,        RSVD2,        RSVD3,         0x0048,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_cam7_pn6,        RSVD0,        SPI4,        RSVD2,        RSVD3,         0x0050,       0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(dap2_din_pc3,        I2S2,        RSVD1,        RSVD2,        RSVD3,          0x4000,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(dap2_dout_pc2,        I2S2,        RSVD1,        RSVD2,        RSVD3,          0x4008,       0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(dap2_fs_pc4,        I2S2,        RSVD1,        RSVD2,        RSVD3,         0x4010,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(dap2_sclk_pc1,        I2S2,        RSVD1,        RSVD2,        RSVD3,         0x4018,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart4_cts_pb3,        UARTD,        RSVD1,        RSVD2,        RSVD3,         0x4020,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart4_rts_pb2,        UARTD,        RSVD1,        RSVD2,        RSVD3,         0x4028,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart4_rx_pb1,        UARTD,        RSVD1,        RSVD2,        RSVD3,         0x4030,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart4_tx_pb0,        UARTD,        RSVD1,        RSVD2,        RSVD3,         0x4038,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_wan4_pc0,        RSVD0,        RSVD1,        RSVD2,        RSVD3,         0x4040,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,     N),
	PINGROUP(gpio_wan3_pb6,        RSVD0,        RSVD1,        RSVD2,        RSVD3,         0x4048,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_wan2_pb5,        RSVD0,        RSVD1,        RSVD2,        RSVD3,         0x4050,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_wan1_pb4,        RSVD0,        RSVD1,        RSVD2,        RSVD3,         0x4058,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gen1_i2c_scl_pc5,        I2C1,        RSVD1,        RSVD2,        RSVD3,         0x4060,        0,        Y,    5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gen1_i2c_sda_pc6,        I2C1,        RSVD1,        RSVD2,        RSVD3,         0x4068,        0,        Y,    5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart1_cts_pt3,        UARTA,        RSVD1,        RSVD2,        RSVD3,          0x5000,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart1_rts_pt2,        UARTA,        RSVD1,        RSVD2,        RSVD3,          0x5008,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart1_rx_pt1,        UARTA,        RSVD1,        RSVD2,        RSVD3,         0x5010,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart1_tx_pt0,        UARTA,        RSVD1,        RSVD2,        RSVD3,         0x5018,        0,        Y,    -1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(directdc1_out3_pq5,        DIRECTDC1,        RSVD1,        RSVD2,        RSVD3,         0x5028,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    15,    17,    Y),
	PINGROUP(directdc1_out2_pq4,        DIRECTDC1,        RSVD1,        RSVD2,        RSVD3,         0x5030,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    15,    17,    Y),
	PINGROUP(directdc1_out1_pq3,        DIRECTDC1,        RSVD1,        RSVD2,        RSVD3,         0x5038,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    15,    17,    Y),
	PINGROUP(directdc1_out0_pq2,        DIRECTDC1,        RSVD1,        RSVD2,        RSVD3,         0x5040,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    15,    17,    Y),
	PINGROUP(directdc1_in_pq1,        DIRECTDC1,        RSVD1,        RSVD2,        RSVD3,         0x5048,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    15,    17,    Y),
	PINGROUP(directdc1_clk_pq0,        DIRECTDC1,        RSVD1,        RSVD2,        RSVD3,         0x5050,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    15,    17,    Y),
	PINGROUP(directdc_comp,        DIRECTDC,        RSVD1,        RSVD2,        RSVD3,         0x5058,        0,        Y,    -1,    -1,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(gpio_pq0_pi0,        RSVD0,        IQC0,        I2S6,        RSVD3,          0x3000,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(gpio_pq1_pi1,        RSVD0,        IQC0,        I2S6,        RSVD3,          0x3008,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(gpio_pq2_pi2,        RSVD0,        IQC0,        I2S6,        RSVD3,         0x3010,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(gpio_pq3_pi3,        RSVD0,        IQC0,        I2S6,        RSVD3,         0x3018,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(gpio_pq4_pi4,        RSVD0,        IQC1,        DTV,        RSVD3,         0x3020,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(gpio_pq5_pi5,        RSVD0,        IQC1,        DTV,        RSVD3,         0x3028,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(gpio_pq6_pi6,        RSVD0,        IQC1,        DTV,        RSVD3,         0x3030,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(gpio_pq7_pi7,        RSVD0,        IQC1,        DTV,        RSVD3,         0x3038,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(gpio_edp2_pp5,        RSVD0,        UARTF,        SDMMC3,        RSVD3,         0x10000,        0,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_edp3_pp6,        RSVD0,        UARTF,        SDMMC1,        RSVD3,         0x10008,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_edp0_pp3,        RSVD0,        UARTF,        SDMMC3,        RSVD3,        0x10010,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_edp1_pp4,        RSVD0,        UARTF,        SDMMC1,        RSVD3,        0x10018,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(dp_aux_ch0_hpd_pp0,        DP,        RSVD1,        RSVD2,        RSVD3,        0x10020,        0,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(dp_aux_ch1_hpd_pp1,        DP,        RSVD1,        RSVD2,        RSVD3,        0x10028,        0,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(hdmi_cec_pp2,        HDMI,        RSVD1,        RSVD2,        RSVD3,        0x10030,        0,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(pex_l2_clkreq_n_pa6,        PE2,        GP,        SATA,        RSVD3,          0x7000,        0,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(pex_wake_n_pa2,        PE,        RSVD1,        RSVD2,        RSVD3,          0x7008,        0,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(pex_l1_clkreq_n_pa4,        PE1,        RSVD1,        RSVD2,        RSVD3,          0x7010,        0,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(pex_l1_rst_n_pa3,        PE1,        RSVD1,        RSVD2,        RSVD3,         0x7018,        0,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(pex_l0_clkreq_n_pa1,        PE0,        RSVD1,        RSVD2,        RSVD3,         0x7020,        0,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(pex_l0_rst_n_pa0,        PE0,        RSVD1,        RSVD2,        RSVD3,         0x7028,        0,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(pex_l2_rst_n_pa5,        PE2,        SOC,        SATA,        RSVD3,         0x7030,        0,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(sdmmc1_clk_pd0,        SDMMC1,        RSVD1,        RSVD2,        RSVD3,          0x8000,        0,        Y,    5,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(sdmmc1_cmd_pd1,        SDMMC1,        RSVD1,        RSVD2,        RSVD3,          0x8008,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,   Y),
	PINGROUP(sdmmc1_comp,        SDMMC1,        RSVD1,        RSVD2,        RSVD3,          0x8010,        0,        Y,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    N,    -1,    -1,   N),
	PINGROUP(sdmmc1_dat3_pd5,        SDMMC1,        RSVD1,        RSVD2,        RSVD3,         0x8014,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(sdmmc1_dat2_pd4,        SDMMC1,        RSVD1,        RSVD2,        RSVD3,         0x801c,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(sdmmc1_dat1_pd3,        SDMMC1,        RSVD1,        RSVD2,        RSVD3,         0x8024,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(sdmmc1_dat0_pd2,        SDMMC1,        RSVD1,        RSVD2,        RSVD3,         0x802c,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(eqos_td3_pe4,        EQOS,        SDMMC2,        RSVD2,        RSVD3,          0x9000,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(eqos_td2_pe3,        EQOS,        SDMMC2,        RSVD2,        RSVD3,          0x9008,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(eqos_td1_pe2,        EQOS,        SDMMC2,        RSVD2,        RSVD3,         0x9010,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(eqos_td0_pe1,        EQOS,        SDMMC2,        RSVD2,        RSVD3,         0x9018,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(eqos_rd3_pf1,        EQOS,        SDMMC2,        RSVD2,        RSVD3,         0x9020,        0,        Y,    5,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(eqos_rd2_pf0,        EQOS,        SDMMC2,        RSVD2,        RSVD3,         0x9028,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(eqos_rd1_pe7,        EQOS,        SDMMC2,        RSVD2,        RSVD3,         0x9030,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(eqos_mdio_pf4,        EQOS,        SOC,        RSVD2,        RSVD3,         0x9038,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(eqos_rd0_pe6,        EQOS,        SDMMC2,        RSVD2,        RSVD3,         0x9040,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(eqos_mdc_pf5,        EQOS,        RSVD1,        RSVD2,        RSVD3,         0x9048,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(eqos_comp,        EQOS,        SDMMC2,        RSVD2,        RSVD3,         0x9050,        0,        Y,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    N,    -1,    -1,    N),
	PINGROUP(eqos_txc_pe0,        EQOS,        SDMMC2,        RSVD2,        RSVD3,         0x9054,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(eqos_rxc_pf3,        EQOS,        SDMMC2,        RSVD2,        RSVD3,         0x905c,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(eqos_tx_ctl_pe5,        EQOS,        SDMMC2,        RSVD2,        RSVD3,         0x9064,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(eqos_rx_ctl_pf2,        EQOS,        SDMMC2,        RSVD2,        RSVD3,         0x906c,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(sdmmc3_dat3_pg5,        SDMMC3,        RSVD1,        RSVD2,        RSVD3,          0xa000,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(sdmmc3_dat2_pg4,        SDMMC3,        RSVD1,        RSVD2,        RSVD3,          0xa008,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(sdmmc3_dat1_pg3,        SDMMC3,        RSVD1,        RSVD2,        RSVD3,         0xa010,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(sdmmc3_dat0_pg2,        SDMMC3,        RSVD1,        RSVD2,        RSVD3,         0xa018,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(sdmmc3_comp,        SDMMC3,        RSVD1,        RSVD2,        RSVD3,          0xa020,        0,        Y,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    N,    -1,    -1,   N),
	PINGROUP(sdmmc3_cmd_pg1,        SDMMC3,        RSVD1,        RSVD2,        RSVD3,         0xa024,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(sdmmc3_clk_pg0,        SDMMC3,        RSVD1,        RSVD1,        RSVD3,         0xa02c,        0,        Y,    5,    6,    -1,    9,    10,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(sdmmc4_clk,        SDMMC4,        RSVD1,        RSVD2,        RSVD3,          0x6004,        0,        Y,    5,    6,    -1,    9,    -1,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(sdmmc4_cmd,        SDMMC4,        RSVD1,        RSVD2,        RSVD3,          0x6008,        0,        Y,    -1,    6,    -1,    9,    -1,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(sdmmc4_dqs,        SDMMC4,        RSVD1,        RSVD2,        RSVD3,          0x600c,        0,        Y,    -1,    6,    -1,    9,    -1,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(sdmmc4_dat7,       SDMMC4,        RSVD1,        RSVD2,        RSVD3,          0x6010,        0,       Y,    -1,    6,    -1,    9,    -1,    -1,    12,    Y,    -1,    -1,   Y),
	PINGROUP(sdmmc4_dat6,       SDMMC4,        RSVD1,        RSVD2,        RSVD3,          0x6014,        0,       Y,    -1,    6,    -1,    9,    -1,    -1,    12,    Y,    -1,    -1,   Y),
	PINGROUP(sdmmc4_dat5,       SDMMC4,        RSVD1,        RSVD2,        RSVD3,          0x6018,        0,       Y,    -1,    6,    -1,    9,    -1,    -1,    12,    Y,    -1,    -1,   Y),
	PINGROUP(sdmmc4_dat4,       SDMMC4,        RSVD1,        RSVD2,        RSVD3,          0x601c,        0,       Y,    -1,    6,    -1,    9,    -1,    -1,    12,    Y,    -1,    -1,   Y),
	PINGROUP(sdmmc4_dat3,       SDMMC4,        RSVD1,        RSVD2,        RSVD3,          0x6020,        0,       Y,    -1,    6,    -1,    9,    -1,    -1,    12,    Y,    -1,    -1,   Y),
	PINGROUP(sdmmc4_dat2,       SDMMC4,        RSVD1,        RSVD2,        RSVD3,          0x6024,        0,       Y,    -1,    6,    -1,    9,    -1,    -1,    12,    Y,    -1,    -1,   Y),
	PINGROUP(sdmmc4_dat1,       SDMMC4,        RSVD1,        RSVD2,        RSVD3,          0x6028,        0,       Y,    -1,    6,    -1,    9,    -1,    -1,    12,    Y,    -1,    -1,   Y),
	PINGROUP(sdmmc4_dat0,       SDMMC4,        RSVD1,        RSVD2,        RSVD3,          0x602c,        0,       Y,    -1,    6,    -1,    9,    -1,    -1,    12,    Y,    -1,    -1,    Y),
	PINGROUP(qspi_io3_pr4,        QSPI,        RSVD1,        RSVD2,        RSVD3,          0xB000,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    15,    17,    Y),
	PINGROUP(qspi_io2_pr3,        QSPI,        RSVD1,        RSVD2,        RSVD3,          0xB008,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    15,    17,    Y),
	PINGROUP(qspi_io1_pr2,        QSPI,        RSVD1,        RSVD2,        RSVD3,         0xB010,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    15,   17,    Y),
	PINGROUP(qspi_io0_pr1,        QSPI,        RSVD1,        RSVD2,        RSVD3,         0xB018,        0,        Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    15,   17,    Y),
	PINGROUP(qspi_sck_pr0,        QSPI,        RSVD1,        RSVD2,        RSVD3,         0xB020,        0,        Y,    5,    6,    -1,    9,    10,    -1,    12,    Y,    15,	17,    Y),
	PINGROUP(qspi_cs_n_pr5,        QSPI,        RSVD1,        RSVD2,        RSVD3,         0xB028,        0,       Y,    -1,    6,    -1,    9,    10,    -1,    12,    Y,    15,	17,    Y),
	PINGROUP(qspi_comp,        QSPI,        RSVD1,        RSVD2,        RSVD3,         0xB030,        0,       Y,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    Y,    -1,	-1,    Y),
	PINGROUP(gpio_sw1_pff1,        RSVD0,        RSVD1,        RSVD2,        RSVD3,         0x1000,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_sw2_pff2,        RSVD0,        RSVD1,        RSVD2,        RSVD3,         0x1008,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_sw3_pff3,        RSVD0,        RSVD1,        RSVD2,        RSVD3,        0x1010,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_sw4_pff4,        RSVD0,        RSVD1,        RSVD2,        RSVD3,        0x1018,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(shutdown,        RSVD0,        RSVD1,        RSVD2,        RSVD3,        0x1020,        1,        Y,	-1,    6,    8,    -1,    -1,    -1,    12,    N,    -1,    -1,    N),
	PINGROUP(pmu_int,        RSVD0,        RSVD1,        RSVD2,        RSVD3,        0x1028,        1,        Y,	-1,    6,    8,    -1,    -1,    -1,    12,    N,    -1,    -1,    N),
	PINGROUP(safe_state_ps3,        SCE,        RSVD1,        RSVD2,        RSVD3,        0x1030,        1,        Y,		-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(vcomp_alert_ps4,        SOC,        RSVD1,        RSVD2,        RSVD3,        0x1038,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,   -1,    N),
	PINGROUP(soc_pwr_req,        RSVD0,        RSVD1,        RSVD2,        RSVD3,        0x1040,        1,        Y,	-1,    6,    8,    -1,    -1,    -1,    12,    N,    -1,   -1,    N),
	PINGROUP(batt_oc_ps2,        SOC,        RSVD1,        RSVD2,        RSVD3,        0x1048,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(clk_32k_in,        RSVD0,        RSVD1,        RSVD2,        RSVD3,        0x1050,        1,        Y,	-1,    6,    8,    -1,    -1,    -1,    -1,    N,    -1,    -1,    N),
	PINGROUP(power_on_pff0,        RSVD0,        RSVD1,        RSVD2,        RSVD3,        0x1058,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(pwr_i2c_scl_ps0,        I2C5,        RSVD1,        RSVD2,        RSVD3,        0x1060,        1,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(pwr_i2c_sda_ps1,        I2C5,        RSVD1,        RSVD2,        RSVD3,        0x1068,        1,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_dis0_pu0,        RSVD0,        GP,        RSVD2,        RSVD3,        0x1080,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_dis1_pu1,        RSVD0,        RSVD1,        DISPLAYA,        RSVD3,        0x1088,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_dis2_pu2,        RSVD0,        GP,        DCA,        RSVD3,        0x1090,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_dis3_pu3,        RSVD0,        RSVD1,        DISPLAYB,        RSVD3,        0x1098,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_dis4_pu4,        RSVD0,        SOC,        DCA,        RSVD3,        0x10a0,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_dis5_pu5,        RSVD0,        GP,        DCC,        RSVD3,        0x10a8,        1,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_wan8_ph3,        RSVD0,        RSVD1,        SPI1,        RSVD3,          0xd000,        0,        Y,		-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_wan7_ph2,        RSVD0,        RSVD1,        SPI1,        RSVD3,          0xd008,        0,        Y,		-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_wan6_ph1,        RSVD0,        RSVD1,        SPI1,        RSVD3,         0xd010,        0,        Y,		-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_wan5_ph0,        RSVD0,        RSVD1,        SPI1,        RSVD3,         0xd018,        0,        Y,		-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart2_tx_px0,        UARTB,        RSVD1,        RSVD2,        RSVD3,         0xd020,        0,        Y,		-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart2_rx_px1,        UARTB,        RSVD1,        RSVD2,        RSVD3,         0xd028,        0,        Y,		-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart2_rts_px2,        UARTB,        RSVD1,        RSVD2,        RSVD3,         0xd030,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart2_cts_px3,        UARTB,        RSVD1,        RSVD2,        RSVD3,         0xd038,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart5_rx_px5,        UARTE,        SPI3,        GP,        RSVD3,         0xd040,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart5_tx_px4,        UARTE,        SPI3,        NV,        RSVD3,         0xd048,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart5_rts_px6,        UARTE,        SPI3,        RSVD2,        RSVD3,         0xd050,        0,        Y,		-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(uart5_cts_px7,        UARTE,        SPI3,        RSVD2,        RSVD3,         0xd058,        0,        Y,		-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_mdm1_py0,        RSVD0,        RSVD1,        RSVD2,        RSVD3,         0xd060,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_mdm2_py1,        RSVD0,        RSVD1,        RSVD2,        RSVD3,         0xd068,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_mdm3_py2,        RSVD0,        RSVD1,        RSVD2,        RSVD3,         0xd070,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_mdm4_py3,        RSVD0,        SPI1,        CCLA,        RSVD3,         0xd078,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_mdm5_py4,        RSVD0,        SPI1,        RSVD2,        RSVD3,         0xd080,        0,        Y,		-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_mdm6_py5,        SOC,        RSVD1,        RSVD2,        RSVD3,         0xd088,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpio_mdm7_py6,        RSVD0,        RSVD1,        RSVD2,        RSVD3,         0xd090,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(bcpu_pwr_req_ph4,        RSVD0,        RSVD1,        RSVD2,        RSVD3,         0xd098,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(mcpu_pwr_req_ph5,        RSVD0,        RSVD1,        RSVD2,        RSVD3,         0xd0a0,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gpu_pwr_req_ph6,        RSVD0,        RSVD1,        RSVD2,        RSVD3,         0xd0a8,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gen7_i2c_scl_pl0,        I2C7,        I2S5,        RSVD2,        RSVD3,         0xd0b0,        0,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gen7_i2c_sda_pl1,        I2C7,        I2S5,        RSVD2,        RSVD3,         0xd0b8,        0,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gen9_i2c_sda_pl3,        I2C9,        I2S5,        RSVD2,        RSVD3,         0xd0c0,        0,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gen9_i2c_scl_pl2,        I2C9,        I2S5,        RSVD2,        RSVD3,         0xd0c8,        0,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(usb_vbus_en0_pl4,        USB,        RSVD1,        RSVD2,        RSVD3,         0xd0d0,        0,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(usb_vbus_en1_pl5,        USB,        RSVD1,        RSVD2,        RSVD3,         0xd0d8,        0,        Y,	5,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gp_pwm7_pl7,        GP,        RSVD1,        RSVD2,        RSVD3,         0xd0e0,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(gp_pwm6_pl6,        GP,        RSVD1,        RSVD2,        RSVD3,         0xd0e8,        0,        Y,	-1,    6,    8,    -1,    10,    11,    12,    N,    -1,    -1,    N),
	PINGROUP(ufs0_rst_pbb1,        UFS0,        RSVD1,        RSVD2,        RSVD3,         0x11000,        0,        Y,		-1,    6,    -1,    9,    10,    -1,    12,    Y,    15,    17,    Y),
	PINGROUP(ufs0_ref_clk_pbb0,        UFS0,        RSVD1,        RSVD2,        RSVD3,         0x11008,        0,        Y,	-1,    6,    -1,    9,    10,    -1,    12,    Y,    15,    17,    Y),

	DRV_PINGROUP(touch_clk,    0x2004,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(uart3_cts,    0x200c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(uart3_rts,    0x2014,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(uart3_rx,    0x201c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(uart3_tx,    0x2024,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gen8_i2c_sda,    0x202c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gen8_i2c_scl,    0x2034,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(uart7_rx,    0x203c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(uart7_tx,    0x2044,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_sen0,    0x204c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_sen1,    0x2054,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_sen2,    0x205c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_sen3,    0x2064,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_sen4,    0x206c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_sen5,    0x2074,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_sen6,    0x207c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_sen7,    0x2084,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_sen8,    0x208c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_sen9,    0x2094,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(can_gpio7,    0x3004,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	1),
	DRV_PINGROUP(can1_dout,    0x300C,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	1),
	DRV_PINGROUP(can1_din,    0x3014,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	1),
	DRV_PINGROUP(can0_dout,    0x301c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	1),
	DRV_PINGROUP(can0_din,    0x3024,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	1),
	DRV_PINGROUP(can_gpio0,    0x302c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	1),
	DRV_PINGROUP(can_gpio1,    0x3034,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	1),
	DRV_PINGROUP(can_gpio2,    0x303c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	1),
	DRV_PINGROUP(can_gpio3,    0x3044,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	1),
	DRV_PINGROUP(can_gpio4,    0x304c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	1),
	DRV_PINGROUP(can_gpio5,    0x3054,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	1),
	DRV_PINGROUP(can_gpio6,    0x305c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	1),
	DRV_PINGROUP(gpio_aud3,     0x1004,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_aud2,     0x100c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_aud1,     0x1014,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_aud0,     0x101c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(aud_mclk,     0x1024,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(dap1_fs,     0x102c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(dap1_din,     0x1034,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(dap1_dout,     0x103c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(dap1_sclk,     0x1044,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(dmic1_clk,     0x2004,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(dmic1_dat,     0x200c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(dmic2_dat,     0x2014,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(dmic2_clk,     0x201c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(dmic4_dat,     0x2024,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(dmic4_clk,     0x202c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(dap4_fs,     0x2034,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(dap4_din,     0x203c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(dap4_dout,     0x2044,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(dap4_sclk,     0x204c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(extperiph2_clk,     0x0004,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(extperiph1_clk,     0x000c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(cam_i2c_sda,     0x0014,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(cam_i2c_scl,     0x001c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_cam1,     0x0024,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_cam2,     0x002c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_cam3,     0x0034,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_cam4,     0x003c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_cam5,     0x0044,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_cam6,     0x004c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_cam7,     0x0054,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(dap2_din,     0x4004,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(dap2_dout,     0x400c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(dap2_fs,     0x4014,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(dap2_sclk,     0x401c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(uart4_cts,     0x4024,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(uart4_rts,     0x402c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(uart4_rx,     0x4034,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(uart4_tx,     0x403c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_wan4,     0x4044,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_wan3,     0x404c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_wan2,     0x4054,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_wan1,     0x405c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gen1_i2c_scl,     0x4064,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gen1_i2c_sda,     0x406c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(uart1_cts,     0x5004,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(uart1_rts,     0x500c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(uart1_rx,     0x5014,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(uart1_tx,     0x501c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(directdc1_out3,     0x502c,    12,    9,    24,    8,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(directdc1_out2,     0x5034,    12,    9,    24,    8,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(directdc1_out1,     0x503c,    12,    9,    24,    8,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(directdc1_out0,     0x5044,    12,    9,    24,    8,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(directdc1_in,     0x504c,    12,    9,    24,    8,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(directdc1_clk,     0x5054,    12,    9,    24,    8,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_pq0,     0x3004,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(gpio_pq1,     0x300c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(gpio_pq2,     0x3014,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(gpio_pq3,     0x301c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(gpio_pq4,     0x3024,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(gpio_pq5,     0x302c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(gpio_pq6,     0x3034,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(gpio_pq7,     0x303c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(gpio_edp2,    0x10004,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_edp3,    0x1000c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_edp0,    0x10014,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_edp1,    0x1001c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(dp_aux_ch0_hpd,    0x10024,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(dp_aux_ch1_hpd,    0x1002c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(hdmi_cec,    0x10034,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(pex_l2_clkreq_n,     0x7004,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(pex_wake_n,     0x700c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(pex_l1_clkreq_n,     0x7014,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(pex_l1_rst_n,     0x701c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(pex_l0_clkreq_n,     0x7024,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(pex_l0_rst_n,     0x702c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(pex_l2_rst_n,     0x7034,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(sdmmc1_clk,     0x8004,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(sdmmc1_cmd,     0x800c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(sdmmc1_dat3,     0x8018,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(sdmmc1_dat2,     0x8020,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(sdmmc1_dat1,     0x8028,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(sdmmc1_dat0,     0x8030,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(eqos_td3,     0x9004,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(eqos_td2,     0x900c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(eqos_td1,     0x9014,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(eqos_td0,     0x901c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(eqos_rd3,     0x9024,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(eqos_rd2,     0x902c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(eqos_rd1,     0x9034,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(eqos_mdio,     0x903c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(eqos_rd0,     0x9044,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(eqos_mdc,     0x904c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(eqos_txc,     0x9058,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(eqos_rxc,     0x9060,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(eqos_tx_ctl,     0x9068,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(eqos_rx_ctl,     0x9070,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(sdmmc3_dat3,     0xa004,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(sdmmc3_dat2,     0xa00c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(sdmmc3_dat1,     0xa014,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(sdmmc3_dat0,     0xa01c,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(sdmmc3_cmd,     0xa028,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(sdmmc3_clk,     0xa030,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(qspi_io3,     0xB004,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(qspi_io2,     0xB00C,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(qspi_io1,     0xB014,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(qspi_io0,     0xB01C,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(qspi_sck,     0xB024,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(qspi_cs_n,     0xB02C,    -1,    -1,    -1,    -1,    28,    2,    30,    2,	0),
	DRV_PINGROUP(gpio_sw1,     0x1004,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_sw2,     0x100c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_sw3,     0x1014,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_sw4,     0x101c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(shutdown,     0x1024,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(pmu_int,     0x102C,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(safe_state,     0x1034,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(vcomp_alert,     0x103c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(soc_pwr_req,     0x1044,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(batt_oc,     0x104c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(clk_32k_in,     0x1054,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(power_on,     0x105c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(pwr_i2c_scl,     0x1064,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(pwr_i2c_sda,     0x106c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_dis0,     0x1084,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_dis1,     0x108c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_dis2,     0x1094,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_dis3,     0x109c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_dis4,     0x10a4,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_dis5,     0x10ac,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	1),
	DRV_PINGROUP(gpio_wan8,     0xd004,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_wan7,     0xd00c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_wan6,     0xd014,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_wan5,     0xd01c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(uart2_tx,     0xd024,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(uart2_rx,     0xd02c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(uart2_rts,     0xd034,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(uart2_cts,     0xd03c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(uart5_rx,     0xd044,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(uart5_tx,     0xd04c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(uart5_rts,     0xd054,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(uart5_cts,     0xd05c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_mdm1,     0xd064,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_mdm2,     0xd06c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_mdm3,     0xd074,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_mdm4,     0xd07c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_mdm5,     0xd084,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_mdm6,     0xd08c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpio_mdm7,     0xd094,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(bcpu_pwr_req,     0xd09c,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(mcpu_pwr_req,     0xd0a4,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gpu_pwr_req,     0xd0ac,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gen7_i2c_scl,     0xd0b4,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gen7_i2c_sda,     0xd0bc,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gen9_i2c_sda,     0xd0c4,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gen9_i2c_scl,     0xd0cc,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(usb_vbus_en0,     0xd0d4,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(usb_vbus_en1,     0xd0dc,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gp_pwm7,     0xd0e4,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(gp_pwm6,     0xd0ec,    12,    5,    20,    5,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(ufs0_rst,    0x11004,    12,    9,    24,    8,    -1,    -1,    -1,    -1,	0),
	DRV_PINGROUP(ufs0_ref_clk,    0x1100c,    12,    9,    24,    8,    -1,    -1,    -1,    -1,	0),

};

static int tegra186_pinctrl_suspend(u32 *pg_data)
{
	return 0;
}

static void tegra186_pinctrl_resume(u32 *pg_data)
{
}

static int tegra186_gpio_request_enable(unsigned pin)
{
	return 0;
}

static const struct tegra_pinctrl_soc_data tegra186_pinctrl = {
	.ngpios = NUM_GPIOS,
	.pins = tegra186_pins,
	.npins = ARRAY_SIZE(tegra186_pins),
	.functions = tegra186_functions,
	.nfunctions = ARRAY_SIZE(tegra186_functions),
	.groups = tegra186_groups,
	.ngroups = ARRAY_SIZE(tegra186_groups),
	.is_gpio_reg_support = true,
	.suspend = tegra186_pinctrl_suspend,
	.resume = tegra186_pinctrl_resume,
	.gpio_request_enable = tegra186_gpio_request_enable,
	.hsm_in_mux = false,
	.schmitt_in_mux = true,
	.drvtype_in_mux = true,
};

static int tegra186_pinctrl_probe(struct platform_device *pdev)
{
	return tegra_pinctrl_probe(pdev, &tegra186_pinctrl);
}

static struct of_device_id tegra186_pinctrl_of_match[] = {
	{ .compatible = "nvidia,tegra186-pinmux", },
	{ },
};
MODULE_DEVICE_TABLE(of, tegra186_pinctrl_of_match);

static struct platform_driver tegra186_pinctrl_driver = {
	.driver = {
		.name = "tegra186-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = tegra186_pinctrl_of_match,
	},
	.probe = tegra186_pinctrl_probe,
	.remove = tegra_pinctrl_remove,
};

static int __init tegra186_pinctrl_init(void)
{
	return platform_driver_register(&tegra186_pinctrl_driver);
}
postcore_initcall_sync(tegra186_pinctrl_init);

static void __exit tegra186_pinctrl_exit(void)
{
	platform_driver_unregister(&tegra186_pinctrl_driver);
}
module_exit(tegra186_pinctrl_exit);

MODULE_AUTHOR("Suresh Mangipudi <smangipudi@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra186 pinctrl driver");
MODULE_LICENSE("GPL v2");
