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

#ifndef _DT_BINDINGS_INTERRUPT_CONTROLLER_TEGRA_T21X_AGIC_H
#define _DT_BINDINGS_INTERRUPT_CONTROLLER_TEGRA_T21X_AGIC_H

#include <dt-bindings/interrupt-controller/arm-gic.h>

#define ROUTE_TO_HOST 0
#define ROUTE_TO_ADSP 1

/* The DT adds an offset of 32 to IRQs defined as SPI */
#define AGIC_IRQ(x)			(x - 32)
/* AMISC Mailbox Full Interrupts */
#define INT_AMISC_MBOX_FULL0		AGIC_IRQ(32)
#define INT_AMISC_MBOX_FULL1		AGIC_IRQ(33)
#define INT_AMISC_MBOX_FULL2		AGIC_IRQ(34)
#define INT_AMISC_MBOX_FULL3		AGIC_IRQ(35)

/* AMISC Mailbox Empty Interrupts */
#define INT_AMISC_MBOX_EMPTY0		AGIC_IRQ(36)
#define INT_AMISC_MBOX_EMPTY1		AGIC_IRQ(37)
#define INT_AMISC_MBOX_EMPTY2		AGIC_IRQ(38)
#define INT_AMISC_MBOX_EMPTY3		AGIC_IRQ(39)

/* AMISC CPU Arbitrated Semaphore Interrupt */
#define INT_AMISC_CPU_ARB_SEMA0		AGIC_IRQ(40)
#define INT_AMISC_CPU_ARB_SEMA1		AGIC_IRQ(41)
#define INT_AMISC_CPU_ARB_SEMA2		AGIC_IRQ(42)
#define INT_AMISC_CPU_ARB_SEMA3		AGIC_IRQ(43)
#define INT_AMISC_CPU_ARB_SEMA4		AGIC_IRQ(44)
#define INT_AMISC_CPU_ARB_SEMA5		AGIC_IRQ(45)
#define INT_AMISC_CPU_ARB_SEMA6		AGIC_IRQ(46)
#define INT_AMISC_CPU_ARB_SEMA7		AGIC_IRQ(47)

/* AMISC ADSP Arbitrated Semaphore Interrupt */
#define INT_AMISC_ADSP_ARB_SEMA0	AGIC_IRQ(48)
#define INT_AMISC_ADSP_ARB_SEMA1	AGIC_IRQ(49)
#define INT_AMISC_ADSP_ARB_SEMA2	AGIC_IRQ(50)
#define INT_AMISC_ADSP_ARB_SEMA3	AGIC_IRQ(51)
#define INT_AMISC_ADSP_ARB_SEMA4	AGIC_IRQ(52)
#define INT_AMISC_ADSP_ARB_SEMA5	AGIC_IRQ(53)
#define INT_AMISC_ADSP_ARB_SEMA6	AGIC_IRQ(54)
#define INT_AMISC_ADSP_ARB_SEMA7	AGIC_IRQ(55)

/* INT_ADMA Channel End of Transfer Interrupt */
#define INT_ADMA_EOT0			AGIC_IRQ(56)
#define INT_ADMA_EOT1			AGIC_IRQ(57)
#define INT_ADMA_EOT2			AGIC_IRQ(58)
#define INT_ADMA_EOT3			AGIC_IRQ(59)
#define INT_ADMA_EOT4			AGIC_IRQ(60)
#define INT_ADMA_EOT5			AGIC_IRQ(61)
#define INT_ADMA_EOT6			AGIC_IRQ(62)
#define INT_ADMA_EOT7			AGIC_IRQ(63)
#define INT_ADMA_EOT8			AGIC_IRQ(64)
#define INT_ADMA_EOT9			AGIC_IRQ(65)
#define INT_ADMA_EOT10			AGIC_IRQ(66)
#define INT_ADMA_EOT11			AGIC_IRQ(67)
#define INT_ADMA_EOT12			AGIC_IRQ(68)
#define INT_ADMA_EOT13			AGIC_IRQ(69)
#define INT_ADMA_EOT14			AGIC_IRQ(70)
#define INT_ADMA_EOT15			AGIC_IRQ(71)
#define INT_ADMA_EOT16			AGIC_IRQ(72)
#define INT_ADMA_EOT17			AGIC_IRQ(73)
#define INT_ADMA_EOT18			AGIC_IRQ(74)
#define INT_ADMA_EOT19			AGIC_IRQ(75)
#define INT_ADMA_EOT20			AGIC_IRQ(76)
#define INT_ADMA_EOT21			AGIC_IRQ(77)

/* ADSP/PTM Performance Monitoring Unit Interrupt */
#define INT_ADSP_PMU			AGIC_IRQ(78)

/* ADSP Watchdog Timer Reset Request */
#define INT_ADSP_WDT			AGIC_IRQ(79)

/* ADSP L2 Cache Controller Interrupt */
#define INT_ADSP_L2CC			AGIC_IRQ(80)

/* AHUB Error Interrupt */
#define INT_AHUB_ERR			AGIC_IRQ(81)

/* AMC Error Interrupt */
#define INT_AMC_ERR			AGIC_IRQ(82)

/* INT_ADMA Error Interrupt */
#define INT_ADMA_ERR			AGIC_IRQ(83)

/* ADSP Standby WFI.  ADSP in idle mode. Waiting for Interrupt */
#define INT_WFI				AGIC_IRQ(84)

/* ADSP Standby WFE.  ADSP in idle mode. Waiting for Event */
#define INT_WFE				AGIC_IRQ(85)

/* A9 debug trigger interface */
#define INT_CTI_IRQ			AGIC_IRQ(86)

/* AMISC Actmon interrupt */
#define INT_ADSP_ACTMON			AGIC_IRQ(87)

#define INT_REGDEC_ERR			AGIC_IRQ(88)

#endif /* _DT_BINDINGS_INTERRUPT_CONTROLLER_TEGRA_T21X_AGIC_H */
