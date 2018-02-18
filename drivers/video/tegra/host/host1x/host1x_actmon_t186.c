/*
 * Tegra Graphics Host Actmon support for T186
 *
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/nvhost.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "dev.h"
#include "chip_support.h"
#include "host1x/host1x_actmon.h"
#include "bus_client_t186.h"
#include "nvhost_scale.h"

static void host1x_actmon_process_isr(u32 hintstat, void *priv);

static void actmon_writel(struct host1x_actmon *actmon, u32 val, u32 reg)
{
	nvhost_dbg(dbg_reg, " r=0x%x v=0x%x", reg, val);
	writel(val, actmon->regs + reg);
}

static u32 actmon_readl(struct host1x_actmon *actmon, u32 reg)
{
	u32 val = readl(actmon->regs + reg);
	nvhost_dbg(dbg_reg, " r=0x%x v=0x%x", reg, val);
	return val;
}

static void host1x_actmon_dump_regs(struct host1x_actmon *actmon)
{
	nvhost_dbg(dbg_reg, "global ctrl: 0x%x\n",
		actmon_readl(actmon, actmon_glb_ctrl_r()));

	nvhost_dbg(dbg_reg, "global intr enable: 0x%x\n",
		actmon_readl(actmon, actmon_glb_intr_en_r()));

	nvhost_dbg(dbg_reg, "global intr status: 0x%x\n",
		actmon_readl(actmon, actmon_glb_intr_status_r()));

	nvhost_dbg(dbg_reg, "local ctrl: 0x%x\n",
		actmon_readl(actmon, actmon_local_ctrl_r()));

	nvhost_dbg(dbg_reg, "local intr enable: 0x%x\n",
		actmon_readl(actmon, actmon_local_intr_en_r()));

	nvhost_dbg(dbg_reg, "local intr status: 0x%x\n",
		actmon_readl(actmon, actmon_local_intr_status_r()));

	nvhost_dbg(dbg_reg, "local avg upper watermark: 0x%x\n",
		actmon_readl(actmon, actmon_local_avg_upper_wmark_r()));

	nvhost_dbg(dbg_reg, "local avg lower watermark: 0x%x\n",
		actmon_readl(actmon, actmon_local_avg_lower_wmark_r()));

	nvhost_dbg(dbg_reg, "local init avg: 0x%x\n",
		actmon_readl(actmon, actmon_local_init_avg_r()));

	nvhost_dbg(dbg_reg, "local count : 0x%x\n",
		actmon_readl(actmon, actmon_local_count_r()));

	nvhost_dbg(dbg_reg, "local avg count : 0x%x\n",
		actmon_readl(actmon, actmon_local_avg_count_r()));

	nvhost_dbg(dbg_reg, "local cumulative count : 0x%x\n",
		actmon_readl(actmon, actmon_local_cumulative_r()));
}

static void host1x_actmon_event_fn(struct host1x_actmon *actmon, int type)
{
	struct platform_device *pdev = actmon->pdev;
	struct nvhost_device_data *engine_pdata = platform_get_drvdata(pdev);

	/* ensure that the device remains powered */
	nvhost_module_busy_noresume(pdev);
	if (pm_runtime_active(&pdev->dev)) {
		/* first, handle scaling */
		nvhost_scale_actmon_irq(pdev, type);

		/* then, rewire the actmon IRQ */
		nvhost_intr_enable_host_irq(&nvhost_get_host(pdev)->intr,
					    engine_pdata->actmon_irq,
					    host1x_actmon_process_isr,
					    actmon);
	}
	host1x_actmon_dump_regs(actmon);
	nvhost_module_idle(pdev);
}

static void host1x_actmon_process_isr(u32 hintstat, void *priv)
{
	struct host1x_actmon *actmon = priv;
	struct platform_device *host_pdev = actmon->host->dev;
	struct nvhost_device_data *engine_pdata =
		platform_get_drvdata(actmon->pdev);
	long val;

	/* first, disable the interrupt */
	nvhost_intr_disable_host_irq(&nvhost_get_host(host_pdev)->intr,
				     engine_pdata->actmon_irq);

	/* get the event type */
	val = actmon_readl(actmon, actmon_glb_intr_status_r());
	actmon_writel(actmon, val, actmon_glb_intr_status_r());
	val = actmon_readl(actmon, actmon_local_intr_status_r());
	actmon_writel(actmon, val, actmon_local_intr_status_r());

	if (actmon_local_intr_status_avg_above_wmark_v(val))
		host1x_actmon_event_fn(actmon, ACTMON_INTR_ABOVE_WMARK);
	else if (actmon_local_intr_status_avg_below_wmark_v(val))
		host1x_actmon_event_fn(actmon, ACTMON_INTR_BELOW_WMARK);
}

/*
 * actmon_update_sample_period_safe(host)
 *
 * This function updates frequency specific values on actmon using the current
 * actmon frequency. The function should be called only when host1x is active.
 *
 */

static void actmon_update_sample_period_safe(struct host1x_actmon *actmon)
{
	long freq_mhz, clks_per_sample;
	u32 val = actmon_readl(actmon, actmon_glb_ctrl_r());

	/* Set SOURCE as TICK */
	val |= actmon_glb_ctrl_source_f(2);

	/* We use MHz and us instead of Hz and s due to numerical limitations */
	freq_mhz = clk_get_rate(actmon->clk) / 1000000;

	if ((freq_mhz * actmon->usecs_per_sample) / 256 > 255) {
		val |= actmon_glb_ctrl_sample_tick_f(1);
		actmon->divider = 65536;
	} else {
		val &= ~actmon_glb_ctrl_sample_tick_f(1);
		actmon->divider = 256;
	}

	clks_per_sample = (freq_mhz * actmon->usecs_per_sample) /
		actmon->divider;
	actmon->clks_per_sample = clks_per_sample + 1;

	val &= ~actmon_glb_ctrl_sample_period_m();
	val |= actmon_glb_ctrl_sample_period_f(clks_per_sample);
	actmon_writel(actmon, val, actmon_glb_ctrl_r());

	/* AVG value depends on sample period => clear it */
	actmon_writel(actmon, 0, actmon_local_init_avg_r());

	host1x_actmon_dump_regs(actmon);
}

static void __iomem *host1x_actmon_get_regs(struct host1x_actmon *actmon)
{
	struct nvhost_device_data *pdata =
		platform_get_drvdata(actmon->pdev);
	void __iomem *actmon_base = NULL;

	actmon_base = get_aperture(actmon->host->dev, HOST1X_ACTMON_APERTURE);
	if (actmon_base)
		return actmon_base + pdata->actmon_regs;

	return NULL;
}

static int host1x_actmon_init(struct host1x_actmon *actmon)
{
	struct platform_device *host_pdev = actmon->host->dev;
	struct nvhost_device_data *host_pdata =
		platform_get_drvdata(host_pdev);
	struct nvhost_device_data *engine_pdata =
		platform_get_drvdata(actmon->pdev);

	u32 val;

	if (actmon->init == ACTMON_READY)
		return 0;

	if (actmon->init == ACTMON_OFF) {
		actmon->usecs_per_sample = 80;
		actmon->k = 4;
	}

	actmon->clk = host_pdata->clk[1];
	if (!actmon->clk)
		return -ENODEV;

	/* Clear average and control registers */
	actmon_writel(actmon, 0, actmon_local_init_avg_r());
	actmon_writel(actmon, 0, actmon_glb_ctrl_r());
	actmon_writel(actmon, 0, actmon_glb_intr_en_r());
	actmon_writel(actmon, 0, actmon_local_intr_en_r());
	actmon_writel(actmon, 0, actmon_local_ctrl_r());
	actmon_writel(actmon, 0, actmon_local_avg_lower_wmark_r());
	actmon_writel(actmon, 0, actmon_local_avg_upper_wmark_r());

	/* Write (normalised) sample period. */
	actmon_update_sample_period_safe(actmon);

	/* Clear interrupt status */
	actmon_writel(actmon, 0xffffffff, actmon_glb_intr_status_r());
	actmon_writel(actmon, 0xffffffff, actmon_local_intr_status_r());

	val = actmon_readl(actmon, actmon_local_ctrl_r());
	val |= actmon_local_ctrl_enb_periodic_f(1);
	val |= actmon_local_ctrl_k_val_f(actmon->k);
	val |= actmon_local_ctrl_actmon_enable_f(1);
	val |= actmon_local_ctrl_enb_cumulative_f(1);
	actmon_writel(actmon, val, actmon_local_ctrl_r());

	/* Enable global interrupt */
	if (engine_pdata->actmon_irq)
		actmon_writel(actmon, 0x1, actmon_glb_intr_en_r());

	nvhost_intr_enable_host_irq(&nvhost_get_host(host_pdev)->intr,
				    engine_pdata->actmon_irq,
				    host1x_actmon_process_isr,
				    actmon);

	actmon->init = ACTMON_READY;

	host1x_actmon_dump_regs(actmon);

	return 0;
}

static void host1x_actmon_deinit(struct host1x_actmon *actmon)
{
	struct platform_device *host_pdev = actmon->host->dev;
	struct nvhost_device_data *engine_pdata =
		platform_get_drvdata(actmon->pdev);

	if (actmon->init != ACTMON_READY)
		return;

	/* Disable interrupts */
	if (engine_pdata->actmon_irq) {
		actmon_writel(actmon, 0x0, actmon_glb_intr_en_r());
		actmon_writel(actmon, 0x0, actmon_local_intr_en_r());
	}

	/* clear intrrupt status registers */
	actmon_writel(actmon, 0xffffffff, actmon_glb_intr_status_r());
	actmon_writel(actmon, 0xffffffff, actmon_local_intr_status_r());

	/* Disable actmon */
	actmon_writel(actmon, 0x0, actmon_local_ctrl_r());
	actmon_writel(actmon, 0x0, actmon_glb_ctrl_r());

	actmon->init = ACTMON_SLEEP;

	nvhost_intr_disable_host_irq(&nvhost_get_host(host_pdev)->intr,
				     engine_pdata->actmon_irq);

	host1x_actmon_dump_regs(actmon);
}

static int host1x_actmon_avg(struct host1x_actmon *actmon, u32 *val)
{
	int err;

	if (actmon->init != ACTMON_READY) {
		*val = 0;
		return 0;
	}

	err = nvhost_module_busy(actmon->host->dev);
	if (err)
		return err;

	rmb();
	*val = actmon_readl(actmon, actmon_local_avg_count_r());
	nvhost_module_idle(actmon->host->dev);

	return 0;
}

static int host1x_actmon_avg_norm(struct host1x_actmon *actmon, u32 *avg)
{
	long val;
	int err;

	if (!(actmon->init == ACTMON_READY && actmon->clks_per_sample > 0 &&
	    actmon->divider)) {
		*avg = 0;
		return 0;
	}

	/* Read load from hardware */
	err = nvhost_module_busy(actmon->host->dev);
	if (err)
		return err;

	rmb();
	val = actmon_readl(actmon, actmon_local_avg_count_r());
	nvhost_module_idle(actmon->host->dev);

	*avg = (val * 1000) / (actmon->clks_per_sample * actmon->divider);

	return 0;
}

static int host1x_actmon_count(struct host1x_actmon *actmon, u32 *val)
{
	int err;

	if (actmon->init != ACTMON_READY) {
		*val = 0;
		return 0;
	}

	/* Read load from hardware */
	err = nvhost_module_busy(actmon->host->dev);
	if (err)
		return err;

	rmb();
	*val = actmon_readl(actmon, actmon_local_count_r());
	nvhost_module_idle(actmon->host->dev);

	return 0;
}

static int host1x_actmon_count_norm(struct host1x_actmon *actmon, u32 *avg)
{
	long val;
	int err;

	if (!(actmon->init == ACTMON_READY && actmon->clks_per_sample > 0 &&
	    actmon->divider)) {
		*avg = 0;
		return 0;
	}

	/* Read load from hardware */
	err = nvhost_module_busy(actmon->host->dev);
	if (err)
		return err;

	rmb();
	val = actmon_readl(actmon, actmon_local_count_r());
	nvhost_module_idle(actmon->host->dev);

	*avg = (val * 1000) / (actmon->clks_per_sample * actmon->divider);

	return 0;
}

static int host1x_set_high_wmark(struct host1x_actmon *actmon, u32 val_scaled)
{
	u32 val = (val_scaled < 1000) ?
		((val_scaled * actmon->clks_per_sample * actmon->divider) /
		1000) : actmon->clks_per_sample * actmon->divider;

	if (actmon->init != ACTMON_READY)
		return 0;

	/* write new watermark */
	actmon_writel(actmon, val, actmon_local_avg_upper_wmark_r());

	/* enable or disable watermark depending on the values */
	val = actmon_readl(actmon, actmon_local_intr_en_r());
	if (val_scaled < 1000)
		val |= actmon_local_intr_en_avg_above_wmark_en_f(1);
	else
		val &= ~actmon_local_intr_en_avg_above_wmark_en_f(1);
	actmon_writel(actmon, val, actmon_local_intr_en_r());

	host1x_actmon_dump_regs(actmon);

	return 0;
}

static int host1x_set_low_wmark(struct host1x_actmon *actmon, u32 val_scaled)
{
	u32 val = (val_scaled < 1000) ?
		((val_scaled * actmon->clks_per_sample * actmon->divider) /
		1000) : actmon->clks_per_sample * actmon->divider;

	if (actmon->init != ACTMON_READY)
		return 0;

	/* write new watermark */
	actmon_writel(actmon, val, actmon_local_avg_lower_wmark_r());

	/* enable or disable watermark depending on the values */
	val = actmon_readl(actmon, actmon_local_intr_en_r());
	if (val_scaled)
		val |= actmon_local_intr_en_avg_below_wmark_en_f(1);
	else
		val &= ~actmon_local_intr_en_avg_below_wmark_en_f(1);
	actmon_writel(actmon, val, actmon_local_intr_en_r());

	host1x_actmon_dump_regs(actmon);

	return 0;
}

static void host1x_actmon_update_sample_period(struct host1x_actmon *actmon)
{
	struct platform_device *pdev = actmon->host->dev;
	int err;

	/* No sense to update actmon if actmon is inactive */
	if (actmon->init != ACTMON_READY)
		return;

	err = nvhost_module_busy(actmon->host->dev);
	if (err) {
		nvhost_warn(&pdev->dev, "failed to power on host1x. sample period update failed");
		return;
	}

	actmon_update_sample_period_safe(actmon);
	nvhost_module_idle(actmon->host->dev);
}

static void host1x_actmon_set_sample_period_norm(struct host1x_actmon *actmon,
						 long usecs)
{
	actmon->usecs_per_sample = usecs;
	host1x_actmon_update_sample_period(actmon);
}

static void host1x_actmon_set_k(struct host1x_actmon *actmon, u32 k)
{
	struct platform_device *pdev = actmon->host->dev;
	long val;
	int err;

	actmon->k = k;

	err = nvhost_module_busy(actmon->host->dev);
	if (err) {
		nvhost_warn(&pdev->dev, "failed to power on host1x. sample period update failed");
		return;
	}

	val = actmon_readl(actmon, actmon_local_ctrl_r());
	val &= ~(actmon_local_ctrl_k_val_m());
	val |= actmon_local_ctrl_k_val_f(actmon->k);
	actmon_writel(actmon, val, actmon_local_ctrl_r());
	nvhost_module_idle(actmon->host->dev);
}

static u32 host1x_actmon_get_k(struct host1x_actmon *actmon)
{
	return actmon->k;
}

static long host1x_actmon_get_sample_period(struct host1x_actmon *actmon)
{
	return actmon->clks_per_sample;
}

static long host1x_actmon_get_sample_period_norm(struct host1x_actmon *actmon)
{
	return actmon->usecs_per_sample;
}

static int host1x_actmon_cumulative(struct host1x_actmon *actmon, u32 *val)
{
	int err;

	if (actmon->init != ACTMON_READY) {
		*val = 0;
		return 0;
	}

	/* Read load from hardware */
	err = nvhost_module_busy(actmon->host->dev);
	if (err)
		return err;

	rmb();
	*val = actmon_readl(actmon, actmon_local_cumulative_r());
	nvhost_module_idle(actmon->host->dev);

	return 0;
}

static int actmon_cumulative_show(struct seq_file *s, void *unused)
{
	struct host1x_actmon *actmon = s->private;
	u32 avg;
	int err;

	err = host1x_actmon_cumulative(actmon, &avg);
	if (!err)
		seq_printf(s, "%d\n", avg);
	return err;
}

static int actmon_cumulative_open(struct inode *inode, struct file *file)
{
	return single_open(file, actmon_cumulative_show, inode->i_private);
}

static const struct file_operations actmon_cumulative_fops = {
	.open		= actmon_cumulative_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void t18x_actmon_debug_init(struct host1x_actmon *actmon,
				     struct dentry *de)
{
	BUG_ON(!actmon);
	debugfs_create_file("actmon_cumulative", S_IRUGO, de,
			actmon, &actmon_cumulative_fops);
}

static const struct nvhost_actmon_ops host1x_actmon_ops = {
	.get_actmon_regs = host1x_actmon_get_regs,
	.init = host1x_actmon_init,
	.deinit = host1x_actmon_deinit,
	.read_avg = host1x_actmon_avg,
	.read_avg_norm = host1x_actmon_avg_norm,
	.read_count = host1x_actmon_count,
	.read_count_norm = host1x_actmon_count_norm,
	.update_sample_period = host1x_actmon_update_sample_period,
	.set_sample_period_norm = host1x_actmon_set_sample_period_norm,
	.get_sample_period_norm = host1x_actmon_get_sample_period_norm,
	.get_sample_period = host1x_actmon_get_sample_period,
	.get_k = host1x_actmon_get_k,
	.set_k = host1x_actmon_set_k,
	.debug_init = t18x_actmon_debug_init,
	.set_high_wmark = host1x_set_high_wmark,
	.set_low_wmark = host1x_set_low_wmark,

};
