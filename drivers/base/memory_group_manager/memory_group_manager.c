/*
 *
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#include <linux/fs.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/module.h>

#include <linux/memory_group_manager.h>

struct dummy_struct_type {
	int a;
	int b;
};

static struct page *example_mgm_alloc_page(
	struct memory_group_manager_device *mgm_dev, int group_id,
	gfp_t gfp_mask, unsigned int order)
{
	WARN_ON(group_id < 0);
	WARN_ON(group_id >= MEMORY_GROUP_MANAGER_NR_GROUPS);

	return alloc_pages(gfp_mask, order);
}

static void example_mgm_free_page(
	struct memory_group_manager_device *mgm_dev, int group_id,
	struct page *page, unsigned int order)
{
	WARN_ON(group_id < 0);
	WARN_ON(group_id >= MEMORY_GROUP_MANAGER_NR_GROUPS);

	__free_pages(page, order);
}

static int memory_group_manager_probe(struct platform_device *pdev)
{
	struct memory_group_manager_device *mgm_dev;
	struct dummy_struct_type *dummy_data;

	mgm_dev = kzalloc(sizeof(*mgm_dev), GFP_KERNEL);
	if (!mgm_dev)
		return -ENOMEM;

	dummy_data = kzalloc(sizeof(*dummy_data), GFP_KERNEL);
	if (!dummy_data) {
		kfree(mgm_dev);
		return -ENOMEM;
	}

	mgm_dev->ops.mgm_alloc_page = example_mgm_alloc_page;
	mgm_dev->ops.mgm_free_page = example_mgm_free_page;
	mgm_dev->data = dummy_data;
	platform_set_drvdata(pdev, mgm_dev);
	dev_info(&pdev->dev, "Memory group manager probed successfully");

	return 0;
}

static int memory_group_manager_remove(struct platform_device *pdev)
{
	struct memory_group_manager_device *mgm_dev =
		platform_get_drvdata(pdev);
	struct dummy_struct_type *dummy_data = mgm_dev->data;

	kfree(dummy_data);
	kfree(mgm_dev);
	dev_info(&pdev->dev, "Memory group manager removed successfully");

	return 0;
}

static const struct of_device_id memory_group_manager_dt_ids[] = {
	{ .compatible = "arm,physical-memory-group-manager" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, memory_group_manager_dt_ids);

static struct platform_driver memory_group_manager_driver = {
	.probe = memory_group_manager_probe,
	.remove = memory_group_manager_remove,
	.driver = {
		.name = "physical-memory-group-manager",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(memory_group_manager_dt_ids),
	}
};

module_platform_driver(memory_group_manager_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ARM Ltd.");
MODULE_VERSION("1.0");
