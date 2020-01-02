/*
 * cnl-sst-dsp.c - CNL SST library generic function
 *
 * Copyright (C) 2015-16, Intel Corporation.
 * Author: Guneshwor Singh <guneshwor.o.singh@intel.com>
 *
 * Modified from:
 *	SKL SST library generic function
 *	Copyright (C) 2014-15, Intel Corporation.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#include <sound/pcm.h>

#include "../common/sst-dsp.h"
#include "../common/sst-ipc.h"
#include "../common/sst-dsp-priv.h"
#include "cnl-sst-dsp.h"

/* various timeout values */
#define CNL_DSP_PU_TO		50
#define CNL_DSP_PD_TO		50
#define CNL_DSP_RESET_TO	50

static int cnl_dsp_core_set_reset_state(struct sst_dsp *ctx, unsigned core)
{
	int ret;

	/* update bits */
	sst_dsp_shim_update_bits_unlocked(ctx,
			CNL_ADSP_REG_ADSPCS, CNL_ADSPCS_CRST(core),
			CNL_ADSPCS_CRST(core));

	/* poll with timeout to check if operation successful */
	ret = sst_dsp_register_poll(ctx,
			CNL_ADSP_REG_ADSPCS,
			CNL_ADSPCS_CRST(core),
			CNL_ADSPCS_CRST(core),
			CNL_DSP_RESET_TO,
			"Set reset");
	if ((sst_dsp_shim_read_unlocked(ctx, CNL_ADSP_REG_ADSPCS) &
				CNL_ADSPCS_CRST(core)) !=
				CNL_ADSPCS_CRST(core)) {
		dev_err(ctx->dev, "DSP core %#x set reset state failed\n",
								core);
		ret = -EIO;
	}

	return ret;
}

static int cnl_dsp_core_unset_reset_state(struct sst_dsp *ctx, unsigned core)
{
	int ret;

	/* update bits */
	sst_dsp_shim_update_bits_unlocked(ctx, CNL_ADSP_REG_ADSPCS,
					CNL_ADSPCS_CRST(core), 0);

	/* poll with timeout to check if operation successful */
	ret = sst_dsp_register_poll(ctx,
			CNL_ADSP_REG_ADSPCS,
			CNL_ADSPCS_CRST(core),
			0,
			CNL_DSP_RESET_TO,
			"Unset reset");

	if ((sst_dsp_shim_read_unlocked(ctx, CNL_ADSP_REG_ADSPCS) &
				 CNL_ADSPCS_CRST(core)) != 0) {
		dev_err(ctx->dev, "DSP core %#x unset reset state failed\n",
								core);
		ret = -EIO;
	}

	return ret;
}

static bool is_cnl_dsp_core_enable(struct sst_dsp *ctx, unsigned core)
{
	int val;
	bool is_enable;

	val = sst_dsp_shim_read_unlocked(ctx, CNL_ADSP_REG_ADSPCS);

	is_enable = (val & CNL_ADSPCS_CPA(core)) &&
			(val & CNL_ADSPCS_SPA(core)) &&
			!(val & CNL_ADSPCS_CRST(core)) &&
			!(val & CNL_ADSPCS_CSTALL(core));

	return is_enable;
}

static int cnl_dsp_reset_core(struct sst_dsp *ctx, unsigned core)
{
	/* stall core */
	sst_dsp_shim_update_bits_unlocked(ctx, CNL_ADSP_REG_ADSPCS,
			CNL_ADSPCS_CSTALL(core),
			CNL_ADSPCS_CSTALL(core));

	/* set reset state */
	return cnl_dsp_core_set_reset_state(ctx, core);
}

static int cnl_dsp_start_core(struct sst_dsp *ctx, unsigned core)
{
	int ret;

	/* unset reset state */
	ret = cnl_dsp_core_unset_reset_state(ctx, core);
	if (ret < 0) {
		dev_dbg(ctx->dev, "DSP core %#x unset reset failed\n", core);
		return ret;
	}

	/* run core */
	sst_dsp_shim_update_bits_unlocked(ctx, CNL_ADSP_REG_ADSPCS,
				CNL_ADSPCS_CSTALL(core), 0);

	if (!is_cnl_dsp_core_enable(ctx, core)) {
		cnl_dsp_reset_core(ctx, core);
		dev_err(ctx->dev, "DSP core %#x enable failed\n", core);
		ret = -EIO;
	}

	return ret;
}

static int cnl_dsp_core_power_up(struct sst_dsp *ctx, unsigned core)
{
	int ret;

	/* update bits */
	sst_dsp_shim_update_bits_unlocked(ctx, CNL_ADSP_REG_ADSPCS,
		CNL_ADSPCS_SPA(core), CNL_ADSPCS_SPA(core));

	/* poll with timeout to check if operation successful */
	ret = sst_dsp_register_poll(ctx,
			CNL_ADSP_REG_ADSPCS,
			CNL_ADSPCS_CPA(core),
			CNL_ADSPCS_CPA(core),
			CNL_DSP_PU_TO,
			"Power up");

	if ((sst_dsp_shim_read_unlocked(ctx, CNL_ADSP_REG_ADSPCS) &
			CNL_ADSPCS_CPA(core)) !=
			CNL_ADSPCS_CPA(core)) {
		dev_err(ctx->dev, "DSP core %#x power up failed\n", core);
		ret = -EIO;
	}

	return ret;
}

static int cnl_dsp_core_power_down(struct sst_dsp *ctx, unsigned core)
{
	/* update bits */
	sst_dsp_shim_update_bits_unlocked(ctx, CNL_ADSP_REG_ADSPCS,
					CNL_ADSPCS_SPA(core), 0);

	/* poll with timeout to check if operation successful */
	return sst_dsp_register_poll(ctx,
			CNL_ADSP_REG_ADSPCS,
			CNL_ADSPCS_CPA(core),
			0,
			CNL_DSP_PD_TO,
			"Power down");
}

int cnl_dsp_enable_core(struct sst_dsp *ctx, unsigned core)
{
	int ret;

	/* power up */
	ret = cnl_dsp_core_power_up(ctx, core);
	if (ret < 0) {
		dev_dbg(ctx->dev, "DSP core %#x power up failed", core);
		return ret;
	}

	return cnl_dsp_start_core(ctx, core);
}

int cnl_dsp_disable_core(struct sst_dsp *ctx, unsigned core)
{
	int ret;

	ret = cnl_dsp_reset_core(ctx, core);
	if (ret < 0) {
		dev_err(ctx->dev, "DSP core %#x reset failed\n", core);
		return ret;
	}

	/* power down core*/
	ret = cnl_dsp_core_power_down(ctx, core);
	if (ret < 0) {
		dev_err(ctx->dev, "DSP core %#x power down failed\n", core);
		return ret;
	}

	if (is_cnl_dsp_core_enable(ctx, core)) {
		dev_err(ctx->dev, "DSP core %#x disable failed\n", core);
		ret = -EIO;
	}

	return ret;
}

irqreturn_t cnl_dsp_sst_interrupt(int irq, void *dev_id)
{
	struct sst_dsp *ctx = dev_id;
	u32 val;
	irqreturn_t result = IRQ_NONE;

	spin_lock(&ctx->spinlock);

	val = sst_dsp_shim_read_unlocked(ctx, CNL_ADSP_REG_ADSPIS);
	ctx->intr_status = val;

	if (val == 0xffffffff) {
		spin_unlock(&ctx->spinlock);
		return IRQ_NONE;
	}

	if (val & CNL_ADSPIS_IPC) {
		cnl_ipc_int_disable(ctx);
		result = IRQ_WAKE_THREAD;
	}

	spin_unlock(&ctx->spinlock);

	return result;
}

void cnl_dsp_free(struct sst_dsp *dsp)
{
	cnl_ipc_int_disable(dsp);

	free_irq(dsp->irq, dsp);
	cnl_dsp_disable_core(dsp, SKL_DSP_CORE_MASK(0));
}
EXPORT_SYMBOL_GPL(cnl_dsp_free);

unsigned cnl_dsp_get_enabled_cores(struct sst_dsp *ctx)
{
	u32 val;
	unsigned en_cores_mask;

	val = sst_dsp_shim_read_unlocked(ctx, CNL_ADSP_REG_ADSPCS);

	/* cores having CPA bit set */
	en_cores_mask = (val & CNL_ADSPCS_CPA(CNL_DSP_CORES_MASK))
						>> CNL_ADSPCS_CPA_SHIFT;
	/* cores having CRST bit cleared */
	en_cores_mask &= (~val & CNL_ADSPCS_CRST(CNL_DSP_CORES_MASK))
						>> CNL_ADSPCS_CRST_SHIFT;
	/* cores having CSTALL bit cleared */
	en_cores_mask &= (~val & CNL_ADSPCS_CSTALL(CNL_DSP_CORES_MASK))
						>> CNL_ADSPCS_CSTALL_SHIFT;
	en_cores_mask &= CNL_DSP_CORES_MASK;

	dev_dbg(ctx->dev, "DSP enabled cores mask = %#x\n", en_cores_mask);

	return en_cores_mask;
}
