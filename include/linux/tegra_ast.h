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

#ifndef _LINUX_TEGRA_AST_H
#define _LINUX_TEGRA_AST_H

struct device;
struct device_node;
struct of_phandle_args;

struct tegra_ast_region_info {
	u8  enabled;
	u8  lock;
	u8  snoop;
	u8  non_secure;

	u8  ns_passthru;
	u8  carveout_id;
	u8  carveout_al;
	u8  vpr_rd;

	u8  vpr_wr;
	u8  vpr_passthru;
	u8  vm_index;
	u8  physical;

	u8  stream_id;
	u8  stream_id_enabled;
	u8  pad[2];

	u64 slave;
	u64 mask;
	u64 master;
	u32 control;
};

void __iomem *tegra_ioremap(struct device *, int index);
void __iomem *tegra_ioremap_byname(struct device *, const char *name);

struct tegra_ast;
struct tegra_ast_region;

struct tegra_ast *tegra_ast_create(struct device *dev);
void tegra_ast_destroy(struct tegra_ast *);

struct tegra_ast_region *tegra_ast_region_map(struct tegra_ast *,
			u32 region, u32 slave_base, u32 size, u32 stream_id);
void tegra_ast_region_unmap(struct tegra_ast_region *);
void *tegra_ast_region_get_mapping(struct tegra_ast_region *, size_t *,
					dma_addr_t *);
struct tegra_ast_region *of_tegra_ast_region_map(struct tegra_ast *,
			const struct of_phandle_args *spec, u32 sid);

void tegra_ast_get_region_info(void __iomem *base,
			u32 region, struct tegra_ast_region_info *info);

#endif
