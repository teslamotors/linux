/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>

#include "gk20a/gk20a.h"
#include "gm206/bios_gm206.h"
#include "gp106/xve_gp106.h"

#include "gp106/hw_xp_gp106.h"
#include "gp106/hw_xve_gp106.h"

/**
 * Init a timer and place the timeout data in @timeout.
 */
static void init_timeout(u32 timeout_ms, u32 *timeout)
{
	*timeout = jiffies + msecs_to_jiffies(timeout_ms);
}

/**
 * Returns 1 if the current time is after @timeout i.e: the timer timed
 * out. Returns 0 if the timer still has time left.
 */
static int check_timeout(u32 *timeout)
{
	unsigned long now = jiffies;
	unsigned long timeout_l = (unsigned long)*timeout;

	if (time_after(now, timeout_l))
		return 1;

	return 0;
}

static void xve_xve_writel_gp106(struct gk20a *g, u32 reg, u32 val)
{
	gk20a_writel(g, NV_PCFG + reg, val);
}

static u32 xve_xve_readl_gp106(struct gk20a *g, u32 reg)
{
	return gk20a_readl(g, NV_PCFG + reg);
}

/**
 * Resets the GPU (except the XVE/XP).
 */
static void xve_reset_gpu_gp106(struct gk20a *g)
{
	u32 reset;

	/*
	 * This resets the GPU except for the XVE/XP (since then we would lose
	 * the dGPU from the bus). t18x has a HW limitation where once that
	 * happens the GPU is gone until the entire system is reset.
	 *
	 * We have to use the auto-deassert register since we won't be able to
	 * access the GPU after the GPU goes into reset. This appears like the
	 * GPU has dropped from the bus and causes nvgpu to reset the entire
	 * system. Whoops!
	 */
	reset = xve_reset_reset_m() |
		xve_reset_gpu_on_sw_reset_m() |
		xve_reset_counter_en_m() |
		xve_reset_counter_val_f(0x7ff) |
		xve_reset_clock_on_sw_reset_m() |
		xve_reset_clock_counter_en_m() |
		xve_reset_clock_counter_val_f(0x7ff);

	g->ops.xve.xve_writel(g, xve_reset_r(), reset | xve_reset_reset_m());

	/*
	 * Don't access GPU until _after_ it's back out of reset!
	 */
	msleep(100);
	g->ops.xve.xve_writel(g, xve_reset_r(), 0);
}

/**
 * Places one of:
 *
 *   %GPU_XVE_SPEED_2P5
 *   %GPU_XVE_SPEED_5P0
 *   %GPU_XVE_SPEED_8P0
 *
 * in the u32 pointed to by @xve_link_speed. If for some reason an unknown PCIe
 * bus speed is detected then *@xve_link_speed is not touched and -ENODEV is
 * returned.
 */
static int xve_get_speed_gp106(struct gk20a *g, u32 *xve_link_speed)
{
	u32 status;
	u32 link_speed, real_link_speed = 0;

	status = g->ops.xve.xve_readl(g, xve_link_control_status_r());

	link_speed = xve_link_control_status_link_speed_v(status);

	/*
	 * Can't use a switch statement becuase switch statements dont work with
	 * function calls.
	 */
	if (link_speed == xve_link_control_status_link_speed_link_speed_2p5_v())
		real_link_speed = GPU_XVE_SPEED_2P5;
	if (link_speed == xve_link_control_status_link_speed_link_speed_5p0_v())
		real_link_speed = GPU_XVE_SPEED_5P0;
	if (link_speed == xve_link_control_status_link_speed_link_speed_8p0_v())
		real_link_speed = GPU_XVE_SPEED_8P0;

	if (!real_link_speed) {
		pr_warn("%s: Unknown PCIe bus speed!\n", __func__);
		return -ENODEV;
	}

	*xve_link_speed = real_link_speed;
	return 0;
}

/**
 * Set the mask for L0s in the XVE.
 *
 * When @status is non-zero the mask for L0s is set which _disables_ L0s. When
 * @status is zero L0s is no longer masked and may be enabled.
 */
static void set_xve_l0s_mask(struct gk20a *g, bool status)
{
	u32 xve_priv;
	u32 status_bit = status ? 1 : 0;

	xve_priv = g->ops.xve.xve_readl(g, xve_priv_xv_r());

	xve_priv = set_field(xve_priv,
		  xve_priv_xv_cya_l0s_enable_m(),
		  xve_priv_xv_cya_l0s_enable_f(status_bit));

	g->ops.xve.xve_writel(g, xve_priv_xv_r(), xve_priv);
}

/**
 * Set the mask for L1 in the XVE.
 *
 * When @status is non-zero the mask for L1 is set which _disables_ L1ss. When
 * @status is zero L1 is no longer masked and may be enabled.
 */
static void set_xve_l1_mask(struct gk20a *g, int status)
{
	u32 xve_priv;
	u32 status_bit = status ? 1 : 0;

	xve_priv = g->ops.xve.xve_readl(g, xve_priv_xv_r());

	xve_priv = set_field(xve_priv,
		  xve_priv_xv_cya_l1_enable_m(),
		  xve_priv_xv_cya_l1_enable_f(status_bit));

	g->ops.xve.xve_writel(g, xve_priv_xv_r(), xve_priv);
}

/**
 * When doing the speed change disable power saving features.
 */
static void xve_disable_aspm_gp106(struct gk20a *g)
{
	mutex_lock(&g->xve_lock);
	if (g->xve_aspm_disable_count == 0) {
		set_xve_l0s_mask(g, true);
		set_xve_l1_mask(g, true);
	}
	g->xve_aspm_disable_count++;

	mutex_unlock(&g->xve_lock);
}

/**
 * Restore the state saved by disable_aspm_gp106().
 */
static void xve_enable_aspm_gp106(struct gk20a *g)
{
	mutex_lock(&g->xve_lock);

	if (g->xve_aspm_disable_count > 0)
		--g->xve_aspm_disable_count;

	if (g->xve_aspm_disable_count == 0) {
		set_xve_l0s_mask(g, g->xve_l0s);
		set_xve_l1_mask(g, g->xve_l1);
	}

	mutex_unlock(&g->xve_lock);
}

/*
 * Error checking is done in xve_set_speed_gp106.
 */
static int __do_xve_set_speed_gp106(struct gk20a *g, u32 next_link_speed)
{
	u32 current_link_speed, new_link_speed;
	u32 dl_mgr, saved_dl_mgr;
	u32 pl_link_config;
	u32 link_control_status, link_speed_setting, link_width;
	u32 timeout;
	int attempts = 10, err_status = 0;

	g->ops.xve.get_speed(g, &current_link_speed);
	xv_sc_dbg(PRE_CHANGE, "Executing PCIe link change.");
	xv_sc_dbg(PRE_CHANGE, "  Current speed:  %s",
		  xve_speed_to_str(current_link_speed));
	xv_sc_dbg(PRE_CHANGE, "  Next speed:     %s",
		  xve_speed_to_str(next_link_speed));
	xv_sc_dbg(PRE_CHANGE, "  PL_LINK_CONFIG: 0x%08x",
		  gk20a_readl(g, xp_pl_link_config_r(0)));

	xv_sc_dbg(DISABLE_ASPM, "Disabling ASPM...");
	xve_disable_aspm_gp106(g);
	xv_sc_dbg(DISABLE_ASPM, "  Done!");

	xv_sc_dbg(DL_SAFE_MODE, "Putting DL in safe mode...");
	saved_dl_mgr = gk20a_readl(g, xp_dl_mgr_r(0));

	/*
	 * Put the DL in safe mode.
	 */
	dl_mgr = saved_dl_mgr;
	dl_mgr |= xp_dl_mgr_safe_timing_f(1);
	gk20a_writel(g, xp_dl_mgr_r(0), dl_mgr);
	xv_sc_dbg(DL_SAFE_MODE, "  Done!");

	init_timeout(GPU_XVE_TIMEOUT_MS, &timeout);

	xv_sc_dbg(CHECK_LINK, "Checking for link idle...");
	while (1) {
		pl_link_config = gk20a_readl(g, xp_pl_link_config_r(0));
		if ((xp_pl_link_config_ltssm_status_f(pl_link_config) ==
		     xp_pl_link_config_ltssm_status_idle_v()) &&
		    (xp_pl_link_config_ltssm_directive_f(pl_link_config) ==
		     xp_pl_link_config_ltssm_directive_normal_operations_v()))
			break;

		if (check_timeout(&timeout)) {
			err_status = -ETIMEDOUT;
			break;
		}
	}

	if (err_status == -ETIMEDOUT)
		/* TODO: debug message. */
		goto done;

	xv_sc_dbg(CHECK_LINK, "  Done");

	xv_sc_dbg(LINK_SETTINGS, "Preparing next link settings");
	pl_link_config &= ~xp_pl_link_config_max_link_rate_m();
	switch (next_link_speed) {
	case GPU_XVE_SPEED_2P5:
		link_speed_setting =
			xve_link_control_status_link_speed_link_speed_2p5_v();
		pl_link_config |= xp_pl_link_config_max_link_rate_f(
			xp_pl_link_config_max_link_rate_2500_mtps_v());
		break;
	case GPU_XVE_SPEED_5P0:
		link_speed_setting =
			xve_link_control_status_link_speed_link_speed_5p0_v();
		pl_link_config |= xp_pl_link_config_max_link_rate_f(
			xp_pl_link_config_max_link_rate_5000_mtps_v());
		break;
	case GPU_XVE_SPEED_8P0:
		link_speed_setting =
			xve_link_control_status_link_speed_link_speed_8p0_v();
		pl_link_config |= xp_pl_link_config_max_link_rate_f(
			xp_pl_link_config_max_link_rate_8000_mtps_v());
		break;
	default:
		BUG(); /* Should never be hit. */
	}

	link_control_status =
		g->ops.xve.xve_readl(g, xve_link_control_status_r());
	link_width = xve_link_control_status_link_width_v(link_control_status);

	pl_link_config &= ~xp_pl_link_config_target_tx_width_m();

	/* Can't use a switch due to oddities in register definitions. */
	if (link_width == xve_link_control_status_link_width_x1_v())
		pl_link_config |= xp_pl_link_config_target_tx_width_f(
			xp_pl_link_config_target_tx_width_x1_v());
	else if (link_width == xve_link_control_status_link_width_x2_v())
		pl_link_config |= xp_pl_link_config_target_tx_width_f(
			xp_pl_link_config_target_tx_width_x2_v());
	else if (link_width == xve_link_control_status_link_width_x4_v())
		pl_link_config |= xp_pl_link_config_target_tx_width_f(
			xp_pl_link_config_target_tx_width_x4_v());
	else if (link_width == xve_link_control_status_link_width_x8_v())
		pl_link_config |= xp_pl_link_config_target_tx_width_f(
			xp_pl_link_config_target_tx_width_x8_v());
	else if (link_width == xve_link_control_status_link_width_x16_v())
		pl_link_config |= xp_pl_link_config_target_tx_width_f(
			xp_pl_link_config_target_tx_width_x16_v());
	else
		BUG();

	xv_sc_dbg(LINK_SETTINGS, "  pl_link_config = 0x%08x", pl_link_config);
	xv_sc_dbg(LINK_SETTINGS, "  Done");

	xv_sc_dbg(EXEC_CHANGE, "Running link speed change...");

	init_timeout(GPU_XVE_TIMEOUT_MS, &timeout);
	while (1) {
		gk20a_writel(g, xp_pl_link_config_r(0), pl_link_config);
		if (pl_link_config ==
		    gk20a_readl(g, xp_pl_link_config_r(0)))
			break;

		if (check_timeout(&timeout)) {
			err_status = -ETIMEDOUT;
			break;
		}
	}

	if (err_status == -ETIMEDOUT)
		goto done;

	xv_sc_dbg(EXEC_CHANGE, "  Wrote PL_LINK_CONFIG.");

	pl_link_config = gk20a_readl(g, xp_pl_link_config_r(0));

	do {
		pl_link_config = set_field(pl_link_config,
			  xp_pl_link_config_ltssm_directive_m(),
			  xp_pl_link_config_ltssm_directive_f(
			  xp_pl_link_config_ltssm_directive_change_speed_v()));

		xv_sc_dbg(EXEC_CHANGE, "  Executing change (0x%08x)!",
			  pl_link_config);
		gk20a_writel(g, xp_pl_link_config_r(0), pl_link_config);

		/*
		 * Read NV_XP_PL_LINK_CONFIG until the link has swapped to
		 * the target speed.
		 */
		init_timeout(GPU_XVE_TIMEOUT_MS, &timeout);
		while (1) {
			pl_link_config = gk20a_readl(g, xp_pl_link_config_r(0));
			if (pl_link_config != 0xfffffff &&
			    (xp_pl_link_config_ltssm_status_f(pl_link_config) ==
			     xp_pl_link_config_ltssm_status_idle_v()) &&
			    (xp_pl_link_config_ltssm_directive_f(pl_link_config) ==
			     xp_pl_link_config_ltssm_directive_normal_operations_v()))
				break;

			if (check_timeout(&timeout)) {
				err_status = -ETIMEDOUT;
				xv_sc_dbg(EXEC_CHANGE, "  timeout; pl_link_config = 0x%x",
					pl_link_config);
				break;
			}
		}

		xv_sc_dbg(EXEC_CHANGE, "  Change done... Checking status");

		if (pl_link_config == 0xffffffff) {
			WARN(1, "GPU fell of PCI bus!?");

			/*
			 * The rest of the driver is probably about to
			 * explode...
			 */
			BUG();
		}

		link_control_status =
			g->ops.xve.xve_readl(g, xve_link_control_status_r());
		xv_sc_dbg(EXEC_CHANGE, "  target %d vs current %d",
			  link_speed_setting,
			  xve_link_control_status_link_speed_v(link_control_status));

		if (err_status == -ETIMEDOUT)
			xv_sc_dbg(EXEC_CHANGE, "  Oops timed out?");
	} while (attempts-- > 0 &&
		 link_speed_setting !=
		 xve_link_control_status_link_speed_v(link_control_status));

	xv_sc_dbg(EXEC_VERIF, "Verifying speed change...");

	/*
	 * Check that the new link speed is actually active. If we failed to
	 * change to the new link speed then return to the link speed setting
	 * pre-speed change.
	 */
	new_link_speed = xve_link_control_status_link_speed_v(
		link_control_status);
	if (link_speed_setting != new_link_speed) {
		u32 link_config = gk20a_readl(g, xp_pl_link_config_r(0));

		xv_sc_dbg(EXEC_VERIF, "  Current and target speeds mismatch!");
		xv_sc_dbg(EXEC_VERIF, "    LINK_CONTROL_STATUS: 0x%08x",
			  g->ops.xve.xve_readl(g, xve_link_control_status_r()));
		xv_sc_dbg(EXEC_VERIF, "    Link speed is %s - should be %s",
			  xve_speed_to_str(new_link_speed),
			  xve_speed_to_str(link_speed_setting));

		link_config &= ~xp_pl_link_config_max_link_rate_m();
		if (new_link_speed ==
		    xve_link_control_status_link_speed_link_speed_2p5_v())
			link_config |= xp_pl_link_config_max_link_rate_f(
				xp_pl_link_config_max_link_rate_2500_mtps_v());
		else if (new_link_speed ==
			 xve_link_control_status_link_speed_link_speed_5p0_v())
			link_config |= xp_pl_link_config_max_link_rate_f(
				xp_pl_link_config_max_link_rate_5000_mtps_v());
		else if (new_link_speed ==
			 xve_link_control_status_link_speed_link_speed_8p0_v())
			link_config |= xp_pl_link_config_max_link_rate_f(
				xp_pl_link_config_max_link_rate_8000_mtps_v());
		else
			link_config |= xp_pl_link_config_max_link_rate_f(
				xp_pl_link_config_max_link_rate_2500_mtps_v());

		gk20a_writel(g, xp_pl_link_config_r(0), link_config);
		err_status = -ENODEV;
	} else {
		xv_sc_dbg(EXEC_VERIF, "  Current and target speeds match!");
		err_status = 0;
	}

done:
	/* Restore safe timings. */
	xv_sc_dbg(CLEANUP, "Restoring saved DL settings...");
	gk20a_writel(g, xp_dl_mgr_r(0), saved_dl_mgr);
	xv_sc_dbg(CLEANUP, "  Done");

	xv_sc_dbg(CLEANUP, "Re-enabling ASPM settings...");
	xve_enable_aspm_gp106(g);
	xv_sc_dbg(CLEANUP, "  Done");

	return err_status;
}

/**
 * Sets the PCIe link speed to @xve_link_speed which must be one of:
 *
 *   %GPU_XVE_SPEED_2P5
 *   %GPU_XVE_SPEED_5P0
 *   %GPU_XVE_SPEED_8P0
 *
 * If an error is encountered an appropriate error will be returned.
 */
static int xve_set_speed_gp106(struct gk20a *g, u32 next_link_speed)
{
	u32 current_link_speed;
	int err;

	if ((next_link_speed & GPU_XVE_SPEED_MASK) == 0)
		return -EINVAL;

	err = g->ops.xve.get_speed(g, &current_link_speed);
	if (err)
		return err;

	/* No-op. */
	if (current_link_speed == next_link_speed)
		return 0;

	return __do_xve_set_speed_gp106(g, next_link_speed);
}

/**
 * Places a bitmask of available speeds for gp106 in @speed_mask.
 */
static void xve_available_speeds_gp106(struct gk20a *g, u32 *speed_mask)
{
	*speed_mask = GPU_XVE_SPEED_2P5 | GPU_XVE_SPEED_5P0;
}

static ssize_t xve_link_speed_write(struct file *filp,
				    const char __user *buff,
				    size_t len, loff_t *off)
{
	struct gk20a *g = ((struct seq_file *)filp->private_data)->private;
	char kbuff[16];
	u32 buff_size, check_len;
	u32 link_speed = 0;
	int ret;

	buff_size = min_t(size_t, 16, len);

	memset(kbuff, 0, 16);
	if (copy_from_user(kbuff, buff, buff_size))
		return -EFAULT;

	check_len = strlen("Gen1");
	if (strncmp(kbuff, "Gen1", check_len) == 0)
		link_speed = GPU_XVE_SPEED_2P5;
	else if (strncmp(kbuff, "Gen2", check_len) == 0)
		link_speed = GPU_XVE_SPEED_5P0;
	else if (strncmp(kbuff, "Gen3", check_len) == 0)
		link_speed = GPU_XVE_SPEED_8P0;
	else
		gk20a_err(g->dev, "%s: Unknown PCIe speed: %s\n",
			  __func__, kbuff);

	if (!link_speed)
		return -EINVAL;

	/* Brief pause... To help rate limit this. */
	msleep(250);

	/*
	 * And actually set the speed. Yay.
	 */
	ret = g->ops.xve.set_speed(g, link_speed);
	if (ret)
		return ret;

	return len;
}

static int xve_link_speed_show(struct seq_file *s, void *unused)
{
	struct gk20a *g = s->private;
	u32 speed;
	int err;

	err = g->ops.xve.get_speed(g, &speed);
	if (err)
		return err;

	seq_printf(s, "Current PCIe speed:\n  %s\n", xve_speed_to_str(speed));

	return 0;
}

static int xve_link_speed_open(struct inode *inode, struct file *file)
{
	return single_open(file, xve_link_speed_show, inode->i_private);
}

static const struct file_operations xve_link_speed_fops = {
	.open = xve_link_speed_open,
	.read = seq_read,
	.write = xve_link_speed_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int xve_available_speeds_show(struct seq_file *s, void *unused)
{
	struct gk20a *g = s->private;
	u32 available_speeds;

	g->ops.xve.available_speeds(g, &available_speeds);

	seq_puts(s, "Available PCIe bus speeds:\n");
	if (available_speeds & GPU_XVE_SPEED_2P5)
		seq_puts(s, "  Gen1\n");
	if (available_speeds & GPU_XVE_SPEED_5P0)
		seq_puts(s, "  Gen2\n");
	if (available_speeds & GPU_XVE_SPEED_8P0)
		seq_puts(s, "  Gen3\n");

	return 0;
}

static int xve_available_speeds_open(struct inode *inode, struct file *file)
{
	return single_open(file, xve_available_speeds_show, inode->i_private);
}

static const struct file_operations xve_available_speeds_fops = {
	.open = xve_available_speeds_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int xve_link_control_status_show(struct seq_file *s, void *unused)
{
	struct gk20a *g = s->private;
	u32 link_status;

	link_status = g->ops.xve.xve_readl(g, xve_link_control_status_r());
	seq_printf(s, "0x%08x\n", link_status);

	return 0;
}

static int xve_link_control_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, xve_link_control_status_show, inode->i_private);
}

static int xve_aspm_l0s_get(void *data, u64 *val)
{
	struct gk20a *g = (struct gk20a *) data;

	mutex_lock(&g->xve_lock);
	*val = g->xve_l0s ? 0ULL : 1ULL;
	mutex_unlock(&g->xve_lock);
	return 0;
}

static int xve_aspm_l0s_set(void *data, u64 val)
{
	struct gk20a *g = (struct gk20a *) data;

	mutex_lock(&g->xve_lock);
	g->xve_l0s = val ? false : true;
	mutex_unlock(&g->xve_lock);
	return 0;
}

static int xve_aspm_l1_get(void *data, u64 *val)
{
	struct gk20a *g = (struct gk20a *) data;

	mutex_lock(&g->xve_lock);
	*val = g->xve_l1 ? 0ULL : 1ULL;
	mutex_unlock(&g->xve_lock);
	return 0;
}

static int xve_aspm_l1_set(void *data, u64 val)
{
	struct gk20a *g = (struct gk20a *) data;

	mutex_lock(&g->xve_lock);
	g->xve_l1 = val ? false : true;
	mutex_unlock(&g->xve_lock);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(
	xve_aspm_l0s_override_fops,
	xve_aspm_l0s_get,
	xve_aspm_l0s_set,
	"%llu\n"
);

DEFINE_SIMPLE_ATTRIBUTE(
	xve_aspm_l1_override_fops,
	xve_aspm_l1_get,
	xve_aspm_l1_set,
	"%llu\n"
);

static const struct file_operations xve_link_control_status_fops = {
	.open = xve_link_control_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int xve_sw_init_gp106(struct device *dev)
{
	int err = -ENODEV;
	struct gk20a *g = get_gk20a(dev);
#ifdef CONFIG_DEBUG_FS
	struct gk20a_platform *plat = gk20a_get_platform(dev);
	struct dentry *gpu_root = plat->debugfs;

	g->debugfs_xve = debugfs_create_dir("xve", gpu_root);
	if (IS_ERR_OR_NULL(g->debugfs_xve))
		goto fail;

	/*
	 * These are just debug nodes. If they fail to get made it's not worth
	 * worrying the higher level SW.
	 */
	debugfs_create_file("link_speed", S_IRUGO,
			    g->debugfs_xve, g,
			    &xve_link_speed_fops);
	debugfs_create_file("available_speeds", S_IRUGO,
			    g->debugfs_xve, g,
			    &xve_available_speeds_fops);
	debugfs_create_file("link_control_status", S_IRUGO,
			    g->debugfs_xve, g,
			    &xve_link_control_status_fops);
	debugfs_create_file("aspm_l0s_override", S_IRUGO,
			    g->debugfs_xve, g,
			    &xve_aspm_l0s_override_fops);
	debugfs_create_file("aspm_l1_override", S_IRUGO,
			    g->debugfs_xve, g,
			    &xve_aspm_l1_override_fops);
	err = 0;
fail:
#endif
	mutex_init(&g->xve_lock);
	return err;
}

static int xve_hw_init_gp106(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	struct gk20a_platform *platform = gk20a_get_platform(dev);

	mutex_lock(&g->xve_lock);
	if (platform->disable_aspm_l0s)
		g->xve_l0s = true;
	if (platform->disable_aspm_l1)
		g->xve_l1 = true;

	set_xve_l0s_mask(g, g->xve_l0s);
	set_xve_l1_mask(g, g->xve_l1);

	mutex_unlock(&g->xve_lock);
	return 0;
}

/*
 * Init the HAL functions and what not. xve_sw_init_gp106() is for initializing
 * all the other stuff like debugfs nodes, etc.
 */
int gp106_init_xve_ops(struct gpu_ops *gops)
{
	gops->xve.sw_init          = xve_sw_init_gp106;
	gops->xve.hw_init          = xve_hw_init_gp106;
	gops->xve.get_speed        = xve_get_speed_gp106;
	gops->xve.set_speed        = xve_set_speed_gp106;
	gops->xve.available_speeds = xve_available_speeds_gp106;
	gops->xve.xve_readl        = xve_xve_readl_gp106;
	gops->xve.xve_writel       = xve_xve_writel_gp106;
	gops->xve.disable_aspm     = xve_disable_aspm_gp106;
	gops->xve.enable_aspm      = xve_enable_aspm_gp106;
	gops->xve.reset_gpu        = xve_reset_gpu_gp106;

	return 0;
}
