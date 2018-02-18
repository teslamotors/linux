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
#ifndef NVGPU_PCI_H
#define NVGPU_PCI_H

#ifdef CONFIG_GK20A_PCI
int nvgpu_pci_init(void);
void nvgpu_pci_exit(void);
#else
static inline int nvgpu_pci_init(void) { return 0; }
static inline void nvgpu_pci_exit(void) {}
#endif

#endif
