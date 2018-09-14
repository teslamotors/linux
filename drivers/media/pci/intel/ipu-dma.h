/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2013 - 2018 Intel Corporation */

#ifndef IPU_DMA_H
#define IPU_DMA_H

#include <linux/iova.h>

struct iommu_domain;

struct ipu_dma_mapping {
	struct iommu_domain *domain;
	struct iova_domain iovad;
	struct kref ref;
};

extern const struct dma_map_ops ipu_dma_ops;

#endif /* IPU_DMA_H */
