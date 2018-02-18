/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
#define pr_fmt(fmt) "ecc-cfg: " fmt

#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/platform/tegra/mc.h>
#include <linux/platform/tegra/mcerr_ecc_t18x.h>

#define MASK(Y, Z)      ((0xFFFFFFFF >> (31-Y)) & (0xFFFFFFFF << Z))
#define CALC(X, Y, Z)	((X & MASK(Y, Z)) >> Z)
#define MCBIT(X, B)	CALC(X, B, B)
#define ABIT(B)         MCBIT(addr, B)

struct mc_cfg_t {
	u32 col_bits;
	u32 row_bits;
	u32 bank_bits;
	u32 channel_bits;
	u32 chanpos;
	u32 bankpos;
	u32 all_bits;
	u32 lsb_bits;
	u32 ecc_bits;
	u32 ecc_ratio;
	u32 subp_bits;
	u32 bank_mask[3];
	u32 chanmask[2];
	u32 addr_cfg_dev0;
	u32 addr_cfg_dev1;
	u32 addr_cfg0;
	u32 emem_bom;
};

static struct mc_cfg_t mc_cfg;

/* invoked via debugfs entry mc/ecc_err/cfg */
int mc_ecc_config_dump(struct seq_file *s, void *v)
{
	seq_printf(s, "col_bits = %d\n", mc_cfg.col_bits);
	seq_printf(s, "row_bits = %d\n", mc_cfg.row_bits);
	seq_printf(s, "bank_bits = %d\n", mc_cfg.bank_bits);
	seq_printf(s, "channel_bits = %d\n", mc_cfg.channel_bits);
	seq_printf(s, "chanpos = %d,bankpos = %d\n", mc_cfg.chanpos,
							mc_cfg.bankpos);
	seq_printf(s, "all_bits = %d,lsb_bits = %d,subp_bits = %d\n",
		mc_cfg.all_bits, mc_cfg.lsb_bits, mc_cfg.subp_bits);
	seq_printf(s, "ecc_bits = %d,ecc_ratio = %d\n", mc_cfg.ecc_bits,
						mc_cfg.ecc_ratio);
	seq_printf(s, "bank_mask[0] = 0x%x\n", mc_cfg.bank_mask[0]);
	seq_printf(s, "bank_mask[1] = 0x%x,bank_mask[2] = 0x%x\n",
			mc_cfg.bank_mask[1], mc_cfg.bank_mask[2]);
	seq_printf(s, "chanmask[0] = 0x%x,chanmask[1] = 0x%x\n",
		mc_cfg.chanmask[0], mc_cfg.chanmask[1]);
	seq_printf(s, "addr_cfg_dev0 = 0x%x,addr_cfg_dev1 = 0x%x\n",
		mc_cfg.addr_cfg_dev0, mc_cfg.addr_cfg_dev1);
	seq_printf(s, "addr_cfg0 = 0x%x\n", mc_cfg.addr_cfg0);
	seq_printf(s, "emem_bom = 0x%x\n", mc_cfg.emem_bom);

	return 0;
}

void mc_ecc_config_read(void)
{
	u32 max_chanbits = 2;
	u32 chanpos, i;

	/* FIXME check for replacing below constants with macros */

	/* LPDDR4 have column bits always 10 */
	mc_cfg.col_bits = 10;
	/*16-bit sub-partition = 1,32-bit sub-partition =2:
	For LPDDR4 it is always 16-bit sub-partition */
	mc_cfg.lsb_bits = 1;
	/*LPDDR4 device has always 8-banks*/
	mc_cfg.bank_bits = 3;
	mc_cfg.bankpos = 10;
	/* 1/8 for ECC and 7/8 for Data */
	mc_cfg.ecc_ratio = 7;
	/*ecc_bits = log2(ecc_ratio+1)*/
	mc_cfg.ecc_bits = 3;
	/* For LPDDR4 it is always dual sub partition */
	mc_cfg.subp_bits = 1;

	mc_cfg.emem_bom = ((mc_readl(MC_EMEM_CFG) & 0x80000000) ==
						0x80000000);
	mc_cfg.addr_cfg_dev0 = mc_readl(MC_EMEM_ADR_CFG_DEV0);
	mc_cfg.addr_cfg_dev1 = mc_readl(MC_EMEM_ADR_CFG_DEV1);
	mc_cfg.addr_cfg0 = mc_readl(MC_EMEM_ADR_CFG);

	mc_cfg.all_bits = (((mc_cfg.addr_cfg_dev0>>16) & 0x0f) + 22);

	if (mc_cfg.all_bits == (12 + 22))
		mc_cfg.all_bits = (8 + 22);
	else if (mc_cfg.all_bits == (13 + 22))
		mc_cfg.all_bits = (7 + 22);

	mc_cfg.row_bits = (mc_cfg.all_bits - mc_cfg.bank_bits -  mc_cfg.col_bits
						- mc_cfg.lsb_bits);

	mc_cfg.bank_mask[0] = mc_readl(MC_EMEM_ADR_CFG_BANK_MASK_0);
	mc_cfg.bank_mask[1] = mc_readl(MC_EMEM_ADR_CFG_BANK_MASK_1);
	mc_cfg.bank_mask[2] = mc_readl(MC_EMEM_ADR_CFG_BANK_MASK_2);

	mc_cfg.chanmask[0] = mc_readl(MC_EMEM_ADR_CFG_CHANNEL_MASK);
	mc_cfg.chanmask[1] = mc_readl(MC_EMEM_ADR_CFG_CHANNEL_MASK_1);

	mc_cfg.chanpos = (1 << (u32)31)-(u32)1;

	for (i = 0; i < max_chanbits; i++) {

		if (mc_cfg.chanmask[i] != 0) {

			mc_cfg.channel_bits += 1;
			for (chanpos = 0; ((1 << chanpos) &
				mc_cfg.chanmask[i]) == 0; chanpos++)
					;

			if (mc_cfg.chanpos > chanpos)
				mc_cfg.chanpos = chanpos;
		}
	}
	if (mc_cfg.channel_bits == 0)
		mc_cfg.chanpos = 0;
}

static u32 mc_reverse_masked_bank(u64 linear, int masked_bank, int bankpos,
				int bankbits)
{
	u32 bank = 0;
	u32 pre_bank = 0;
	u32 pos = bankpos + bankbits;
	u64 bank_bit;
	u32 i, j;

	for (i = (bankbits - 1); ((i >= 0) && (i < bankbits)); --i) {
		pre_bank <<= 1;

		bank_bit = (mc_cfg.bank_mask[i] & linear);
		bank_bit &= ~((1ll << (bankpos+bankbits)) - 1);
		while (bank_bit) {
			pre_bank ^= bank_bit & 1;
			bank_bit >>= 1;
		}
		pre_bank ^= (masked_bank>>i) & 1;
	}

	for (i = (bankbits - 1); ((i >= 0) && (i < bankbits)); --i) {
		bank <<= 1;
		pos -= 1;
		for (j = (bankbits-1); ((j >= 0) && (j < bankbits)); --j) {
			if ((mc_cfg.bank_mask[j]>>pos) & 1) {
				bank |= (pre_bank>>j) & 1;
				break;
			}
		}
	}
	return bank;
}
static u64 mc_generate_chan_offset(u64 addr, u32 chanbits)
{
	u32 chanoffset = 0;
	u32 max_chanbits = 2;
	u32 mask;
	u64 cur_addr;
	u32 i;

	for (i = (max_chanbits - 1); ((i >= 0) && (i < 2)); i--) {
		mask = mc_cfg.chanmask[i];
		cur_addr = addr;
		chanoffset = chanoffset << 1;
		while (mask) {
			if (mask & 1)
				chanoffset ^= (cur_addr & 1);
			mask = (mask >> 1);
			cur_addr = (cur_addr >> 1);
		}
	}
	return chanoffset & ~(-1<<chanbits);
}
static u64 mc_translate_update_ch(u64 addr, u32 ch)
{
	u32 cur_chan;
	u32 chanbits = 2;
	u32 chanpos = 10;
	u32 chanoffset;

	for (cur_chan = 0; (cur_chan < (1 << chanbits)); ++cur_chan) {

		addr = ((addr >> (chanpos + chanbits)) << (chanpos + chanbits)
		| (cur_chan & ((1 << chanbits) - 1)) << chanpos
		| (addr & ((1 << chanpos) - 1)));

		chanoffset = mc_generate_chan_offset(addr, chanbits);

		if (chanoffset == ch)
			break;
	}
	return addr;
}

static u64 mc_addr_translate_core(u64 addr, u32 device, u32 ch, u32 row,
					u32 bank, u32 col, u32 subp, u32 lsb)
{
	u32 rcl = (mc_cfg.row_bits + mc_cfg.col_bits + mc_cfg.lsb_bits) - 1;
	u32 rcls = rcl + mc_cfg.subp_bits;
	u32 rclsb = rcls + mc_cfg.bank_bits;
	u32 rclsbc = rclsb + mc_cfg.channel_bits;

	addr = ((addr & ~(MASK(rcl, 0))) | ((CALC(row, mc_cfg.row_bits-1, 0) <<
		(mc_cfg.col_bits+mc_cfg.lsb_bits)) |
		(CALC(col, mc_cfg.col_bits-1, 0) << mc_cfg.lsb_bits) |
		CALC(lsb, 0, 0)));

	if (CALC(addr, 10, 8) == 0x7) {
		addr = (addr & ~(MASK(rcl, 0))) | (((CALC(addr, rcl, 11) << 7) |
			CALC(addr, 7, 0)));
	} else {
		addr = (addr & ~(MASK(rcl, 8))) | ((((CALC(addr, rcl, 11) *
			mc_cfg.ecc_ratio)) + (CALC(addr, 10, 8))) << 8);
	}

	addr = (addr & ~(MASK(rcls, 0))) | (CALC(addr, rcl, 5) << 6) |
		((subp ^ MCBIT(addr, 7) ^ MCBIT(addr, 5)) << 5) |
		CALC(addr, 4, 0);

	addr = (addr & ~(MASK(rclsb, 0))) | (CALC(addr, rcls, 10) << 13) |
			(CALC(bank, 2, 0) << 10) | CALC(addr, 9, 0);

	if (mc_cfg.channel_bits) {
		addr = (addr & ~(MASK(rclsbc, 0))) |
			(CALC(addr, rclsb, 10) << 12) |
			(CALC(0, mc_cfg.channel_bits-1, 0) << 10) |
			CALC(addr, 9, 0);
	}
	return addr;
}

static u64 mc_emem_bom(void)
{
	if (mc_cfg.emem_bom)
		return 0x40000000;
	else
		return 0x80000000;
}

static u64 mc_device_size(void)
{
	/* Subp and lsb are each 1-bit */
	u64 size = 1ll << (mc_cfg.channel_bits + mc_cfg.row_bits+
			mc_cfg.bank_bits + mc_cfg.col_bits + 2);
	return size;
}

static u64 mc_device_ecc_size(void)
{
	return mc_device_size()/(mc_cfg.ecc_ratio + 1);
}

static u64 mc_device_bom(u32 dev)
{
	if (dev == 0)
		return mc_emem_bom();

	return mc_device_bom(dev - 1) + mc_device_size() - mc_device_ecc_size();
}

static u64 mc_addr_translate_device(u64 addr, u32 col, u32 device, u32 ch)
{
	u64 device_bom = mc_device_bom(device);

	addr = (u64)(((u64)CALC(addr, 31, 0)) + (device_bom));

	addr = mc_translate_update_ch(addr, ch);

	return addr;
}

u64 mc_addr_translate(u32 device, u32 ch, u32 row, u32 bank, u32 col, u32 subp,
									u32 lsb)
{
	/* FIXME check for replacing below constants with macros */
	u64 addr = 0;

	addr = mc_addr_translate_core(addr, device, ch, row, bank,
							col, subp, lsb);
	addr = mc_addr_translate_device(addr, col, device, ch);

	addr = ((addr >> (10 + 2)) << 10) | (addr & ((1 << 10) - 1));
	bank = mc_reverse_masked_bank(addr, bank, 10, 3);

	addr = 0;

	addr = mc_addr_translate_core(addr, device, ch, row, bank,
							col, subp, lsb);
	addr = mc_addr_translate_device(addr, col, device, ch);

	return addr;
}
