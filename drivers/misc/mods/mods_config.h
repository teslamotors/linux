/*
 * mods_config.h - This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2008-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MODS_CONFIG_H_
#define _MODS_CONFIG_H_

#define MODS_KERNEL_VERSION KERNEL_VERSION(3, 18, 0)

#define MODS_IRQ_HANDLE_NO_REGS 1
#define MODS_HAS_SET_MEMORY 1
#define MODS_ACPI_DEVID_64 1
#define MODS_HAS_WC 1
#define MODS_HAS_DEV_TO_NUMA_NODE 1
#define MODS_HAS_NEW_ACPI_WALK 1
#ifdef CONFIG_DEBUG_FS
#define MODS_HAS_DEBUGFS 1
#endif
#if defined(CONFIG_TEGRA_KFUSE)
#define MODS_HAS_KFUSE 1
#endif
#ifdef CONFIG_DMA_SHARED_BUFFER
#define MODS_HAS_DMABUF 1
#endif
#define MODS_MULTI_INSTANCE_DEFAULT_VALUE 1
#define MODS_HAS_IORESOURCE_MEM_64 1
#undef MODS_HAS_NEW_ACPI_HANDLE /* new in 3.13 */
#if defined(CONFIG_ARCH_TEGRA) || defined(CONFIG_PLATFORM_TEGRA)
#define MODS_TEGRA 1
#endif
#if defined(CONFIG_TEGRA_CLK_FRAMEWORK) ||	\
	(defined(CONFIG_COMMON_CLK) && defined(CONFIG_OF_RESOLVE) && \
	 defined(CONFIG_OF_DYNAMIC))
#define MODS_HAS_CLOCK 1
#endif
#ifdef CONFIG_NET
#define MODS_HAS_NET 1
#endif
#ifdef CONFIG_ZONE_DMA32
#define MODS_HAS_DMA32	1
#endif

#endif /* _MODS_CONFIG_H_  */

/* vim: set ts=8 sw=8 noet: */
