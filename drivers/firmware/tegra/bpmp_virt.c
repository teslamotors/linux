/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/dma-mapping.h>
#include <linux/of_address.h>
#include <linux/tegra-ivc.h>
#include <asm/io.h>
#include "mail_t186.h"
#include "bpmp.h"

static void *virt_base;
static bool is_virt = false;

void *bpmp_get_virt_for_alloc(void *virt, dma_addr_t phys)
{
	if (is_virt)
		return virt_base + phys;
	else
		return virt;
}

void *bpmp_get_virt_for_free(void *virt, dma_addr_t phys)
{
	if (is_virt)
		return (void *)(virt - virt_base);
	else
		return virt;
}

void bpmp_setup_allocator(struct device *dev)
{
	uint32_t mempool_id;
	int ret;
	struct tegra_hv_ivm_cookie *ivm;

	ivm = virt_get_mempool(&mempool_id);

	if (IS_ERR_OR_NULL(ivm)) {
		if (!IS_ERR(ivm))
			dev_err(dev, "No mempool found\n");

		return;
	}

	dev_info(dev, "Found mempool with id %u\n", mempool_id);
	dev_info(dev, "ivm %pa\n", &ivm->ipa);

	virt_base = ioremap_cache(ivm->ipa, ivm->size);

	ret = dma_declare_coherent_memory(dev, ivm->ipa, 0, ivm->size,
			DMA_MEMORY_NOMAP | DMA_MEMORY_EXCLUSIVE);
	if (!(ret & DMA_MEMORY_NOMAP))
		dev_err(dev, "dma_declare_coherent_memory failed\n");

	is_virt = true;
}
