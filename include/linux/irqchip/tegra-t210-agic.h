/*
 * include/linux/irqchip/tegra-t210-agic.h
 *
 * Header file for managing AGIC interrupt controller
 *
 * Copyright (C) 2014-2016 NVIDIA Corporation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _TEGRA_T210_AGIC_H_
#define _TEGRA_T210_AGIC_H_

#define TEGRA_AGIC_COMPAT "nvidia,tegra210-agic"

enum tegra_agic_cpu {
	TEGRA_AGIC_APE_HOST = 0,
	TEGRA_AGIC_ADSP = 1,
	MAX_AGIC_INTERFACES,
};

/* AMISC Mailbox Full Interrupts */
#define INT_AGIC_START			32
#define INT_AMISC_MBOX_FULL0		32
#define INT_AMISC_MBOX_FULL1		33
#define INT_AMISC_MBOX_FULL2		34
#define INT_AMISC_MBOX_FULL3		35

/* AMISC Mailbox Empty Interrupts */
#define INT_AMISC_MBOX_EMPTY0		36
#define INT_AMISC_MBOX_EMPTY1		37
#define INT_AMISC_MBOX_EMPTY2		38
#define INT_AMISC_MBOX_EMPTY3		39

/* AMISC CPU Arbitrated Semaphore Interrupt */
#define INT_AMISC_CPU_ARB_SEMA0		40
#define INT_AMISC_CPU_ARB_SEMA1		41
#define INT_AMISC_CPU_ARB_SEMA2		42
#define INT_AMISC_CPU_ARB_SEMA3		43
#define INT_AMISC_CPU_ARB_SEMA4		44
#define INT_AMISC_CPU_ARB_SEMA5		45
#define INT_AMISC_CPU_ARB_SEMA6		46
#define INT_AMISC_CPU_ARB_SEMA7		47

/* AMISC ADSP Arbitrated Semaphore Interrupt */
#define INT_AMISC_ADSP_ARB_SEMA0	48
#define INT_AMISC_ADSP_ARB_SEMA1	49
#define INT_AMISC_ADSP_ARB_SEMA2	50
#define INT_AMISC_ADSP_ARB_SEMA3	51
#define INT_AMISC_ADSP_ARB_SEMA4	52
#define INT_AMISC_ADSP_ARB_SEMA5	53
#define INT_AMISC_ADSP_ARB_SEMA6	54
#define INT_AMISC_ADSP_ARB_SEMA7	55

/* INT_ADMA Channel End of Transfer Interrupt */
#define INT_ADMA_EOT0			56
#define INT_ADMA_EOT1			57
#define INT_ADMA_EOT2			58
#define INT_ADMA_EOT3			59
#define INT_ADMA_EOT4			60
#define INT_ADMA_EOT5			61
#define INT_ADMA_EOT6			62
#define INT_ADMA_EOT7			63
#define INT_ADMA_EOT8			64
#define INT_ADMA_EOT9			65
#define INT_ADMA_EOT10			66
#define INT_ADMA_EOT11			67
#define INT_ADMA_EOT12			68
#define INT_ADMA_EOT13			69
#define INT_ADMA_EOT14			70
#define INT_ADMA_EOT15			71
#define INT_ADMA_EOT16			72
#define INT_ADMA_EOT17			73
#define INT_ADMA_EOT18			74
#define INT_ADMA_EOT19			75
#define INT_ADMA_EOT20			76
#define INT_ADMA_EOT21			77

/* ADSP/PTM Performance Monitoring Unit Interrupt */
#define INT_ADSP_PMU			78

/* ADSP Watchdog Timer Reset Request */
#define INT_ADSP_WDT			79

/* ADSP L2 Cache Controller Interrupt */
#define INT_ADSP_L2CC			80

/* AHUB Error Interrupt */
#define INT_AHUB_ERR			81

/* AMC Error Interrupt */
#define INT_AMC_ERR			82

/* INT_ADMA Error Interrupt */
#define INT_ADMA_ERR			83

/* ADSP Standby WFI.  ADSP in idle mode. Waiting for Interrupt */
#define INT_WFI				84

/* ADSP Standby WFE.  ADSP in idle mode. Waiting for Event */
#define INT_WFE				85

/* AMISC Actmon interrupt */
#define INT_ADSP_ACTMON			87

#define INT_AGIC_END			87

#endif /* _TEGRA_T210_AGIC_H_ */
