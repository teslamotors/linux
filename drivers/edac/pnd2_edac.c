/*
 * Driver for Pondicherry2 memory controller.
 *
 * Copyright (c) 2016, Intel Corporation.
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
 * [Derived from sb_edac.c]
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/edac.h>
#include <linux/mmzone.h>
#include <linux/smp.h>
#include <linux/bitmap.h>
#include <linux/math64.h>
#include <linux/mod_devicetable.h>
#include <asm/cpu_device_id.h>
#include <asm/processor.h>
#include <asm/mce.h>

#include "edac_core.h"
#include "pnd2_regs.h"

#define	NUM_CHANNELS		4

#define GET_BITFIELD(v, lo, hi) \
	(((v) & GENMASK_ULL(hi, lo)) >> (lo))

/* Static vars */
static struct pnd2_dev {
	struct mem_ctl_info *mci;
} pnd2_dev;

struct pnd2_pvt {
	struct pnd2_dev	*pnd2_dev;
	int		dimm_geom[NUM_CHANNELS];
	u64		tolm, tohm;
};

#define PND2_MSG_SIZE	256

/*
 * Debug macros
 */
#define pnd2_printk(level, fmt, arg...)			\
	edac_printk(level, "pnd2", fmt, ##arg)

#define pnd2_mc_printk(mci, level, fmt, arg...)		\
	edac_mc_chipset_printk(mci, level, "pnd2", fmt, ##arg)

static void show_debug_result(u64 addr, int channel, u64 pmiaddr, int rank,
			      int bank, int row, int col);

#define MOT_CHAN_INTLV_BIT_1SLC_2CH 12
#define	MOT_CHAN_INTLV_BIT_2SLC_2CH 13
#define SELECTOR_DISABLED (-1)
#define _4GB (1ul << 32)

#define PMI_ADDRESS_WIDTH	31
#define	PND_MAX_PHYS_BIT	38

#define	ASYMSHIFT		28
#define CH_HASH_MASK_LSB	6
#define SLICE_HASH_MASK_LSB	6
#define MOT_SLC_INTLV_BIT	12
#define LOG2_PMI_ADDR_GRANULARITY	5

#define MOT_SHIFT	24

#define u64_lshift(val, s) ((u64)(val) << (s))

#include "linux/platform_data/sbi_apl.h"


int sbi_send(int port, int offset, int opcode, u32 *data)
{
	int ret;
	int read = 0;
	struct sbi_apl_message sbi_arg;

	memset(&sbi_arg, 0, sizeof(sbi_arg));

	if (opcode == 0 || opcode == 4 || opcode == 6)
		read = 1;
	else
		sbi_arg.data = *data;

	sbi_arg.opcode = opcode;
	sbi_arg.port_address = port;
	sbi_arg.register_offset = offset;
	ret = sbi_apl_commit(&sbi_arg);
	if (ret || sbi_arg.status)
		edac_dbg(2, "sbi_send status=%d ret=%d data=%xh",
			 sbi_arg.status, ret, sbi_arg.data);

	if (ret == 0)
		ret = sbi_arg.status;

	if (ret == 0 && read)
		*data = sbi_arg.data;

	return ret;
}


static void _rd_reg(int port, int off, int op, void *p, size_t sz, char *name)
{
	int	status;

	edac_dbg(2, "read %s port=%xh off=%xh op=%xh\n", name, port, off, op);
	switch (sz) {
	case 8:
		status = sbi_send(port, off + 4, op, (u32 *)(p + 4));
	case 4:
		status = sbi_send(port, off, op, (u32 *)p);
		pnd2_printk(KERN_DEBUG, "%s = 0x%x%08x status=%d\n", name,
			    sz == 8 ? *((u32 *)(p+4)) : 0, *((u32 *)p), status);
		break;
	}
}

#define RD_REGP(regp, regname, portsuffix)	\
	_rd_reg(regname ## _port ## portsuffix,	\
		regname##_offset,		\
		regname##_r_opcode,		\
		regp, sizeof(struct regname),	\
		#regname)

#define RD_REG(regp, regname)			\
	_rd_reg(regname ## _port,		\
		regname##_offset,		\
		regname##_r_opcode,		\
		regp, sizeof(struct regname),	\
		#regname)

/*
 * System address space is divided into multiple regions with
 * different interleave rules in each. The as0/as1 regions
 * have no interleaving at all. The as2 region is interleaved
 * between two channels. The mot region is magic and may overlap
 * other regions, with its interleave rules taking precedence.
 * Addresses not in any of these regions are interleaved across
 * all four channels.
 */
static struct region {
	u64	base;
	u64	limit;
	u8	enabled;
} mot, as0, as1, as2;

static u64 top_lm, top_hm;
static bool two_slices, two_channels;

static u8 validsymmlmcbitvec;
static u8 validasymmlmcbitvec;
static u8 validmlmcbitvec;

static int sliceselector = -1;
static int channelselector = -1;
static u64 slicehashmask;
static u64 channelhashmask;

static void mkregion(char *name, struct region *rp, u64 base, u64 limit)
{
	rp->enabled = 1;
	rp->base = base;
	rp->limit = limit;
	edac_dbg(2, "region:%s [%llx,%llx]\n", name, base, limit);
}

static void mkregionmask(char *name, struct region *rp, u64 base, u64 mask)
{
	if (mask == 0) {
		pr_info(FW_BUG "MOT mask cannot be zero\n");
		return;
	}
	if (mask != GENMASK_ULL(PND_MAX_PHYS_BIT, __ffs(mask))) {
		pr_info(FW_BUG "MOT mask not power of two\n");
		return;
	}
	if (base & ~mask) {
		pr_info(FW_BUG "MOT region base/mask alignment error\n");
		return;
	}
	rp->base = base;
	rp->limit = (base | ~mask) & GENMASK_ULL(PND_MAX_PHYS_BIT, 0);
	rp->enabled = 1;
	edac_dbg(2, "region:%s [%llx,%llx]\n", name, base, rp->limit);
}

static bool inregion(struct region *rp, u64 addr)
{
	if (!rp->enabled)
		return false;
	return rp->base <= addr && addr <= rp->limit;
}

static int sym_mask(struct b_cr_slice_channel_hash *p)
{
	int mask = 0xf;

	if (p->slice_0_mem_disabled)
		mask &= 0xc;
	else
		mask &= 0xc | p->sym_slice0_channel_enabled;

	if (p->slice_1_disabled)
		mask &= 0x3;
	else
		mask &= (p->sym_slice1_channel_enabled << 2) | 3;

	if (p->ch_1_disabled || p->enable_pmi_dual_data_mode)
		mask &= 0x5;

	return mask;
}

int asym_mask(struct b_cr_slice_channel_hash *p,
	      struct b_cr_asym_mem_region0_0_0_0_mchbar *as0,
	      struct b_cr_asym_mem_region1_0_0_0_mchbar *as1,
	      struct b_cr_asym_2way_mem_region_0_0_0_mchbar *as2way)
{
	int mask = 0;
	static int intlv[] = { 0x5, 0xA, 0x3, 0xC };

	if (as2way->asym_2way_interleave_enable)
		mask = intlv[as2way->asym_2way_intlv_mode];
	if (as0->slice0_asym_enable)
		mask |= (1 << as0->slice0_asym_channel_select);
	if (as1->slice1_asym_enable)
		mask |= (4 << as1->slice1_asym_channel_select);
	if (p->slice_0_mem_disabled)
		mask &= 0xc;
	if (p->slice_1_disabled)
		mask &= 0x3;
	if (p->ch_1_disabled || p->enable_pmi_dual_data_mode)
		mask &= 0x5;
	return mask;
}

static struct b_cr_tolud_0_0_0_pci tolud;
static struct b_cr_touud_lo_0_0_0_pci touud_lo;
static struct b_cr_touud_hi_0_0_0_pci touud_hi;
static struct b_cr_asym_mem_region0_0_0_0_mchbar asym0;
static struct b_cr_asym_mem_region1_0_0_0_mchbar asym1;
static struct b_cr_asym_2way_mem_region_0_0_0_mchbar asym2way;
static struct b_cr_mot_out_base_0_0_0_mchbar motbase;
static struct b_cr_mot_out_mask_0_0_0_mchbar motmask;
static struct b_cr_slice_channel_hash chash;
static struct d_cr_drp0 drp0[NUM_CHANNELS];

/*
 * Read all the h/w config registers once here (they don't
 * change at run time. Figure out which address ranges have
 * which interleave characteristics.
 */
static void get_registers(void)
{
	static int intlv[] = { 10, 11, 12, 12 };

	RD_REG(&tolud, b_cr_tolud_0_0_0_pci);
	RD_REG(&touud_lo, b_cr_touud_lo_0_0_0_pci);
	RD_REG(&touud_hi, b_cr_touud_hi_0_0_0_pci);
	RD_REG(&asym0, b_cr_asym_mem_region0_0_0_0_mchbar);
	RD_REG(&asym1, b_cr_asym_mem_region1_0_0_0_mchbar);
	RD_REG(&asym2way, b_cr_asym_2way_mem_region_0_0_0_mchbar);
	RD_REG(&motbase, b_cr_mot_out_base_0_0_0_mchbar);
	RD_REG(&motmask, b_cr_mot_out_mask_0_0_0_mchbar);
	RD_REG(&chash, b_cr_slice_channel_hash);

	/*
	 * Only two DIMMs on Juniper Hill board.
	 * Slice 0 channel 0 is d_cr_drp0_port2 - save in &drp0[0]
	 * slice 1 channel 0 is d_cr_drp0_port1 - save in &drp0[2]
	 */
	RD_REGP(&drp0[0], d_cr_drp0, 2);
	RD_REGP(&drp0[1], d_cr_drp0, 0); /* guess */
	RD_REGP(&drp0[2], d_cr_drp0, 1);
	RD_REGP(&drp0[3], d_cr_drp0, 3); /* guess */

	if (asym0.slice0_asym_enable) {
		mkregion("as0", &as0,
			 u64_lshift(asym0.slice0_asym_base, ASYMSHIFT),
			 u64_lshift(asym0.slice0_asym_limit, ASYMSHIFT) +
				GENMASK_ULL(ASYMSHIFT-1, 0));
	}

	if (asym1.slice1_asym_enable) {
		mkregion("as1", &as1,
			 u64_lshift(asym1.slice1_asym_base, ASYMSHIFT),
			 u64_lshift(asym1.slice1_asym_limit, ASYMSHIFT) +
				GENMASK_ULL(ASYMSHIFT-1, 0));
	}

	if (asym2way.asym_2way_interleave_enable) {
		mkregion("as2way", &as2,
			 u64_lshift(asym2way.asym_2way_base, ASYMSHIFT),
			 u64_lshift(asym2way.asym_2way_limit, ASYMSHIFT) +
				GENMASK_ULL(ASYMSHIFT-1, 0));
	}

	if (motbase.imr_en) {
		mkregionmask("mot", &mot,
			u64_lshift(motbase.mot_out_base, MOT_SHIFT),
			u64_lshift(motmask.mot_out_mask, MOT_SHIFT));
	}

	top_lm = u64_lshift(tolud.tolud, 20);

	top_hm = u64_lshift(touud_hi.touud, 32) |
		 u64_lshift(touud_lo.touud, 20);


	two_slices = !chash.slice_1_disabled &&
			!chash.slice_0_mem_disabled &&
			(chash.sym_slice0_channel_enabled != 0) &&
			(chash.sym_slice1_channel_enabled != 0);
	two_channels = !chash.ch_1_disabled &&
			!chash.enable_pmi_dual_data_mode &&
			((chash.sym_slice0_channel_enabled == 3) ||
			 (chash.sym_slice1_channel_enabled == 3));

	validsymmlmcbitvec = sym_mask(&chash);
	validasymmlmcbitvec = asym_mask(&chash, &asym0, &asym1, &asym2way);
	validmlmcbitvec = validsymmlmcbitvec | validasymmlmcbitvec;

	if (two_slices && !two_channels) {
		if (chash.hvm_mode)
			sliceselector = 29;
		else
			sliceselector = intlv[chash.interleave_mode];
	} else if (!two_slices && two_channels) {
		if (chash.hvm_mode)
			channelselector = 29;
		else
			channelselector = intlv[chash.interleave_mode];
	} else if (two_slices && two_channels) {

		if (chash.hvm_mode) {
			sliceselector = 29;
			channelselector = 30;
		} else {
			sliceselector = intlv[chash.interleave_mode];
			channelselector = intlv[chash.interleave_mode] + 1;
		}
	}

	if (two_slices) {
		if (!chash.hvm_mode)
			slicehashmask = chash.slice_hash_mask <<
						SLICE_HASH_MASK_LSB;
		if (!two_channels)
			slicehashmask |= BIT_ULL(sliceselector);
	}
	if (two_channels) {
		if (!chash.hvm_mode)
			channelhashmask = chash.ch_hash_mask <<
						CH_HASH_MASK_LSB;
		if (!two_slices)
			channelhashmask |= BIT_ULL(channelselector);
	}
}

/* Get a contiguous memory address (remove the MMIO gap) */
static u64 sys2contig(u64 sys)
{
	return (sys < _4GB) ? sys : sys - (_4GB - top_lm);
}

/* squeeze out one address bit, shift upper part down to fill gap */
static void removeaddrbit(u64 *addr, int bitidx)
{
	u64	mask;

	if (bitidx == -1)
		return;

	mask = (1ull << bitidx) - 1;
	*addr = ((*addr >> 1) & ~mask) | (*addr & mask);
}

/* XOR all the bits from addr specified in mask */
static int hashbymask(u64 addr, u64 mask)
{
	u64 result = addr & mask;

	result = (result >> 32) ^ result;
	result = (result >> 16) ^ result;
	result = (result >> 8) ^ result;
	result = (result >> 4) ^ result;
	result = (result >> 2) ^ result;
	result = (result >> 1) ^ result;

	return (int)result & 1;
}

/*
 * First stage decode. Take the system address and figure out which
 * second stage will deal with it based on interleave modes.
 */
static int sys2pmi(const u64 addr, u32 *pmiidx, u64 *pmiaddr, char *msg)
{
	u64 contigmemaddr;
	int motintlvbit = two_slices ? MOT_CHAN_INTLV_BIT_2SLC_2CH :
		MOT_CHAN_INTLV_BIT_1SLC_2CH;
	int sliceintlvbittormv = SELECTOR_DISABLED;
	int channelintlvbittormv = SELECTOR_DISABLED;

	/* Determine if address is in the MOT region. */
	bool mothit = inregion(&mot, addr);

	/* Calculate the number of symmetric regions enabled. */
	int numsymchannels = hweight8(validsymmlmcbitvec);

	/*
	 * The amount we need to shift the asym base can be determined by the
	 * number of enabled symmetric channels.
	 * NOTE: This can only work because symmetric memory is not supposed
	 * to do a 3-way interleave.
	 */
	int symchannelshift = numsymchannels >> 1;

	*pmiidx = 0u;

	/* Give up if address is out of range, or in MMIO gap */
	if (addr >= (1ul << PND_MAX_PHYS_BIT) ||
		(addr >= top_lm && addr < _4GB) ||
		addr >= top_hm) {
		snprintf(msg, PND2_MSG_SIZE,
			 "Error address 0x%08Lx is not DRAM", addr);
		return -EINVAL;
	}

	/* Get a contiguous memory address (remove the MMIO gap) */
	contigmemaddr = sys2contig(addr);

	if (inregion(&as0, addr)) {
		u64 contigbase, contigoffset, adjcontigbase;

		*pmiidx = asym0.slice0_asym_channel_select;
		contigbase = sys2contig(as0.base);
		contigoffset = contigmemaddr - contigbase;
		adjcontigbase = (contigbase >> symchannelshift) *
			((chash.sym_slice0_channel_enabled >>
			  (*pmiidx & 1)) & 1);

		contigmemaddr = contigoffset + ((numsymchannels > 0) ?
				adjcontigbase : 0ull);
	} else if (inregion(&as1, addr)) {
		u64 contigbase, contigoffset, adjcontigbase;
		*pmiidx = 2u + asym1.slice1_asym_channel_select;
		contigbase = sys2contig(as1.base);
		contigoffset = contigmemaddr - contigbase;
		adjcontigbase = (contigbase >> symchannelshift) *
			((chash.sym_slice1_channel_enabled >>
			  (*pmiidx & 1)) & 1);
		contigmemaddr = contigoffset + ((numsymchannels > 0) ?
					adjcontigbase : 0ull);
	} else if (inregion(&as2, addr) &&
		   (asym2way.asym_2way_intlv_mode == 0x3ul)) {
		bool channel1;
		u64 contigbase, contigoffset;

		motintlvbit = MOT_CHAN_INTLV_BIT_1SLC_2CH;
		*pmiidx = (asym2way.asym_2way_intlv_mode & 1) << 1;
		channel1 = mothit ? ((bool) ((addr >> motintlvbit) & 1)) :
			hashbymask(contigmemaddr, channelhashmask);

		*pmiidx |= (unsigned int)channel1;

		contigbase = sys2contig(as2.base);

		channelintlvbittormv = mothit ? motintlvbit : channelselector;
		contigoffset = contigmemaddr - contigbase;

		removeaddrbit(&contigoffset, channelintlvbittormv);
		contigmemaddr = (contigbase >> symchannelshift) + contigoffset;
	} else {
		/* Otherwise we're in normal, boring symmetric mode. */
		if (two_slices) {
			bool slice1;

			if (mothit) {
				sliceintlvbittormv = MOT_SLC_INTLV_BIT;
				slice1 = (addr >> MOT_SLC_INTLV_BIT) & 1;
			} else {
				sliceintlvbittormv = sliceselector;
				slice1 = hashbymask(addr, slicehashmask);
			}
			*pmiidx = (unsigned int)slice1 << 1;
		}

		if (two_channels) {
			bool channel1;

			motintlvbit = two_slices ?
				MOT_CHAN_INTLV_BIT_2SLC_2CH :
				MOT_CHAN_INTLV_BIT_1SLC_2CH;

			if (mothit) {
				channelintlvbittormv = motintlvbit;
				channel1 = (addr >> motintlvbit) & 1;
			} else {
				channelintlvbittormv = channelselector;
				channel1 = hashbymask(contigmemaddr,
						      channelhashmask);
			}

			*pmiidx |= (unsigned int)channel1;
		}
	}

	/* Remove the channelselector bit first */
	removeaddrbit(&contigmemaddr, channelintlvbittormv);

	/* Remove the slice bit (we remove it second because it must be lower */
	removeaddrbit(&contigmemaddr, sliceintlvbittormv);

	*pmiaddr = contigmemaddr >> LOG2_PMI_ADDR_GRANULARITY;

	return 0;
}

/*
 * Translate PMI address to memory (rank, row, bank, column)
 */
#define	C(n) (0x10 | (n))	/* column */
#define	B(n) (0x20 | (n))	/* bank */
#define	R(n) (0x40 | (n))	/* row */
#define RS   (0x80)		/* rank */

/* addrdec values */
#define	AMAP_1KB	0
#define	AMAP_2KB	1
#define	AMAP_4KB	2
#define	AMAP_RSVD	3

/* dden values */
#define	DEN_4Gb		0
#define DEN_8Gb		2

/*  dwid values */
#define X8		0
#define X16		1


static struct dimm_geometry {
	u8	addrdec;
	u8	dden;
	u8	dwid;
	u8	rowbits, colbits;
	u16	bits[PMI_ADDRESS_WIDTH];
} dimms[12] = {
{
	.addrdec = AMAP_1KB, .dden = DEN_4Gb, .dwid = X16,
	.rowbits = 15, .colbits = 10,
	.bits = {
		 C(2),  C(3),  C(4),  C(5),  C(6),  B(0),  B(1),  B(2),  R(0),
		 R(1),  R(2),  R(3),  R(4),  R(5),  R(6),  R(7),  R(8),  R(9),
		R(10),  C(7),  C(8),  C(9), R(11),    RS, R(12), R(13), R(14),
		    0,     0,     0,     0
	}
},
{
	.addrdec = AMAP_1KB, .dden = DEN_4Gb, .dwid = X8,
	.rowbits = 16, .colbits = 10,
	.bits = {
		 C(2),  C(3),  C(4),  C(5),  C(6),  B(0),  B(1),  B(2),  R(0),
		 R(1),  R(2),  R(3),  R(4),  R(5),  R(6),  R(7),  R(8),  R(9),
		R(10),  C(7),  C(8),  C(9), R(11),    RS, R(12), R(13), R(14),
		R(15),     0,     0,     0
	}
},
{
	.addrdec = AMAP_1KB, .dden = DEN_8Gb, .dwid = X16,
	.rowbits = 16, .colbits = 10,
	.bits = {
		 C(2),  C(3),  C(4),  C(5),  C(6),  B(0),  B(1),  B(2),  R(0),
		 R(1),  R(2),  R(3),  R(4),  R(5),  R(6),  R(7),  R(8),  R(9),
		R(10),  C(7),  C(8),  C(9), R(11),    RS, R(12), R(13), R(14),
		R(15),     0,     0,     0
	}
},
{
	.addrdec = AMAP_1KB, .dden = DEN_8Gb, .dwid = X8,
	.rowbits = 16, .colbits = 11,
	.bits = {
		 C(2),  C(3),  C(4),  C(5),  C(6),  B(0),  B(1),  B(2),  R(0),
		 R(1),  R(2),  R(3),  R(4),  R(5),  R(6),  R(7),  R(8),  R(9),
		R(10),  C(7),  C(8),  C(9), R(11),    RS, C(11), R(12), R(13),
		R(14), R(15),     0,     0
	}
},
{
	.addrdec = AMAP_2KB, .dden = DEN_4Gb, .dwid = X16,
	.rowbits = 15, .colbits = 10,
	.bits = {
		 C(2),  C(3),  C(4),  C(5),  C(6),  C(7),  B(0),  B(1),  B(2),
		 R(0),  R(1),  R(2),  R(3),  R(4),  R(5),  R(6),  R(7),  R(8),
		 R(9), R(10),  C(8),  C(9), R(11),    RS, R(12), R(13), R(14),
		    0,     0,     0,     0
	}
},
{
	.addrdec = AMAP_2KB, .dden = DEN_4Gb, .dwid = X8,
	.rowbits = 16, .colbits = 10,
	.bits = {
		 C(2),  C(3),  C(4),  C(5),  C(6),  C(7),  B(0),  B(1),  B(2),
		 R(0),  R(1),  R(2),  R(3),  R(4),  R(5),  R(6),  R(7),  R(8),
		 R(9), R(10),  C(8),  C(9), R(11),    RS, R(12), R(13), R(14),
		R(15),     0,     0,     0
	}
},
{
	.addrdec = AMAP_2KB, .dden = DEN_8Gb, .dwid = X16,
	.rowbits = 16, .colbits = 10,
	.bits = {
		 C(2),  C(3),  C(4),  C(5),  C(6),  C(7),  B(0),  B(1),  B(2),
		 R(0),  R(1),  R(2),  R(3),  R(4),  R(5),  R(6),  R(7),  R(8),
		 R(9), R(10),  C(8),  C(9), R(11),    RS, R(12), R(13), R(14),
		R(15),     0,     0,     0
	}
},
{
	.addrdec = AMAP_2KB, .dden = DEN_8Gb, .dwid = X8,
	.rowbits = 16, .colbits = 11,
	.bits = {
		 C(2),  C(3),  C(4),  C(5),  C(6),  C(7),  B(0),  B(1),  B(2),
		 R(0),  R(1),  R(2),  R(3),  R(4),  R(5),  R(6),  R(7),  R(8),
		 R(9), R(10),  C(8),  C(9), R(11),    RS, C(11), R(12), R(13),
		R(14), R(15),     0,     0
	}
},
{
	.addrdec = AMAP_4KB, .dden = DEN_4Gb, .dwid = X16,
	.rowbits = 15, .colbits = 10,
	.bits = {
		 C(2),  C(3),  C(4),  C(5),  C(6),  C(7),  C(8),  B(0),  B(1),
		 B(2),  R(0),  R(1),  R(2),  R(3),  R(4),  R(5),  R(6),  R(7),
		 R(8),  R(9), R(10),  C(9), R(11),    RS, R(12), R(13), R(14),
		    0,     0,     0,     0
	}
},
{
	.addrdec = AMAP_4KB, .dden = DEN_4Gb, .dwid = X8,
	.rowbits = 16, .colbits = 10,
	.bits = {
		 C(2),  C(3),  C(4),  C(5),  C(6),  C(7),  C(8),  B(0),  B(1),
		 B(2),  R(0),  R(1),  R(2),  R(3),  R(4),  R(5),  R(6),  R(7),
		 R(8),  R(9), R(10),  C(9), R(11),    RS, R(12), R(13), R(14),
		R(15),     0,     0,     0
	}
},
{
	.addrdec = AMAP_4KB, .dden = DEN_8Gb, .dwid = X16,
	.rowbits = 16, .colbits = 10,
	.bits = {
		 C(2),  C(3),  C(4),  C(5),  C(6),  C(7),  C(8),  B(0),  B(1),
		 B(2),  R(0),  R(1),  R(2),  R(3),  R(4),  R(5),  R(6),  R(7),
		 R(8),  R(9), R(10),  C(9), R(11),    RS, R(12), R(13), R(14),
		R(15),     0,     0,     0
	}
},
{
	.addrdec = AMAP_4KB, .dden = DEN_8Gb, .dwid = X8,
	.rowbits = 16, .colbits = 11,
	.bits = {
		 C(2),  C(3),  C(4),  C(5),  C(6),  C(7),  C(8),  B(0),  B(1),
		 B(2),  R(0),  R(1),  R(2),  R(3),  R(4),  R(5),  R(6),  R(7),
		 R(8),  R(9), R(10),  C(9), R(11),    RS, C(11), R(12), R(13),
		R(14), R(15),     0,     0
	}
}
};

static int bankhash(u64 pmiaddr, int idx, int shft)
{
	int bhash = 0;

	switch (idx) {
	case 0:
		bhash ^= ((pmiaddr >> (12 + shft)) ^
			  (pmiaddr >> (9 + shft))) & 1;
		break;
	case 1:
		bhash ^= (((pmiaddr >> (10 + shft)) ^
			  (pmiaddr >> (8 + shft))) & 1) << 1;
		bhash ^= ((pmiaddr >> 22) & 1) << 1;
		break;
	case 2:
		bhash ^= (((pmiaddr >> (13 + shft)) ^
			  (pmiaddr >> (11 + shft))) & 1) << 2;
		break;
	}
	return bhash;
}

static int rankhash(u64 pmiaddr)
{
	return ((pmiaddr >> 16) ^ (pmiaddr >> 10)) & 1;
}

/*
 * Second stage decode. Compute rank, bank, row & column.
 */
static int pmi2mem(u64 pmiaddr, int g, struct d_cr_drp0 *drp0,
	int *colp, int *bankp, int *rowp, int *rankp, char *msg)
{
	int	i, skiprs = 0;
	struct dimm_geometry *d = &dimms[g];
	int	column = 0, bank = 0, row = 0, rank = 0;
	int	type, idx;

	for (i = 0; i < PMI_ADDRESS_WIDTH; i++) {
		int	bit = (pmiaddr >> i) & 1;

		if (i + skiprs >= PMI_ADDRESS_WIDTH) {
			snprintf(msg, PND2_MSG_SIZE,
				 "bad dimm_geometry[] table\n");
			return -EINVAL;
		}

		type = d->bits[i + skiprs] & ~0xF;
		idx = d->bits[i + skiprs] & 0xf;

		/*
		 * On single rank DIMMs ignore the rank select bit
		 * and shift remainder of "bits[]" down one place.
		 */
		if (type == RS && (drp0->rken0 + drp0->rken1) == 1) {
			skiprs = 1;
			type = d->bits[i + skiprs] & ~0xF;
			idx = d->bits[i + skiprs] & 0xf;
		}
		switch (type) {
		case C(0):
			column |= (bit << idx);
			break;
		case B(0):
			bank |= (bit << idx);
			if (drp0->bahen)
				bank ^= bankhash(pmiaddr, idx, d->addrdec);
			break;
		case R(0):
			row |= (bit << idx);
			break;
		case RS:
			rank = bit;
			if (drp0->rsien)
				rank ^= rankhash(pmiaddr);
			break;
		default:
			if (bit) {
				snprintf(msg, PND2_MSG_SIZE,
					 "bad translation\n");
				return -EINVAL;
			}
			goto done;
		}
	}
done:
	*colp = column;
	*bankp = bank;
	*rowp = row;
	*rankp = rank;

	return 0;
}

static int check_channel(int ch)
{
	if (drp0[ch].dramtype != 0) {
		pnd2_printk(KERN_INFO, "Unsupported dimm in channel %d\n", ch);
		return -EINVAL;
	} else if (drp0[ch].eccen == 0) {
		pnd2_printk(KERN_INFO, "ECC disabled on channel %d\n", ch);
		return -EINVAL;
	}
	return 0;
}

static int check_if_ecc_is_active(void)
{
	int	i, ret = 0;

	/* check dramtype and ECC mode for each present DIMM */
	for (i = 0; i < NUM_CHANNELS; i++)
		if (validmlmcbitvec & BIT(i))
			ret = check_channel(i);
	return ret;
}

static int get_memory_error_data(struct mem_ctl_info *mci,
				 u64 addr,
				 int *chan, int *rankp,
				 int *rowp, int *bankp, int *colp, char *msg)
{
	struct pnd2_pvt *pvt = mci->pvt_info;
	u32			pmiidx;
	u64			pmiaddr;
	int			ret;
	int			rank, row, bank, col;

	ret = sys2pmi(addr, &pmiidx, &pmiaddr, msg);
	if (ret)
		return ret;
	ret = pmi2mem(pmiaddr, pvt->dimm_geom[pmiidx], &drp0[pmiidx],
			&col, &bank, &row, &rank, msg);
	if (ret)
		return ret;

	show_debug_result(addr, pmiidx, pmiaddr, rank, bank, row, col);

	*chan = pmiidx;
	*rankp = rank;
	*rowp = row;
	*bankp = bank;
	*colp = col;

	edac_dbg(0, "pmi:%d pmiaddr:%llx rank:%d row:%d bank:%d col:%d\n",
		pmiidx, pmiaddr, rank, row, bank, col);

	return 0;
}

static void pnd2_mce_output_error(struct mem_ctl_info *mci,
				    const struct mce *m)
{
	enum hw_event_mc_err_type tp_event;
	char *type, *optype, msg[PND2_MSG_SIZE];
	bool ripv = GET_BITFIELD(m->mcgstatus, 0, 0);
	bool overflow = GET_BITFIELD(m->status, 62, 62);
	bool uncorrected_error = GET_BITFIELD(m->status, 61, 61);
	bool recoverable;
	u32 core_err_cnt = GET_BITFIELD(m->status, 38, 52);
	u32 mscod = GET_BITFIELD(m->status, 16, 31);
	u32 errcode = GET_BITFIELD(m->status, 0, 15);
	u32 optypenum = GET_BITFIELD(m->status, 4, 6);
	int chan = -1, rank = -1, row = -1, bank = -1, col = -1;
	int rc;

	recoverable = GET_BITFIELD(m->status, 56, 56);

	if (uncorrected_error) {
		if (ripv) {
			type = "FATAL";
			tp_event = HW_EVENT_ERR_FATAL;
		} else {
			type = "NON_FATAL";
			tp_event = HW_EVENT_ERR_UNCORRECTED;
		}
	} else {
		type = "CORRECTED";
		tp_event = HW_EVENT_ERR_CORRECTED;
	}

	/*
	 * According with Table 15-9 of the Intel Architecture spec vol 3A,
	 * memory errors should fit in this mask:
	 *	000f 0000 1mmm cccc (binary)
	 * where:
	 *	f = Correction Report Filtering Bit. If 1, subsequent errors
	 *	    won't be shown
	 *	mmm = error type
	 *	cccc = channel
	 * If the mask doesn't match, report an error to the parsing logic
	 */
	if (!((errcode & 0xef80) == 0x80)) {
		optype = "Can't parse: it is not a mem";
	} else {
		switch (optypenum) {
		case 0:
			optype = "generic undef request error";
			break;
		case 1:
			optype = "memory read error";
			break;
		case 2:
			optype = "memory write error";
			break;
		case 3:
			optype = "addr/cmd error";
			break;
		case 4:
			optype = "memory scrubbing error";
			break;
		default:
			optype = "reserved";
			break;
		}
	}

	/* Only decode errors with an valid address (ADDRV) */
	if (!GET_BITFIELD(m->status, 58, 58))
		return;

	rc = get_memory_error_data(mci, m->addr, &chan, &rank, &row, &bank,
				   &col, msg);
	if (rc)
		goto address_error;

	snprintf(msg, sizeof(msg),
		 "%s%s err_code:%04x:%04x channel:%d rank:%d row:%d bank:%d col:%d",
		 overflow ? " OVERFLOW" : "",
		 (uncorrected_error && recoverable) ? " recoverable" : "",
		 mscod, errcode,
		 chan, rank, row, bank, col);

	edac_dbg(0, "%s\n", msg);

	/* Call the helper to output message */
	edac_mc_handle_error(tp_event, mci, core_err_cnt,
			     m->addr >> PAGE_SHIFT, m->addr & ~PAGE_MASK, 0,
			     chan, 0, -1,
			     optype, msg);
	return;

address_error:
	edac_mc_handle_error(tp_event, mci, core_err_cnt, 0, 0, 0,
				-1, -1, -1,
				msg, "");
}

static void get_dimm_config(struct mem_ctl_info *mci)
{
	int	i, g;
	u64	capacity;
	struct dimm_info *dimm;
	struct pnd2_pvt	*pvt = mci->pvt_info;
	struct d_cr_drp0 *d;

	for (i = 0; i < NUM_CHANNELS; i++) {
		if (!(validmlmcbitvec & BIT(i)))
			continue;
		dimm = EDAC_DIMM_PTR(mci->layers, mci->dimms, mci->n_layers,
				     i, 0, 0);
		if (!dimm) {
			edac_dbg(0, "No allocated dimm for channel %d\n", i);
			continue;
		}
		d = &drp0[i];
		for (g = 0; g < ARRAY_SIZE(dimms); g++)
			if (dimms[g].addrdec == d->addrdec &&
			    dimms[g].dden == d->dden &&
			    dimms[g].dwid == d->dwid)
				break;
		if (g == ARRAY_SIZE(dimms)) {
			edac_dbg(0, "channel %d: unrecognized dimm\n", i);
			continue;
		}
		pvt->dimm_geom[i] = g;

		capacity =
			(d->rken0 + d->rken1) *		/* ranks */
			8 *				/* banks */
			(1ul << dimms[g].rowbits) *	/* rows */
			(1ul << dimms[g].colbits);	/* columns */
		edac_dbg(0, "channel %d: %lld MByte dimm\n", i,
			 capacity >> (20 - 3));
		dimm->nr_pages = MiB_TO_PAGES(capacity >> (20 - 3));
		dimm->grain = 32;
		dimm->dtype = (d->dwid == 0) ? DEV_X8 : DEV_X16;
		dimm->mtype = MEM_DDR3;
		dimm->edac_mode = EDAC_SECDED;
		snprintf(dimm->label, sizeof(dimm->label),
			"slice%d_chan%d", i/2, i%2);
	}
}

static int pnd2_register_mci(struct pnd2_dev *pnd2_dev)
{
	struct mem_ctl_info *mci;
	struct edac_mc_layer layers[2];
	struct pnd2_pvt *pvt;
	int rc;

	rc = check_if_ecc_is_active();
	if (rc < 0)
		return rc;

	/* allocate a new MC control structure */
	layers[0].type = EDAC_MC_LAYER_CHANNEL;
	layers[0].size = NUM_CHANNELS;
	layers[0].is_virt_csrow = false;
	layers[1].type = EDAC_MC_LAYER_SLOT;
	layers[1].size = 1; /* Only one DIMM per channel */
	layers[1].is_virt_csrow = true;
	mci = edac_mc_alloc(0, ARRAY_SIZE(layers), layers,
			    sizeof(*pvt));
	if (!mci)
		return -ENOMEM;

	pvt = mci->pvt_info;
	memset(pvt, 0, sizeof(*pvt));

	pvt->pnd2_dev = pnd2_dev;

	mci->mod_name = "pnd2_edac.c";
	mci->dev_name = "pnd2";
	mci->ctl_name = "Pondicherry2";

	/* Get dimm basic config and the memory layout */
	get_dimm_config(mci);

	pnd2_dev->mci = mci;

	if (edac_mc_add_mc(mci)) {
		edac_dbg(0, "MC: failed edac_mc_add_mc()\n");
		rc = -EINVAL;
		goto fail;
	}

	return 0;
fail:
	kfree(mci->ctl_name);
	edac_mc_free(mci);
	pnd2_dev->mci = NULL;
	return rc;
}

static void pnd2_unregister_mci(struct pnd2_dev *pnd2_dev)
{
	struct mem_ctl_info *mci = pnd2_dev->mci;
	struct pnd2_pvt *pvt;

	if (unlikely(!mci || !mci->pvt_info)) {
		pnd2_printk(KERN_ERR, "Couldn't find mci handler\n");
		return;
	}

	pvt = mci->pvt_info;

	/* Remove MC sysfs nodes */
	edac_mc_del_mc(NULL);

	edac_dbg(1, "%s: free mci struct\n", mci->ctl_name);
	edac_mc_free(mci);
	pnd2_dev->mci = NULL;
}

/*
 * Callback function registered with core kernel mce code.
 * Called once for each logged error.
 */
static int pnd2_mce_check_error(struct notifier_block *nb, unsigned long val,
				void *data)
{
	struct mce *mce = (struct mce *)data;
	struct mem_ctl_info *mci;
	struct pnd2_pvt *pvt;
	char *type;

	if (get_edac_report_status() == EDAC_REPORTING_DISABLED)
		return NOTIFY_DONE;

	mci = pnd2_dev.mci;
	if (!mci)
		return NOTIFY_DONE;
	pvt = mci->pvt_info;

	/*
	 * Just let mcelog handle it if the error is
	 * outside the memory controller. A memory error
	 * is indicated by bit 7 = 1 and bits = 8-11,13-15 = 0.
	 * bit 12 has an special meaning.
	 */
	if ((mce->status & 0xefff) >> 7 != 1)
		return NOTIFY_DONE;

	if (mce->mcgstatus & MCG_STATUS_MCIP)
		type = "Exception";
	else
		type = "Event";

	pnd2_mc_printk(mci, KERN_DEBUG, "HANDLING MCE MEMORY ERROR\n");

	pnd2_mc_printk(mci, KERN_DEBUG,
			"CPU %d: Machine Check %s: %Lx Bank %d: %016Lx\n",
			mce->extcpu, type,
			mce->mcgstatus, mce->bank, mce->status);
	pnd2_mc_printk(mci, KERN_DEBUG, "TSC %llx ", mce->tsc);
	pnd2_mc_printk(mci, KERN_DEBUG, "ADDR %llx ", mce->addr);
	pnd2_mc_printk(mci, KERN_DEBUG, "MISC %llx ", mce->misc);

	pnd2_mc_printk(mci, KERN_DEBUG,
			"PROCESSOR %u:%x TIME %llu SOCKET %u APIC %x\n",
			mce->cpuvendor, mce->cpuid,
			mce->time, mce->socketid, mce->apicid);

	pnd2_mce_output_error(mci, mce);

	/* Advice mcelog that the error were handled */
	return NOTIFY_STOP;
}

static struct notifier_block pnd2_mce_dec = {
	.notifier_call	= pnd2_mce_check_error,
};

#ifdef CONFIG_EDAC_DEBUG
/*
 * Debug feature. Make /sys/kernel/debug/pnd2_test/pnd2_debug_addr.
 * Write an address to this file to exercise the address decode
 * logic in this driver.
 */
static struct dentry *pnd2_test;
static u64 pnd2_fake_addr;
#define PND2_BLOB_SIZE 1024
static char pnd2_result[PND2_BLOB_SIZE];
static struct debugfs_blob_wrapper pnd2_blob = {
	.data = pnd2_result,
	.size = 0
};

static void show_debug_result(u64 addr, int channel, u64 pmiaddr, int rank,
			      int bank, int row, int col)
{
	snprintf(pnd2_blob.data, PND2_BLOB_SIZE,
	"Addr= %llx\nchannel= %x\npmiAddr=%llx\nRANK= %x\nBANK= %x\nROW= %x\nCOL= %x\n",
	addr, channel, pmiaddr, rank, bank, row, col);
	pnd2_blob.size = strlen(pnd2_blob.data);
}

static int debugfs_u64_set(void *data, u64 val)
{
	struct mce m;

	*(u64 *)data = val;
	m.mcgstatus = 0;
	m.status = BIT_ULL(58) + 0x9F ; /* ADDRV + MemRd + Unknown channel */
	m.addr = val;
	pnd2_mce_output_error(pnd2_dev.mci, &m);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_u64_wo, NULL, debugfs_u64_set, "%llu\n");

static struct dentry *mydebugfs_create(const char *name, umode_t mode,
				       struct dentry *parent, u64 *value)
{
	return debugfs_create_file(name, mode, parent, value, &fops_u64_wo);
}

static void setup_pnd2_debug(void)
{
	pnd2_test = debugfs_create_dir("pnd2_test", NULL);
	mydebugfs_create("pnd2_debug_addr", S_IWUSR, pnd2_test,
			 &pnd2_fake_addr);
	debugfs_create_blob("pnd2_debug_results", S_IRUSR, pnd2_test,
			    &pnd2_blob);
}

static void teardown_pnd2_debug(void)
{
	debugfs_remove_recursive(pnd2_test);
}
#else
static void show_debug_result(u64 addr, int channel, u64 pmiaddr, int rank,
			      int bank, int row, int col)
{
}

static void setup_pnd2_debug(void)
{
}

static void teardown_pnd2_debug(void)
{
}
#endif

static int pnd2_probe(const struct x86_cpu_id *id)
{
	edac_dbg(2, "\n");

	get_registers();
	return pnd2_register_mci(&pnd2_dev);
}

static void pnd2_remove(void)
{
	edac_dbg(0, "\n");

	pnd2_unregister_mci(&pnd2_dev);
}

static const struct x86_cpu_id pnd2_cpuids[] = {
	{ X86_VENDOR_INTEL, 6, 0x5c, 0, 0 },	/* Goldmont */
	{ }
};
MODULE_DEVICE_TABLE(x86cpu, pnd2_cpuids);

static int __init pnd2_init(void)
{
	const struct x86_cpu_id *id;
	int rc;

	edac_dbg(2, "\n");

	id = x86_match_cpu(pnd2_cpuids);
	if (!id)
		return -ENODEV;

	/* Ensure that the OPSTATE is set correctly for POLL or NMI */
	opstate_init();

	rc = pnd2_probe(id);
	if (rc < 0) {
		pnd2_printk(KERN_ERR, "Failed to register device with error %d.\n",
			rc);
		return rc;
	}

	if (pnd2_dev.mci) {
		mce_register_decode_chain(&pnd2_mce_dec);
		setup_pnd2_debug();
		return 0;
	}

	return -ENODEV;
}

static void __exit pnd2_exit(void)
{
	edac_dbg(2, "\n");
	pnd2_remove();
	mce_unregister_decode_chain(&pnd2_mce_dec);
	teardown_pnd2_debug();
}

module_init(pnd2_init);
module_exit(pnd2_exit);

module_param(edac_op_state, int, 0444);
MODULE_PARM_DESC(edac_op_state, "EDAC Error Reporting state: 0=Poll,1=NMI");

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Tony Luck");
MODULE_DESCRIPTION("MC Driver for Intel SoC using Pondicherry memory controller");
