/*
 * drivers/video/tegra/dc/nvsr.c
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION, All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include "nvsr.h"
#include "nvsr_regs.h"
#include "dpaux_regs.h"

#define HIMAX_HX8880_A 1

#define NVSR_ERR(...) dev_err(&nvsr->dc->ndev->dev, "NVSR: " __VA_ARGS__);
#define NVSR_WARN(...) dev_warn(&nvsr->dc->ndev->dev, "NVSR: " __VA_ARGS__);
#define NVSR_INFO(...) dev_info(&nvsr->dc->ndev->dev, "NVSR: " __VA_ARGS__);

#define NVSR_RET(ret, ...) \
{ \
	if (ret) { \
		NVSR_ERR(__VA_ARGS__); \
		return; \
	} \
}

#define NVSR_RETV(ret, ...) \
{ \
	if (ret) { \
		NVSR_ERR(__VA_ARGS__); \
		return ret; \
	} \
}

/* Wait for 3 frames max to enter/exit */
#define SR_ENTRY_MAX_TRIES 3
#define SR_EXIT_MAX_TRIES 3

static unsigned long
tegra_dc_nvsr_poll_register(struct tegra_dc_nvsr_data *nvsr,
	u32 reg, u32 mask, u32 bytes, u32 exp_val,
	u32 poll_interval_us, u32 timeout_ms)
{
	unsigned long timeout_jf = jiffies + msecs_to_jiffies(timeout_ms);
	u32 reg_val;
	u32 ret;

	do {
		ret = nvsr->reg_ops.read(nvsr, reg, bytes, &reg_val);
		NVSR_RETV(ret, "Failed to read status register.\n");

		if ((reg_val & mask) == exp_val)
			return 0;	/* success */

		usleep_range(poll_interval_us, poll_interval_us << 1);
	} while (time_after(timeout_jf, jiffies));

	NVSR_ERR("Timed out while polling register 0x%x\n", reg);
	return jiffies - timeout_jf + 1;
}


static inline unsigned long
tegra_dc_nvsr_wait_until_status(struct tegra_dc_nvsr_data *nvsr, u32 status)
{
	/* Poll every quarter frame for 3 frames */
	u32 interval_us = nvsr->sr_timing_frame_time_us / 4;
	u32 timeout_ms = nvsr->sr_timing_frame_time_us / 1000 * 3;

	if (tegra_dc_nvsr_poll_register(nvsr, NVSR_STATUS,
		NVSR_STATUS_SR_STATE_MASK, 1, status,
		interval_us, timeout_ms))
		return -ETIMEDOUT;

	return 0;
}

static int tegra_dc_nvsr_pre_disable_link(struct tegra_dc_nvsr_data *nvsr)
{
	if (nvsr->link_status != LINK_ENABLED)
		return 0;

	if (nvsr->dc->out->type == TEGRA_DC_OUT_NVSR_DP) {
		/* Unlocked in tegra_dc_nvsr_disable_link() */
		mutex_lock(&nvsr->link_lock);
		tegra_dc_dp_pre_disable_link(nvsr->out_data.dp);
		nvsr->link_status = LINK_DISABLING;
		return 0;
	}

	return -ENODEV;
}

static int
tegra_dc_nvsr_disable_link(struct tegra_dc_nvsr_data *nvsr, bool powerdown)
{
	if (nvsr->dc->out->type == TEGRA_DC_OUT_NVSR_DP) {
		tegra_dc_nvsr_pre_disable_link(nvsr);
		tegra_dc_dp_disable_link(nvsr->out_data.dp, powerdown);
		if (nvsr->link_status == LINK_DISABLING)
			/* Locked in tegra_dc_nvsr_pre_disable_link() */
			mutex_unlock(&nvsr->link_lock);
		nvsr->link_status = LINK_DISABLED;
		return 0;
	}

	return -ENODEV;
}

static int tegra_dc_nvsr_enable_link(struct tegra_dc_nvsr_data *nvsr)
{
	int ret = -ENODEV;

	if (nvsr->dc->out->type == TEGRA_DC_OUT_NVSR_DP) {
		mutex_lock(&nvsr->link_lock);
		if (nvsr->link_status == LINK_ENABLED) {
			trace_printk("Enabled link (noop)\n");
			mutex_unlock(&nvsr->link_lock);
			return 0;
		}
		tegra_dc_dp_enable_link(nvsr->out_data.dp);
		nvsr->link_status = LINK_ENABLED;
		mutex_unlock(&nvsr->link_lock);
		ret = 0;
	}

	return ret;
}

static int tegra_dc_nvsr_enter_sr(struct tegra_dc_nvsr_data *nvsr)
{
	u32 val;
	int ret;

	/* Enable SR */
	val = NVSR_SRC_CTL0_SR_ENABLE_CTL_ENABLED;
	val |= NVSR_SRC_CTL0_SR_ENABLE_MASK_ENABLE;
	/* Request SR entry */
	val |= NVSR_SRC_CTL0_SR_ENTRY_REQ_YES;
	val |= NVSR_SRC_CTL0_SR_ENTRY_MASK_ENABLE;
	ret = nvsr->reg_ops.write(nvsr, NVSR_SRC_CTL0, 1, val);
	NVSR_RETV(ret, "Failed to request SR entry\n");

	nvsr->sr_active = true;

	return 0;
}

static int tegra_dc_nvsr_exit_sr(struct tegra_dc_nvsr_data *nvsr)
{
	u32 val;
	int ret;

	if (!nvsr->sr_active)
		return 0;

	switch (nvsr->resync_method) {
	case NVSR_RESYNC_CTL_METHOD_FL:
		val = NVSR_RESYNC_CTL_METHOD_FL;
		val |= nvsr->resync_delay << NVSR_RESYNC_CTL_DELAY_SHIFT;
		break;
	case NVSR_RESYNC_CTL_METHOD_SS:
	case NVSR_RESYNC_CTL_METHOD_BS:
		NVSR_WARN("Sliding Sync and Blank Stretching resync methods not supported; defaulting to Immediate\n");
	case NVSR_RESYNC_CTL_METHOD_IMM:
	default:
		val = NVSR_RESYNC_CTL_METHOD_IMM;
		break;
	}

	ret = nvsr->reg_ops.write(nvsr, NVSR_RESYNC_CTL, 1, val);
	NVSR_RETV(ret, "Couldn't write resync method, aborting SR exit\n");

	val = NVSR_SRC_CTL1_SIDEBAND_EXIT_SEL_YES;
	val |= NVSR_SRC_CTL1_SIDEBAND_EXIT_MASK_ENABLE;
	ret = nvsr->reg_ops.write(nvsr, NVSR_SRC_CTL1, 1, val);
	NVSR_RETV(ret, "Couldn't select sideband exit, aborting SR exit\n");

	/* Request SR exit */
	val = NVSR_SRC_CTL0_SR_ENABLE_MASK_ENABLE;
	val |= NVSR_SRC_CTL0_SR_ENABLE_CTL_ENABLED;
	val |= NVSR_SRC_CTL0_SR_EXIT_REQ_YES;
	val |= NVSR_SRC_CTL0_SR_EXIT_MASK_ENABLE;
	ret = nvsr->reg_ops.write(nvsr, NVSR_SRC_CTL0, 1, val);
	NVSR_RETV(ret, "Couldn't request SR exit, aborting SR exit\n");

	ret = tegra_dc_nvsr_wait_until_status(nvsr,
		NVSR_STATUS_SR_STATE_IDLE);
	NVSR_RETV(ret, "Timed out while waiting for SR exit\n");

	nvsr->sr_active = false;

	return 0;
}

static int tegra_dc_nvsr_src_power_on(struct tegra_dc_nvsr_data *nvsr)
{
	int ret;
	u32 reg_val = 0;

	ret = nvsr->reg_ops.read(nvsr, NVSR_STATUS, 1, &reg_val);
	if (ret) {
		NVSR_ERR("Failed to read status register.\n");
		return ret;
	}

	/* If SRC is on, we're done */
	if ((reg_val & NVSR_STATUS_SR_STATE_MASK)
		== NVSR_STATUS_SR_STATE_IDLE)
		return 0;

	reg_val = NVSR_SRC_CTL0_SR_ENABLE_CTL_ENABLED;
	reg_val |= NVSR_SRC_CTL0_SR_ENABLE_MASK_ENABLE;
	ret = nvsr->reg_ops.write(nvsr, NVSR_SRC_CTL0, 1, reg_val);
	NVSR_RETV(ret, "Failed to write to control register.\n");

	ret = tegra_dc_nvsr_wait_until_status(nvsr,
		NVSR_STATUS_SR_STATE_IDLE);
	NVSR_RETV(ret, "Failed to power on the SRC.\n");

	nvsr->src_on = true;
	return 0;
}

static int tegra_dc_nvsr_src_power_off(struct tegra_dc_nvsr_data *nvsr)
{
	int ret;
	u32 reg_val = 0;

	ret = nvsr->reg_ops.read(nvsr, NVSR_STATUS, 1, &reg_val);
	if (ret) {
		NVSR_ERR("Failed to read status register.\n");
		return ret;
	}

	if ((reg_val & NVSR_STATUS_SR_STATE_MASK)
		== NVSR_STATUS_SR_STATE_OFFLINE)
		return 0;

	reg_val = NVSR_SRC_CTL0_SR_ENABLE_CTL_DISABLED;
	reg_val |= NVSR_SRC_CTL0_SR_ENABLE_MASK_ENABLE;
	ret = nvsr->reg_ops.write(nvsr, NVSR_SRC_CTL0, 1, reg_val);
	NVSR_RETV(ret, "Failed to write to control register.\n");

	ret = tegra_dc_nvsr_wait_until_status(nvsr,
		NVSR_STATUS_SR_STATE_OFFLINE);
	NVSR_RETV(ret, "Failed to power off the SRC.\n");

	nvsr->src_on = false;
	nvsr->sr_active = false;
	nvsr->is_init = false;

	return 0;
}

/* Read SRC capabilities and assess possible refresh modes. */
static int tegra_dc_nvsr_query_capabilities(struct tegra_dc_nvsr_data *nvsr)
{
	u32 reg_val;
	int ret;

	ret = nvsr->reg_ops.read(nvsr, NVSR_SR_CAPS0, 1, &reg_val);
	NVSR_RETV(ret, "Failed to read cap reg 0.\n");

	nvsr->cap.sr =
		reg_val & NVSR_SR_CAPS0_SR_CAPABLE_MASK >>
		NVSR_SR_CAPS0_SR_CAPABLE_SHIFT;
	nvsr->cap.sr_entry_req =
		reg_val & NVSR_SR_CAPS0_SR_ENTRY_REQ_MASK >>
		NVSR_SR_CAPS0_SR_ENTRY_REQ_SHIFT;
	nvsr->cap.sr_exit_req =
		reg_val & NVSR_SR_CAPS0_SR_EXIT_REQ_MASK >>
		NVSR_SR_CAPS0_SR_EXIT_REQ_SHIFT;

	if (!nvsr->cap.sr || !nvsr->cap.sr_entry_req ||
		!nvsr->cap.sr_exit_req) {
		NVSR_ERR("SRC can't support self-refresh.\n");
		return 0;
	}

	nvsr->cap.resync =
		reg_val & NVSR_SR_CAPS0_RESYNC_CAP_MASK >>
		NVSR_SR_CAPS0_RESYNC_CAP_SHIFT;

	nvsr->cap.sparse_mode_support = true;

	if (!nvsr->src_id.device) {
		ret = nvsr->reg_ops.read(nvsr, NVSR_SRC_ID0, 4, &reg_val);
		NVSR_RETV(ret, "Failed to read device ID.\n");
		nvsr->src_id.device = reg_val & 0xffff;
		nvsr->src_id.vendor = reg_val >> 16;
	}

	ret = nvsr->reg_ops.read(nvsr, NVSR_SR_CAPS2, 1, &reg_val);
	NVSR_RETV(ret, "Failed to read capabilities register 2.\n");

	if ((reg_val & NVSR_SR_CAPS2_SEPERATE_PCLK_SUPPORTED) &&
		(reg_val & NVSR_SR_CAPS2_BUFFERED_REFRESH_SUPPORTED)) {
		u32 max_pclk = 0;
		ret = nvsr->reg_ops.read(nvsr, NVSR_SR_MAX_PCLK0, 2, &max_pclk);
		if (ret)
			NVSR_WARN("Failed to read SRC input max pclk\n");

		if (max_pclk > 0) {
			/* NVSR stores SRC input max pclk in units of 20KHz */
			nvsr->cap.max_pt_pclk = max_pclk * 20000;
			nvsr->cap.sep_pclk = true;
			nvsr->cap.buffered_mode_support =
				reg_val & NVSR_SR_CAPS2_FRAME_LOCK_MASK >>
				NVSR_SR_CAPS2_FRAME_LOCK_SHIFT;

			nvsr->cap.burst_mode_support =
				nvsr->cap.resync & NVSR_SR_CAPS0_RESYNC_CAP_FL;
		} else
			NVSR_WARN("SRC input max pclk is 0.\n");
	}

	nvsr->cap.is_init = true;

	return 0;
}

/* Write self-refresh timing: SRC to panel */
static int tegra_dc_nvsr_write_sr_timing(struct tegra_dc_nvsr_data *nvsr)
{
	int ret;
	struct tegra_dc_mode *mode = &nvsr->sr_timing;
	u32 val, hblank, vblank, pclk;

	/* NVSR stores pclk in units of 20KHz */
	val = DIV_ROUND_UP(mode->pclk, 20000);
	ret = nvsr->reg_ops.write(nvsr, NVSR_SRMODE_PIXEL_CLOCK0, 2, val);
	NVSR_RETV(ret, "Failed to write SR pclk\n");
	pclk = val * 20000;

	/* Horizontal timing */

	ret = nvsr->reg_ops.write(nvsr, NVSR_SRMODE_TIMING0, 2, mode->h_active);
	NVSR_RETV(ret, "Failed to write SR Hactive\n");

	val = mode->h_front_porch + mode->h_sync_width + mode->h_back_porch;
	ret = nvsr->reg_ops.write(nvsr, NVSR_SRMODE_TIMING2, 2, val);
	NVSR_RETV(ret, "Failed to write SR Hblank\n");
	hblank = val;

	ret = nvsr->reg_ops.write(nvsr, NVSR_SRMODE_TIMING4, 2,
		mode->h_front_porch);
	NVSR_RETV(ret, "Failed to write SR Hfp\n");

	ret = nvsr->reg_ops.write(nvsr, NVSR_SRMODE_TIMING6, 2,
		mode->h_back_porch);
	NVSR_RETV(ret, "Failed to write SR Hbp\n");

	ret = nvsr->reg_ops.write(nvsr, NVSR_SRMODE_TIMING8, 1,
		mode->h_sync_width);
	NVSR_RETV(ret, "Failed to write SR Hsync width\n");

	val = (mode->flags & TEGRA_DC_MODE_FLAG_NEG_H_SYNC) ?
		NVSR_SRMODE_TIMING9_HSYNC_POL_NEG :
		NVSR_SRMODE_TIMING9_HSYNC_POL_POS;
	ret = nvsr->reg_ops.write(nvsr, NVSR_SRMODE_TIMING9, 1, val);
	NVSR_RETV(ret, "Failed to write SR Hsync pulse polarity\n");

	/* Vertical timing */

	ret = nvsr->reg_ops.write(nvsr, NVSR_SRMODE_TIMING10, 2,
		mode->v_active);
	NVSR_RETV(ret, "Failed to write SR Vactive\n");

	val = mode->v_front_porch + mode->v_sync_width + mode->v_back_porch;
	ret = nvsr->reg_ops.write(nvsr, NVSR_SRMODE_TIMING12, 2, val);
	NVSR_RETV(ret, "Failed to write SR Vblank\n");
	vblank = val;

	ret = nvsr->reg_ops.write(nvsr, NVSR_SRMODE_TIMING14, 2,
		mode->v_front_porch);
	NVSR_RETV(ret, "Failed to write SR Vfp\n");

	ret = nvsr->reg_ops.write(nvsr, NVSR_SRMODE_TIMING16, 2,
		mode->v_back_porch);
	NVSR_RETV(ret, "Failed to write SR Vbp\n");

	ret = nvsr->reg_ops.write(nvsr, NVSR_SRMODE_TIMING18, 1,
		mode->v_sync_width);
	NVSR_RETV(ret, "Failed to write SR Vsync width\n");

	val = (mode->flags & TEGRA_DC_MODE_FLAG_NEG_V_SYNC) ?
		NVSR_SRMODE_TIMING19_VSYNC_POL_NEG :
		NVSR_SRMODE_TIMING19_VSYNC_POL_POS;

	ret = nvsr->reg_ops.write(nvsr, NVSR_SRMODE_TIMING19, 1, val);
	NVSR_RETV(ret, "Failed to write SR Hsync pulse polarity\n");

	nvsr->sr_timing_frame_time_us =
		pclk / (mode->h_active + hblank) / (mode->v_active + vblank);
	nvsr->sr_timing_frame_time_us = 1000000 / nvsr->sr_timing_frame_time_us;

	return 0;
}

/* Write pass-through timing: DC to SRC */
static int tegra_dc_nvsr_write_pt_timing(struct tegra_dc_nvsr_data *nvsr)
{
	int ret;
	struct tegra_dc_mode *mode = &nvsr->pt_timing;
	u32 val, hblank, vblank, pclk;


	if (mode->pclk > nvsr->cap.max_pt_pclk) {
		NVSR_ERR("DC->SRC pixel clock (%dHz) > max (%dHz)\n",
			mode->pclk, nvsr->cap.max_pt_pclk);
		return -EINVAL;
	}

	/* NVSR stores pclk in units of 20KHz */
	val = DIV_ROUND_UP(mode->pclk, 20000);
	ret = nvsr->reg_ops.write(nvsr, NVSR_PTMODE_PIXEL_CLOCK0, 2, val);
	NVSR_RETV(ret, "Failed to write PT pclk\n");
	pclk = val * 20000;

	/* Horizontal timing */

	ret = nvsr->reg_ops.write(nvsr, NVSR_PTMODE_TIMING0, 2, mode->h_active);
	NVSR_RETV(ret, "Failed to write PT Hactive\n");

	val = mode->h_front_porch + mode->h_sync_width + mode->h_back_porch;
	ret = nvsr->reg_ops.write(nvsr, NVSR_PTMODE_TIMING2, 2, val);
	NVSR_RETV(ret, "Failed to write PT Hblank\n");
	hblank = val;

	ret = nvsr->reg_ops.write(nvsr, NVSR_PTMODE_TIMING4, 2,
		mode->h_front_porch);
	NVSR_RETV(ret, "Failed to write PT Hfp\n");

	ret = nvsr->reg_ops.write(nvsr, NVSR_PTMODE_TIMING6, 2,
		mode->h_back_porch);
	NVSR_RETV(ret, "Failed to write PT Hbp\n");

	ret = nvsr->reg_ops.write(nvsr, NVSR_PTMODE_TIMING8, 1,
		mode->h_sync_width);
	NVSR_RETV(ret, "Failed to write PT Hsync width\n");

	val = (mode->flags & TEGRA_DC_MODE_FLAG_NEG_H_SYNC) ?
		NVSR_PTMODE_TIMING9_HSYNC_POL_NEG :
		NVSR_PTMODE_TIMING9_HSYNC_POL_POS;
	ret = nvsr->reg_ops.write(nvsr, NVSR_PTMODE_TIMING9, 1, val);
	NVSR_RETV(ret, "Failed to write PT Hsync pulse polarity\n");

	/* Vertical timing */

	ret = nvsr->reg_ops.write(nvsr, NVSR_PTMODE_TIMING10, 2,
		mode->v_active);
	NVSR_RETV(ret, "Failed to write PT Vactive\n");

	val = mode->v_front_porch + mode->v_sync_width + mode->v_back_porch;
	ret = nvsr->reg_ops.write(nvsr, NVSR_PTMODE_TIMING12, 2, val);
	NVSR_RETV(ret, "Failed to write PT Vblank\n");
	vblank = val;

	ret = nvsr->reg_ops.write(nvsr, NVSR_PTMODE_TIMING14, 2,
		mode->v_front_porch);
	NVSR_RETV(ret, "Failed to write PT Vfp\n");

	ret = nvsr->reg_ops.write(nvsr, NVSR_PTMODE_TIMING16, 2,
		mode->v_back_porch);
	NVSR_RETV(ret, "Failed to write PT Vbp\n");

	ret = nvsr->reg_ops.write(nvsr, NVSR_PTMODE_TIMING18, 1,
		mode->v_sync_width);
	NVSR_RETV(ret, "Failed to write PT Vsync width\n");

	val = (mode->flags & TEGRA_DC_MODE_FLAG_NEG_V_SYNC) ?
		NVSR_PTMODE_TIMING19_VSYNC_POL_NEG :
		NVSR_PTMODE_TIMING19_VSYNC_POL_POS;
	ret = nvsr->reg_ops.write(nvsr, NVSR_PTMODE_TIMING19, 1, val);
	NVSR_RETV(ret, "Failed to write PT Hsync pulse polarity\n");

	nvsr->pt_timing_frame_time_us =
		pclk / (mode->h_active + hblank) / (mode->v_active + vblank);
	nvsr->pt_timing_frame_time_us = 1000000 / nvsr->pt_timing_frame_time_us;

	return 0;
}

static int tegra_dc_nvsr_update_timings(struct tegra_dc_nvsr_data *nvsr)
{
	int ret, i;
	struct tegra_dc_mode *modes = nvsr->dc->out->modes;
	struct tegra_dc_mode *mode = &nvsr->dc->mode;
	int n_modes = nvsr->dc->out->n_modes;
	u32 min_vblank, max_vblank, min_hblank, max_hblank;
	u32 vblank, hblank;

	/* If SR isn't supported, no need to program timings */
	if (!nvsr->cap.sparse_mode_support)
		return 0;

	/* Find min/max blank timings */
	if (n_modes) {
		min_vblank = max_vblank =
			modes[0].v_front_porch + modes[0].v_sync_width
			+ modes[0].v_back_porch;
		min_hblank = max_hblank =
			modes[0].h_front_porch + modes[0].h_sync_width
			+ modes[0].h_back_porch;

		for (i = 1; i < n_modes; i++) {
			vblank = modes[i].v_front_porch + modes[i].v_sync_width
				+ modes[i].v_back_porch;
			hblank = modes[i].h_front_porch + modes[i].h_sync_width
				+ modes[i].h_back_porch;

			if (vblank > max_vblank)
				max_vblank = vblank;
			if (vblank < min_vblank)
				min_vblank = vblank;
			if (hblank > max_hblank)
				max_hblank = hblank;
			if (hblank < min_hblank)
				min_hblank = hblank;
		}
	} else {
		max_vblank = min_vblank =
			mode->v_front_porch + mode->v_sync_width +
			mode->v_back_porch;
		max_hblank = min_hblank =
			mode->h_front_porch + mode->h_sync_width +
			mode->h_back_porch;
	}


	ret = nvsr->reg_ops.write(nvsr, NVSR_BLANK_TIMING0, 2, min_vblank);
	NVSR_RETV(ret, "Failed to init timing\n");
	ret = nvsr->reg_ops.write(nvsr, NVSR_BLANK_TIMING2, 2, max_vblank);
	NVSR_RETV(ret, "Failed to init timing\n");
	ret = nvsr->reg_ops.write(nvsr, NVSR_BLANK_TIMING4, 2, min_hblank);
	NVSR_RETV(ret, "Failed to init timing\n");
	ret = nvsr->reg_ops.write(nvsr, NVSR_BLANK_TIMING6, 2, max_hblank);
	NVSR_RETV(ret, "Failed to init timing\n");

	ret = tegra_dc_nvsr_write_pt_timing(nvsr);
	NVSR_RETV(ret, "Failed to init pass-through timing\n");
	ret = tegra_dc_nvsr_write_sr_timing(nvsr);
	NVSR_RETV(ret, "Failed to init self-refresh timing\n");

	return 0;
}

static int tegra_nvsr_read_dpaux(struct tegra_dc_nvsr_data *nvsr,
	u32 reg, u32 size, u32 *val)
{
	int ret, i;
	u32 aux_stat;
	u8 data[DP_AUX_MAX_BYTES] = {0};

	ret = tegra_dc_dpaux_read(nvsr->out_data.dp, DPAUX_DP_AUXCTL_CMD_AUXRD,
		reg, data, &size, &aux_stat);
	NVSR_RETV(ret,
		"DPAUX read failed: reg = 0x%x, size = %d, aux_stat = %d\n",
		reg, size, aux_stat);

	*val = 0;
	for (i = 0; i < DP_AUX_MAX_BYTES; i++)
		*val |= data[i] << (i * 8);

	return 0;
}

static int tegra_nvsr_write_dpaux(struct tegra_dc_nvsr_data *nvsr,
	u32 reg, u32 size, u64 val)
{
	int i, ret;
	u32 aux_stat;
	u8 data[DP_AUX_MAX_BYTES] = {0};

	for (i = 0; i < size; i++) {
		data[i] = val & 0xffllu;
		val = val >> 8;
	}

	ret = tegra_dc_dpaux_write(nvsr->out_data.dp, DPAUX_DP_AUXCTL_CMD_AUXWR,
		reg, data, &size, &aux_stat);

	if (ret && (nvsr->src_id.device != HX8880_A))
		return ret;

	return 0;
}

/* Enter Sparse and turn off link */
static int tegra_dc_nvsr_sparse_enable(struct tegra_dc_nvsr_data *nvsr,
	bool powerdown_link)
{
	int ret = 0;
	struct tegra_dc *dc = nvsr->dc;

	if (!nvsr->cap.sparse_mode_support)
		return -ENODEV;

	if (nvsr->sr_active)
		return 0;

	mutex_lock(&nvsr->dc->lock);

	/* Start a new self-refresh state */
	ret = tegra_dc_nvsr_enter_sr(nvsr);
	NVSR_RETV(ret, "Entering SR failed\n");
	ret = tegra_dc_nvsr_wait_until_status(nvsr,
		NVSR_STATUS_SR_STATE_SR_ACTIVE);
	NVSR_RETV(ret, "Timed out while waiting for SR entry\n");

	tegra_dc_nvsr_disable_link(nvsr, powerdown_link);

	dc->out->flags |= TEGRA_DC_OUT_NVSR_MODE;

	nvsr->sparse_active = true;

	mutex_unlock(&nvsr->dc->lock);
	return ret;
}

inline void tegra_dc_nvsr_irq(struct tegra_dc_nvsr_data *nvsr,
	unsigned long status)
{
	if ((status & MSF_INT) && (nvsr->waiting_on_framelock)) {
		tegra_dc_mask_interrupt(nvsr->dc, MSF_INT);
		nvsr->waiting_on_framelock = false;
		complete(&nvsr->framelock_comp);
	}
}

static inline int tegra_dc_nvsr_reset_src(struct tegra_dc_nvsr_data *nvsr)
{
	int ret;

	ret = tegra_dc_nvsr_src_power_off(nvsr);
	NVSR_RETV(ret, "Failed to reset SRC.\n");
	ret = tegra_dc_nvsr_src_power_on(nvsr);
	NVSR_RETV(ret, "Failed to reset SRC.\n");

	return 0;
}

static inline int tegra_dc_nvsr_config_resync_method
	(struct tegra_dc_nvsr_data *nvsr)
{
	u32 val, ret;
	struct tegra_dc *dc = nvsr->dc;

	if (nvsr->cap.resync & NVSR_SR_CAPS0_RESYNC_CAP_FL) {
		nvsr->resync_method = NVSR_RESYNC_CTL_METHOD_FL;

		/* Configure MSF pin */
		val = PIN_OUTPUT_LSPI_OUTPUT_DIS;
		tegra_dc_writel(dc, val, DC_COM_PIN_OUTPUT_ENABLE3);
		val = MSF_ENABLE | MSF_LSPI | MSF_POLARITY_LOW;
		tegra_dc_writel(dc, val, DC_CMD_DISPLAY_COMMAND_OPTION0);
		/* Enable MSF (framelock) interrupt in DC */
		val = tegra_dc_readl(dc, DC_CMD_INT_ENABLE);
		val |= MSF_INT;
		tegra_dc_writel(dc, val, DC_CMD_INT_ENABLE);

		/* Set entry via sideband */
		val = NVSR_SRC_CTL1_SIDEBAND_ENTRY_SEL_YES;
		val |= NVSR_SRC_CTL1_SIDEBAND_ENTRY_MASK_ENABLE;
		ret = nvsr->reg_ops.write(nvsr, NVSR_SRC_CTL1, 1, val);
		NVSR_RETV(ret, "Failed to enter SR via sideband\n");
	} else if ((nvsr->cap.resync & NVSR_SR_CAPS0_RESYNC_CAP_BS) ||
			(nvsr->cap.resync & NVSR_SR_CAPS0_RESYNC_CAP_SS)) {
		NVSR_WARN("Sliding Sync and Blank Stretching resync methods not supported; defaulting to Immediate\n");
		nvsr->resync_method = NVSR_RESYNC_CTL_METHOD_IMM;
	} else
		nvsr->resync_method = NVSR_RESYNC_CTL_METHOD_IMM;

	nvsr->resync_delay = 0;
	return 0;
}

static int tegra_dc_nvsr_init_src(struct tegra_dc_nvsr_data *nvsr)
{
	int ret;
	u32 val;

#if HIMAX_HX8880_A
	/* This WAR is needed before performing any DPAUX transactions */
	nvsr->src_id.device = HX8880_A;
#endif

	/* Reset SRC if it's not in idle */
	ret = nvsr->reg_ops.read(nvsr, NVSR_STATUS, 1, &val);
	NVSR_RETV(ret, "Failed to read status register.\n");
	if ((val & NVSR_STATUS_SR_STATE_MASK)
		!= NVSR_STATUS_SR_STATE_IDLE) {
		/* Reset SRC */
		ret = tegra_dc_nvsr_reset_src(nvsr);
		NVSR_RETV(ret, "Failed to reset SRC\n");
	}

	/* Can't populate capabilities during init since DP interface
	 * isn't enabled yet at that point. Read capabilities here instead,
	 * just once */
	if (unlikely(!nvsr->cap.is_init)) {
		ret = tegra_dc_nvsr_query_capabilities(nvsr);
		NVSR_RETV(ret, "Can't ready capabilities\n");

		nvsr->pt_timing = nvsr->dc->mode;
		nvsr->sr_timing = nvsr->dc->mode;
	}

	ret = tegra_dc_nvsr_update_timings(nvsr);
	NVSR_RETV(ret, "Failed to write SRC timings\n");

	ret = tegra_dc_nvsr_config_resync_method(nvsr);
	NVSR_RETV(ret, "Failed to configure resync method\n");

	nvsr->is_init = true;

	return 0;
}

static int tegra_dc_nvsr_sparse_disable(struct tegra_dc_nvsr_data *nvsr)
{
	int ret;
	struct tegra_dc *dc = nvsr->dc;

	if (!nvsr->sr_active)
		return 0;

	mutex_lock(&nvsr->dc->lock);

	tegra_dc_get(dc);

	tegra_dc_nvsr_enable_link(nvsr);

	/* Program current frame as NC, to be triggered in MSF/FL interrupt */
	tegra_dc_writel(dc, GENERAL_ACT_REQ | NC_HOST_TRIG,
					DC_CMD_STATE_CONTROL);

	/* Request SR-exit from SRC, and wait */
	ret = tegra_dc_nvsr_exit_sr(nvsr);
	if (ret) {
		NVSR_ERR("Exiting SR failed\n");

		/* Attempt to reset and re-initialize SRC. */
		BUG_ON(tegra_dc_nvsr_init_src(nvsr));
	}

	/* Switch DC NC-->C during the single NC frame */
	tegra_dc_writel(dc, DISP_CTRL_MODE_C_DISPLAY,
					DC_CMD_DISPLAY_COMMAND);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	dc->out->flags &= ~TEGRA_DC_OUT_NVSR_MODE;

	tegra_dc_put(dc);

	nvsr->sparse_active = false;
	mutex_unlock(&nvsr->dc->lock);
	return 0;
}

static void tegra_dc_nvsr_destroy(struct tegra_dc *dc)
{
	struct tegra_dc_nvsr_data *nvsr = dc->nvsr;
	if (nvsr->out_ops.destroy)
		nvsr->out_ops.destroy(dc);

	kfree(nvsr);
}

static bool tegra_dc_nvsr_detect(struct tegra_dc *dc)
{
	struct tegra_dc_nvsr_data *nvsr = dc->nvsr;
	if (nvsr->out_ops.detect)
		return nvsr->out_ops.detect(dc);

	return -EINVAL;
}

static void tegra_dc_nvsr_enable(struct tegra_dc *dc)
{
	struct tegra_dc_nvsr_data *nvsr = dc->nvsr;

	if (nvsr->out_ops.enable) {
		nvsr->out_ops.enable(dc);
		nvsr->link_status = LINK_ENABLED;
	}

	tegra_dc_nvsr_init_src(nvsr);
}

static void tegra_dc_nvsr_disable(struct tegra_dc *dc)
{
	struct tegra_dc_nvsr_data *nvsr = dc->nvsr;

	tegra_dc_nvsr_src_power_off(nvsr);

	if (nvsr->out_ops.disable)
		nvsr->out_ops.disable(dc);

	nvsr->link_status = LINK_DISABLED;
}

static void tegra_dc_nvsr_hold(struct tegra_dc *dc)
{
	struct tegra_dc_nvsr_data *nvsr = dc->nvsr;
	if (nvsr->out_ops.hold)
		nvsr->out_ops.hold(dc);
}

static void tegra_dc_nvsr_release(struct tegra_dc *dc)
{
	struct tegra_dc_nvsr_data *nvsr = dc->nvsr;
	if (nvsr->out_ops.release)
		nvsr->out_ops.release(dc);
}

static void tegra_dc_nvsr_idle(struct tegra_dc *dc)
{
	struct tegra_dc_nvsr_data *nvsr = dc->nvsr;
	if (nvsr->out_ops.idle)
		nvsr->out_ops.idle(dc);
}

static void tegra_dc_nvsr_suspend(struct tegra_dc *dc)
{
	struct tegra_dc_nvsr_data *nvsr = dc->nvsr;
	if (nvsr->out_ops.suspend)
		nvsr->out_ops.suspend(dc);
}

static void tegra_dc_nvsr_resume(struct tegra_dc *dc)
{
	struct tegra_dc_nvsr_data *nvsr = dc->nvsr;
	if (nvsr->out_ops.resume)
		nvsr->out_ops.resume(dc);
}

static bool tegra_dc_nvsr_mode_filter(const struct tegra_dc *dc,
		struct fb_videomode *mode)
{
	struct tegra_dc_nvsr_data *nvsr = dc->nvsr;
	if (nvsr->out_ops.mode_filter)
		return nvsr->out_ops.mode_filter(dc, mode);

	return -EINVAL;
}

static long tegra_dc_nvsr_setup_clk(struct tegra_dc *dc, struct clk *clk)
{
	struct tegra_dc_nvsr_data *nvsr = dc->nvsr;
	if (nvsr->out_ops.setup_clk)
		return nvsr->out_ops.setup_clk(dc, clk);

	return -EINVAL;
}

static void tegra_dc_nvsr_modeset_notifier(struct tegra_dc *dc)
{
	struct tegra_dc_nvsr_data *nvsr = dc->nvsr;
	if (nvsr->out_ops.modeset_notifier)
		return nvsr->out_ops.modeset_notifier(dc);
}

static void tegra_dc_nvsr_init_out_ops(struct tegra_dc_nvsr_data *nvsr,
	struct tegra_dc_out_ops *out_ops)
{
	if (out_ops->init)
		nvsr->out_ops.init = out_ops->init;

	if (out_ops->destroy) {
		tegra_dc_nvsr_ops.destroy = tegra_dc_nvsr_destroy;
		nvsr->out_ops.destroy = out_ops->destroy;
	}

	if (out_ops->detect) {
		tegra_dc_nvsr_ops.detect = tegra_dc_nvsr_detect;
		nvsr->out_ops.detect = out_ops->detect;
	}

	if (out_ops->enable) {
		tegra_dc_nvsr_ops.enable = tegra_dc_nvsr_enable;
		nvsr->out_ops.enable = out_ops->enable;
	}

	if (out_ops->disable) {
		tegra_dc_nvsr_ops.disable = tegra_dc_nvsr_disable;
		nvsr->out_ops.disable = out_ops->disable;
	}

	if (out_ops->hold) {
		tegra_dc_nvsr_ops.hold = tegra_dc_nvsr_hold;
		nvsr->out_ops.hold = out_ops->hold;
	}

	if (out_ops->release) {
		tegra_dc_nvsr_ops.release = tegra_dc_nvsr_release;
		nvsr->out_ops.release = out_ops->release;
	}
	if (out_ops->idle) {
		tegra_dc_nvsr_ops.idle = tegra_dc_nvsr_idle;
		nvsr->out_ops.idle = out_ops->idle;
	}
	if (out_ops->suspend) {
		tegra_dc_nvsr_ops.suspend = tegra_dc_nvsr_suspend;
		nvsr->out_ops.suspend = out_ops->suspend;
	}
	if (out_ops->resume) {
		tegra_dc_nvsr_ops.resume = tegra_dc_nvsr_resume;
		nvsr->out_ops.resume = out_ops->resume;
	}
	if (out_ops->mode_filter) {
		tegra_dc_nvsr_ops.mode_filter = tegra_dc_nvsr_mode_filter;
		nvsr->out_ops.mode_filter = out_ops->mode_filter;
	}
	if (out_ops->setup_clk) {
		tegra_dc_nvsr_ops.setup_clk = tegra_dc_nvsr_setup_clk;
		nvsr->out_ops.setup_clk = out_ops->setup_clk;
	}
	if (out_ops->modeset_notifier) {
		tegra_dc_nvsr_ops.modeset_notifier =
			tegra_dc_nvsr_modeset_notifier;
		nvsr->out_ops.modeset_notifier = out_ops->modeset_notifier;
	}
}

#ifdef CONFIG_DEBUG_FS

static void tegra_dc_nvsr_get(struct tegra_dc_nvsr_data *nvsr)
{
	tegra_dc_get(nvsr->dc);
	clk_prepare_enable(nvsr->aux_clk);
}

static void tegra_dc_nvsr_put(struct tegra_dc_nvsr_data *nvsr)
{
	clk_disable_unprepare(nvsr->aux_clk);
	tegra_dc_put(nvsr->dc);
}

static int nvsr_dbg_read_read_show(struct seq_file *s, void *unused)
{ return 0; }
static int nvsr_dbg_read_write_show(struct seq_file *s, void *unused)
{ return 0; }

static int nvsr_dbg_read_open(struct inode *inode, struct file *file)
{
	return single_open(file, nvsr_dbg_read_read_show, inode->i_private);
}

static int nvsr_dbg_write_open(struct inode *inode, struct file *file)
{
	return single_open(file, nvsr_dbg_read_write_show, inode->i_private);
}

static const char *tegra_dc_nvsr_state_to_string(u32 val)
{
	val &= NVSR_STATUS_SR_STATE_MASK;
	switch (val) {
	case NVSR_STATUS_SR_STATE_OFFLINE:
		return "Offline";
	case NVSR_STATUS_SR_STATE_IDLE:
		return "Idle";
	case NVSR_STATUS_SR_STATE_SR_ENTRY_TRIG:
		return "SR-entry triggered";
	case NVSR_STATUS_SR_STATE_SR_ENTRY_CACHING:
		return "SR-entry caching";
	case NVSR_STATUS_SR_STATE_SR_ENTRY_RDY:
		return "SR-entry ready";
	case NVSR_STATUS_SR_STATE_SR_ACTIVE:
		return "SR active";
	case NVSR_STATUS_SR_STATE_SR_EXIT_TRIG:
		return "SR-exit triggered";
	case NVSR_STATUS_SR_STATE_SR_EXIT_RESYNC:
		return "SR-exit resynching";
	default:
		return "Unknown";
	}

	/* Should never get here */
	return "";
}

static int nvsr_dbg_status_show(struct seq_file *s, void *unused)
{
	struct tegra_dc_nvsr_data *nvsr = s->private;
	u32 val, val2;
	int ret;

	if (!nvsr->is_init) {
		seq_puts(s, "NVSR has not been initialized.\n");
		return -EINVAL;
	}

	tegra_dc_nvsr_get(nvsr);

	seq_puts(s, "SRC Info\n");
	seq_puts(s, "--------\n");
	seq_printf(s, "Device ID: \t\t\t\t%d\n", nvsr->src_id.device);
	seq_printf(s, "Vendor ID: \t\t\t\t%d\n", nvsr->src_id.vendor);
	seq_printf(s, "Sparse Refresh mode supported: \t\t%d\n",
		nvsr->cap.sparse_mode_support);
	seq_printf(s, "\tEntry request band capability: \t%d\n",
		nvsr->cap.sr_entry_req);
	seq_printf(s, "\tExit request band capability: \t%d\n",
		nvsr->cap.sr_exit_req);
	seq_printf(s, "\tResync capabilities:\n");
	if (!nvsr->cap.resync) {
		seq_puts(s, "\t\tImmediate\n");
	} else {
		if (nvsr->cap.resync &
			NVSR_SR_CAPS0_RESYNC_CAP_FL)
			seq_puts(s, "\t\tFramelock\n");
		if (nvsr->cap.resync &
			NVSR_SR_CAPS0_RESYNC_CAP_BS)
			seq_puts(s, "\t\tBlank stretching\n");
		if (nvsr->cap.resync &
			NVSR_SR_CAPS0_RESYNC_CAP_SS)
			seq_puts(s, "\t\tSliding sync\n");
	}
	seq_printf(s, "Buffered mode supported: \t\t%d\n",
		nvsr->cap.buffered_mode_support);
	seq_printf(s, "Burst mode supported: \t\t\t%d\n",
		nvsr->cap.burst_mode_support);
	seq_printf(s, "\tSRC max input pclk: \t\t%d\n",
		nvsr->cap.max_pt_pclk);
	seq_puts(s, "\n");
	seq_puts(s, "SRC status\n");
	seq_puts(s, "----------\n");
	seq_printf(s, "In Sparse Refresh (mode %d): \t\t%d\n",
		nvsr->sr_active, nvsr->sparse_active);
	seq_printf(s, "Resync method: \t\t\t%s\n",
		!nvsr->resync_method ?
		"Immediate" :
		nvsr->resync_method == NVSR_SR_CAPS0_RESYNC_CAP_FL ?
		"Framelock" :
		nvsr->resync_method == NVSR_RESYNC_CTL_METHOD_BS ?
		"Blank stretching" :
		nvsr->resync_method == NVSR_SR_CAPS0_RESYNC_CAP_SS ?
		"Sliding sync" :
		"unknown");
	seq_printf(s, "Resync delay (frames): \t\t%d\n", nvsr->resync_delay);
	seq_printf(s, "SRC powered: \t\t\t%d\n", nvsr->src_on);

	ret = nvsr->reg_ops.read(nvsr, NVSR_STATUS, 1, &val);
	seq_printf(s, "Current SRC status: \t%s\n",
		tegra_dc_nvsr_state_to_string(val));

	seq_puts(s, "PT Timing:\n");

	ret = nvsr->reg_ops.read(nvsr, NVSR_PTMODE_PIXEL_CLOCK0, 2, &val);
	seq_printf(s, "\tPixel clock: \t%dHz\n", ret ? -1 : val * 20000);

	/* Horizontal PT timing */

	ret = nvsr->reg_ops.read(nvsr, NVSR_PTMODE_TIMING0, 2, &val);
	seq_printf(s, "\tHactive: \t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_PTMODE_TIMING2, 2, &val);
	seq_printf(s, "\tHblank: \t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_PTMODE_TIMING4, 2, &val);
	seq_printf(s, "\tHfp: \t\t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_PTMODE_TIMING6, 2, &val);
	seq_printf(s, "\tHbp: \t\t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_PTMODE_TIMING8, 1, &val);
	seq_printf(s, "\tHsync width: \t%d\n", val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_PTMODE_TIMING9, 1, &val);
	val2 = val & NVSR_PTMODE_TIMING9_HORZ_BORDER_MASK;
	val = (val & NVSR_PTMODE_TIMING9_HSYNC_POL_MASK) >>
		NVSR_PTMODE_TIMING9_HSYNC_POL_SHIFT;
	seq_printf(s, "\tHborder: \t%d\n", ret ? -1 : val2);
	seq_printf(s, "\tHsync pulse polarity: \t%d\n", ret ? -1 : val);

	/* Vertical PT timing */

	ret = nvsr->reg_ops.read(nvsr, NVSR_PTMODE_TIMING10, 2, &val);
	seq_printf(s, "\tVactive: \t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_PTMODE_TIMING12, 2, &val);
	seq_printf(s, "\tVblank \t\t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_PTMODE_TIMING14, 2, &val);
	seq_printf(s, "\tVfp: \t\t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_PTMODE_TIMING16, 2, &val);
	seq_printf(s, "\tVbp: \t\t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_PTMODE_TIMING18, 1, &val);
	seq_printf(s, "\tVsync width: \t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_PTMODE_TIMING19, 1, &val);
	val2 = val & NVSR_PTMODE_TIMING19_VERT_BORDER_MASK;
	val = (val & NVSR_PTMODE_TIMING19_VSYNC_POL_MASK) >>
		NVSR_PTMODE_TIMING19_VSYNC_POL_SHIFT;
	seq_printf(s, "\tVborder: \t%d\n", ret ? -1 : val2);
	seq_printf(s, "\tVsync pulse polarity: \t%d\n", ret ? -1 : val);
	seq_printf(s, "\tFrame time: \t%dus\n", nvsr->pt_timing_frame_time_us);

	seq_puts(s, "SR Timing:\n");

	ret = nvsr->reg_ops.read(nvsr, NVSR_SRMODE_PIXEL_CLOCK0, 2, &val);
	seq_printf(s, "\tPixel clock: \t%dHz\n", ret ? -1 : val * 20000);

	/* Horizontal SR timing */

	ret = nvsr->reg_ops.read(nvsr, NVSR_SRMODE_TIMING0, 2, &val);
	seq_printf(s, "\tHactive: \t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_SRMODE_TIMING2, 2, &val);
	seq_printf(s, "\tHblank: \t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_SRMODE_TIMING4, 2, &val);
	seq_printf(s, "\tHfp: \t\t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_SRMODE_TIMING6, 2, &val);
	seq_printf(s, "\tHbp: \t\t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_SRMODE_TIMING8, 1, &val);
	seq_printf(s, "\tHsync width: \t%d\n", val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_SRMODE_TIMING9, 1, &val);
	val2 = val & NVSR_SRMODE_TIMING9_HORZ_BORDER_MASK;
	val = (val & NVSR_SRMODE_TIMING9_HSYNC_POL_MASK) >>
		NVSR_SRMODE_TIMING9_HSYNC_POL_SHIFT;
	seq_printf(s, "\tHborder: \t%d\n", ret ? -1 : val2);
	seq_printf(s, "\tHsync pulse polarity: \t%d\n", ret ? -1 : val);

	/* Vertical SR timing */

	ret = nvsr->reg_ops.read(nvsr, NVSR_SRMODE_TIMING10, 2, &val);
	seq_printf(s, "\tVactive: \t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_SRMODE_TIMING12, 2, &val);
	seq_printf(s, "\tVblank \t\t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_SRMODE_TIMING14, 2, &val);
	seq_printf(s, "\tVfp: \t\t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_SRMODE_TIMING16, 2, &val);
	seq_printf(s, "\tVbp: \t\t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_SRMODE_TIMING18, 1, &val);
	seq_printf(s, "\tVsync width: \t%d\n", ret ? -1 : val);

	ret = nvsr->reg_ops.read(nvsr, NVSR_SRMODE_TIMING19, 1, &val);
	val2 = val & NVSR_SRMODE_TIMING19_VERT_BORDER;
	val = (val & NVSR_SRMODE_TIMING19_VSYNC_POL_MASK) >>
		NVSR_SRMODE_TIMING19_VSYNC_POL_SHIFT;
	seq_printf(s, "\tVborder: \t%d\n", ret ? -1 : val2);
	seq_printf(s, "\tVsync pulse polarity: \t%d\n", ret ? -1 : val);
	seq_printf(s, "\tFrame time: \t%dus\n", nvsr->sr_timing_frame_time_us);

	tegra_dc_nvsr_put(nvsr);

	return 0;
}

static int nvsr_dbg_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, nvsr_dbg_status_show, inode->i_private);
}

static ssize_t nvsr_dbg_read_write(struct file *file,
	const char __user *userbuf, size_t count, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	struct tegra_dc_nvsr_data *nvsr = m ? m->private : NULL;
	char buf[128], *addr, *size, *pbuf;
	u32 uaddr, usize, val;

	if (!nvsr)
		return -ENODEV;

	if (sizeof(buf) <= count)
		return -EFAULT;

	if (copy_from_user(buf, userbuf, sizeof(buf)))
		return -EFAULT;

	buf[count] = '\0';
	strim(buf);

	pbuf = buf;
	addr = strsep(&pbuf, " ");
	size = strsep(&pbuf, " ");

	if (!size) {
		NVSR_INFO("Syntax error: <addr_hex> <num_bytes>\n");
		return -EINVAL;
	}

	if (kstrtou32(addr, 16, &uaddr)) {
		NVSR_INFO("Can't parse address \"%s\"\n", addr);
		return -EINVAL;
	}

	if (kstrtou32(size, 10, &usize)) {
		NVSR_INFO("Can't parse data size\"%s\"\n", size);
		return -EINVAL;
	}

	NVSR_INFO("Reading %u bytes from address 0x%x:\n", usize, uaddr);

	tegra_dc_nvsr_get(nvsr);

	if (nvsr->reg_ops.read(nvsr, uaddr, usize, &val))
		NVSR_INFO("Failed to read register.\n");

	NVSR_INFO("NVSR: Read back 0x%x\n", val);

	tegra_dc_nvsr_put(nvsr);

	return count;
}

static ssize_t nvsr_dbg_write_write(struct file *file,
	const char __user *userbuf, size_t count, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	struct tegra_dc_nvsr_data *nvsr = m ? m->private : NULL;
	char buf[128], *addr, *val, *size, *pbuf;
	u32 uaddr, usize;
	u64 uval;

	if (!nvsr)
		return -ENODEV;

	if (sizeof(buf) <= count)
		return -EFAULT;

	if (copy_from_user(buf, userbuf, sizeof(buf)))
		return -EFAULT;

	buf[count] = '\0';
	strim(buf);

	pbuf = buf;
	addr = strsep(&pbuf, " ");
	val = strsep(&pbuf, " ");
	size = strsep(&pbuf, " ");

	if (!val || !size) {
		NVSR_INFO("Syntax error: <addr_hex> <data_hex> <num_bytes>\n");
		return -EINVAL;
	}

	if (kstrtou32(addr, 16, &uaddr)) {
		NVSR_INFO("Can't parse address \"%s\"\n", addr);
		return -EINVAL;
	}

	if (kstrtou64(val, 16, &uval)) {
		NVSR_INFO("Can't parse value\"%s\"\n", val);
		return -EINVAL;
	}

	if (kstrtou32(size, 10, &usize)) {
		NVSR_INFO("Can't parse data size\"%s\"\n", size);
		return -EINVAL;
	}

	NVSR_INFO("Writing value of 0x%llx (%u bytes) to address 0x%x...\n",
		uval, usize, uaddr);

	tegra_dc_nvsr_get(nvsr);

	if (nvsr->reg_ops.write(nvsr, uaddr, usize, uval))
		NVSR_INFO("Failed to write to register.\n");

	tegra_dc_nvsr_put(nvsr);

	return count;
}

static const struct file_operations nvsr_dbg_read_fops = {
	.open		= nvsr_dbg_read_open,
	.read		= seq_read,
	.write		= nvsr_dbg_read_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations nvsr_dbg_write_fops = {
	.open		= nvsr_dbg_write_open,
	.read		= seq_read,
	.write		= nvsr_dbg_write_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations nvsr_dbg_status_fops = {
	.open		= nvsr_dbg_status_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void tegra_dc_nvsr_debug_create(struct tegra_dc_nvsr_data *nvsr)
{
	struct dentry *retval, *nvsrdir;

	nvsrdir = debugfs_create_dir("tegra_nvsr", NULL);
	if (!nvsrdir)
		return;

	retval = debugfs_create_file("read", S_IRUGO | S_IWUGO,
		nvsrdir, nvsr, &nvsr_dbg_read_fops);
	if (!retval)
		goto free_out;
	retval = debugfs_create_file("write", S_IRUGO | S_IWUGO,
		nvsrdir, nvsr, &nvsr_dbg_write_fops);
	if (!retval)
		goto free_out;
	retval = debugfs_create_file("status", S_IRUGO,
		nvsrdir, nvsr, &nvsr_dbg_status_fops);
	if (!retval)
		goto free_out;

	return;

free_out:
	debugfs_remove_recursive(nvsrdir);
	return;
}

#else
static inline void tegra_dc_nvsr_debug_create(struct tegra_dc_nvsr_data *nvsr)
{ }
#endif

static int tegra_dc_nvsr_init(struct tegra_dc *dc)
{
	int ret = 0;

	struct tegra_dc_nvsr_data *nvsr = kzalloc(sizeof(*nvsr), GFP_KERNEL);
	if (!nvsr) {
		ret = -ENOMEM;
		goto err_nvsr_init_out;
	}

	nvsr->dc = dc;
	dc->nvsr = nvsr;

	switch (dc->out->type) {
	case TEGRA_DC_OUT_NVSR_DP:
		tegra_dc_nvsr_init_out_ops(nvsr, &tegra_dc_dp_ops);
		if (nvsr->out_ops.init) {
			ret = nvsr->out_ops.init(dc);
			NVSR_RETV(ret, "Out ops init failed.\n");
		}
		nvsr->out_data.dp = tegra_dc_get_outdata(dc);
		nvsr->aux_clk = nvsr->out_data.dp->dpaux_clk;
		nvsr->reg_ops.read = tegra_nvsr_read_dpaux;
		nvsr->reg_ops.write = tegra_nvsr_write_dpaux;
		break;
	default:
		pr_err("NVSR output interface not found.\n");
		ret = -EINVAL;
		goto err_nvsr_init_free;
	}

	tegra_dc_nvsr_debug_create(nvsr);
	init_completion(&nvsr->framelock_comp);
	mutex_init(&nvsr->link_lock);

	return ret;

err_nvsr_init_free:
	kfree(nvsr);
err_nvsr_init_out:
	return ret;
}

struct tegra_dc_out_ops tegra_dc_nvsr_ops = {
	.init = tegra_dc_nvsr_init,
};

static struct kobject *nvsr_kobj;

static ssize_t sparse_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	struct device *dev = container_of((kobj->parent), struct device, kobj);
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);

	return snprintf(buf, PAGE_SIZE, "%d\n", dc->nvsr->sparse_active);
}

static ssize_t sparse_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct device *dev = container_of((kobj->parent), struct device, kobj);
	struct platform_device *ndev = to_platform_device(dev);
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	struct tegra_dc_nvsr_data *nvsr = dc->nvsr;
	u32 val;

	if (kstrtou32(buf, 10, &val) < 0) {
		dev_err(dev, "NVSR: bad sparse input: \"%s\"\n", buf);
		return -EINVAL;
	}

	if (val == 1)
		NVSR_RETV(tegra_dc_nvsr_sparse_enable(nvsr, false),
			"Failed to enter sparse\n")
	else if (val == 2)
		NVSR_RETV(tegra_dc_nvsr_sparse_enable(nvsr, true),
			"Failed to enter sparse\n")
	else if (!val)
		NVSR_RETV(tegra_dc_nvsr_sparse_disable(nvsr),
			"Failed to exit SR\n")

	return count;
}

static struct kobj_attribute nvsr_attr_sparse =
	__ATTR(sparse, S_IRUGO|S_IWUSR, sparse_show, sparse_store);

static struct attribute *nvsr_attrs[] = {
	&nvsr_attr_sparse.attr,
	NULL,
};

static struct attribute_group nvsr_attr_group = {
	.attrs = nvsr_attrs,
};

/* Sysfs initializer */
int nvsr_create_sysfs(struct device *dev)
{
	int retval;

	nvsr_kobj = kobject_create_and_add("nvsr", &dev->kobj);

	if (!nvsr_kobj)
		return -ENOMEM;

	retval = sysfs_create_group(nvsr_kobj, &nvsr_attr_group);

	if (retval) {
		kobject_put(nvsr_kobj);
		dev_err(dev, "%s: failed to create attributes\n", __func__);
	}

	return retval;
}

/* Sysfs destructor */
void nvsr_remove_sysfs(struct device *dev)
{
	if (nvsr_kobj) {
		sysfs_remove_group(nvsr_kobj, &nvsr_attr_group);
		kobject_put(nvsr_kobj);
	}
}

