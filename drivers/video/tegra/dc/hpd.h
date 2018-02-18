/*
 * drivers/video/tegra/dc/hpd.h
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION, All rights reserved.
 * Author: Animesh Kishore <ankishore@nvidia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_VIDEO_TEGRA_DC_HPD_H__
#define __DRIVERS_VIDEO_TEGRA_DC_HPD_H__

#include "edid.h"

struct tegra_hpd_ops {
	/* custom init state machine */
	void (*init)(void *drv_data);

	/* return if hpd asserted/de-asserted */
	bool (*get_hpd_state)(void *drv_data);

	/*
	 * panel is disconnected here.
	 * Disable display and notify others.
	 * Userspace has not yet been notified.
	 */
	void (*disable)(void *drv_data);

	/* prepare to initiate edid read tx */
	bool (*edid_read_prepare)(void *drv_data);

	/* edid is available but no notification sent yet. */
	void (*edid_ready)(void *drv_data);

	/*
	 * hpd dropped but came back again
	 * in < HPD_DROP_TIMEOUT_MS.
	 * Check for any edid change has been done.
	 */
	void (*edid_recheck)(void *drv_data);

	/* filter modes not supported by host */
	bool (*(*get_mode_filter)(void *drv_data))
	(const struct tegra_dc *dc, struct fb_videomode *mode);

	/* core func to read edid over host-sink connection */
	i2c_transfer_func_t (*edid_read)(void *drv_data);

	/*
	 * edid is available. Notification sent.
	 * Add more notification here.
	 */
	void (*edid_notify)(void *drv_data);
};

struct tegra_hpd_data {
	struct delayed_work dwork;
	int shutdown;
	int state;
	int pending_hpd_evt;
	void *drv_data;
	struct tegra_hpd_ops *ops;

	struct fb_monspecs mon_spec;

	struct tegra_edid *edid;
	int edid_reads;

	struct tegra_edid_hdmi_eld eld;
	bool eld_retrieved;

	struct rt_mutex lock;

#ifdef CONFIG_SWITCH
	const char *hpd_switch_name;
	struct switch_dev hpd_switch;
#endif

	struct tegra_dc *dc;
};

enum {
	/*
	 * The initial state for the state machine.  When entering RESET, we
	 * shut down all output and then proceed to the CHECK_PLUG state after a
	 * short debounce delay.
	 */
	STATE_HPD_RESET = 0,

	/*
	 * After the debounce delay, check the status of the HPD line.  If its
	 * low, then the cable is unplugged and we go directly to DONE_DISABLED.
	 * If it is high, then the cable is plugged and we proceed to CHECK_EDID
	 * in order to read the EDID and figure out the next step.
	 */
	STATE_PLUG,

	/*
	 * CHECK_EDID is the state we stay in attempting to read the EDID
	 * information after we check the plug state and discover that we are
	 * plugged in.  If we max out our retries and fail to read the EDID, we
	 * move to DONE_DISABLED.  If we successfully read the EDID, we move on
	 * to DONE_ENABLE, set an initial video mode, then signal to the high
	 * level that we are ready for final mode selection.
	 */
	STATE_CHECK_EDID,

	/*
	 * DONE_DISABLED is the state we stay in after being reset and either
	 * discovering that no cable is plugged in or after we think a cable is
	 * plugged in but fail to read EDID.
	 */
	STATE_DONE_DISABLED,

	/*
	 * DONE_ENABLED is the state we say in after being reset and disovering
	 * a valid EDID at the other end of a plugged cable.
	 */
	STATE_DONE_ENABLED,

	/*
	 * Some sinks will drop HPD as soon as the TMDS signals start up.  They
	 * will hold HPD low for about second and then re-assert it.  If the
	 * source simply holds steady and does not disable the TMDS lines, the
	 * sink seems to accept the video mode after having gone out to lunch
	 * for a bit.  This seems to be the behavior of various sources which
	 * work with panels like this, so it is the behavior we emulate here.
	 * If HPD drops while we are in DONE_ENABLED, set a timer for 1.5
	 * seconds and transition to WAIT_FOR_HPD_REASSERT.  If HPD has not come
	 * back within this time limit, then go ahead and transition to RESET
	 * and shut the system down.  If HPD does come back within this time
	 * limit, then check the EDID again.  If it has not changed, then we
	 * assume that we are still hooked to the same panel and just go back to
	 * DONE_ENABLED.  If the EDID fails to read or has changed, we
	 * transition to RESET and start the system over again.
	 */
	STATE_WAIT_FOR_HPD_REASSERT,

	/*
	 * RECHECK_EDID is the state we stay in while attempting to re-read the
	 * EDID following an HPD drop and re-assert which occurs while we are in
	 * the DONE_ENABLED state.  see HPD_STATE_DONE_WAIT_FOR_HPD_REASSERT
	 * for more details.
	 */
	STATE_RECHECK_EDID,

	/*
	 * Initial state at boot that checks if HPD is already initialized
	 * by bootloader and not go to HPD_STATE_RESET which would disable
	 * HPD and cause blanking of the bootloader displayed image.
	 */
	STATE_INIT_FROM_BOOTLOADER,

	/*
	 * STATE_COUNT must be the final state in the enum.
	 * 1) Do not add states after STATE_COUNT.
	 * 2) Do not assign explicit values to the states.
	 * 3) Do not reorder states in the list without reordering the dispatch
	 *    table in hpd.c
	 */
	HPD_STATE_COUNT,
};

void tegra_hpd_init(struct tegra_hpd_data *data, struct tegra_dc *dc,
			void *drv_data, struct tegra_hpd_ops *ops);
void tegra_hpd_shutdown(struct tegra_hpd_data *data);
void tegra_hpd_set_pending_evt(struct tegra_hpd_data *data);
int tegra_hpd_get_state(struct tegra_hpd_data *data);

#endif
