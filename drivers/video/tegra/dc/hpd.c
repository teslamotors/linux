/*
 * drivers/video/tegra/dc/hpd.c
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

#include <linux/kernel.h>
#include <mach/dc.h>
#include <mach/fb.h>
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif
#include <video/tegrafb.h>
#include "dc_priv.h"

#ifdef CONFIG_ADF_TEGRA
#include "tegra_adf.h"
#endif

#include "hpd.h"

#define MAX_EDID_READ_ATTEMPTS 5
#define HPD_EDID_MAX_LENGTH 512

/*
 * how long of an HPD drop before we consider it gone for good.
 * this is mostly a preference to work around monitors users
 * reported that occasionally drop HPD.
 */
#define HPD_STABILIZE_MS 40
#define HPD_DROP_TIMEOUT_MS 1500
#define CHECK_PLUG_STATE_DELAY_MS 10
#define CHECK_EDID_DELAY_MS 60

static const char * const state_names[] = {
	"Reset",
	"Check Plug",
	"Check EDID",
	"Disabled",
	"Enabled",
	"Wait for HPD reassert",
	"Recheck EDID",
	"Takeover from bootloader",
};

static void set_hpd_state(struct tegra_hpd_data *data,
			int target_state, int resched_time);

static void hpd_disable(struct tegra_hpd_data *data)
{
#ifdef CONFIG_SWITCH
	if (data->hpd_switch.name) {
		switch_set_state(&data->hpd_switch, 0);
		pr_info("hpd: hpd_switch 0\n");
	}
#endif
	if (data->dc->connected) {
		pr_info("hpd: DC from connected to disconnected\n");
		data->dc->connected = false;
		tegra_dc_disable(data->dc);
	}

#ifdef CONFIG_ADF_TEGRA
	if (data->dc->adf)
		tegra_adf_process_hotplug_disconnected(data->dc->adf);
#else
	if (data->dc->fb)
		tegra_fb_update_monspecs(data->dc->fb, NULL, NULL);
#endif

	if (data->ops->disable)
		data->ops->disable(data->drv_data);

#ifdef CONFIG_TEGRA_DC_EXTENSIONS
	tegra_dc_ext_process_hotplug(data->dc->ndev->id);
#endif
}

static const char *get_hpd_switch_name(struct tegra_hpd_data *data)
{
	const char *name = NULL;

	if (data->hpd_switch_name)
		name = data->hpd_switch_name;

	return name;
}

/* returns bytes read, or negative error */
static int read_edid_into_buffer(struct tegra_hpd_data *data,
				 u8 *edid_data, size_t edid_data_len)
{
#define EXT_BLOCK_COUNT_OFFSET 0x7e

	int err, i;
	int extension_blocks;
	int max_ext_blocks = (edid_data_len / 128) - 1;

	err = tegra_edid_read_block(data->edid, 0, edid_data);
	if (err) {
		pr_err("hpd: tegra_edid_read_block(0) returned err %d\n", err);
		return err;
	}

	extension_blocks = edid_data[EXT_BLOCK_COUNT_OFFSET];
	pr_info("hpd: extension_blocks = %d, max_ext_blocks = %d\n",
		extension_blocks, max_ext_blocks);
	if (extension_blocks > max_ext_blocks)
		extension_blocks = max_ext_blocks;
	for (i = 1; i <= extension_blocks; i++) {
		err = tegra_edid_read_block(data->edid, i,
					edid_data + i * 128);
		if (err) {
			pr_err(
			"hpd: tegra_edid_read_block(%d) returned err %d\n",
			i, err);
			return err;
		}
	}
	return i * 128;

#undef EXT_BLOCK_COUNT_OFFSET
}

/*
 * re-read the edid and check to see if it has changed. Return 0 on a
 * successful E-EDID read, or non-zero error code on failure. If we succeed,
 * set match to 1 if the old E-EDID matches the new E-EDID. Otherwise, set
 * match to 0.
 */
static int recheck_edid(struct tegra_hpd_data *data, int *match)
{
	int ret;
	u8 tmp[HPD_EDID_MAX_LENGTH] = {0};

	ret = read_edid_into_buffer(data, tmp, sizeof(tmp));
	pr_info("hpd: read_edid_into_buffer() returned %d\n", ret);
	if (ret > 0) {
		struct tegra_dc_edid *dc_edid = tegra_edid_get_data(data->edid);

		pr_info("hpd: old edid len = %ld\n", (long int)dc_edid->len);
		*match = !!((ret == dc_edid->len) &&
			  !memcmp(tmp, dc_edid->buf, dc_edid->len));
		if (*match == 0) {
			print_hex_dump(KERN_INFO, "tmp :", DUMP_PREFIX_ADDRESS,
				       16, 4, tmp, ret, true);
			print_hex_dump(KERN_INFO, "data:", DUMP_PREFIX_ADDRESS,
				       16, 4, dc_edid->buf, dc_edid->len, true);
		}
		tegra_edid_put_data(dc_edid);
		ret = 0;
	}

	return ret;
}

static void edid_read_notify(struct tegra_hpd_data *data)
{
#ifdef CONFIG_ADF_TEGRA
	tegra_adf_process_hotplug_connected(data->dc->adf, &data->mon_spec);
#endif
#ifdef CONFIG_TEGRA_DC_EXTENSIONS
	tegra_fb_update_monspecs(data->dc->fb, &data->mon_spec,
				(data->ops->get_mode_filter) ?
				(data->ops->get_mode_filter(data->drv_data)) :
				NULL);
	tegra_fb_update_fix(data->dc->fb, &data->mon_spec);
#endif
#ifdef CONFIG_SWITCH
	if (data->hpd_switch.name) {
		switch_set_state(&data->hpd_switch, 1);
		pr_info("hpd: Display connected, hpd_switch 1\n");
	}
#endif
	data->dc->connected = true;

#ifdef CONFIG_TEGRA_DC_EXTENSIONS
	tegra_dc_ext_process_hotplug(data->dc->ndev->id);
#endif

	if (data->ops->edid_notify)
		data->ops->edid_notify(data->drv_data);
}

static void hpd_reset_state(struct tegra_hpd_data *data)
{
	/*
	 * Shut everything down, then schedule a
	 * check of the plug state in the near future.
	 */
	hpd_disable(data);
	set_hpd_state(data, STATE_PLUG,
			CHECK_PLUG_STATE_DELAY_MS);
}

static void hpd_plug_state(struct tegra_hpd_data *data)
{
	if (data->ops->get_hpd_state(data->drv_data)) {
		/*
		 * Looks like there is something plugged in.
		 * Get ready to read the sink's EDID information.
		 */
		data->edid_reads = 0;

		set_hpd_state(data, STATE_CHECK_EDID,
				CHECK_EDID_DELAY_MS);
	} else {
		/*
		 * Nothing plugged in, so we are finished. Go to the
		 * DONE_DISABLED state and stay there until the next HPD event.
		 */
		hpd_disable(data);
		set_hpd_state(data, STATE_DONE_DISABLED, -1);
	}
}

static void edid_check_state(struct tegra_hpd_data *data)
{
	memset(&data->mon_spec, 0, sizeof(data->mon_spec));

	if (fb_console_mapped()) {
		/* Set default videomode on dc before enabling it */
		tegra_dc_set_default_videomode(data->dc);
	}

	if (!data->ops->get_hpd_state(data->drv_data)) {
		/* hpd dropped - stop EDID read */
		pr_info("hpd: dropped, abort EDID read\n");
		goto end_disabled;
	}

	if (data->ops->edid_read_prepare)
		data->ops->edid_read_prepare(data->drv_data);

	if (tegra_edid_get_monspecs(data->edid, &data->mon_spec)) {
		/*
		 * Failed to read EDID. If we still have retry attempts left,
		 * schedule another attempt. Otherwise give up and just go to
		 * the disabled state.
		 */
		data->edid_reads++;
		if (data->edid_reads >= MAX_EDID_READ_ATTEMPTS) {
			pr_info("hpd: EDID read failed %d times. Giving up.\n",
				data->edid_reads);
			goto end_disabled;
		} else {
			set_hpd_state(data, STATE_CHECK_EDID,
					CHECK_EDID_DELAY_MS);
		}

		return;
	}

	if (tegra_edid_get_eld(data->edid, &data->eld) < 0) {
		pr_err("hpd: error populating eld\n");
		goto end_disabled;
	}
	data->eld_retrieved = true;

	if (data->ops->edid_ready)
		data->ops->edid_ready(data->drv_data);

	edid_read_notify(data);

	set_hpd_state(data, STATE_DONE_ENABLED, -1);

	return;
end_disabled:
	data->eld_retrieved = false;
	hpd_disable(data);
	set_hpd_state(data, STATE_DONE_DISABLED, -1);
}

static void wait_for_hpd_reassert_state(struct tegra_hpd_data *data)
{
	/*
	 * Looks like HPD dropped and really did stay low.
	 * Go ahead and reset the system.
	 */
	set_hpd_state(data, STATE_HPD_RESET, 0);
}

static void edid_recheck_state(struct tegra_hpd_data *data)
{
	int match, tgt_state, timeout;

	tgt_state = STATE_HPD_RESET;
	timeout = 0;

	if (recheck_edid(data, &match)) {
		/*
		 * Failed to read EDID. If we still have retry attempts left,
		 * schedule another attempt. Otherwise give up and reset;
		 */
		data->edid_reads++;
		if (data->edid_reads >= MAX_EDID_READ_ATTEMPTS) {
			pr_info("hpd: EDID retry %d times. Giving up.\n",
				data->edid_reads);
		} else {
			tgt_state = STATE_RECHECK_EDID;
			timeout = CHECK_EDID_DELAY_MS;
		}
	} else {
		/*
		 * Successful read! If the EDID is unchanged, just go back to
		 * the DONE_ENABLED state and do nothing. If something changed,
		 * just reset the whole system.
		 */
		if (match) {
			pr_info("hpd: No EDID change, taking no action.\n");
			tgt_state = STATE_DONE_ENABLED;
			timeout = -1;
		} else {
			pr_info("hpd: EDID change, reset hpd state machine\n");
		}
	}

	set_hpd_state(data, tgt_state, timeout);

	/*
	 * This callback should always proceed next hpd
	 * state set. Thereby, callers can query regarding
	 * next state.
	 */
	if (data->ops->edid_recheck)
		data->ops->edid_recheck(data->drv_data);
}

typedef void (*dispatch_func_t)(struct tegra_hpd_data *data);
static const dispatch_func_t state_machine_dispatch[] = {
	hpd_reset_state,		/* STATE_HPD_RESET */
	hpd_plug_state,			/* STATE_PLUG */
	edid_check_state,		/* STATE_CHECK_EDID */
	NULL,				/* STATE_DONE_DISABLED */
	NULL,				/* STATE_DONE_ENABLED */
	wait_for_hpd_reassert_state,	/* STATE_WAIT_FOR_HPD_REASSERT */
	edid_recheck_state,		/* STATE_RECHECK_EDID */
	NULL,				/* STATE_INIT_FROM_BOOTLOADER */
};

static void handle_hpd_evt(struct tegra_hpd_data *data, int cur_hpd)
{
	int tgt_state;
	int timeout = 0;

	if ((STATE_DONE_ENABLED == data->state) && !cur_hpd) {
		/*
		 * Did HPD drop while we were in DONE_ENABLED ? If so, hold
		 * steady and wait to see if it comes back.
		 */
		tgt_state = STATE_WAIT_FOR_HPD_REASSERT;
		timeout = HPD_DROP_TIMEOUT_MS;
	} else if (STATE_WAIT_FOR_HPD_REASSERT == data->state &&
		cur_hpd) {
		/*
		 * Looks like HPD dropped and eventually came back.  Re-read the
		 * EDID and reset the system only if the EDID has changed.
		 */
		data->edid_reads = 0;
		tgt_state = STATE_RECHECK_EDID;
		timeout = CHECK_EDID_DELAY_MS;
	} else if (STATE_DONE_ENABLED == data->state && cur_hpd) {
		if (!tegra_dc_ext_is_userspace_active()) {
			/* No userspace running. Enable DC with cached mode */
			pr_info("hpd: No EDID change. No userspace active. "
			"Using cached mode to initialize dc!\n");
			data->dc->use_cached_mode = true;
			tgt_state = STATE_CHECK_EDID;
		} else  {
			/*
			 * Looks like HPD dropped but came back quickly,
			 * ignore it.
			 */
			pr_info("hpd: ignoring bouncing hpd\n");
			return;
		}
	} else if (STATE_INIT_FROM_BOOTLOADER == data->state && cur_hpd) {
		/*
		 * We follow the same protocol as STATE_HPD_RESET in the
		 * last branch here, but avoid actually entering that state so
		 * we do not actively disable HPD.  Worker will check HPD
		 * level again when it's woke up after 40ms.
		 */
		tgt_state = STATE_PLUG;
		timeout = HPD_STABILIZE_MS;
	} else {
		/*
		 * Looks like there was HPD activity while we were neither
		 * waiting for it to go away during steady state output, nor
		 * looking for it to come back after such an event.  Wait until
		 * HPD has been steady for at least 40 mSec, then restart the
		 * state machine.
		 */
		tgt_state = STATE_HPD_RESET;
		timeout = HPD_STABILIZE_MS;
	}

	set_hpd_state(data, tgt_state, timeout);
}

static void hpd_worker(struct work_struct *work)
{
	int pending_hpd_evt, cur_hpd;
	struct tegra_hpd_data *data = container_of(
					to_delayed_work(work),
					struct tegra_hpd_data, dwork);

	/*
	 * Observe and clear pending flag
	 * and latch the current HPD state.
	 */
	rt_mutex_lock(&data->lock);
	pending_hpd_evt = data->pending_hpd_evt;
	data->pending_hpd_evt = 0;
	rt_mutex_unlock(&data->lock);
	cur_hpd = data->ops->get_hpd_state(data->drv_data);

	pr_info("hpd: state %d (%s), hpd %d, pending_hpd_evt %d\n",
		data->state, state_names[data->state],
		cur_hpd, pending_hpd_evt);

	if (pending_hpd_evt) {
		/*
		 * If we were woken up because of HPD activity, just schedule
		 * the next appropriate task and get out.
		 */
		handle_hpd_evt(data, cur_hpd);
	} else if (data->state < ARRAY_SIZE(state_machine_dispatch)) {
		dispatch_func_t func = state_machine_dispatch[data->state];

		if (NULL == func)
			pr_warn("hpd: NULL state handler in state %d\n",
				data->state);
		else
			func(data);
	} else {
		pr_warn("hpd: unexpected state scheduled %d",
			data->state);
	}
}

static void sched_hpd_work(struct tegra_hpd_data *data, int resched_time)
{
	cancel_delayed_work(&data->dwork);

	/*
	 * system_nrt_wq is non-reentrant
	 * and guarantees that any given work is
	 * never executed parallelly by multiple CPUs
	 */
	if ((resched_time >= 0) && !data->shutdown)
		queue_delayed_work(system_wq,
				&data->dwork,
				msecs_to_jiffies(resched_time));
}

static void set_hpd_state(struct tegra_hpd_data *data,
			int target_state, int resched_time)
{
	rt_mutex_lock(&data->lock);

	pr_info("hpd: switching from state %d (%s) to state %d (%s)\n",
		data->state, state_names[data->state],
		target_state, state_names[target_state]);
	data->state = target_state;

	/*
	 * If the pending_hpd_evt flag is already set, don't bother to
	 * reschedule the state machine worker.  We should be able to assert
	 * that there is a worker callback already scheduled, and that it is
	 * scheduled to run immediately.  This is particularly important when
	 * making the transition to the steady state ENABLED or DISABLED states.
	 * If an HPD event occurs while the worker is in flight, after the
	 * worker checks the state of the pending HPD flag, and then the state
	 * machine transitions to ENABLE or DISABLED, the system would end up
	 * canceling the callback to handle the HPD event were it not for this
	 * check.
	 */
	if (!data->pending_hpd_evt)
		sched_hpd_work(data, resched_time);

	rt_mutex_unlock(&data->lock);
}

void tegra_hpd_shutdown(struct tegra_hpd_data *data)
{
	data->shutdown = 1;
	cancel_delayed_work_sync(&data->dwork);
	tegra_edid_destroy(data->edid);

#ifdef CONFIG_SWITCH
	data->hpd_switch.name = get_hpd_switch_name(data);

	if (data->hpd_switch.name) {
		switch_dev_unregister(&data->hpd_switch);
	}
#endif
}

int tegra_hpd_get_state(struct tegra_hpd_data *data)
{
	int ret;

	rt_mutex_lock(&data->lock);
	ret = data->state;
	rt_mutex_unlock(&data->lock);

	return ret;
}

void tegra_hpd_set_pending_evt(struct tegra_hpd_data *data)
{
	rt_mutex_lock(&data->lock);

	/* We always schedule work any time there is a pending HPD event */
	data->pending_hpd_evt = 1;
	sched_hpd_work(data, 0);

	rt_mutex_unlock(&data->lock);
}

void tegra_hpd_init(struct tegra_hpd_data *data,
			struct tegra_dc *dc,
			void *drv_data,
			struct tegra_hpd_ops *ops)
{
	int err;

	BUG_ON(!dc || !data || !ops ||
		!ops->get_hpd_state ||
		!ops->edid_read);

	if (ops->init)
		ops->init(drv_data);

	data->drv_data = drv_data;
	data->state = STATE_INIT_FROM_BOOTLOADER;
	data->pending_hpd_evt = 0;
	data->shutdown = 0;
	data->ops = ops;
	data->dc = dc;

	data->edid = tegra_edid_create(dc,
			data->ops->edid_read(data->drv_data));
	if (IS_ERR_OR_NULL(data->edid)) {
		pr_err("hpd: edid create failed\n");
		return;
	}
	tegra_dc_set_edid(dc, data->edid);
	data->eld_retrieved = false;
	data->edid_reads = 0;

	memset(&data->mon_spec, 0, sizeof(data->mon_spec));

#ifdef CONFIG_SWITCH
	data->hpd_switch.name = get_hpd_switch_name(data);

	if (data->hpd_switch.name) {
		err = switch_dev_register(&data->hpd_switch);
		BUG_ON(err);
	}
#endif

	rt_mutex_init(&data->lock);

	INIT_DELAYED_WORK(&data->dwork, hpd_worker);
}
