/*
 * Copyright (C) 2010 NVIDIA, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/kernel.h>
#include <linux/init.h>

#include "board-ventana.h"
#include "tegra2_emc.h"


static const struct tegra_emc_table ventana_emc_tables_elpida[] = {
	{
		.rate = 150000,   /* SDRAM frquency */
		.regs = {
			0x00000009,   /* RC */
			0x00000014,   /* RFC */
			0x00000007,   /* RAS */
			0x00000004,   /* RP */
			0x00000006,   /* R2W */
			0x00000004,   /* W2R */
			0x00000002,   /* R2P */
			0x00000009,   /* W2P */
			0x00000003,   /* RD_RCD */
			0x00000003,   /* WR_RCD */
			0x00000002,   /* RRD */
			0x00000002,   /* REXT */
			0x00000002,   /* WDV */
			0x00000005,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000008,   /* QSAFE */
			0x0000000b,   /* RDV */
			0x0000021f,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000003,   /* PDEX2WR */
			0x00000003,   /* PDEX2RD */
			0x00000004,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000a,   /* RW2PDEN */
			0x00000015,   /* TXSR */
			0x00000003,   /* TCKE */
			0x00000008,   /* TFAW */
			0x00000004,   /* TRPAB */
			0x00000006,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x00000270,   /* TREFBW */
			0x00000000,   /* QUSE_EXTRA */
			0x00000001,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000082,   /* FBIO_CFG5 */
			0xA04C04AE,   /* CFG_DIG_DLL */
			0x007FC010,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x0000000e,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	},
	{
		.rate = 300000,   /* SDRAM frquency */
		.regs = {
			0x00000012,   /* RC */
			0x00000027,   /* RFC */
			0x0000000D,   /* RAS */
			0x00000007,   /* RP */
			0x00000007,   /* R2W */
			0x00000005,   /* W2R */
			0x00000003,   /* R2P */
			0x00000009,   /* W2P */
			0x00000006,   /* RD_RCD */
			0x00000006,   /* WR_RCD */
			0x00000003,   /* RRD */
			0x00000003,   /* REXT */
			0x00000002,   /* WDV */
			0x00000006,   /* QUSE */
			0x00000003,   /* QRST */
			0x00000009,   /* QSAFE */
			0x0000000c,   /* RDV */
			0x0000045f,   /* REFRESH */
			0x00000000,   /* BURST_REFRESH_NUM */
			0x00000004,   /* PDEX2WR */
			0x00000004,   /* PDEX2RD */
			0x00000007,   /* PCHG2PDEN */
			0x00000008,   /* ACT2PDEN */
			0x00000001,   /* AR2PDEN */
			0x0000000e,   /* RW2PDEN */
			0x0000002A,   /* TXSR */
			0x00000003,   /* TCKE */
			0x0000000F,   /* TFAW */
			0x00000008,   /* TRPAB */
			0x00000005,   /* TCLKSTABLE */
			0x00000002,   /* TCLKSTOP */
			0x000004E1,   /* TREFBW */
			0x00000005,   /* QUSE_EXTRA */
			0x00000002,   /* FBIO_CFG6 */
			0x00000000,   /* ODT_WRITE */
			0x00000000,   /* ODT_READ */
			0x00000282,   /* FBIO_CFG5 */
			0xE03C048B,   /* CFG_DIG_DLL */
			0x007FC010,   /* DLL_XFORM_DQS */
			0x00000000,   /* DLL_XFORM_QUSE */
			0x00000000,   /* ZCAL_REF_CNT */
			0x0000001B,   /* ZCAL_WAIT_CNT */
			0x00000000,   /* AUTO_CAL_INTERVAL */
			0x00000000,   /* CFG_CLKTRIM_0 */
			0x00000000,   /* CFG_CLKTRIM_1 */
			0x00000000,   /* CFG_CLKTRIM_2 */
		}
	}
};

int ventana_emc_init(void)
{
	tegra_init_emc(ventana_emc_tables_elpida,
		ARRAY_SIZE(ventana_emc_tables_elpida));
	return 0;
}
