/*
 * Copyright (C) 2017 Intel, Inc.
 * Copyright (C) 2016 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/trusty/smcall.h>
#include <linux/trusty/smwall.h>
#include <linux/trusty/trusty.h>


void *trusty_wall_base(struct device *dev)
{
	struct trusty_wall_dev_state *s;

	s = platform_get_drvdata(to_platform_device(dev));

	if (NULL == s)
		return NULL;

	return s->va;
}
EXPORT_SYMBOL(trusty_wall_base);

void *trusty_wall_per_cpu_item_ptr(struct device *dev, unsigned int cpu,
		u32 item_id, size_t exp_sz)
{
	uint i;
	struct sm_wall_toc *toc;
	struct sm_wall_toc_item *item;
	struct trusty_wall_dev_state *s;

	s = platform_get_drvdata(to_platform_device(dev));

	if (!s->va) {
		dev_dbg(s->dev, "No smwall buffer is set\n");
		return NULL;
	}

	toc = (struct sm_wall_toc *)s->va;
	if (toc->version != SM_WALL_TOC_VER) {
		dev_err(s->dev, "Unexpected toc version: %d\n", toc->version);
		return NULL;
	}

	if (cpu >= toc->cpu_num) {
		dev_err(s->dev, "Unsupported cpu (%d) requested\n", cpu);
		return NULL;
	}

	item = (struct sm_wall_toc_item *)((uintptr_t)toc +
			toc->per_cpu_toc_offset);
	for (i = 0; i < toc->per_cpu_num_items; i++, item++) {
		if (item->id != item_id)
			continue;

		if (item->size != exp_sz) {
			dev_err(s->dev,
					"Size mismatch (%zd vs. %zd) for item_id %d\n",
					(size_t)item->size, exp_sz, item_id);
			return NULL;
		}

		return s->va + toc->per_cpu_base_offset +
			cpu * toc->per_cpu_region_size + item->offset;
	}
	return NULL;
}
EXPORT_SYMBOL(trusty_wall_per_cpu_item_ptr);

static int trusty_wall_setup(struct trusty_wall_dev_state *s)
{
	int ret;
	void *va;
	size_t sz;

	/* check if wall feature is supported by Trusted OS */
	ret = trusty_fast_call32(s->trusty_dev, SMC_FC_GET_WALL_SIZE, 0, 0, 0);
	if (ret == SM_ERR_UNDEFINED_SMC || ret == SM_ERR_NOT_SUPPORTED) {
		/* wall is not supported */
		dev_notice(s->dev, "smwall: is not supported by Trusted OS\n");
		return 0;
	} else if (ret < 0) {
		dev_err(s->dev, "smwall: failed (%d) to query buffer size\n",
				ret);
		return ret;
	} else if (ret == 0) {
		dev_notice(s->dev, "smwall: zero-sized buffer requested\n");
		return 0;
	}
	sz = (size_t)ret;

	/* allocate memory for shared buffer */
	va = alloc_pages_exact(sz, GFP_KERNEL | __GFP_ZERO);
	if (!va) {
		dev_err(s->dev, "smwall: failed to allocate buffer\n");
		return -ENOMEM;
	}

	/* call into Trusted OS to setup wall */
	ret = trusty_call32_mem_buf(s->trusty_dev, SMC_SC_SETUP_WALL,
			virt_to_page(va), sz, PAGE_KERNEL);
	if (ret < 0) {
		dev_err(s->dev, "smwall: TEE returned (%d)\n", ret);
		free_pages_exact(va, sz);
		return -ENODEV;
	}

	dev_info(s->dev, "smwall: initialized %zu bytes\n", sz);

	s->va = va;
	s->sz = sz;

	return 0;
}

static void trusty_wall_destroy(struct trusty_wall_dev_state *s)
{
	int ret;

	ret = trusty_std_call32(s->trusty_dev, SMC_SC_DESTROY_WALL, 0, 0, 0);
	if (ret) {
		/**
		 * It should never happen, but if it happens, it is
		 * unsafe to free buffer so we have to leak memory
		 */
		dev_err(s->dev, "Failed (%d) to destroy the wall buffer\n",
			ret);
	} else {
		free_pages_exact(s->va, s->sz);
	}
}

static int trusty_wall_probe(struct platform_device *pdev)
{
	int ret;
	struct trusty_wall_dev_state *s;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s)
		return -ENOMEM;

	s->dev = &pdev->dev;
	s->trusty_dev = s->dev->parent;
	platform_set_drvdata(pdev, s);

	ret = trusty_wall_setup(s);
	if (ret < 0) {
		dev_warn(s->dev, "Failed (%d) to setup the wall\n", ret);
		kfree(s);
		return ret;
	}

	return 0;
}

static int trusty_wall_remove(struct platform_device *pdev)
{
	struct trusty_wall_dev_state *s = platform_get_drvdata(pdev);

	trusty_wall_destroy(s);

	return 0;
}

static const struct of_device_id trusty_wall_of_match[] = {
	{ .compatible = "android, trusty-wall-v1", },
	{},
};

MODULE_DEVICE_TABLE(of, trusty_wall_of_match);

static struct platform_driver trusty_wall_driver = {
	.probe = trusty_wall_probe,
	.remove = trusty_wall_remove,
	.driver = {
		.name = "trusty-wall",
		.owner = THIS_MODULE,
		.of_match_table = trusty_wall_of_match,
	},
};

module_platform_driver(trusty_wall_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Trusty smwall driver");
