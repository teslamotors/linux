/*
 * Copyright (c) 2010-2012, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef _IOMEM_H_
#define _IOMEM_H_

struct iowr_buffer {
    unsigned long startmem;
    int nbytes;
    const void *buffer;
};

struct iord_buffer {
    unsigned long startmem;
    int nbytes;
    void *buffer;
};

#define TEGRA_IOC_MAGIC     't'
#define IOCTL_TEGRA_IOREAD  _IOR(TEGRA_IOC_MAGIC, 1, struct iord_buffer)
#define IOCTL_TEGRA_IOWRITE _IOR(TEGRA_IOC_MAGIC, 2, struct iowr_buffer)

#endif

