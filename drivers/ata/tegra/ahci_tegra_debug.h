/*
 * drivers/ata/ahci_tegra_debug.h
 *
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _AHCI_TEGRA_DEBUG_H
#define _AHCI_TEGRA_DEBUG_H


#include <linux/debugfs.h>
#include <linux/seq_file.h>


int tegra_ahci_dbg_dump_show(struct seq_file *s, void *data);
void tegra_ahci_dbg_dump_regs(void);
int tegra_ahci_dump_debuginit(void *data);
#endif
