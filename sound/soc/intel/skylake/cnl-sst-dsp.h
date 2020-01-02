/*
 * Cannonlake SST DSP Support
 *
 * Copyright (C) 2015-16, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef __CNL_SST_DSP_H__
#define __CNL_SST_DSP_H__

#include <linux/interrupt.h>
#include <sound/memalloc.h>

struct sst_dsp;
struct skl_sst;
struct sst_dsp_device;

/* Intel HD Audio General DSP Registers */
#define CNL_ADSP_GEN_BASE		0x0
#define CNL_ADSP_REG_ADSPCS		(CNL_ADSP_GEN_BASE + 0x04)
#define CNL_ADSP_REG_ADSPIC		(CNL_ADSP_GEN_BASE + 0x08)
#define CNL_ADSP_REG_ADSPIS		(CNL_ADSP_GEN_BASE + 0x0C)

/* Intel HD Audio Inter-Processor Communication Registers */
#define CNL_ADSP_IPC_BASE               0xC0
#define CNL_ADSP_REG_HIPCTDR            (CNL_ADSP_IPC_BASE + 0x00)
#define CNL_ADSP_REG_HIPCTDA            (CNL_ADSP_IPC_BASE + 0x04)
#define CNL_ADSP_REG_HIPCTDD            (CNL_ADSP_IPC_BASE + 0x08)
#define CNL_ADSP_REG_HIPCIDR            (CNL_ADSP_IPC_BASE + 0x10)
#define CNL_ADSP_REG_HIPCIDA            (CNL_ADSP_IPC_BASE + 0x14)
#define CNL_ADSP_REG_HIPCIDD            (CNL_ADSP_IPC_BASE + 0x18)
#define CNL_ADSP_REG_HIPCCST            (CNL_ADSP_IPC_BASE + 0x20)
#define CNL_ADSP_REG_HIPCCSR            (CNL_ADSP_IPC_BASE + 0x24)
#define CNL_ADSP_REG_HIPCCTL            (CNL_ADSP_IPC_BASE + 0x28)

/* HIPCTDR */
#define CNL_ADSP_REG_HIPCTDR_BUSY	BIT(31)

/* HIPCTDA */
#define CNL_ADSP_REG_HIPCTDA_DONE	BIT(31)

/* HIPCIDR */
#define CNL_ADSP_REG_HIPCIDR_BUSY	BIT(31)

/* HIPCIDA */
#define CNL_ADSP_REG_HIPCIDA_DONE	BIT(31)

/* CNL HIPCCTL */
#define CNL_ADSP_REG_HIPCCTL_DONE	BIT(1)
#define CNL_ADSP_REG_HIPCCTL_BUSY	BIT(0)

/* CNL HIPCT */
#define CNL_ADSP_REG_HIPCT_BUSY		BIT(31)

/* Intel HD Audio SRAM Window 1 */
#define CNL_ADSP_SRAM1_BASE		0xA0000

#define CNL_ADSP_MMIO_LEN		0x10000

#define CNL_ADSP_W0_STAT_SZ		0x1000

#define CNL_ADSP_W0_UP_SZ		0x1000

#define CNL_ADSP_W1_SZ			0x1000

#define CNL_FW_STS_MASK			0xf

#define CNL_FW_INIT			0x1
#define CNL_FW_RFW_START		0xf

#define CNL_ADSPIC_IPC			1
#define CNL_ADSPIS_IPC			1

/* ADSPCS - Audio DSP Control & Status */
#define CNL_DSP_CORES		4
#define CNL_DSP_CORES_MASK	((1 << CNL_DSP_CORES) - 1)

/* Core Reset - asserted high */
#define CNL_ADSPCS_CRST_SHIFT	0
#define CNL_ADSPCS_CRST(x)	(x << CNL_ADSPCS_CRST_SHIFT)

/* Core run/stall - when set to '1' core is stalled */
#define CNL_ADSPCS_CSTALL_SHIFT	8
#define CNL_ADSPCS_CSTALL(x)	(x << CNL_ADSPCS_CSTALL_SHIFT)

/* Set Power Active - when set to '1' turn cores on */
#define CNL_ADSPCS_SPA_SHIFT	16
#define CNL_ADSPCS_SPA(x)	(x << CNL_ADSPCS_SPA_SHIFT)

/* Current Power Active - power status of cores, set by hardware */
#define CNL_ADSPCS_CPA_SHIFT	24
#define CNL_ADSPCS_CPA(x)	(x << CNL_ADSPCS_CPA_SHIFT)

int cnl_dsp_enable_core(struct sst_dsp *ctx, unsigned core);
int cnl_dsp_disable_core(struct sst_dsp *ctx, unsigned core);
irqreturn_t cnl_dsp_sst_interrupt(int irq, void *dev_id);
void cnl_dsp_free(struct sst_dsp *dsp);

int cnl_sst_dsp_init_hw(struct device *dev, struct skl_sst **dsp,
				struct dsp_init *d);
int cnl_sst_dsp_init_fw(struct device *dev, struct skl_sst *ctx);
void cnl_sst_dsp_cleanup(struct device *dev, struct skl_sst *ctx);

void cnl_ipc_int_disable(struct sst_dsp *ctx);
unsigned cnl_dsp_get_enabled_cores(struct sst_dsp *ctx);
#endif /*__CNL_SST_DSP_H__*/
