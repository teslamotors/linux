/*
 * drivers/video/tegra/dc/nvdisp/nvdisp_bandwidth.c
 *
 * Copyright (c) 2016, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/platform/tegra/emc_bwmgr.h>
#include <linux/platform/tegra/isomgr.h>
#include <linux/platform/tegra/latency_allowance.h>

#include <mach/dc.h>
#include <mach/tegra_dc_ext.h>

#include "dc_priv.h"
#include "nvdisp.h"

#ifdef CONFIG_TEGRA_ISOMGR

/*
 * NVDISP_BW_DEDI_KBPS, NVDISP_BW_TOTAL_MC_LATENCY, and
 * NVDISP_BW_REQUIRED_HUBCLK_HZ were calculated with these settings
 * (all values were rounded up to the nearest unit):
 *	- 3 active heads
 *	- 2 active windows on each head:
 *		- 4096x2160@60p
 *		- 4BPP packed
 *		- BLx4
 *		- LUT disabled
 *		- Scaling disabled
 *		- Rotation disabled
 *	- Cursor active on each head:
 *		- 4BPP packed
 *		- Pitch
 *		- LUT disabled
 *
 * Refer to setDefaultSysParams in libnvimp/nvimp.c for the default sys params
 * that were used.
 */

/* Dedicated bw that is allocated to display (KB/s) */
#define NVDISP_BW_DEDI_KBPS		15207000

/* Total MC request latency that display can tolerate (usec) */
#define NVDISP_BW_TOTAL_MC_LATENCY	1

/* Required hubclk rate for the given display config (Hz) */
#define NVDISP_BW_REQUIRED_HUBCLK_HZ	358000000

/* Output id that we pass to tegra_dc_ext_process_bandwidth_negotiate */
#define NVDISP_BW_OUTPUT_ID		0

/* Global bw info shared across all heads */
static struct nvdisp_isoclient_bw_info ihub_bw_info;

/* Protects access to ihub_bw_info */
static DEFINE_MUTEX(tegra_nvdisp_bw_lock);


static void tegra_dc_set_latency_allowance(u32 bw)
{
	struct dc_to_la_params disp_params;
	unsigned long emc_freq_hz = tegra_bwmgr_get_emc_rate();

	/* Zero out this struct since it's ignored by the LA/PTSA driver. */
	memset(&disp_params, 0, sizeof(disp_params));

	/*
	 * Our bw is in KB/s, but LA takes MB/s. Round up to the
	 * next MB/s.
	 */
	if (bw != U32_MAX)
		bw = bw / 1000 + 1;

	if (tegra_set_disp_latency_allowance(TEGRA_LA_NVDISPLAYR,
					emc_freq_hz,
					bw,
					disp_params))
		pr_err("Failed to set latency allowance\n");
}

static void tegra_nvdisp_program_bandwidth(u32 bw,
					u32 latency,
					bool use_new_bw,
					bool la_dirty)
{
	if (!tegra_platform_is_silicon())
		return;

	if (IS_ERR_OR_NULL(ihub_bw_info.isomgr_handle))
		return;

	if (use_new_bw) {
		/* If we've already reserved the requested bw, return. */
		if (bw == ihub_bw_info.reserved_bw_kbps)
			return;

		latency = tegra_isomgr_reserve(ihub_bw_info.isomgr_handle,
								bw, latency);
		if (latency) {
			latency =
			tegra_isomgr_realize(ihub_bw_info.isomgr_handle);
			if (!latency) {
				WARN_ONCE(!latency,
					"tegra_isomgr_realize failed\n");
				return;
			}
		} else {
			pr_err("Failed to reserve bw %u.\n", bw);
#ifdef CONFIG_TEGRA_DC_EXTENSIONS
			tegra_dc_ext_process_bandwidth_renegotiate(
						NVDISP_BW_OUTPUT_ID, NULL);
#endif
		}

		ihub_bw_info.reserved_bw_kbps = bw;
	}

	if (use_new_bw || la_dirty)
		tegra_dc_set_latency_allowance(bw);
}

/*
 * tegra_dc_program_bandwidth
 *
 * If use_new = true, use ihub_bw_info.new_bw_kbps to program the
 * bw if dc->new_bw_pending is also set. Else, just assume the current
 * bw is fine.
 *
 * Calling this function both before and after a flip is sufficient to
 * select the best possible frequency and latency allowance.
 *
 * @dc		dc instance
 * @uew_new	forces ihub_bw_info.new_bw_kbps programming
 */
void tegra_dc_program_bandwidth(struct tegra_dc *dc, bool use_new)
{
	if (!dc->enabled)
		return;

	mutex_lock(&tegra_nvdisp_bw_lock);

	if (use_new && dc->new_bw_pending)
		clk_set_rate(hubclk, dc->imp_results[dc->ctrl_num].hubclk);

	tegra_nvdisp_program_bandwidth(ihub_bw_info.new_bw_kbps,
			dc->imp_results[dc->ctrl_num].total_latency,
			use_new && dc->new_bw_pending,
			dc->la_dirty);

	dc->la_dirty = false;
	if (use_new)
		dc->new_bw_pending = false;

	mutex_unlock(&tegra_nvdisp_bw_lock);
}

/*
 * tegra_dc_calc_min_bandwidth - returns the dedicated bw
 *
 * @dc		dc instance
 *
 * @retval	dedicated bw
 */
long tegra_dc_calc_min_bandwidth(struct tegra_dc *dc)
{
	return NVDISP_BW_DEDI_KBPS;
}

/*
 * tegra_dc_bandwidth_negotiate_bw - handles proposed bw
 *
 * @dc		dc instance
 * @windows	unused
 * @n		unused
 *
 * @retval	0 on sucess
 * @retval	-1 on failure to reserve or realize bw
 */
int tegra_dc_bandwidth_negotiate_bw(struct tegra_dc *dc,
				struct tegra_dc_win *windows[],
				int n)
{
	u32 bw;
	u32 latency;
	int ret = 0;

	/*
	 * Isomgr will update the available bw through a callback.
	 *
	 * There are four possible cases:
	 * A) If the available bw is less than the proposed bw, fail the ioctl.
	 * B) If the proposed bw is equal to the reserved bw, just return.
	 * C) If the proposed bw is less than the reserved bw, the bw will be
	 *    adjusted in the next flip. If necessary, the hubclk rate will be
	 *    decreased before the new bw takes effect.
	 * D) If the proposed bw is larger than the reserved bw, the bw will
	 *    take effect immediately. If necessary, the hubclk rate will be
	 *    increased after the new bw takes effect.
	 *
	 * dc->new_bw_pending will be set for C and D to stall other PROPOSE
	 * calls until the new bandwidth takes effect at the end of the next
	 * flip.
	 *
	 * We're also skipping the LA check here since IMP already validates the
	 * MC and LA values.
	 */
	mutex_lock(&tegra_nvdisp_bw_lock);

	if (IS_ERR_OR_NULL(ihub_bw_info.isomgr_handle)) {
		ret = -EINVAL;
		goto unlock_and_ret;
	}

	bw = dc->imp_results[dc->ctrl_num].required_total_bw_kbps;
	latency = dc->imp_results[dc->ctrl_num].total_latency;

	if (bw > ihub_bw_info.available_bw) { /* Case A */
		ret = -EINVAL;
		goto unlock_and_ret;
	} else if (bw == ihub_bw_info.reserved_bw_kbps) { /* Case B */
		goto unlock_and_ret;
	} else if (bw < ihub_bw_info.reserved_bw_kbps) { /* Case C */
		ihub_bw_info.new_bw_kbps = bw;
		dc->new_bw_pending = true;
		goto unlock_and_ret;
	}

	/* Case D */
	latency = tegra_isomgr_reserve(ihub_bw_info.isomgr_handle,
								bw, latency);
	if (latency) {
		latency = tegra_isomgr_realize(ihub_bw_info.isomgr_handle);
		if (!latency) {
			WARN_ONCE(!latency, "tegra_isomgr_realize failed\n");
			ret = -EINVAL;
			goto unlock_and_ret;
		}
	} else {
		dev_dbg(&dc->ndev->dev, "Failed to reserve proposed bw %u.\n",
									bw);
		ret = -EINVAL;
		goto unlock_and_ret;
	}

	clk_set_rate(hubclk, dc->imp_results[dc->ctrl_num].hubclk);

	/*
	 * Since we just reserved more bw, the LA settings need to be
	 * updated at the start of the next flip. Set dc->la_dirty
	 * accordingly.
	 */
	ihub_bw_info.reserved_bw_kbps = bw;
	ihub_bw_info.new_bw_kbps = bw;
	dc->new_bw_pending = true;
	dc->la_dirty = true;

unlock_and_ret:
	mutex_unlock(&tegra_nvdisp_bw_lock);
	return ret;
}
EXPORT_SYMBOL(tegra_dc_bandwidth_negotiate_bw);

static void tegra_nvdisp_bandwidth_renegotiate(void *p, u32 avail_bw)
{
	struct tegra_dc_bw_data data;
	struct nvdisp_isoclient_bw_info *bw_info = p;

	if (WARN_ONCE(!bw_info, "bw_info is NULL!"))
		return;

	mutex_lock(&tegra_nvdisp_bw_lock);
	if (bw_info->available_bw == avail_bw)
		goto unlock_and_exit;

	data.total_bw = tegra_isomgr_get_total_iso_bw();
	data.avail_bw = avail_bw;
	data.resvd_bw = bw_info->reserved_bw_kbps;

#ifdef CONFIG_TEGRA_DC_EXTENSIONS
	tegra_dc_ext_process_bandwidth_renegotiate(NVDISP_BW_OUTPUT_ID, &data);
#endif

	bw_info->available_bw = avail_bw;
unlock_and_exit:
	mutex_unlock(&tegra_nvdisp_bw_lock);
}

/*
 * tegra_nvdisp_isomgr_attach - save reference to ihub_bw_info
 *
 * @dc	dc instance
 */
void tegra_nvdisp_isomgr_attach(struct tegra_dc *dc)
{
	if (WARN_ONCE(!dc, "dc is NULL!"))
		return;

	dc->ihub_bw_info = &ihub_bw_info;

	/* Set defaults for total required bw, MC latency, and hubclk rate. */
	dc->imp_results[dc->ctrl_num].required_total_bw_kbps =
						NVDISP_BW_DEDI_KBPS;
	dc->imp_results[dc->ctrl_num].total_latency =
						NVDISP_BW_TOTAL_MC_LATENCY;
	dc->imp_results[dc->ctrl_num].hubclk =
						NVDISP_BW_REQUIRED_HUBCLK_HZ;
}

/*
 * tegra_nvdisp_isomgr_register - register the IHUB ISO BW client
 *
 * Expected to be called only once in the init path.
 *
 * @client	ISO client id
 * @udedi_bw	dedicated bw needed by client
 *
 * @retval	0 on success
 * @retval	-ENOENT if unable to register with isomgr
 */
int tegra_nvdisp_isomgr_register(enum tegra_iso_client client,
				u32 udedi_bw)
{
	mutex_lock(&tegra_nvdisp_bw_lock);

	ihub_bw_info.isomgr_handle = tegra_isomgr_register(client, udedi_bw,
					tegra_nvdisp_bandwidth_renegotiate,
					&ihub_bw_info);
	if (IS_ERR_OR_NULL(ihub_bw_info.isomgr_handle)) {
		mutex_unlock(&tegra_nvdisp_bw_lock);
		return -ENOENT;
	}

	/*
	 * Use the maximum value so we can try to reserve as much as
	 * needed until we are told by isomgr to backoff.
	 */
	ihub_bw_info.available_bw = UINT_MAX;

	/* Go ahead and reserve the dedicated bw for display. */
	ihub_bw_info.new_bw_kbps = udedi_bw;
	tegra_nvdisp_program_bandwidth(udedi_bw,
				NVDISP_BW_TOTAL_MC_LATENCY,
				true,
				true);

	mutex_unlock(&tegra_nvdisp_bw_lock);

	return 0;
}

/*
 * tegra_nvdisp_isomgr_unregister - unregister the IHUB ISO BW client
 */
void tegra_nvdisp_isomgr_unregister(void)
{
	mutex_lock(&tegra_nvdisp_bw_lock);

	if (!IS_ERR_OR_NULL(ihub_bw_info.isomgr_handle))
		tegra_isomgr_unregister(ihub_bw_info.isomgr_handle);
	memset(&ihub_bw_info, 0, sizeof(ihub_bw_info));

	mutex_unlock(&tegra_nvdisp_bw_lock);
}
#else
void tegra_dc_program_bandwidth(struct tegra_dc *dc, bool use_new) {};
long tegra_dc_calc_min_bandwidth(struct tegra_dc *dc)
{
	return -ENOSYS;
};
#endif /* CONFIG_TEGRA_ISOMGR */

/*
 * These functions are no longer supported since we don't know about per-head/
 * per-window bw requirements.
 */
void tegra_dc_clear_bandwidth(struct tegra_dc *dc) {};
unsigned long tegra_dc_get_bandwidth(struct tegra_dc_win *windows[], int n)
{
	return -ENOSYS;
};
EXPORT_SYMBOL(tegra_dc_get_bandwidth);
