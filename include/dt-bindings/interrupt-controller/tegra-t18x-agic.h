/*
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

#ifndef _DT_BINDINGS_INTERRUPT_CONTROLLER_TEGRA_T18X_AGIC_H
#define _DT_BINDINGS_INTERRUPT_CONTROLLER_TEGRA_T18X_AGIC_H

#include <dt-bindings/interrupt-controller/arm-gic.h>

#define ROUTE_TO_HOST_INTF0	0
#define ROUTE_TO_HOST_INTF1	1
#define ROUTE_TO_HOST_INTF2	2
#define ROUTE_TO_HOST_INTF3	3
#define ROUTE_TO_ADSP		4

/* The DT adds an offset of 32 to IRQs defined as SPI */
#define AGIC_IRQ(x)			(x - 32)

/* INT_ADMA Channel End of Transfer Interrupt */
#define INT_ADMA_EOT0			AGIC_IRQ(32)
#define INT_ADMA_EOT1			AGIC_IRQ(33)
#define INT_ADMA_EOT2			AGIC_IRQ(34)
#define INT_ADMA_EOT3			AGIC_IRQ(35)
#define INT_ADMA_EOT4			AGIC_IRQ(36)
#define INT_ADMA_EOT5			AGIC_IRQ(37)
#define INT_ADMA_EOT6			AGIC_IRQ(38)
#define INT_ADMA_EOT7			AGIC_IRQ(39)
#define INT_ADMA_EOT8			AGIC_IRQ(40)
#define INT_ADMA_EOT9			AGIC_IRQ(41)
#define INT_ADMA_EOT10			AGIC_IRQ(42)
#define INT_ADMA_EOT11			AGIC_IRQ(43)
#define INT_ADMA_EOT12			AGIC_IRQ(44)
#define INT_ADMA_EOT13			AGIC_IRQ(45)
#define INT_ADMA_EOT14			AGIC_IRQ(46)
#define INT_ADMA_EOT15			AGIC_IRQ(47)
#define INT_ADMA_EOT16			AGIC_IRQ(48)
#define INT_ADMA_EOT17			AGIC_IRQ(49)
#define INT_ADMA_EOT18			AGIC_IRQ(50)
#define INT_ADMA_EOT19			AGIC_IRQ(51)
#define INT_ADMA_EOT20			AGIC_IRQ(52)
#define INT_ADMA_EOT21			AGIC_IRQ(53)
#define INT_ADMA_EOT22			AGIC_IRQ(54)
#define INT_ADMA_EOT23			AGIC_IRQ(55)
#define INT_ADMA_EOT24			AGIC_IRQ(56)
#define INT_ADMA_EOT25			AGIC_IRQ(57)
#define INT_ADMA_EOT26			AGIC_IRQ(58)
#define INT_ADMA_EOT27			AGIC_IRQ(59)
#define INT_ADMA_EOT28			AGIC_IRQ(60)
#define INT_ADMA_EOT29			AGIC_IRQ(61)
#define INT_ADMA_EOT30			AGIC_IRQ(62)
#define INT_ADMA_EOT31			AGIC_IRQ(63)

/* AMISC Mailbox Full Interrupts */
#define INT_AMISC_MBOX_FULL0		AGIC_IRQ(64)
#define INT_AMISC_MBOX_FULL1		AGIC_IRQ(65)
#define INT_AMISC_MBOX_FULL2		AGIC_IRQ(66)
#define INT_AMISC_MBOX_FULL3		AGIC_IRQ(67)
#define INT_AMISC_MBOX_FULL4		AGIC_IRQ(68)
#define INT_AMISC_MBOX_FULL5		AGIC_IRQ(69)
#define INT_AMISC_MBOX_FULL6		AGIC_IRQ(70)
#define INT_AMISC_MBOX_FULL7		AGIC_IRQ(71)

/* AMISC Mailbox Empty Interrupts */
#define INT_AMISC_MBOX_EMPTY0		AGIC_IRQ(72)
#define INT_AMISC_MBOX_EMPTY1		AGIC_IRQ(73)
#define INT_AMISC_MBOX_EMPTY2		AGIC_IRQ(74)
#define INT_AMISC_MBOX_EMPTY3		AGIC_IRQ(75)
#define INT_AMISC_MBOX_EMPTY4		AGIC_IRQ(76)
#define INT_AMISC_MBOX_EMPTY5		AGIC_IRQ(77)
#define INT_AMISC_MBOX_EMPTY6		AGIC_IRQ(78)
#define INT_AMISC_MBOX_EMPTY7		AGIC_IRQ(79)

/* AHSP SHARED INTERRUPT */
#define INT_AHSP_SHRD0			AGIC_IRQ(80)
#define INT_AHSP_SHRD1			AGIC_IRQ(81)
#define INT_AHSP_SHRD2			AGIC_IRQ(82)
#define INT_AHSP_SHRD3			AGIC_IRQ(83)
#define INT_AHSP_SHRD4			AGIC_IRQ(84)

/* ADSP/PTM Performance Monitoring Unit Interrupt */
#define INT_ADSP_PMU			AGIC_IRQ(85)

/* ADSP Watchdog Timer Reset Request */
#define INT_ADSP_WDT			AGIC_IRQ(86)

/* ADSP L2 Cache Controller Interrupt */
#define INT_ADSP_L2CC			AGIC_IRQ(87)

/* AHUB Error Interrupt */
#define INT_AHUB_ERR			AGIC_IRQ(88)

/* AMC Error Interrupt */
#define INT_AMC_ERR			AGIC_IRQ(89)

/* INT_ADMA Error Interrupt */
#define INT_ADMA_ERR0			AGIC_IRQ(90)
#define INT_ADMA_ERR1			AGIC_IRQ(91)
#define INT_ADMA_ERR2			AGIC_IRQ(92)
#define INT_ADMA_ERR3			AGIC_IRQ(93)

/* ADSP Standby WFI.  ADSP in idle mode. Waiting for Interrupt */
#define INT_WFI				AGIC_IRQ(94)

/* ADSP Standby WFE.  ADSP in idle mode. Waiting for Event */
#define INT_WFE				AGIC_IRQ(95)

#define INT_ADSP_CTI			AGIC_IRQ(96)

/* Activity monitoring on ADSP.  This interrupt is from AMISC */
#define INT_ADSP_ACTMON			AGIC_IRQ(97)
#define INT_ADSP_ACTMON_RESERVED0	AGIC_IRQ(98)
#define INT_ADSP_ACTMON_RESERVED1	AGIC_IRQ(99)

/* LIC to APE Interrupts */
#define INT_LIC_TO_APE0			AGIC_IRQ(100)
#define INT_LIC_TO_APE1			AGIC_IRQ(101)

/* VI Notify High Interrupt */
#define INT_VI_NOTIFY_HIGH		AGIC_IRQ(102)
#define INT_VI_NOTIFY_LOW		AGIC_IRQ(103)

/* I2C {1,3,8} Interrupt */
#define INT_I2C_IRQ1			AGIC_IRQ(104)
#define INT_I2C_IRQ3			AGIC_IRQ(105)
#define INT_I2C_IRQ8			AGIC_IRQ(106)

/* System Door Bell interrupt */
#define INT_SHSP2APE_DB			AGIC_IRQ(107)

/* Top WDT FIQ interrupt */
#define INT_TOP_WDT_FIQ			AGIC_IRQ(108)

/* Top WDT IRQ interrupt */
#define INT_TOP_WDT_IRQ			AGIC_IRQ(109)

/* ATKE Timer IRQ interrupt */
#define INT_ATKE_TMR0			AGIC_IRQ(110)
#define INT_ATKE_TMR1			AGIC_IRQ(111)
#define INT_ATKE_TMR2			AGIC_IRQ(112)
#define INT_ATKE_TMR3			AGIC_IRQ(113)

/* ATKE WDT FIQ interrupt */
#define INT_ATKE_WDT_FIQ		AGIC_IRQ(114)

/* ATKE WDT IRQ interrupt */
#define INT_ATKE_WDT_IRQ		AGIC_IRQ(115)

/* ATKE WDT error interrupt */
#define INT_ATKE_WDT_ERR		AGIC_IRQ(116)

/* UART interrupt to AGIC (In FPGA platform only) */
#define INT_UART_FPGA			AGIC_IRQ(117)

#endif /* _DT_BINDINGS_INTERRUPT_CONTROLLER_TEGRA_T18X_AGIC_H */
