/*
 * drivers/ata/ahci_tegra_debug.c
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

#include "ahci_tegra_debug.h"
#include "ahci_tegra.h"

#define	TEGRA_AHCI_DUMP_REGS(s, fmt, args...) { if (s != NULL)\
			seq_printf(s, fmt, ##args); else  printk(fmt, ##args); }
#define	TEGRA_AHCI_DUMP_STRING(s, str) { if (s != NULL)\
			seq_puts(s, str); else printk(str); }

/*Global Variable*/
void *tegra_ahci_data;

static void
tegra_ahci_dbg_print_regs(struct seq_file *s, u32 *ptr, u32 base, u32 regs)
{
#define REGS_PER_LINE   4

	u32 i, j;
	u32 lines = regs / REGS_PER_LINE;

	for (i = 0; i < lines; i++) {
		TEGRA_AHCI_DUMP_REGS(s, "0x%08x: ", base+(i*16));
		for (j = 0; j < REGS_PER_LINE; ++j) {
			TEGRA_AHCI_DUMP_REGS(s, "0x%08x ", readl(ptr));
			++ptr;
		}
		TEGRA_AHCI_DUMP_STRING(s, "\n");
	}
#undef REGS_PER_LINE
}

int tegra_ahci_dbg_dump_show(struct seq_file *s, void *data)
{
	struct ahci_host_priv *hpriv = NULL;
	struct tegra_ahci_priv *tegra = NULL;
	u32 base;
	u32 *ptr;
	u32 i;

	if (s)
		hpriv = s->private;
	else
		hpriv = data;

	tegra = hpriv->plat_data;

	tegra_ahci_scfg_writel(hpriv, T_SATA0_INDEX_CH1, T_SATA0_INDEX);

	base = tegra->res[TEGRA_SATA_CONFIG]->start;
	ptr = tegra->base_list[TEGRA_SATA_CONFIG];
	TEGRA_AHCI_DUMP_STRING(s, "SATA CONFIG Registers:\n");
	TEGRA_AHCI_DUMP_STRING(s, "----------------------\n");
	tegra_ahci_dbg_print_regs(s, ptr, base, 0x400);

	base = tegra->res[TEGRA_SATA_AHCI]->start;
	ptr = hpriv->mmio;
	TEGRA_AHCI_DUMP_STRING(s, "\nAHCI HBA Registers:\n");
	TEGRA_AHCI_DUMP_STRING(s, "-------------------\n");
	tegra_ahci_dbg_print_regs(s, ptr, base, 64);

	for (i = 0; i < hpriv->nports; ++i) {
		base = (tegra->res[TEGRA_SATA_AHCI]->start) + 0x100 + (0x80*i);
		ptr = hpriv->mmio + 0x100;
		TEGRA_AHCI_DUMP_REGS(s, "\nPort %u Registers:\n", i);
		TEGRA_AHCI_DUMP_STRING(s, "---------------\n");
		tegra_ahci_dbg_print_regs(s, ptr, base, 20);
	}

	tegra_ahci_scfg_writel(hpriv, T_SATA0_INDEX_NONE_SELECTED,
								T_SATA0_INDEX);

	return 0;
}

static int tegra_ahci_dbg_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, tegra_ahci_dbg_dump_show, inode->i_private);
}

void tegra_ahci_dbg_dump_regs(void)
{
	tegra_ahci_dbg_dump_show(NULL, tegra_ahci_data);
}

static const struct file_operations debug_fops = {
	.open           = tegra_ahci_dbg_dump_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

int tegra_ahci_dump_debuginit(void *data)
{
	tegra_ahci_data = data;
	(void) debugfs_create_file("tegra_ahci", S_IRUGO,
			NULL, data, &debug_fops);
	return 0;
}

