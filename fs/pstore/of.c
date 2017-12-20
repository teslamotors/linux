/* fs/pstore/of.c
 *
 * Copyright 2017 Codethink Ltd.
 *	Ben Dooks <ben.dooks@codethink.co.uk>
 *
 * Handle OF reserved-memory nodes for pstore
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>

static int pstore_of_init(struct reserved_mem *rmem, struct device *dev)
{
	dev_info(dev, "%s(%p)\n", __func__, rmem);
	return 0;
}

static void pstore_of_release(struct reserved_mem *rmem, struct device *dev)
{
	dev_info(dev, "%s(%p)\n", __func__, rmem);
}

static const struct reserved_mem_ops pstore_of_ops = {
	.device_init		= pstore_of_init,
	.device_release		= pstore_of_release,
};

static struct reserved_mem *pstore_of_rmem;

static int pstore_of_add(void)
{
	struct device_node *node;

	if (!pstore_of_rmem)
		return 0;

	node = of_find_node_by_path("/reserved-memory");
	if (!node) {
		pr_err("%s: did not find reserved memory node\n", __func__);
		return -ENOENT;
	}

	node = of_find_compatible_node(node, NULL, "ramoops");
	if (node) {
		pr_info("creating platdevice for %s\n",
			of_node_full_name(node));
		of_platform_device_create(node, NULL, NULL);
	} else
		pr_err("%s: cannot find ramoops node?\n", __func__);

	return 0;
}

postcore_initcall(pstore_of_add);

static int __init pstore_of_setup(struct reserved_mem *rmem)
{
	/* rmem->base, rmem->size */
	pr_info("%s: pstore reserved-memory of_block", __func__);
	rmem->ops = &pstore_of_ops;
	pstore_of_rmem = rmem;

	return 0;
}

RESERVEDMEM_OF_DECLARE(pstore, "ramoops", pstore_of_setup);
