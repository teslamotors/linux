/*
 * Copyright (c) 2014, NVIDIA CORPORATION. All Rights Reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/edp.h>
#include <linux/sysedp.h>
#include <linux/tegra-soc.h>
#include <linux/tegra-fuse.h>
#include <linux/regulator/consumer.h>

#include <mach/edp.h>

#include <linux/platform/tegra/dvfs.h>
#include <linux/platform/tegra/clock.h>
#include <linux/platform/tegra/common.h>
#include <linux/platform/tegra/cpu-tegra.h>

struct cpu_edp_platform_data {
	const char *name;
	char *tz_name;
	char *clk_name;
	u32 *hard_cap;
	int n_caps;
	int freq_step;
	int reg_edp;
	int reg_idle_max;
};
struct cpu_edp {
	struct cpu_edp_platform_data pdata;
	struct tegra_ppm *ppm;
	int temperature;

	unsigned long cdev_state; /* not actually meaningful */
	struct thermal_cooling_device *cdev;
	struct thermal_zone_device *tz;
	struct mutex cdev_lock;
};

static struct cpu_edp s_cpu = {
	.temperature = 75, /* assume we're running hot */
};

unsigned int tegra_get_sysedp_max_freq(int cpupwr,
				       int online_cpus, int cpu_mode)
{
	if (WARN_ONCE(!s_cpu.ppm, "Init call ordering issue?\n"))
		return 0;

	if (cpu_mode != MODE_G) {
		/* TODO: for T210, handle MODE_LP */
		return 0;
	}

	return tegra_ppm_get_maxf(s_cpu.ppm, cpupwr,
				  TEGRA_PPM_UNITS_MILLIWATTS,
				  s_cpu.temperature, online_cpus);
}

unsigned int tegra_get_edp_max_freq(int online_cpus,
				    int cpu_mode)
{
	unsigned int res;

	if (WARN_ONCE(!s_cpu.ppm, "Init call ordering issue?\n"))
		return 0;

	if (cpu_mode != MODE_G) {
		/* TODO: for T210, handle MODE_LP */
		return 0;
	}

	res = tegra_ppm_get_maxf(s_cpu.ppm, s_cpu.pdata.reg_edp,
				 TEGRA_PPM_UNITS_MILLIAMPS,
				 s_cpu.temperature, online_cpus);

	if (s_cpu.pdata.hard_cap && s_cpu.pdata.hard_cap[online_cpus - 1]
	    && res > s_cpu.pdata.hard_cap[online_cpus - 1])
		res = s_cpu.pdata.hard_cap[online_cpus - 1];

	return res;
}

unsigned int tegra_get_reg_idle_freq(int online_cpus)
{
	unsigned int res;

	if (WARN_ONCE(!s_cpu.ppm, "Init call ordering issue?\n"))
		return 0;

	/* TODO: for T210, handle MODE_LP */
	res = tegra_ppm_get_maxf(s_cpu.ppm, s_cpu.pdata.reg_idle_max,
				 TEGRA_PPM_UNITS_MILLIAMPS,
				 s_cpu.temperature, online_cpus);

	return res;

}

bool tegra_is_edp_reg_idle_supported(void)
{
	return (s_cpu.pdata.reg_idle_max != 0);
}

bool tegra_is_cpu_edp_supported(void)
{
	return s_cpu.pdata.reg_edp != 0;
}

void tegra_recalculate_cpu_edp_limits(void)
{
	if (!tegra_is_cpu_edp_supported())
		return;

	tegra_ppm_drop_cache(s_cpu.ppm);
}

static int tegra_cpu_edp_get_max_state(struct thermal_cooling_device *cdev,
				unsigned long *max_state)
{
	*max_state = 1024; /* meaningless largish number */
	return 0;
}

static int tegra_cpu_edp_get_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *cur_state)
{
	struct cpu_edp *ctx = cdev->devdata;
	*cur_state = ctx->cdev_state;
	return 0;
}

static int cpu_edp_get_tz(struct cpu_edp *ctx)
{
	int ret = 0;

	mutex_lock(&ctx->cdev_lock);
	if (!ctx->tz) {
		ctx->tz = thermal_zone_get_zone_by_name(ctx->pdata.tz_name);

		if (IS_ERR(ctx->tz)) {
			ctx->tz = NULL;
			ret = -ENODEV;
		}
	}
	mutex_unlock(&ctx->cdev_lock);

	return ret;
}

static int tegra_cpu_edp_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long cur_state)
{
	struct cpu_edp *ctx = cdev->devdata;

	if (!ctx->tz &&
	    WARN_ONCE(cpu_edp_get_tz(ctx),
		      "%s: can't find thermal zone to get temperature\n",
		      __func__))
		return -ENODEV;

	mutex_lock(&ctx->cdev_lock);
	ctx->cdev_state = cur_state;

	/* get temperature, convert from mC to C, and quantize to 4 degrees */
	ctx->temperature = (ctx->tz->temperature + 3999) / 4000 * 4;

	tegra_cpu_set_speed_cap(NULL);
	tegra_edp_update_max_cpus();

	mutex_unlock(&ctx->cdev_lock);

	return 0;
}

static struct thermal_cooling_device_ops tegra_cpu_edp_cooling_ops = {
	.get_max_state = tegra_cpu_edp_get_max_state,
	.get_cur_state = tegra_cpu_edp_get_cur_state,
	.set_cur_state = tegra_cpu_edp_set_cur_state,
};

static int __init cpu_edp_cdev_init(struct cpu_edp *ctx, struct device_node *np)
{
	mutex_init(&ctx->cdev_lock);

	if (cpu_edp_get_tz(ctx))
		pr_info("%s: Thermal zone %s not (yet?) available\n",
			__func__, ctx->pdata.tz_name);

	ctx->cdev = thermal_of_cooling_device_register(
		np, "cpu_edp", ctx, &tegra_cpu_edp_cooling_ops);
	if (IS_ERR_OR_NULL(ctx->cdev)) {
		ctx->cdev = NULL;
		return -ENODEV;
	}

	return 0;
}

#ifdef CONFIG_DEBUG_FS
static int __init tegra_edp_debugfs_init(struct dentry *edp_dir)
{
	if (!tegra_platform_is_silicon())
		return -ENOSYS;

	if (!edp_dir)
		return -ENOMEM;

	debugfs_create_u32("reg_idle_ma", S_IRUGO | S_IWUSR,
			   edp_dir, &s_cpu.pdata.reg_idle_max);
	debugfs_create_u32("reg_edp_ma", S_IRUGO | S_IWUSR,
			   edp_dir, &s_cpu.pdata.reg_edp);
	debugfs_create_u32("temperature", S_IRUGO,
			   edp_dir, &s_cpu.temperature);
	if (s_cpu.pdata.hard_cap)
		debugfs_create_u32_array(
			"hard_cap", S_IRUGO,
			edp_dir, s_cpu.pdata.hard_cap,
			s_cpu.pdata.n_caps);

	return 0;
}
#else
static int __init tegra_edp_debugfs_init(void)
{ return 0; }
#endif /* CONFIG_DEBUG_FS */

static int __init tegra_cpu_edp_parse_dt(struct platform_device *pdev,
					 struct cpu_edp_platform_data *pdata)
{

	struct device_node *np = pdev->dev.of_node;

	if (WARN(of_property_read_u32(np, "nvidia,freq_step",
				      &pdata->freq_step),
		 "missing required parameter: nvidia,freq_step\n"))
		return -ENODATA;

	if (WARN(of_property_read_u32(np, "nvidia,edp_limit",
				      &pdata->reg_edp),
		 "missing required parameter: nvidia,edp_limit\n"))
		return -ENODATA;

	if (of_find_property(np, "nvidia,regulator_idle_limit", NULL) &&
	    WARN(of_property_read_u32(np, "nvidia,regulator_idle_limit",
				      &pdata->reg_idle_max),
		 "missing parameter value: nvidia,regulator_idle_limit\n"))
		return -ENODATA;

	if (WARN(of_property_read_string(np, "nvidia,edp_clk",
				      (const char **)&pdata->clk_name),
		 "missing required parameter: nvidia,edp_clk\n"))
		return -ENODATA;

	pdata->n_caps = of_property_count_u32(np, "nvidia,hard_cap");
	if (pdata->n_caps > 0) {
		pdata->hard_cap = devm_kzalloc(&pdev->dev,
					       sizeof(u32[pdata->n_caps]),
					       GFP_KERNEL);
		if (!pdata->hard_cap)
			return -ENOMEM;

		if (WARN(of_property_read_u32_array(np, "nvidia,hard_cap",
						    (u32 *)pdata->hard_cap,
						    pdata->n_caps),
			 "missing parameter value: nvidia,hard_cap\n"))
			return -ENODATA;
	}

	pdata->name = dev_name(&pdev->dev);
	pdata->tz_name = "CPU-therm";

	return 0;
}

static int __init tegra_cpu_edp_probe(struct platform_device *pdev)
{
	struct clk *cpu_clk;
	struct fv_relation *fv;
	struct tegra_ppm_params *params;
	unsigned iddq_ma;

	struct cpu_edp *ctx = &s_cpu;
	int ret;
	struct dentry *edp_dir;

	if (WARN(!pdev || !pdev->dev.of_node,
		 "DT node required but absent\n"))
		return -EINVAL;

	params = of_read_tegra_ppm_params(pdev->dev.of_node);
	if (WARN(IS_ERR_OR_NULL(params),
		 "CPU EDP management initialization failed\n"))
		return PTR_ERR(params);

	ret = tegra_cpu_edp_parse_dt(pdev, &ctx->pdata);
	if (WARN(ret, "CPU EDP management initialization failed\n"))
		return ret;

	cpu_clk = tegra_get_clock_by_name(ctx->pdata.clk_name);
	fv = fv_relation_create(cpu_clk, ctx->pdata.freq_step, 220,
				      tegra_dvfs_predict_mv_at_hz_max_tfloor);

	if (WARN(IS_ERR_OR_NULL(fv),
		 "Failed CPU EDP mgmt init. freq/volt data unavailable\n"))
		return PTR_ERR(fv);

	iddq_ma = tegra_get_cpu_iddq_value();
	pr_debug("%s: CPU IDDQ value %d\n", __func__, iddq_ma);

	edp_dir = debugfs_create_dir("cpu_edp", NULL);

	ctx->ppm = tegra_ppm_create("cpu_edp", fv, params,
				    iddq_ma, edp_dir);

	if (WARN(IS_ERR_OR_NULL(ctx->ppm),
		 "Failed CPU EDP mgmt init. Couldn't create power model\n"))
		return PTR_ERR(ctx->ppm);

	ret = cpu_edp_cdev_init(ctx, pdev->dev.of_node);
	if (ret) {
		dev_err(&pdev->dev, "Failed CPU EDP mgmt thermal registration\n");
		return ret;
	}

	tegra_edp_debugfs_init(edp_dir);
	return 0;
}


static struct of_device_id tegra_cpu_edp_of_match[] = {
	{ .compatible = "nvidia,tegra124-cpu-edp-capping" },
	{ },
};
MODULE_DEVICE_TABLE(of, tegra_cpu_edp_of_match);

static struct platform_driver tegra_cpu_edp_driver = {
	.driver = {
		.name = "tegra_cpu_edp",
		.owner = THIS_MODULE,
		.of_match_table = tegra_cpu_edp_of_match,
	},
};

static int __init tegra_cpu_edp_driver_init(void)
{
	return platform_driver_probe(&tegra_cpu_edp_driver,
				     tegra_cpu_edp_probe);
}
module_init(tegra_cpu_edp_driver_init);

MODULE_AUTHOR("NVIDIA Corp.");
MODULE_DESCRIPTION("Tegra CPU EDP management");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:tegra-cpu-edp");
