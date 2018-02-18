/*
 * drivers/platform/tegra/tegra18_throttle.c
 *
 * Based on drivers/platform/tegra/tegra_throttle.c
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/thermal.h>
#include <linux/slab.h>
#include <linux/tegra_throttle.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/pm_qos.h>
#include <linux/platform_device.h>
#include <linux/platform/tegra/tegra18_cpu_map.h>
#include <linux/platform/tegra/emc_bwmgr.h>

static DEFINE_MUTEX(bthrot_list_lock);
static LIST_HEAD(bthrot_list);

enum throttle_type {
	THROT_DEFAULT,
	THROT_MCPU,
	THROT_BCPU,
	THROT_EMC,
	THROT_GPU,
};

/* Tracks the final throttle freq. for the given clk */
struct bthrot_freqs {
	const char *cap_name;
	struct clk *cap_clk;
	unsigned long cap_freq;
	enum throttle_type type;
};

/* Array of final bthrot caps for each clk */
static struct bthrot_freqs *cap_freqs_table;
static int num_cap_clks;

struct balanced_throttle {
	struct thermal_cooling_device *cdev;
	char *cdev_type;
	struct list_head node;
	unsigned long cur_state;
	int throttle_count;
	int throt_tab_size;
	u32 *throt_tab;
};

static struct tegra_bwmgr_client *emc_throt_handle;
static struct pm_qos_request gpu_cap;

#define CAP_TBL_CAP_NAME(index)	(cap_freqs_table[index].cap_name)
#define CAP_TBL_CAP_CLK(index)	(cap_freqs_table[index].cap_clk)
#define CAP_TBL_CAP_FREQ(index)	(cap_freqs_table[index].cap_freq)
#define CAP_TBL_CAP_TYPE(index) (cap_freqs_table[index].type)

#define THROT_TBL_IDX(row, col)		(((row) * num_cap_clks) + (col))
#define THROT_VAL(tbl, row, col)	(tbl)[(THROT_TBL_IDX(row, col))]

bool tegra_is_throttling(int *count)
{
	struct balanced_throttle *bthrot;
	bool is_throttling = false;
	int lcount = 0;

	mutex_lock(&bthrot_list_lock);
	list_for_each_entry(bthrot, &bthrot_list, node) {
		if (bthrot->cur_state)
			is_throttling = true;
		lcount += bthrot->throttle_count;
	}
	mutex_unlock(&bthrot_list_lock);

	if (count)
		*count = lcount;
	return is_throttling;
}

static int
tegra_throttle_get_max_state(struct thermal_cooling_device *cdev,
				unsigned long *max_state)
{
	struct balanced_throttle *bthrot = cdev->devdata;

	*max_state = bthrot->throt_tab_size / num_cap_clks;

	return 0;
}

static int
tegra_throttle_get_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *cur_state)
{
	struct balanced_throttle *bthrot = cdev->devdata;

	*cur_state = bthrot->cur_state;

	return 0;
}

static DEFINE_PER_CPU(unsigned long, max_cpu_rate);

/* Apply thermal constraints on all policy updates */
static int tegra_throttle_policy_notifier(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	unsigned long cpu_max;

	if (event != CPUFREQ_ADJUST)
		return 0;

	cpu_max = per_cpu(max_cpu_rate, policy->cpu);

	if (policy->max != cpu_max)
		cpufreq_verify_within_limits(policy, 0, cpu_max);

	return 0;
}

static struct notifier_block tegra_throttle_cpufreq_nb = {
	.notifier_call = tegra_throttle_policy_notifier,
};

/* Must be called with the bthrot_list_lock held */
static void tegra_throttle_update_cpu_cap(enum throttle_type type,
					unsigned long rate)
{
	int cpu;
	struct cpufreq_policy policy;

	if (type != THROT_BCPU && type != THROT_MCPU)
		return;

	/* Hz to Khz for cpufreq */
	rate = rate / 1000;

	for_each_present_cpu(cpu) {
		if (cpufreq_get_policy(&policy, cpu))
			continue;

		if (type == THROT_MCPU && !tegra18_is_cpu_denver(cpu))
			continue;

		if (type == THROT_BCPU && !tegra18_is_cpu_arm(cpu))
			continue;

		per_cpu(max_cpu_rate, cpu) = rate;
		pr_debug("tegra_throttle: cpu=%d max_rate=%lu\n", cpu, rate);
		cpufreq_update_policy(cpu);
	}
}

/* Must be called with bthrot_list_lock held */
static void tegra_throttle_set_cap_clk(unsigned long cap_rate,
					int cap_clk_index)
{
	unsigned long cur_rate, max_rate = NO_CAP;
	unsigned int type = CAP_TBL_CAP_TYPE(cap_clk_index);
	struct clk *c = CAP_TBL_CAP_CLK(cap_clk_index);
	int ret;

	/* Khz to Hz for clk_set_rate */
	if (cap_rate != NO_CAP)
		max_rate = cap_rate * 1000UL;

	if (CAP_TBL_CAP_FREQ(cap_clk_index) == max_rate)
		return;

	switch (type) {
	case THROT_DEFAULT:
		if (max_rate > LONG_MAX)
			max_rate = LONG_MAX;
		max_rate = clk_round_rate(c, max_rate);
		clk_set_max_rate(c, max_rate);
		cur_rate = clk_get_rate(c);
		if (cur_rate > max_rate) {
			ret = clk_set_rate(c, max_rate);
			if (ret) {
				pr_err("%s: Set max rate failed: %s - %lu\n",
				    __func__, CAP_TBL_CAP_NAME(cap_clk_index),
				    max_rate);
				break;
			}
		}
		CAP_TBL_CAP_FREQ(cap_clk_index) = max_rate;
		break;
	case THROT_MCPU:
	case THROT_BCPU:
		tegra_throttle_update_cpu_cap(type, max_rate);
		CAP_TBL_CAP_FREQ(cap_clk_index) = max_rate;
		break;
	case THROT_EMC:
		if (!IS_ERR_OR_NULL(emc_throt_handle)) {
			tegra_bwmgr_set_emc(emc_throt_handle, max_rate,
						TEGRA_BWMGR_SET_EMC_CAP);
			CAP_TBL_CAP_FREQ(cap_clk_index) = max_rate;
		}
		break;
	case THROT_GPU:
		if (cap_rate == NO_CAP)
			cap_rate = PM_QOS_GPU_FREQ_MAX_DEFAULT_VALUE;
		pm_qos_update_request(&gpu_cap, cap_rate);
		CAP_TBL_CAP_FREQ(cap_clk_index) = max_rate;
		break;
	default:
		break;
	}
}

static int
tegra_throttle_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long cur_state)
{
	struct balanced_throttle *bthrot = cdev->devdata;
	int i, ret = 0;

	mutex_lock(&bthrot_list_lock);

	if (bthrot->cur_state == cur_state)
		goto out;

	if (bthrot->cur_state == 0 && cur_state)
		bthrot->throttle_count++;

	bthrot->cur_state = cur_state;

	/* Pick the min freq. for each cap clk based on the cdev's cur_state */
	for (i = 0; i < num_cap_clks; i++) {
		unsigned long cap_rate = NO_CAP;
		list_for_each_entry(bthrot, &bthrot_list, node) {
			unsigned long cur_cap;

			if (!bthrot->cur_state)
				continue;

			cur_cap = THROT_VAL(bthrot->throt_tab,
						bthrot->cur_state - 1, i);
			if (cur_cap < cap_rate)
				cap_rate = cur_cap;
		}
		tegra_throttle_set_cap_clk(cap_rate, i);
	}

out:
	mutex_unlock(&bthrot_list_lock);

	return ret;
}

static struct thermal_cooling_device_ops tegra_throttle_cooling_ops = {
	.get_max_state = tegra_throttle_get_max_state,
	.get_cur_state = tegra_throttle_get_cur_state,
	.set_cur_state = tegra_throttle_set_cur_state,
};

#ifdef CONFIG_DEBUG_FS
static int table_show(struct seq_file *s, void *data)
{
	struct balanced_throttle *bthrot = s->private;
	int i, j;
	int rows = bthrot->throt_tab_size / num_cap_clks;

	for (i = 0; i < rows; i++) {
		seq_printf(s, "%s[%d] =", i < 10 ? " " : "", i);
		for (j = 0; j < num_cap_clks; j++) {
			unsigned long val = THROT_VAL(bthrot->throt_tab, i, j);
			if (val == NO_CAP || val > 9000000ULL)
				seq_puts(s, "  NO CAP");
			else
				seq_printf(s, " %7lu", val);
		}
		seq_puts(s, "\n");
	}

	return 0;
}

static int table_open(struct inode *inode, struct file *file)
{
	return single_open(file, table_show, inode->i_private);
}

static const struct file_operations table_fops = {
	.open		= table_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int cap_freqs_get(void *data, u64 *val)
{
	*val = *((unsigned long *)data);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(cap_freqs_fops, cap_freqs_get, NULL, "%llu\n");

static struct dentry *throttle_debugfs_root;

static int tegra_throttle_init_debugfs(void)
{
	int i;
	char buf[50];
	struct dentry *rv;
	struct balanced_throttle *bthrot;

	throttle_debugfs_root = debugfs_create_dir("tegra_throttle", NULL);
	if (IS_ERR_OR_NULL(throttle_debugfs_root))
		pr_err("%s: debugfs_create_dir 'tegra_throttle' FAILED.\n",
			__func__);

	list_for_each_entry(bthrot, &bthrot_list, node) {
		rv = debugfs_create_file(bthrot->cdev_type, 0644,
					 throttle_debugfs_root,
					 bthrot, &table_fops);
		if (IS_ERR_OR_NULL(rv))
			return -ENODEV;
	}

	for (i = 0; i < num_cap_clks; i++) {
		snprintf(buf, sizeof(buf), "cap_%s", CAP_TBL_CAP_NAME(i));
		rv = debugfs_create_file(buf, 0644, throttle_debugfs_root,
					&CAP_TBL_CAP_FREQ(i), &cap_freqs_fops);
		if (IS_ERR_OR_NULL(rv))
			return -ENODEV;
	}

	return 0;
}

static void tegra_throttle_exit_debugfs(void)
{
	debugfs_remove_recursive(throttle_debugfs_root);
}
#else /* CONFIG_DEBUG_FS */
static int tegra_throttle_init_debugfs(void)
{
	return 0;
}
static void tegra_throttle_exit_debugfs(void) {}
#endif /* end CONFIG_DEBUG_FS */


static int balanced_throttle_register(struct balanced_throttle *bthrot)
{
	bthrot->cdev = thermal_cooling_device_register(
						bthrot->cdev_type,
						bthrot,
						&tegra_throttle_cooling_ops);
	if (IS_ERR_OR_NULL(bthrot->cdev)) {
		bthrot->cdev = NULL;
		return -ENODEV;
	}

	return 0;
}

static int parse_table(struct device *dev, struct device_node *np,
			struct balanced_throttle *bthrot)
{
	u32 len;
	int ret = 0;

	if (!of_get_property(np, "throttle_table", &len)) {
		dev_err(dev, "%s: No throttle_table?\n", np->full_name);
		ret = -EINVAL;
		goto err;
	}

	len = len / sizeof(u32);

	if ((len % num_cap_clks) != 0) {
		dev_err(dev, "%s: Invald throttle table length:%d clks:%d\n",
			np->full_name, len, num_cap_clks);
		goto err;
	}

	bthrot->throt_tab = devm_kzalloc(dev, sizeof(u32) * len,
					      GFP_KERNEL);
	if (IS_ERR_OR_NULL(bthrot->throt_tab)) {
		ret = -ENOMEM;
		goto err;
	}

	ret = of_property_read_u32_array(np, "throttle_table",
					 bthrot->throt_tab, len);
	if (ret) {
		dev_err(dev, "malformed table %s : entries:%d; ret: %d\n",
			np->full_name, len, ret);
		goto err;
	}
	bthrot->throt_tab_size = len;

err:
	return ret;
}

static int parse_throttle_dt_data(struct device *dev)
{
	size_t size;
	int i, ret = 0;
	const char *str;
	struct device_node *child;
	struct balanced_throttle *pdata = NULL;
	struct device_node *np = dev->of_node;

	num_cap_clks = of_property_count_strings(np, "clock-names");
	if (num_cap_clks <= 0) {
		pr_err("%s: Invalid clock-names property\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	pr_info("%s: Num cap clks = %d\n", __func__, num_cap_clks);

	size = sizeof(*cap_freqs_table) * num_cap_clks;
	cap_freqs_table = devm_kzalloc(dev, size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(cap_freqs_table)) {
		ret = -ENOMEM;
		goto err;
	}

	/* Populate cap clks used for all balanced throttling cdevs */
	for (i = 0; i < num_cap_clks; i++) {

		of_property_read_string_index(np, "clock-names", i,
						 &CAP_TBL_CAP_NAME(i));
		CAP_TBL_CAP_CLK(i) = NULL;
		CAP_TBL_CAP_FREQ(i) = NO_CAP;
		CAP_TBL_CAP_TYPE(i) = THROT_DEFAULT;

		if (!strcmp("mcpu", CAP_TBL_CAP_NAME(i)))
			CAP_TBL_CAP_TYPE(i) = THROT_MCPU;
		else if (!strcmp("bcpu", CAP_TBL_CAP_NAME(i)))
			CAP_TBL_CAP_TYPE(i) = THROT_BCPU;
		else if (!strcmp("emc", CAP_TBL_CAP_NAME(i)))
			CAP_TBL_CAP_TYPE(i) = THROT_EMC;
		else if (!strcmp("gpu", CAP_TBL_CAP_NAME(i))) {
			CAP_TBL_CAP_TYPE(i) = THROT_GPU;
			pm_qos_add_request(&gpu_cap, PM_QOS_GPU_FREQ_MAX,
					   PM_QOS_GPU_FREQ_MAX_DEFAULT_VALUE);
		}

		if (CAP_TBL_CAP_TYPE(i) == THROT_DEFAULT) {
			CAP_TBL_CAP_CLK(i) = of_clk_get(np, i);
			if (IS_ERR(CAP_TBL_CAP_CLK(i))) {
				pr_err("%s: of_clk_get failed %s idx=%d|%ld\n",
						__func__, np->full_name, i,
						PTR_ERR(CAP_TBL_CAP_CLK(i)));
				ret = -EINVAL;
				goto err;
			}
		}
		pr_info("%s: clk=%s type=%d\n", __func__, CAP_TBL_CAP_NAME(i),
			CAP_TBL_CAP_TYPE(i));
	}

	for_each_child_of_node(np, child) {
		if (!of_device_is_available(child))
			continue;

		pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
		if (IS_ERR_OR_NULL(pdata)) {
			ret = -ENOMEM;
			goto err;
		}

		if (of_property_read_string(child, "cdev-type", &str) == 0)
			pdata->cdev_type = (char *)str;

		ret = parse_table(dev, child, pdata);
		if  (ret)
			goto err;

		mutex_lock(&bthrot_list_lock);
		list_add(&pdata->node, &bthrot_list);
		mutex_unlock(&bthrot_list_lock);
	}

	if (!pdata) {
		dev_err(dev, "No cdevs available\n");
		ret = -EINVAL;
		goto err;
	}

err:
	return ret;
}

static void balanced_throttle_unregister(void)
{
	struct balanced_throttle *bthrot;
	int i;

	list_for_each_entry(bthrot, &bthrot_list, node) {
		if (!bthrot->cdev)
			continue;
		thermal_cooling_device_unregister(bthrot->cdev);
	}

	for (i = 0; i < num_cap_clks; i++)
		tegra_throttle_set_cap_clk(NO_CAP, i);
}

static int tegra_throttle_probe(struct platform_device *pdev)
{
	struct balanced_throttle *bthrot;
	int cpu, ret, num_cdevs = 0;

	ret = parse_throttle_dt_data(&pdev->dev);

	if (ret) {
		dev_err(&pdev->dev, "Platform data parse failed.\n");
		return ret;
	}

	for_each_possible_cpu(cpu)
		per_cpu(max_cpu_rate, cpu) = ULONG_MAX;

	cpufreq_register_notifier(&tegra_throttle_cpufreq_nb,
					CPUFREQ_POLICY_NOTIFIER);

	emc_throt_handle = tegra_bwmgr_register(TEGRA_BWMGR_CLIENT_THERMAL_CAP);
	if (IS_ERR_OR_NULL(emc_throt_handle))
		pr_err("%s: No valid bwmgr handle for emc\n", __func__);

	list_for_each_entry(bthrot, &bthrot_list, node) {
		ret = balanced_throttle_register(bthrot);
		if (ret) {
			balanced_throttle_unregister();
			dev_err(&pdev->dev,
				"balanced_throttle_register FAILED.\n");
			return ret;
		}
		num_cdevs++;
	}

	tegra_throttle_init_debugfs();

	pr_info("%s: probe successful. #cdevs=%d\n", __func__, num_cdevs);

	return 0;
}

static int tegra_throttle_remove(struct platform_device *pdev)
{
	cpufreq_unregister_notifier(&tegra_throttle_cpufreq_nb,
					CPUFREQ_POLICY_NOTIFIER);

	mutex_lock(&bthrot_list_lock);
	balanced_throttle_unregister();
	mutex_unlock(&bthrot_list_lock);

	tegra_bwmgr_unregister(emc_throt_handle);

	tegra_throttle_exit_debugfs();

	return 0;
}

static struct of_device_id tegra_throttle_of_match[] = {
	{ .compatible = "nvidia,tegra18x-balanced-throttle", },
	{ },
};

static struct platform_driver tegra_throttle_driver = {
	.driver = {
		.name = "tegra18-throttle",
		.owner = THIS_MODULE,
		.of_match_table = tegra_throttle_of_match,
	},
	.probe = tegra_throttle_probe,
	.remove = tegra_throttle_remove,
};

module_platform_driver(tegra_throttle_driver);

MODULE_DESCRIPTION("Tegra Balanced Throttle Driver for T18x platforms");
MODULE_AUTHOR("NVIDIA");
MODULE_LICENSE("GPL");
