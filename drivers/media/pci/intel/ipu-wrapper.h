/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2013 - 2018 Intel Corporation */

#ifndef IPU_WRAPPER_H
#define IPU_WRAPPER_H

#define ISYS_SSID 1
#define PSYS_SSID 0

#define ISYS_MMID 1
#define PSYS_MMID 0

struct device;

void ipu_wrapper_init(int mmid, struct device *dev, void __iomem *base);

#endif /* IPU_WRAPPER_H */
