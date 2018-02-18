/*
 * Copyright (c) 2015-2016 NVIDIA CORPORATION. All rights reserved.
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
#include <linux/module.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/tegra_ast.h>
#include <linux/io.h>

void __iomem *tegra_ioremap(struct device *dev, int index)
{
	struct resource mem;
	int err = of_address_to_resource(dev->of_node, index, &mem);
	if (err)
		return IOMEM_ERR_PTR(err);

	/* NOTE: assumes size is large enough for caller */
	return devm_ioremap_resource(dev, &mem);
}
EXPORT_SYMBOL(tegra_ioremap);

void __iomem *tegra_ioremap_byname(struct device *dev, const char *name)
{
	int index = of_property_match_string(dev->of_node, "reg-names", name);
	if (index < 0)
		return IOMEM_ERR_PTR(-ENOENT);
	return tegra_ioremap(dev, index);
}
EXPORT_SYMBOL(tegra_ioremap_byname);

#define TEGRA_APS_AST_STREAMID_CTL		0x20
#define TEGRA_APS_AST_REGION_0_SLAVE_BASE_LO	0x100
#define TEGRA_APS_AST_REGION_0_SLAVE_BASE_HI	0x104
#define TEGRA_APS_AST_REGION_0_MASK_LO		0x108
#define TEGRA_APS_AST_REGION_0_MASK_HI		0x10c
#define TEGRA_APS_AST_REGION_0_MASTER_BASE_LO	0x110
#define TEGRA_APS_AST_REGION_0_MASTER_BASE_HI	0x114
#define TEGRA_APS_AST_REGION_0_CONTROL		0x118

#define TEGRA_APS_AST_REGION_STRIDE		0x20

#define AST_MAX_REGION			7
#define AST_ADDR_MASK			0xfffff000
#define AST_ADDR_MASK64			(~0xfffULL)

/* TEGRA_APS_AST_REGION_<x>_CONTROL register fields */
#define AST_RGN_CTRL_VM_INDEX		15
#define AST_RGN_CTRL_NON_SECURE		0x8
#define AST_RGN_CTRL_SNOOP		0x4

#define AST_MAX_VMINDEX			15
#define AST_MAX_STREAMID		255
#define AST_STREAMID(id)		((id) << 8u)
#define AST_STREAMID_CTL_ENABLE		0x01

/* TEGRA_APS_AST_REGION_<x>_SLAVE_BASE_LO register fields */
#define AST_SLV_BASE_LO_ENABLE		1

struct tegra_ast
{
	void __iomem *bases[2];
	struct device dev;
};

static inline struct tegra_ast *to_tegra_ast(struct device *dev)
{
	return container_of(dev, struct tegra_ast, dev);
}

static void tegra_ast_dev_release(struct device *dev)
{
	kfree(to_tegra_ast(dev));
}

struct tegra_ast *tegra_ast_create(struct device *dev)
{
	struct tegra_ast *ast;
	int err, i;
	static const char * const names[ARRAY_SIZE(ast->bases)] = {
		"ast-cpu",
		"ast-dma",
	};

	ast = kzalloc(sizeof(*ast), GFP_KERNEL);
	if (unlikely(ast == NULL))
		return ERR_PTR(-ENOMEM);

	ast->dev.parent = dev;
	ast->dev.release = tegra_ast_dev_release;
	dev_set_name(&ast->dev, "%s:ast", dev_name(dev));
	device_initialize(&ast->dev);

	/* TODO: dedicated OF node for AST */
	for (i = 0; i < ARRAY_SIZE(ast->bases); i++) {
		ast->bases[i] = tegra_ioremap_byname(dev, names[i]);
		if (IS_ERR(ast->bases[i])) {
			if (i > 0 && PTR_ERR(ast->bases[i]) == -ENOENT) {
				ast->bases[i] = NULL;
				continue;
			}
			dev_err(dev, "cannot ioremap %s", names[i]);
			err = PTR_ERR(ast->bases[i]);
			goto error;
		}
	}

	err = device_add(&ast->dev);
	if (err)
		goto error;

	return ast;
error:
	put_device(&ast->dev);
	return ERR_PTR(err);
}
EXPORT_SYMBOL(tegra_ast_create);

void tegra_ast_destroy(struct tegra_ast *ast)
{
	if (!IS_ERR_OR_NULL(ast))
		device_unregister(&ast->dev);
}

struct tegra_ast_region {
	u8 ast_id;
	u8 stream_id;
	u8 vmids[2];
	u32 slave_base;
	size_t size;
	void *base;
	dma_addr_t dma;
	struct device dev;
};

static void tegra_ast_region_enable(struct tegra_ast_region *region)
{
	struct tegra_ast *ast = to_tegra_ast(region->dev.parent);
	u32 ast_sid = AST_STREAMID(region->stream_id);
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(ast->bases); i++) {
		u32 r;
		void __iomem *base = ast->bases[i];
		unsigned vmidx = region->vmids[i];

		if (base == NULL)
			continue;

		/*
		 * TODO: MB1 should set stream_id to one VM, and this would no
		 * longer be necessary.
		 */
		r = readl(base + TEGRA_APS_AST_STREAMID_CTL + (4 * vmidx));
		if ((r & 0x0000ff00) != ast_sid)
			writel(ast_sid | AST_STREAMID_CTL_ENABLE, base +
				TEGRA_APS_AST_STREAMID_CTL + (4 * vmidx));

		base += region->ast_id * TEGRA_APS_AST_REGION_STRIDE;

		writel(0, base + TEGRA_APS_AST_REGION_0_MASK_HI);
		writel((region->size - 1) & AST_ADDR_MASK,
			base + TEGRA_APS_AST_REGION_0_MASK_LO);

		writel(0, base + TEGRA_APS_AST_REGION_0_MASTER_BASE_HI);
		writel(region->dma & AST_ADDR_MASK,
			base + TEGRA_APS_AST_REGION_0_MASTER_BASE_LO);

		writel(AST_RGN_CTRL_NON_SECURE | AST_RGN_CTRL_SNOOP |
				(vmidx << AST_RGN_CTRL_VM_INDEX),
			base + TEGRA_APS_AST_REGION_0_CONTROL);

		writel(0, base + TEGRA_APS_AST_REGION_0_SLAVE_BASE_HI);
		writel((region->slave_base & AST_ADDR_MASK) |
			AST_SLV_BASE_LO_ENABLE,
			base + TEGRA_APS_AST_REGION_0_SLAVE_BASE_LO);
	}
}

static void tegra_ast_region_disable(struct tegra_ast_region *region)
{
	struct tegra_ast *ast = to_tegra_ast(region->dev.parent);
	u32 offset = region->ast_id * TEGRA_APS_AST_REGION_STRIDE;
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(ast->bases); i++) {
		void __iomem *base = ast->bases[i] + offset;

		if (base == NULL)
			continue;

		writel(0, base + TEGRA_APS_AST_REGION_0_SLAVE_BASE_LO);
	}
}

static void tegra_ast_region_dev_release(struct device *dev)
{
	struct tegra_ast_region *region =
		container_of(dev, struct tegra_ast_region, dev);

	kfree(region);
}

struct tegra_ast_region *tegra_ast_region_map(struct tegra_ast *ast,
	u32 ast_id, u32 slave_base, u32 size, u32 stream_id)
{
	struct tegra_ast_region *region;
	unsigned i;
	int err;

	if (ast_id > AST_MAX_REGION || stream_id > AST_MAX_STREAMID)
		return ERR_PTR(-EINVAL);
	if (size & (size - 1))
		return ERR_PTR(-EOPNOTSUPP);

	region = kzalloc(sizeof(*region), GFP_KERNEL);
	if (unlikely(region == NULL))
		return ERR_PTR(-ENOMEM);

	region->ast_id = ast_id;
	region->stream_id = stream_id;
	region->slave_base = slave_base;
	region->size = size;

	region->base = dma_alloc_coherent(ast->dev.parent, size, &region->dma,
						GFP_KERNEL | __GFP_ZERO);
	if (unlikely(region->base == NULL)) {
		kfree(region);
		return ERR_PTR(-ENOMEM);
	}

	region->dev.parent = &ast->dev;
	region->dev.release = tegra_ast_region_dev_release;
	dev_set_name(&region->dev, "%s:%u", dev_name(&ast->dev), ast_id);
	device_initialize(&region->dev);

	/* Check for VM indeces before modifying anything. */
	for (i = 0; i < ARRAY_SIZE(ast->bases); i++) {
		void __iomem *base = ast->bases[i];
		unsigned vmidx;

		if (base == NULL)
			continue;

		for (vmidx = 0; vmidx <= AST_MAX_VMINDEX; vmidx++) {
			u32 r = readl(base + TEGRA_APS_AST_STREAMID_CTL +
					(4 * vmidx));
			if ((r & 0x0000ff00) >> 8 == stream_id)
				break;
		}

		if (vmidx > AST_MAX_VMINDEX) {
			dev_warn(&region->dev,
					"stream ID %u not in AST %u VM table",
					stream_id, i);
			/*
			 * TODO: This is racy. Return an error once MB1 sets
			 * stream_id in at least one of the vmidx table,
			 * and remove this fallback loop.
			 */
			for (vmidx = 0; vmidx <= AST_MAX_VMINDEX; vmidx++) {
				u32 r = readl(base +
						TEGRA_APS_AST_STREAMID_CTL +
						(4 * vmidx));
				if (r & 0x0000ff00)
					break;
			}
		}

		if (vmidx > AST_MAX_VMINDEX) {
			err = -ENOBUFS;
			goto error;
		}

		dev_dbg(&region->dev, "stream ID %u using AST %u VM ID %u",
			stream_id, i, vmidx);
		region->vmids[i] = vmidx;
	}

	tegra_ast_region_enable(region);

	err = device_add(&region->dev);
	if (err)
		goto error;

	return region;
error:
	dma_free_coherent(ast->dev.parent, region->size,
				region->base, region->dma);
	put_device(&region->dev);
	return ERR_PTR(err);
}
EXPORT_SYMBOL(tegra_ast_region_map);

void tegra_ast_region_unmap(struct tegra_ast_region *region)
{
	if (IS_ERR_OR_NULL(region))
		return;

	tegra_ast_region_disable(region);
	dma_free_coherent(region->dev.parent->parent, region->size,
				region->base, region->dma);
	device_unregister(&region->dev);
}
EXPORT_SYMBOL(tegra_ast_region_unmap);

void *tegra_ast_region_get_mapping(struct tegra_ast_region *region,
					size_t *size, dma_addr_t *dma)
{
	BUG_ON(IS_ERR_OR_NULL(region));
	*size = region->size;
	*dma = region->dma;
	return region->base;
}
EXPORT_SYMBOL(tegra_ast_region_get_mapping);

struct tegra_ast_region *of_tegra_ast_region_map(struct tegra_ast *ast,
				const struct of_phandle_args *spec, u32 sid)
{
	u32 id, va, size;

	if (spec->args_count < 3)
		return ERR_PTR(-EINVAL);

	id = spec->args[0];
	va = spec->args[1];
	size = spec->args[2];

	return tegra_ast_region_map(ast, id, va, size, sid);
}
EXPORT_SYMBOL(of_tegra_ast_region_map);

void tegra_ast_get_region_info(void __iomem *base,
			u32 region,
			struct tegra_ast_region_info *info)
{
	u32 offset = region * TEGRA_APS_AST_REGION_STRIDE;
	u32 vmidx, stream_id, control;
	u64 lo, hi;

	control = readl(base + TEGRA_APS_AST_REGION_0_CONTROL + offset);
	info->control = control;

	info->lock = (control & BIT(0)) != 0;
	info->snoop = (control & BIT(2)) != 0;
	info->non_secure = (control & BIT(3)) != 0;
	info->ns_passthru = (control & BIT(4)) != 0;
	info->carveout_id = (control >> 5) & (0x1f);
	info->carveout_al = (control >> 10) & 0x3;
	info->vpr_rd = (control & BIT(12)) != 0;
	info->vpr_wr = (control & BIT(13)) != 0;
	info->vpr_passthru = (control & BIT(14)) != 0;
	vmidx = (control >> AST_RGN_CTRL_VM_INDEX) & 0xf;
	info->vm_index = vmidx;
	info->physical = (control & BIT(19)) != 0;
	stream_id = readl(base + TEGRA_APS_AST_STREAMID_CTL + (4 * vmidx));
	info->stream_id = stream_id >> 8;
	info->stream_id_enabled = (stream_id & BIT(0)) != 0;

	lo = readl(base + TEGRA_APS_AST_REGION_0_SLAVE_BASE_LO + offset);
	hi = readl(base + TEGRA_APS_AST_REGION_0_SLAVE_BASE_HI + offset);

	info->slave = ((hi << 32U) + lo) & AST_ADDR_MASK64;
	info->enabled = (lo & BIT(0)) != 0;

	hi = readl(base + TEGRA_APS_AST_REGION_0_MASK_HI + offset);
	lo = readl(base + TEGRA_APS_AST_REGION_0_MASK_LO + offset);

	info->mask = ((hi << 32) + lo) | ~AST_ADDR_MASK64;

	hi = readl(base + TEGRA_APS_AST_REGION_0_MASTER_BASE_HI + offset);
	lo = readl(base + TEGRA_APS_AST_REGION_0_MASTER_BASE_LO + offset);

	info->master = ((hi << 32U) + lo) & AST_ADDR_MASK64;
}
EXPORT_SYMBOL(tegra_ast_get_region_info);

MODULE_DESCRIPTION("Tegra Adress Space Translation class driver");
MODULE_LICENSE("GPL");
