/*
 * Copyright (c) 2013--2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef INTEL_IPU4_WRAPPER_2401_H
#define INTEL_IPU4_WRAPPER_2401_H

#define ISYS_SSID 1
#define PSYS_SSID 0

#define ISYS_MMID 1
#define PSYS_MMID 0

struct device;
struct ia_css_env;
struct intel_ipu4_isys_iomem_filter;
struct iommu_domain;
struct firmware;

int intel_ipu4_wrapper_register_buffer(dma_addr_t iova,
		void *addr, size_t bytes);
void intel_ipu4_wrapper_unregister_buffer(dma_addr_t iova);

void intel_ipu4_wrapper_init(void __iomem *basepsys, void __iomem *baseisys,
			unsigned int flags);
void intel_ipu4_wrapper_set_device(struct device *dev, int mmid);

int intel_ipu4_wrapper_add_shared_memory_buffer(int mmid, void *addr,
						dma_addr_t dma_addr,
						unsigned long size);
int intel_ipu4_wrapper_remove_shared_memory_buffer(int mmid, void *addr);

void *intel_ipu4_wrapper_get_isys_fw(void);

#endif /* INTEL_IPU4_WRAPPER_2401_H */
