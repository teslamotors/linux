/*
 * system-pmic.c -- Core power off/reset functionality from system PMIC.
 *
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/psci.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/power/reset/system-pmic.h>

struct system_pmic_dev {
	struct device *pmic_dev;
	void *pmic_drv_data;
	bool allow_power_off;
	bool allow_power_reset;
	bool avoid_power_off_command;
	struct system_pmic_ops *ops;
	bool power_on_event[SYSTEM_PMIC_MAX_POWER_ON_EVENT];
	void *power_on_data[SYSTEM_PMIC_MAX_POWER_ON_EVENT];
};

static struct system_pmic_dev *system_pmic_dev;

void (*soc_specific_power_off)(void);
EXPORT_SYMBOL(soc_specific_power_off);

void (*system_pmic_post_power_off_handler)(void);

static void system_pmic_prepare_power_off(void)
{
	if (system_pmic_dev->ops->prepare_power_off)
		system_pmic_dev->ops->prepare_power_off(
			system_pmic_dev->pmic_drv_data);
}

static void system_pmic_power_reset(void)
{
	system_pmic_dev->ops->power_reset(system_pmic_dev->pmic_drv_data);
	dev_err(system_pmic_dev->pmic_dev,
		"System PMIC is not able to reset system\n");
	while (1);
}

static void system_pmic_power_off(void)
{
	int i;

	for (i = 0; i < SYSTEM_PMIC_MAX_POWER_ON_EVENT; ++i) {
		if (!system_pmic_dev->power_on_event[i])
			continue;
		if (!system_pmic_dev->ops->configure_power_on)
			break;
		system_pmic_dev->ops->configure_power_on(
			system_pmic_dev->pmic_drv_data, i,
			system_pmic_dev->power_on_data[i]);
	}
	system_pmic_dev->ops->power_off(system_pmic_dev->pmic_drv_data);
	if (soc_specific_power_off) {
		dev_err(system_pmic_dev->pmic_dev,
			"SoC specific power off sequence\n");
		soc_specific_power_off();
	}

	if (system_pmic_dev->avoid_power_off_command) {
		dev_info(system_pmic_dev->pmic_dev,
				"Avoid PMIC power off\n");
		if (system_pmic_post_power_off_handler)
			system_pmic_post_power_off_handler();
	}
	dev_err(system_pmic_dev->pmic_dev,
		"System PMIC is not able to power off system\n");
	while (1);
}

int system_pmic_set_power_on_event(enum system_pmic_power_on_event event,
	void *data)
{
	if (!system_pmic_dev) {
		pr_err("System PMIC is not initialized\n");
		return -EINVAL;
	}

	if (!system_pmic_dev->ops->configure_power_on) {
		pr_err("System PMIC does not support power on event\n");
		return -ENOTSUPP;
	}

	system_pmic_dev->power_on_event[event] = 1;
	system_pmic_dev->power_on_data[event] = data;
	return 0;
}
EXPORT_SYMBOL_GPL(system_pmic_set_power_on_event);

#ifdef CONFIG_DEBUG_FS
static struct dentry *debugfs_root;

static ssize_t system_pmic_show_poweroff_commands(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[50] = { 0, };
	ssize_t ret = 0;

	if (system_pmic_dev->avoid_power_off_command)
		ret = snprintf(buf, sizeof(buf),
				"Avoid power off commands Enabled\n");
	else
		ret = snprintf(buf, sizeof(buf),
				"Avoid power off commands disabled\n");

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static ssize_t system_pmic_store_poweroff_commands(struct file *file,
			const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[50] = { 0, };
	ssize_t buf_size;
	bool enabled;
	int ret;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if ((*buf == 'E') || (*buf == 'e'))
		enabled = true;
	else if ((*buf == 'D') || (*buf == 'd'))
		enabled = false;
	else
		return -EINVAL;

	if (enabled && !system_pmic_dev->avoid_power_off_command) {
		pm_power_off = NULL;
		pm_power_off = system_pmic_post_power_off_handler;
		system_pmic_dev->allow_power_off = false;
		system_pmic_dev->avoid_power_off_command = true;
		if (system_pmic_dev->ops->override_poweroff_config)
			system_pmic_dev->ops->override_poweroff_config(
				system_pmic_dev->pmic_drv_data, enabled);
		ret = snprintf(buf, sizeof(buf),
				"Avoid power off commands Enabled now\n");
	} else if (!enabled && system_pmic_dev->avoid_power_off_command) {
		pm_power_off = system_pmic_power_off;
		system_pmic_dev->allow_power_off = true;
		system_pmic_dev->avoid_power_off_command = false;
		if (system_pmic_dev->ops->override_poweroff_config)
			system_pmic_dev->ops->override_poweroff_config(
				system_pmic_dev->pmic_drv_data, enabled);
		ret = snprintf(buf, sizeof(buf),
				"Avoid power off commands disabled now\n");
	}

	return count;
}

static const struct file_operations system_pmic_debug_fops = {
	.open		= simple_open,
	.write		= system_pmic_store_poweroff_commands,
	.read		= system_pmic_show_poweroff_commands,
};

static int system_pmic_debugfs_init(void)
{
	debugfs_root = debugfs_create_dir("system_pmic", NULL);
	if (!debugfs_root)
		pr_warn("system_pmic: Failed to create debugfs directory\n");

	debugfs_create_file("avoid_system_pmic_poweroff", S_IRUGO | S_IWUSR,
			debugfs_root, system_pmic_dev, &system_pmic_debug_fops);
	return 0;
}
#else
static int system_pmic_debugfs_init(void)
{
	return 0;
}
#endif

struct system_pmic_dev *system_pmic_register(struct device *dev,
	struct system_pmic_ops *ops, struct system_pmic_config *config,
	void *drv_data)
{
	if (system_pmic_dev) {
		pr_err("System PMIC is already registerd\n");
		return ERR_PTR(-EBUSY);
	}

	system_pmic_dev = kzalloc(sizeof(*system_pmic_dev), GFP_KERNEL);
	if (!system_pmic_dev) {
		dev_err(dev, "Memory alloc for system_pmic_dev failed\n");
		return ERR_PTR(-ENOMEM);
	}

	system_pmic_dev->pmic_dev = dev;
	system_pmic_dev->ops = ops;
	system_pmic_dev->pmic_drv_data = drv_data;
	system_pmic_dev->allow_power_off = config->allow_power_off;
	system_pmic_dev->allow_power_reset = config->allow_power_reset;
	system_pmic_dev->avoid_power_off_command =
			config->avoid_power_off_command;

	if (system_pmic_dev->allow_power_off) {
		if (!ops->power_off)
			goto scrub;
		pm_power_off = system_pmic_power_off;
	}

	if (system_pmic_dev->allow_power_reset) {
		if (!ops->power_reset)
			goto scrub;
		pm_power_reset = system_pmic_power_reset;
	}

	psci_prepare_poweroff = system_pmic_prepare_power_off;

	system_pmic_debugfs_init();

	return system_pmic_dev;

scrub:
	dev_err(dev, "Illegal option\n");
	return ERR_PTR(-EINVAL);
}
EXPORT_SYMBOL_GPL(system_pmic_register);

void system_pmic_unregister(struct system_pmic_dev *pmic_dev)
{
	if (pmic_dev != system_pmic_dev) {
		pr_err("System PMIC can not unregister\n");
		return;
	}

	if (system_pmic_dev->allow_power_off)
		pm_power_off = NULL;

	if (system_pmic_dev->allow_power_reset)
		pm_power_reset = NULL;

	psci_prepare_poweroff = NULL;

	kfree(system_pmic_dev);
	system_pmic_dev = NULL;
}
EXPORT_SYMBOL_GPL(system_pmic_unregister);
