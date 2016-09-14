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

void intel_ipu4_wrapper_init(int mmid, struct device *dev,
			     void __iomem *base);

#endif /* INTEL_IPU4_WRAPPER_2401_H */
