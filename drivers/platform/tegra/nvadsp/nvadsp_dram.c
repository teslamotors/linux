/*
 * nvadsp_dram.c
 *
 * DRAM Sharing with ADSP
 *
 * Copyright (C) 2014 NVIDIA Corporation. All rights reserved.
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

#include <linux/tegra_nvadsp.h>

nvadsp_iova_addr_t
nvadsp_dram_map_single(struct device *nvadsp_dev,
		       void *cpu_addr, size_t size,
		       nvadsp_data_direction_t direction)
{
	return DMA_ERROR_CODE;
}

void
nvadsp_dram_unmap_single(struct device *nvadsp_dev,
			 nvadsp_iova_addr_t iova_addr, size_t size,
			 nvadsp_data_direction_t direction)
{
	return;
}

nvadsp_iova_addr_t
nvadsp_dram_map_page(struct device *nvadsp_dev,
		     struct page *page, unsigned long offset, size_t size,
		     nvadsp_data_direction_t direction)
{
	return DMA_ERROR_CODE;
}

void
nvadsp_dram_unmap_page(struct device *nvadsp_dev,
		       nvadsp_iova_addr_t iova_addr, size_t size,
		       nvadsp_data_direction_t direction)
{
	return;
}

void
nvadsp_dram_sync_single_for_cpu(struct device *nvadsp_dev,
				nvadsp_iova_addr_t iova_addr, size_t size,
				nvadsp_data_direction_t direction)
{
	return;
}

void
nvadsp_dram_sync_single_for_device(struct device *nvadsp_dev,
				   nvadsp_iova_addr_t iova_addr, size_t size,
				   nvadsp_data_direction_t direction)
{
	return;
}
