/*
 * drivers/platform/tegra/tegra_throttle.c
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/thermal.h>
#include <linux/tegra_throttle.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <linux/platform/tegra/clock.h>
#include <linux/platform/tegra/cpu-tegra.h>

/* cpu_throttle_lock is tegra_cpu_lock from cpu-tegra.c */
static DEFINE_MUTEX(bthrot_list_lock);
static LIST_HEAD(bthrot_list);
static LIST_HEAD(bthrot_cdev_list);
static int num_throt;
static struct cpufreq_frequency_table *cpu_freq_table;
static unsigned long cpu_throttle_lowest_speed;
static unsigned long cpu_cap_freq;

static struct {
	const char *cap_name;
	struct clk *cap_clk;
	unsigned long cap_freq;
} cap_freqs_table[] = {
#if defined(CONFIG_ARCH_TEGRA_12x_SOC) || defined(CONFIG_ARCH_TEGRA_21x_SOC)
	{ .cap_name = "cap.throttle.gbus" },
#endif
#ifdef CONFIG_TEGRA_GPU_DVFS
	{ .cap_name = "cap.throttle.c2bus" },
	{ .cap_name = "cap.throttle.c3bus" },
#else
	{ .cap_name = "cap.throttle.cbus" },
#endif
	{ .cap_name = "cap.throttle.sclk" },
	{ .cap_name = "cap.throttle.emc" },
};

static bool tegra_throttle_init_failed;
static const int NUM_OF_CAP_FREQS_DT = 6; /* fixed at 6 by DT format */

struct throttle_table {
	unsigned long cap_freqs[NUM_OF_CAP_FREQS];
};

struct balanced_throttle {
	struct thermal_cooling_device *cdev;
	struct list_head node;
	unsigned long cur_state;
	int throttle_count;
	int throt_tab_size;
	struct throttle_table *throt_tab;
};

struct tegra_throttle_data {
	char *cdev_type;
	struct throttle_table *throt_tab;
	u32 num_states;
	struct list_head list;
};

#define CAP_TBL_CAP_NAME(index)	(cap_freqs_table[index].cap_name)
#define CAP_TBL_CAP_CLK(index)	(cap_freqs_table[index].cap_clk)
#define CAP_TBL_CAP_FREQ(index)	(cap_freqs_table[index].cap_freq)

#ifndef CONFIG_TEGRA_THERMAL_THROTTLE_EXACT_FREQ
static unsigned long clip_to_table(unsigned long cpu_freq)
{
	int i;

	if (IS_ERR_OR_NULL(cpu_freq_table))
		return -EINVAL;

	for (i = 0; cpu_freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (cpu_freq_table[i].frequency > cpu_freq)
			break;
	}
	i = (i == 0) ? 0 : i-1;
	return cpu_freq_table[i].frequency;
}
#else
static unsigned long clip_to_table(unsigned long cpu_freq)
{
	return cpu_freq;
}
#endif /* CONFIG_TEGRA_THERMAL_THROTTLE_EXACT_FREQ */

unsigned long tegra_throttle_governor_speed(unsigned long requested_speed)
{
	unsigned long ret;

	if (cpu_cap_freq == NO_CAP ||
			cpu_cap_freq == 0)
		return requested_speed;
	ret = min(requested_speed, cpu_cap_freq);

	if (ret != requested_speed)
		pr_debug("%s: Thermal throttling %lu kHz down to %lu kHz\n",
				__func__, requested_speed, ret);
	return ret;
}

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

	*max_state = bthrot->throt_tab_size;

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

static void tegra_throttle_set_cap_clk(struct throttle_table *throt_tab,
					int cap_clk_index)
{
	unsigned long cap_rate, clk_rate;

	if (tegra_throttle_init_failed)
		return;

	cap_rate = throt_tab->cap_freqs[cap_clk_index];

	if (cap_rate == NO_CAP)
		clk_rate = clk_get_max_rate(CAP_TBL_CAP_CLK(cap_clk_index-1));
	else
		clk_rate = cap_rate * 1000UL;

	if (CAP_TBL_CAP_FREQ(cap_clk_index-1) != clk_rate) {
		clk_set_rate(CAP_TBL_CAP_CLK(cap_clk_index-1), clk_rate);
		CAP_TBL_CAP_FREQ(cap_clk_index-1) = clk_rate;
	}
}

static void
tegra_throttle_cap_freqs_update(struct throttle_table *throt_tab,
				int direction)
{
	int i;
	int num_of_cap_clocks = ARRAY_SIZE(cap_freqs_table);

	if (direction == 1) { /* performance up : throttle less */
		for (i = num_of_cap_clocks; i > 0; i--)
			tegra_throttle_set_cap_clk(throt_tab, i);
	} else { /* performance down : throotle more */
		for (i = 1; i <= num_of_cap_clocks; i++)
			tegra_throttle_set_cap_clk(throt_tab, i);
	}
}

static int
tegra_throttle_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long cur_state)
{
	struct balanced_throttle *bthrot = cdev->devdata;
	int direction;
	int i;
	int num_of_cap_clocks = ARRAY_SIZE(cap_freqs_table);
	unsigned long bthrot_speed;
	struct throttle_table *throt_entry;
	struct throttle_table cur_throt_freq;

	if (cpu_freq_table == NULL)
		return 0;

	if (bthrot->cur_state == cur_state)
		return 0;

	if (bthrot->cur_state == 0 && cur_state)
		bthrot->throttle_count++;

	direction = bthrot->cur_state >= cur_state;
	bthrot->cur_state = cur_state;

	for (i = 0; i <= num_of_cap_clocks; i++)
		cur_throt_freq.cap_freqs[i] = NO_CAP;

	mutex_lock(&bthrot_list_lock);
	list_for_each_entry(bthrot, &bthrot_list, node) {
		if (bthrot->cur_state) {
			throt_entry = &bthrot->throt_tab[bthrot->cur_state-1];
			for (i = 0; i <= num_of_cap_clocks; i++) {
				cur_throt_freq.cap_freqs[i] = min(
						cur_throt_freq.cap_freqs[i],
						throt_entry->cap_freqs[i]);
			}
		}
	}

	tegra_throttle_cap_freqs_update(&cur_throt_freq, direction);

	bthrot_speed = cur_throt_freq.cap_freqs[0];
	if (bthrot_speed == CPU_THROT_LOW)
		bthrot_speed = cpu_throttle_lowest_speed;
	else
		bthrot_speed = clip_to_table(bthrot_speed);

	cpu_cap_freq = bthrot_speed;
	tegra_cpu_set_speed_cap(NULL);
	mutex_unlock(&bthrot_list_lock);

	return 0;
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

	for (i = 0; i < bthrot->throt_tab_size; i++) {
		seq_printf(s, "%s[%d] =", i < 10 ? " " : "", i);
		for (j = 0; j <= ARRAY_SIZE(cap_freqs_table); j++)
			if ((bthrot->throt_tab[i].cap_freqs[j] == NO_CAP) ||
			    (bthrot->throt_tab[i].cap_freqs[j] > 9000000ULL))
				seq_puts(s, "  NO CAP");
			else
				seq_printf(s, " %7lu",
					bthrot->throt_tab[i].cap_freqs[j]);
		seq_puts(s, "\n");
	}

	return 0;
}

static int table_open(struct inode *inode, struct file *file)
{
	return single_open(file, table_show, inode->i_private);
}

static ssize_t table_write(struct file *file,
	const char __user *userbuf, size_t count, loff_t *ppos)
{
	struct balanced_throttle *bthrot =
			((struct seq_file *)(file->private_data))->private;
	char buf[80], temp_buf[10], *cur_pos;
	int table_idx, i;
	unsigned long cap_rate;

	if (sizeof(buf) <= count)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	/* terminate buffer and trim - white spaces may be appended
	 *  at the end when invoked from shell command line */
	buf[count] = '\0';
	strim(buf);
	cur_pos = buf;

	/* get table index */
	if (sscanf(cur_pos, "[%d] = ", &table_idx) != 1)
		return -EINVAL;
	sscanf(cur_pos, "[%s] = ", temp_buf);
	cur_pos += strlen(temp_buf) + 4;
	if ((table_idx < 0) || (table_idx >= bthrot->throt_tab_size))
		return -EINVAL;

	/* CPU FREQ and DVFS FREQS == DVFS FREQS + 1(cpu) */
	for (i = 0; i < ARRAY_SIZE(cap_freqs_table) + 1; i++) {
		if (sscanf(cur_pos, "%lu", &cap_rate) != 1)
			return -EINVAL;
		sscanf(cur_pos, "%s", temp_buf);
		cur_pos += strlen(temp_buf) + 1;

		bthrot->throt_tab[table_idx].cap_freqs[i] = cap_rate;
	}

	return count;
}

static const struct file_operations table_fops = {
	.open		= table_open,
	.read		= seq_read,
	.write		= table_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct dentry *throttle_debugfs_root;
#endif /* CONFIG_DEBUG_FS */


static struct thermal_cooling_device *balanced_throttle_register(
		struct balanced_throttle *bthrot,
		char *type)
{
#ifdef CONFIG_DEBUG_FS
	char name[32];
	struct dentry *rv;
#endif

	bthrot->cdev = thermal_cooling_device_register(
						type,
						bthrot,
						&tegra_throttle_cooling_ops);
	if (IS_ERR(bthrot->cdev)) {
		bthrot->cdev = NULL;
		return ERR_PTR(-ENODEV);
	}

	mutex_lock(&bthrot_list_lock);
	num_throt++;
	list_add(&bthrot->node, &bthrot_list);
	mutex_unlock(&bthrot_list_lock);

#ifdef CONFIG_DEBUG_FS
	if (throttle_debugfs_root) {
		sprintf(name, "throttle_table%d", num_throt);
		rv = debugfs_create_file(name, 0644, throttle_debugfs_root,
					 bthrot, &table_fops);
		if (IS_ERR_OR_NULL(rv))
			return ERR_PTR(-ENODEV);
		rv = debugfs_create_symlink(type, throttle_debugfs_root, name);
		if (IS_ERR_OR_NULL(rv))
			return ERR_PTR(-ENODEV);
	}
#endif

	return bthrot->cdev;
}

static int tegra_throttle_init(void)
{
	int i;
	struct clk *c;
	struct tegra_cpufreq_table_data *table_data =
		tegra_cpufreq_table_get();

	if (IS_ERR_OR_NULL(table_data))
		return -EINVAL;

	cpu_freq_table = table_data->freq_table;
	cpu_throttle_lowest_speed =
		cpu_freq_table[table_data->throttle_lowest_index].frequency;

#ifdef CONFIG_DEBUG_FS
	throttle_debugfs_root = debugfs_create_dir("tegra_throttle", NULL);
	if (IS_ERR_OR_NULL(throttle_debugfs_root))
		pr_err("%s: debugfs_create_dir 'tegra_throttle' FAILED.\n",
			__func__);
#endif

	for (i = 0; i < ARRAY_SIZE(cap_freqs_table); i++) {
		c = tegra_get_clock_by_name(CAP_TBL_CAP_NAME(i));
		if (!c) {
			pr_err("tegra_throttle: cannot get clock %s\n",
				CAP_TBL_CAP_NAME(i));
			tegra_throttle_init_failed = true;
			continue;
		}

		CAP_TBL_CAP_CLK(i) = c;
		CAP_TBL_CAP_FREQ(i) = clk_get_max_rate(c);
	}
	pr_info("tegra_throttle : init %s\n",
		tegra_throttle_init_failed ? "FAILED" : "passed");
	return 0;
}

static void tegra_throttle_exit(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(throttle_debugfs_root);
#endif
}


static struct throttle_table
	*throttle_parse_dt_thrt_tbl(struct device *dev,
		struct device_node *np, int num_states)
{
	int i, j;
	u32 *val;
	int ret = 0;
	int count = 0;
	struct throttle_table *throt_tab;

#if !(defined(CONFIG_ARCH_TEGRA_12x_SOC) || defined(CONFIG_ARCH_TEGRA_21x_SOC))
	dev_err(dev,
		"ERROR: throttle_table DT not supported for this ARCH\n");
	return NULL;
#endif
	pr_debug("%s: Num CAP Freqs DT %d.\n", __func__, NUM_OF_CAP_FREQS_DT);
	pr_debug("%s: Num CAP Freqs    %d.\n", __func__, NUM_OF_CAP_FREQS);

	throt_tab = devm_kzalloc(dev,
			sizeof(struct throttle_table)*num_states, GFP_KERNEL);
	if (IS_ERR_OR_NULL(throt_tab))
		return NULL;

	val = kzalloc(sizeof(u32)*num_states*NUM_OF_CAP_FREQS_DT, GFP_KERNEL);
	if (IS_ERR_OR_NULL(val))
		return NULL;

	ret = of_property_read_u32_array(np, "throttle_table",
					val, num_states*NUM_OF_CAP_FREQS_DT);
	if (ret) {
		dev_err(dev,
			"malformed throttle_table property in %s : no. of entries:%d; return value: %d\n",
			np->full_name, num_states*NUM_OF_CAP_FREQS_DT, ret);
		return NULL;
	}

	for (i = 0; i < num_states; ++i) {
		for (j = 0; j < NUM_OF_CAP_FREQS; ++j)
			throt_tab[i].cap_freqs[j] = val[count++];
		for (; j < NUM_OF_CAP_FREQS_DT; j++)
			count++; /* eat the rest of DT entries */
	}

	kfree(val);
	return throt_tab;
}

static struct tegra_throttle_data
		*parse_throttle_dt_data(struct device *dev)
{
	u32 val;
	const char *str;
	struct device_node *child;
	struct tegra_throttle_data *pdata = NULL;
	struct device_node *np = dev->of_node;

	for_each_child_of_node(np, child) {
		/* Check whether child is enabled or not */
		if (!of_device_is_available(child))
			continue;

		pdata = devm_kzalloc(dev,
				sizeof(struct tegra_throttle_data), GFP_KERNEL);
		if (IS_ERR_OR_NULL(pdata))
			return NULL;

		if (of_property_read_string(child, "cdev-type", &str) == 0)
			pdata->cdev_type = (char *)str;

		if (of_property_read_u32(child, "num_states", &val) == 0)
			pdata->num_states = val;

		of_get_property(np, "throttle_table", &val);
		if (pdata->num_states != val) {
			pr_info("%s: 'throttle_table' size (%d) does not match 'num-states' (%d) in DT.\n",
				__func__, val, pdata->num_states);
			return NULL;
		}

		pdata->throt_tab =
			throttle_parse_dt_thrt_tbl(dev, child, val);
		if  (!pdata->throt_tab)
			return NULL;

		list_add(&pdata->list, &bthrot_cdev_list);

	}

	return pdata;
}

static int get_start_index(struct throttle_table *throt_tab,
			u32 num_states, unsigned long cpu_g_maxf)
{
	int si;

	for (si = 0; si < num_states; ++si)
		if (throt_tab[si].cap_freqs[0] < cpu_g_maxf)
			return si;

	return -EINVAL;
}

static int tegra_throttle_probe(struct platform_device *pdev)
{
	struct clk *clk_cpu_g;
	int ret, start_index;
	unsigned long cpu_g_maxf;
	struct tegra_throttle_data *pdata;
	struct balanced_throttle *bthrot;
	struct thermal_cooling_device *cdev;

	ret = tegra_throttle_init();
	if (ret) {
		dev_err(&pdev->dev, "tegra throttle init failed.\n");
		return -ENOMEM;
	}

	pdata = parse_throttle_dt_data(&pdev->dev);

	if (!pdata) {
		dev_err(&pdev->dev, "No Platform data.\n");
		return -EIO;
	}

	clk_cpu_g = tegra_get_clock_by_name("cpu_g");
	cpu_g_maxf = clk_get_max_rate(clk_cpu_g);

	list_for_each_entry(pdata, &bthrot_cdev_list, list) {
		bthrot = devm_kzalloc(&pdev->dev,
			sizeof(struct balanced_throttle), GFP_KERNEL);
		if (IS_ERR(bthrot))
			return -ENOMEM;

		start_index = get_start_index(pdata->throt_tab,
					      pdata->num_states,
					      cpu_g_maxf / 1000);
		if (start_index < 0) {
			dev_err(&pdev->dev, "tegra throttle init FAILED.\n");
			return -EINVAL;
		}

		bthrot->throt_tab_size = pdata->num_states - start_index;
		bthrot->throt_tab      = pdata->throt_tab + start_index;

		cdev = balanced_throttle_register(bthrot, pdata->cdev_type);
		if (IS_ERR_OR_NULL(cdev))
			dev_err(&pdev->dev,
				"balanced_throttle_register FAILED.\n");
	}

	return 0;
}

static int tegra_throttle_remove(struct platform_device *pdev)
{
	struct balanced_throttle *bthrot;

	mutex_lock(&bthrot_list_lock);
	list_for_each_entry(bthrot, &bthrot_list, node) {
		thermal_cooling_device_unregister(bthrot->cdev);
	}
	mutex_unlock(&bthrot_list_lock);

	tegra_throttle_exit();

	return 0;
}

static struct of_device_id tegra_throttle_of_match[] = {
	{ .compatible = "nvidia,balanced-throttle", },
	{ },
};

static struct platform_driver tegra_throttle_driver = {
	.driver = {
		.name = "tegra-throttle",
		.owner = THIS_MODULE,
		.of_match_table = tegra_throttle_of_match,
	},
	.probe = tegra_throttle_probe,
	.remove = tegra_throttle_remove,
};

module_platform_driver(tegra_throttle_driver);

MODULE_DESCRIPTION("Tegra Balanced Throttle Driver");
MODULE_AUTHOR("NVIDIA");
MODULE_LICENSE("GPL");
