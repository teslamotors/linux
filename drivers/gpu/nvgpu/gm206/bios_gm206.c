/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/firmware.h>

#include "gk20a/gk20a.h"
#include "gm20b/fifo_gm20b.h"
#include "fifo_gm206.h"
#include "hw_pwr_gm206.h"
#include "hw_mc_gm206.h"
#include "hw_xve_gm206.h"
#include "hw_top_gm206.h"
#include "bios_gm206.h"
#include "nvgpu_common.h"
#include "gp106/pmu_mclk_gp106.h"

#define BIT_HEADER_ID 0xb8ff
#define BIT_HEADER_SIGNATURE 0x00544942
#define BIOS_SIZE 0x40000
#define PCI_EXP_ROM_SIG 0xaa55
#define PCI_EXP_ROM_SIG_NV 0x4e56
#define ROM_FILE_PAYLOAD_OFFSET 0xa00
#define PMU_BOOT_TIMEOUT_DEFAULT	100 /* usec */
#define PMU_BOOT_TIMEOUT_MAX		2000000 /* usec */
#define BIOS_OVERLAY_NAME "bios-%04x.rom"
#define BIOS_OVERLAY_NAME_FORMATTED "bios-xxxx.rom"

static u16 gm206_bios_rdu16(struct gk20a *g, int offset)
{
	u16 val = (g->bios.data[offset+1] << 8) + g->bios.data[offset];
	return val;
}

static u32 gm206_bios_rdu32(struct gk20a *g, int offset)
{
	u32 val = (g->bios.data[offset+3] << 24) +
		  (g->bios.data[offset+2] << 16) +
		  (g->bios.data[offset+1] << 8) +
		  g->bios.data[offset];
	return val;
}

struct bit {
	u16 id;
	u32 signature;
	u16 bcd_version;
	u8 header_size;
	u8 token_size;
	u8 token_entries;
	u8 header_checksum;
} __packed;

#define TOKEN_ID_BIOSDATA 0x42
#define TOKEN_ID_NVINIT_PTRS 0x49
#define TOKEN_ID_FALCON_DATA 0x70
#define TOKEN_ID_PERF_PTRS 0x50
#define TOKEN_ID_CLOCK_PTRS 0x43
#define TOKEN_ID_VIRT_PTRS 0x56
#define TOKEN_ID_MEMORY_PTRS 0x4D

#define MEMORY_PTRS_V1 1
#define MEMORY_PTRS_V2 2

struct memory_ptrs_v1 {
	u8 rsvd0[2];
	u8 mem_strap_data_count;
	u16 mem_strap_xlat_tbl_ptr;
	u8 rsvd1[8];
} __packed;

struct memory_ptrs_v2 {
	u8 mem_strap_data_count;
	u16 mem_strap_xlat_tbl_ptr;
	u8 rsvd[14];
} __packed;

struct biosdata {
	u32 version;
	u8 oem_version;
	u8 checksum;
	u16 int15callbackspost;
	u16 int16callbackssystem;
	u16 boardid;
	u16 framecount;
	u8 biosmoddate[8];
} __packed;

struct nvinit_ptrs {
	u16 initscript_table_ptr;
	u16 macro_index_table_ptr;
	u16 macro_table_ptr;
	u16 condition_table_ptr;
	u16 io_condition_table_ptr;
	u16 io_flag_condition_table_ptr;
	u16 init_function_table_ptr;
	u16 vbios_private_table_ptr;
	u16 data_arrays_table_ptr;
	u16 pcie_settings_script_ptr;
	u16 devinit_tables_ptr;
	u16 devinit_tables_size;
	u16 bootscripts_ptr;
	u16 bootscripts_size;
	u16 nvlink_config_data_ptr;
} __packed;

struct falcon_data_v2 {
	u32 falcon_ucode_table_ptr;
} __packed;

struct falcon_ucode_table_hdr_v1 {
	u8 version;
	u8 header_size;
	u8 entry_size;
	u8 entry_count;
	u8 desc_version;
	u8 desc_size;
} __packed;

struct falcon_ucode_table_entry_v1 {
	u8 application_id;
	u8 target_id;
	u32 desc_ptr;
} __packed;

#define TARGET_ID_PMU 0x01
#define APPLICATION_ID_DEVINIT 0x04
#define APPLICATION_ID_PRE_OS 0x01

struct falcon_ucode_desc_v1 {
	union {
		u32 v_desc;
		u32 stored_size;
	} hdr_size;
	u32 uncompressed_size;
	u32 virtual_entry;
	u32 interface_offset;
	u32 imem_phys_base;
	u32 imem_load_size;
	u32 imem_virt_base;
	u32 imem_sec_base;
	u32 imem_sec_size;
	u32 dmem_offset;
	u32 dmem_phys_base;
	u32 dmem_load_size;
} __packed;

struct application_interface_table_hdr_v1 {
	u8 version;
	u8 header_size;
	u8 entry_size;
	u8 entry_count;
} __packed;

struct application_interface_entry_v1 {
	u32 id;
	u32 dmem_offset;
} __packed;

#define APPINFO_ID_DEVINIT 0x01

struct devinit_engine_interface {
	u32 field0;
	u32 field1;
	u32 tables_phys_base;
	u32 tables_virt_base;
	u32 script_phys_base;
	u32 script_virt_base;
	u32 script_virt_entry;
	u16 script_size;
	u8 memory_strap_count;
	u8 reserved;
	u32 memory_information_table_virt_base;
	u32 empty_script_virt_base;
	u32 cond_table_virt_base;
	u32 io_cond_table_virt_base;
	u32 data_arrays_table_virt_base;
	u32 gpio_assignment_table_virt_base;
} __packed;

struct pci_exp_rom {
	u16 sig;
	u8 reserved[0x16];
	u16 pci_data_struct_ptr;
	u32 size_of_block;
} __packed;

struct pci_data_struct {
	u32 sig;
	u16 vendor_id;
	u16 device_id;
	u16 device_list_ptr;
	u16 pci_data_struct_len;
	u8 pci_data_struct_rev;
	u8 class_code[3];
	u16 image_len;
	u16 vendor_rom_rev;
	u8 code_type;
	u8 last_image;
	u16 max_runtime_image_len;
} __packed;

struct pci_ext_data_struct {
	u32 sig;
	u16 nv_pci_data_ext_rev;
	u16 nv_pci_data_ext_len;
	u16 sub_image_len;
	u8 priv_last_image;
	u8 flags;
} __packed;

static int gm206_bios_parse_rom(struct gk20a *g)
{
	int offset = 0;
	int last = 0;

	while (!last) {
		struct pci_exp_rom *pci_rom;
		struct pci_data_struct *pci_data;
		struct pci_ext_data_struct *pci_ext_data;

		pci_rom = (struct pci_exp_rom *)&g->bios.data[offset];
		gk20a_dbg_fn("pci rom sig %04x ptr %04x block %x",
				pci_rom->sig, pci_rom->pci_data_struct_ptr,
				pci_rom->size_of_block);

		if (pci_rom->sig != PCI_EXP_ROM_SIG &&
		    pci_rom->sig != PCI_EXP_ROM_SIG_NV) {
			gk20a_err(g->dev, "invalid VBIOS signature");
			return -EINVAL;
		}

		pci_data =
			(struct pci_data_struct *)
			&g->bios.data[offset + pci_rom->pci_data_struct_ptr];
		gk20a_dbg_fn("pci data sig %08x len %d image len %x type %x last %d max %08x",
				pci_data->sig, pci_data->pci_data_struct_len,
				pci_data->image_len, pci_data->code_type,
				pci_data->last_image,
				pci_data->max_runtime_image_len);

		if (pci_data->code_type == 0x3) {
			pci_ext_data = (struct pci_ext_data_struct *)
				&g->bios.data[(offset +
					       pci_rom->pci_data_struct_ptr +
					       pci_data->pci_data_struct_len +
					       0xf)
					      & ~0xf];
			gk20a_dbg_fn("pci ext data sig %08x rev %x len %x sub_image_len %x priv_last %d flags %x",
					pci_ext_data->sig,
					pci_ext_data->nv_pci_data_ext_rev,
					pci_ext_data->nv_pci_data_ext_len,
					pci_ext_data->sub_image_len,
					pci_ext_data->priv_last_image,
					pci_ext_data->flags);

			gk20a_dbg_fn("expansion rom offset %x",
					pci_data->image_len * 512);
			g->bios.expansion_rom_offset =
				pci_data->image_len * 512;
			offset += pci_ext_data->sub_image_len * 512;
			last = pci_ext_data->priv_last_image;
		} else {
			offset += pci_data->image_len * 512;
			last = pci_data->last_image;
		}
	}

	return 0;
}

static void gm206_bios_parse_biosdata(struct gk20a *g, int offset)
{
	struct biosdata biosdata;

	memcpy(&biosdata, &g->bios.data[offset], sizeof(biosdata));
	gk20a_dbg_fn("bios version %x, oem version %x",
			biosdata.version,
			biosdata.oem_version);

	g->gpu_characteristics.vbios_version = biosdata.version;
	g->gpu_characteristics.vbios_oem_version = biosdata.oem_version;
}

static void gm206_bios_parse_nvinit_ptrs(struct gk20a *g, int offset)
{
	struct nvinit_ptrs nvinit_ptrs;

	memcpy(&nvinit_ptrs, &g->bios.data[offset], sizeof(nvinit_ptrs));
	gk20a_dbg_fn("devinit ptr %x size %d", nvinit_ptrs.devinit_tables_ptr,
			nvinit_ptrs.devinit_tables_size);
	gk20a_dbg_fn("bootscripts ptr %x size %d", nvinit_ptrs.bootscripts_ptr,
			nvinit_ptrs.bootscripts_size);

	g->bios.devinit_tables = &g->bios.data[nvinit_ptrs.devinit_tables_ptr];
	g->bios.devinit_tables_size = nvinit_ptrs.devinit_tables_size;
	g->bios.bootscripts = &g->bios.data[nvinit_ptrs.bootscripts_ptr];
	g->bios.bootscripts_size = nvinit_ptrs.bootscripts_size;
	g->bios.condition_table_ptr = nvinit_ptrs.condition_table_ptr;
}

static void gm206_bios_parse_memory_ptrs(struct gk20a *g, int offset, u8 version)
{
	struct memory_ptrs_v1 v1;
	struct memory_ptrs_v2 v2;

	switch (version) {
	case MEMORY_PTRS_V1:
		memcpy(&v1, &g->bios.data[offset], sizeof(v1));
		g->bios.mem_strap_data_count = v1.mem_strap_data_count;
		g->bios.mem_strap_xlat_tbl_ptr = v1.mem_strap_xlat_tbl_ptr;
		return;
	case MEMORY_PTRS_V2:
		memcpy(&v2, &g->bios.data[offset], sizeof(v2));
		g->bios.mem_strap_data_count = v2.mem_strap_data_count;
		g->bios.mem_strap_xlat_tbl_ptr = v2.mem_strap_xlat_tbl_ptr;
		return;
	default:
		gk20a_err(dev_from_gk20a(g),
			"unknown vbios memory table version %x", version);
		return;
	}
}

static void gm206_bios_parse_devinit_appinfo(struct gk20a *g, int dmem_offset)
{
	struct devinit_engine_interface interface;

	memcpy(&interface, &g->bios.devinit.dmem[dmem_offset], sizeof(interface));
	gk20a_dbg_fn("devinit tables phys %x script phys %x size %d",
			interface.tables_phys_base,
			interface.script_phys_base,
			interface.script_size);

	g->bios.devinit_tables_phys_base = interface.tables_phys_base;
	g->bios.devinit_script_phys_base = interface.script_phys_base;
}

static int gm206_bios_parse_appinfo_table(struct gk20a *g, int offset)
{
	struct application_interface_table_hdr_v1 hdr;
	int i;

	memcpy(&hdr, &g->bios.data[offset], sizeof(hdr));

	gk20a_dbg_fn("appInfoHdr ver %d size %d entrySize %d entryCount %d",
			hdr.version, hdr.header_size,
			hdr.entry_size, hdr.entry_count);

	if (hdr.version != 1)
		return 0;

	offset += sizeof(hdr);
	for (i = 0; i < hdr.entry_count; i++) {
		struct application_interface_entry_v1 entry;

		memcpy(&entry, &g->bios.data[offset], sizeof(entry));

		gk20a_dbg_fn("appInfo id %d dmem_offset %d",
				entry.id, entry.dmem_offset);

		if (entry.id == APPINFO_ID_DEVINIT)
			gm206_bios_parse_devinit_appinfo(g, entry.dmem_offset);

		offset += hdr.entry_size;
	}

	return 0;
}

static int gm206_bios_parse_falcon_ucode_desc(struct gk20a *g,
		struct nvgpu_bios_ucode *ucode, int offset)
{
	struct falcon_ucode_desc_v1 desc;

	memcpy(&desc, &g->bios.data[offset], sizeof(desc));
	gk20a_dbg_info("falcon ucode desc stored size %d uncompressed size %d",
			desc.hdr_size.stored_size, desc.uncompressed_size);
	gk20a_dbg_info("falcon ucode desc virtualEntry %x, interfaceOffset %x",
			desc.virtual_entry, desc.interface_offset);
	gk20a_dbg_info("falcon ucode IMEM phys base %x, load size %x virt base %x sec base %x sec size %x",
			desc.imem_phys_base, desc.imem_load_size,
			desc.imem_virt_base, desc.imem_sec_base,
			desc.imem_sec_size);
	gk20a_dbg_info("falcon ucode DMEM offset %d phys base %x, load size %d",
			desc.dmem_offset, desc.dmem_phys_base,
			desc.dmem_load_size);

	if (desc.hdr_size.stored_size != desc.uncompressed_size) {
		gk20a_dbg_info("does not match");
		return -EINVAL;
	}

	ucode->code_entry_point = desc.virtual_entry;
	ucode->bootloader = &g->bios.data[offset] + sizeof(desc);
	ucode->bootloader_phys_base = desc.imem_phys_base;
	ucode->bootloader_size = desc.imem_load_size - desc.imem_sec_size;
	ucode->ucode = ucode->bootloader + ucode->bootloader_size;
	ucode->phys_base = ucode->bootloader_phys_base + ucode->bootloader_size;
	ucode->size = desc.imem_sec_size;
	ucode->dmem = ucode->bootloader + desc.dmem_offset;
	ucode->dmem_phys_base = desc.dmem_phys_base;
	ucode->dmem_size = desc.dmem_load_size;

	return gm206_bios_parse_appinfo_table(g,
			offset + sizeof(desc) +
			desc.dmem_offset + desc.interface_offset);
}

static int gm206_bios_parse_falcon_ucode_table(struct gk20a *g, int offset)
{
	struct falcon_ucode_table_hdr_v1 hdr;
	int i;

	memcpy(&hdr, &g->bios.data[offset], sizeof(hdr));
	gk20a_dbg_fn("falcon ucode table ver %d size %d entrySize %d entryCount %d descVer %d descSize %d",
			hdr.version, hdr.header_size,
			hdr.entry_size, hdr.entry_count,
			hdr.desc_version, hdr.desc_size);

	if (hdr.version != 1)
		return -EINVAL;

	offset += hdr.header_size;

	for (i = 0; i < hdr.entry_count; i++) {
		struct falcon_ucode_table_entry_v1 entry;

		memcpy(&entry, &g->bios.data[offset], sizeof(entry));

		gk20a_dbg_fn("falcon ucode table entry appid %x targetId %x descPtr %x",
				entry.application_id, entry.target_id,
				entry.desc_ptr);

		if (entry.target_id == TARGET_ID_PMU &&
		    entry.application_id == APPLICATION_ID_DEVINIT) {
			int err;

			err = gm206_bios_parse_falcon_ucode_desc(g,
					&g->bios.devinit, entry.desc_ptr);
			if (err)
				err = gm206_bios_parse_falcon_ucode_desc(g,
					&g->bios.devinit,
					entry.desc_ptr +
					 g->bios.expansion_rom_offset);

			if (err)
				gk20a_err(dev_from_gk20a(g),
					  "could not parse devinit ucode desc");
		} else if (entry.target_id == TARGET_ID_PMU &&
		    entry.application_id == APPLICATION_ID_PRE_OS) {
			int err;

			err = gm206_bios_parse_falcon_ucode_desc(g,
					&g->bios.preos, entry.desc_ptr);
			if (err)
				err = gm206_bios_parse_falcon_ucode_desc(g,
					&g->bios.preos,
					entry.desc_ptr +
					 g->bios.expansion_rom_offset);

			if (err)
				gk20a_err(dev_from_gk20a(g),
					  "could not parse preos ucode desc");
		}

		offset += hdr.entry_size;
	}

	return 0;
}

static void gm206_bios_parse_falcon_data_v2(struct gk20a *g, int offset)
{
	struct falcon_data_v2 falcon_data;
	int err;

	memcpy(&falcon_data, &g->bios.data[offset], sizeof(falcon_data));
	gk20a_dbg_fn("falcon ucode table ptr %x",
			falcon_data.falcon_ucode_table_ptr);
	err = gm206_bios_parse_falcon_ucode_table(g,
			falcon_data.falcon_ucode_table_ptr);
	if (err)
		err = gm206_bios_parse_falcon_ucode_table(g,
				falcon_data.falcon_ucode_table_ptr +
			g->bios.expansion_rom_offset);

	if (err)
		gk20a_err(dev_from_gk20a(g),
			  "could not parse falcon ucode table");
}

static void *gm206_bios_get_perf_table_ptrs(struct gk20a *g,
		struct bit_token *ptoken, u8 table_id)
{
	u32 perf_table_id_offset = 0;
	u8 *perf_table_ptr = NULL;
	u8 data_size = 4;

	if (ptoken != NULL) {

		if (ptoken->token_id == TOKEN_ID_VIRT_PTRS) {
			perf_table_id_offset = *((u16 *)&g->bios.data[
				ptoken->data_ptr +
				(table_id * PERF_PTRS_WIDTH_16)]);
			data_size = PERF_PTRS_WIDTH_16;
		} else {
			perf_table_id_offset = *((u32 *)&g->bios.data[
				ptoken->data_ptr +
				(table_id * PERF_PTRS_WIDTH)]);
			data_size = PERF_PTRS_WIDTH;
		}
	} else
		return (void *)perf_table_ptr;

	if (table_id < (ptoken->data_size/data_size)) {

		gk20a_dbg_info("Perf_Tbl_ID-offset 0x%x Tbl_ID_Ptr-offset- 0x%x",
					(ptoken->data_ptr +
					(table_id * data_size)),
					perf_table_id_offset);

		if (perf_table_id_offset != 0) {
			/* check is perf_table_id_offset is > 64k */
			if (perf_table_id_offset & ~0xFFFF)
				perf_table_ptr =
					&g->bios.data[g->bios.expansion_rom_offset +
						perf_table_id_offset];
			else
				perf_table_ptr =
					&g->bios.data[perf_table_id_offset];
		} else
			gk20a_warn(g->dev, "PERF TABLE ID %d is NULL",
					table_id);
	} else
		gk20a_warn(g->dev, "INVALID PERF TABLE ID - %d ", table_id);

	return (void *)perf_table_ptr;
}

static void gm206_bios_parse_bit(struct gk20a *g, int offset)
{
	struct bit bit;
	struct bit_token bit_token;
	int i;

	gk20a_dbg_fn("");
	memcpy(&bit, &g->bios.data[offset], sizeof(bit));

	gk20a_dbg_info("BIT header: %04x %08x", bit.id, bit.signature);
	gk20a_dbg_info("tokens: %d entries * %d bytes",
			bit.token_entries, bit.token_size);

	offset += bit.header_size;
	for (i = 0; i < bit.token_entries; i++) {
		memcpy(&bit_token, &g->bios.data[offset], sizeof(bit_token));

		gk20a_dbg_info("BIT token id %d ptr %d size %d ver %d",
				bit_token.token_id, bit_token.data_ptr,
				bit_token.data_size, bit_token.data_version);

		switch (bit_token.token_id) {
		case TOKEN_ID_BIOSDATA:
			gm206_bios_parse_biosdata(g, bit_token.data_ptr);
			break;
		case TOKEN_ID_NVINIT_PTRS:
			gm206_bios_parse_nvinit_ptrs(g, bit_token.data_ptr);
			break;
		case TOKEN_ID_FALCON_DATA:
			if (bit_token.data_version == 2)
				gm206_bios_parse_falcon_data_v2(g,
						bit_token.data_ptr);
			break;
		case TOKEN_ID_PERF_PTRS:
			g->bios.perf_token =
				(struct bit_token *)&g->bios.data[offset];
			break;
		case TOKEN_ID_CLOCK_PTRS:
			g->bios.clock_token =
				(struct bit_token *)&g->bios.data[offset];
			break;
		case TOKEN_ID_VIRT_PTRS:
			g->bios.virt_token =
				(struct bit_token *)&g->bios.data[offset];
			break;
		case TOKEN_ID_MEMORY_PTRS:
			gm206_bios_parse_memory_ptrs(g, bit_token.data_ptr,
				bit_token.data_version);
		default:
			break;
		}

		offset += bit.token_size;
	}
	gk20a_dbg_fn("done");
}

static u32 __gm206_bios_readbyte(struct gk20a *g, u32 offset)
{
	return (u32) g->bios.data[offset];
}

u8 gm206_bios_read_u8(struct gk20a *g, u32 offset)
{
	return (u8) __gm206_bios_readbyte(g, offset);
}

s8 gm206_bios_read_s8(struct gk20a *g, u32 offset)
{
	u32 val;
	val = __gm206_bios_readbyte(g, offset);
	val = val & 0x80 ? (val | ~0xff) : val;

	return (s8) val;
}

u16 gm206_bios_read_u16(struct gk20a *g, u32 offset)
{
	u16 val;

	val = __gm206_bios_readbyte(g, offset) |
		(__gm206_bios_readbyte(g, offset+1) << 8);

	return val;
}

u32 gm206_bios_read_u32(struct gk20a *g, u32 offset)
{
	u32 val;

	val = __gm206_bios_readbyte(g, offset) |
		(__gm206_bios_readbyte(g, offset+1) << 8) |
		(__gm206_bios_readbyte(g, offset+2) << 16) |
		(__gm206_bios_readbyte(g, offset+3) << 24);

	return val;
}

static void upload_code(struct gk20a *g, u32 dst,
			u8 *src, u32 size, u8 port, bool sec)
{
	u32 i, words;
	u32 *src_u32 = (u32 *)src;
	u32 blk;
	u32 tag = 0;

	gk20a_dbg_info("upload %d bytes to %x", size, dst);

	words = size >> 2;

	blk = dst >> 8;
	tag = blk;

	gk20a_dbg_info("upload %d words to %x block %d",
			words, dst, blk);

	gk20a_writel(g, pwr_falcon_imemc_r(port),
		pwr_falcon_imemc_offs_f(dst >> 2) |
		pwr_falcon_imemc_blk_f(blk) |
		pwr_falcon_imemc_aincw_f(1) |
		sec << 28);

	for (i = 0; i < words; i++) {
		if (i % 64 == 0) {
			gk20a_writel(g, 0x10a188, tag);
			tag++;
		}

		gk20a_writel(g, pwr_falcon_imemd_r(port), src_u32[i]);
	}

	while (i % 64) {
		gk20a_writel(g, pwr_falcon_imemd_r(port), 0);
		i++;
	}
}

static void upload_data(struct gk20a *g, u32 dst, u8 *src, u32 size, u8 port)
{
	u32 i, words;
	u32 *src_u32 = (u32 *)src;
	u32 blk;

	gk20a_dbg_info("upload %d bytes to %x", size, dst);

	words = DIV_ROUND_UP(size, 4);

	blk = dst >> 8;

	gk20a_dbg_info("upload %d words to %x blk %d",
			words, dst, blk);
	gk20a_writel(g, pwr_falcon_dmemc_r(port),
		pwr_falcon_dmemc_offs_f(dst >> 2) |
		pwr_falcon_dmemc_blk_f(blk) |
		pwr_falcon_dmemc_aincw_f(1));

	for (i = 0; i < words; i++)
		gk20a_writel(g, pwr_falcon_dmemd_r(port), src_u32[i]);
}

static int gm206_bios_devinit(struct gk20a *g)
{
	int retries = PMU_BOOT_TIMEOUT_MAX / PMU_BOOT_TIMEOUT_DEFAULT;
	int err = 0;
	int devinit_completed;

	gk20a_dbg_fn("");
	g->ops.pmu.reset(g);

	do {
		u32 w = gk20a_readl(g, pwr_falcon_dmactl_r()) &
			(pwr_falcon_dmactl_dmem_scrubbing_m() |
			 pwr_falcon_dmactl_imem_scrubbing_m());

		if (!w) {
			gk20a_dbg_fn("done");
			break;
		}
		udelay(PMU_BOOT_TIMEOUT_DEFAULT);
	} while (--retries || !tegra_platform_is_silicon());

	/*  todo check retries */
	upload_code(g, g->bios.devinit.bootloader_phys_base,
			g->bios.devinit.bootloader,
			g->bios.devinit.bootloader_size,
			0, 0);
	upload_code(g, g->bios.devinit.phys_base,
			g->bios.devinit.ucode,
			g->bios.devinit.size,
			0, 1);
	upload_data(g, g->bios.devinit.dmem_phys_base,
			g->bios.devinit.dmem,
			g->bios.devinit.dmem_size,
			0);
	upload_data(g, g->bios.devinit_tables_phys_base,
			g->bios.devinit_tables,
			g->bios.devinit_tables_size,
			0);
	upload_data(g, g->bios.devinit_script_phys_base,
			g->bios.bootscripts,
			g->bios.bootscripts_size,
			0);

	gk20a_writel(g, pwr_falcon_bootvec_r(),
		pwr_falcon_bootvec_vec_f(g->bios.devinit.code_entry_point));
	gk20a_writel(g, pwr_falcon_dmactl_r(),
			pwr_falcon_dmactl_require_ctx_f(0));
	gk20a_writel(g, pwr_falcon_cpuctl_r(),
		pwr_falcon_cpuctl_startcpu_f(1));

	retries = PMU_BOOT_TIMEOUT_MAX / PMU_BOOT_TIMEOUT_DEFAULT;
	do {
		devinit_completed = pwr_falcon_cpuctl_halt_intr_v(
				gk20a_readl(g, pwr_falcon_cpuctl_r())) &&
				    top_scratch1_devinit_completed_v(
				gk20a_readl(g, top_scratch1_r()));
		udelay(PMU_BOOT_TIMEOUT_DEFAULT);
	} while (!devinit_completed && retries--);

	gk20a_writel(g, pwr_falcon_irqsclr_r(),
	     pwr_falcon_irqstat_halt_true_f());
	gk20a_readl(g, pwr_falcon_irqsclr_r());

	if (!retries)
		err = -EINVAL;

	gk20a_dbg_fn("done");
	return err;
}

static int gm206_bios_preos(struct gk20a *g)
{
	int retries = PMU_BOOT_TIMEOUT_MAX / PMU_BOOT_TIMEOUT_DEFAULT;
	int err = 0;
	int val;

	gk20a_dbg_fn("");
	g->ops.pmu.reset(g);

	do {
		u32 w = gk20a_readl(g, pwr_falcon_dmactl_r()) &
			(pwr_falcon_dmactl_dmem_scrubbing_m() |
			 pwr_falcon_dmactl_imem_scrubbing_m());

		if (!w) {
			gk20a_dbg_fn("done");
			break;
		}
		udelay(PMU_BOOT_TIMEOUT_DEFAULT);
	} while (--retries || !tegra_platform_is_silicon());

	/*  todo check retries */
	upload_code(g, g->bios.preos.bootloader_phys_base,
			g->bios.preos.bootloader,
			g->bios.preos.bootloader_size,
			0, 0);
	upload_code(g, g->bios.preos.phys_base,
			g->bios.preos.ucode,
			g->bios.preos.size,
			0, 1);
	upload_data(g, g->bios.preos.dmem_phys_base,
			g->bios.preos.dmem,
			g->bios.preos.dmem_size,
			0);

	gk20a_writel(g, pwr_falcon_bootvec_r(),
		pwr_falcon_bootvec_vec_f(g->bios.preos.code_entry_point));
	gk20a_writel(g, pwr_falcon_dmactl_r(),
			pwr_falcon_dmactl_require_ctx_f(0));
	gk20a_writel(g, pwr_falcon_cpuctl_r(),
		pwr_falcon_cpuctl_startcpu_f(1));

	retries = PMU_BOOT_TIMEOUT_MAX / PMU_BOOT_TIMEOUT_DEFAULT;
	do {
		val = pwr_falcon_cpuctl_halt_intr_v(
				gk20a_readl(g, pwr_falcon_cpuctl_r()));
		udelay(PMU_BOOT_TIMEOUT_DEFAULT);
	} while (!val && retries--);

	gk20a_writel(g, pwr_falcon_irqsclr_r(),
	     pwr_falcon_irqstat_halt_true_f());
	gk20a_readl(g, pwr_falcon_irqsclr_r());

	if (!retries)
		err = -EINVAL;

	gk20a_dbg_fn("done");
	return err;
}

static int gm206_bios_init(struct gk20a *g)
{
	int i;
	struct gk20a_platform *platform = dev_get_drvdata(g->dev);
	struct dentry *d;
	const struct firmware *bios_fw;
	int err;
	bool found = 0;
	struct pci_dev *pdev = to_pci_dev(g->dev);
	char rom_name[sizeof(BIOS_OVERLAY_NAME_FORMATTED)];

	gk20a_dbg_fn("");

	snprintf(rom_name, sizeof(rom_name), BIOS_OVERLAY_NAME, pdev->device);
	gk20a_dbg_info("checking for VBIOS overlay %s", rom_name);
	bios_fw = nvgpu_request_firmware(g, rom_name,
			NVGPU_REQUEST_FIRMWARE_NO_WARN |
			NVGPU_REQUEST_FIRMWARE_NO_SOC);
	if (bios_fw) {
		gk20a_dbg_info("using VBIOS overlay");
		g->bios.data = (void *)&bios_fw->data[ROM_FILE_PAYLOAD_OFFSET];
		g->bios.size = bios_fw->size - ROM_FILE_PAYLOAD_OFFSET;
	} else {
		gk20a_dbg_info("reading bios from EEPROM");
		g->bios.size = BIOS_SIZE;
		g->bios.data = kzalloc(BIOS_SIZE, GFP_KERNEL);
		if (!g->bios.data)
			return -ENOMEM;
		gk20a_writel(g, NV_PCFG + xve_rom_ctrl_r(),
				xve_rom_ctrl_rom_shadow_disabled_f());
		for (i = 0; i < g->bios.size/4; i++) {
			u32 val = be32_to_cpu(gk20a_readl(g, 0x300000 + i*4));

			g->bios.data[(i*4)] = (val >> 24) & 0xff;
			g->bios.data[(i*4)+1] = (val >> 16) & 0xff;
			g->bios.data[(i*4)+2] = (val >> 8) & 0xff;
			g->bios.data[(i*4)+3] = val & 0xff;
		}
		gk20a_writel(g, NV_PCFG + xve_rom_ctrl_r(),
				xve_rom_ctrl_rom_shadow_enabled_f());
	}

	err = gm206_bios_parse_rom(g);
	if (err)
		return err;

	gk20a_dbg_info("read bios");
	for (i = 0; i < g->bios.size; i++) {
		if (gm206_bios_rdu16(g, i) == BIT_HEADER_ID &&
		    gm206_bios_rdu32(g, i+2) ==  BIT_HEADER_SIGNATURE) {
			gm206_bios_parse_bit(g, i);
			found = true;
		}
	}

	if (!found) {
		gk20a_err(g->dev, "no valid VBIOS found");
		return -EINVAL;
	}

	if (g->gpu_characteristics.vbios_version <
	    platform->vbios_min_version) {
		gk20a_err(g->dev, "unsupported VBIOS version %08x",
				g->gpu_characteristics.vbios_version);
		return -EINVAL;
	}

	/* WAR for HW2.5 RevA (INA3221 is missing) */
	if ((g->pci_vendor_id == PCI_VENDOR_ID_NVIDIA) &&
		(g->pci_device_id == 0x1c75) &&
		(g->gpu_characteristics.vbios_version == 0x86065300)) {
			g->power_sensor_missing = true;
	}

	g->bios_blob.data = g->bios.data;
	g->bios_blob.size = g->bios.size;

	d = debugfs_create_blob("bios", S_IRUGO, platform->debugfs,
			&g->bios_blob);
	if (!d)
		gk20a_err(g->dev, "No debugfs?");

	gk20a_dbg_fn("done");

	err = gm206_bios_devinit(g);
	if (err) {
		gk20a_err(g->dev, "devinit failed");
		return err;
	}

	if (platform->run_preos) {
		err = gm206_bios_preos(g);
		if (err) {
			gk20a_err(g->dev, "pre-os failed");
			return err;
		}
	}

	return 0;
}

void gm206_init_bios(struct gpu_ops *gops)
{
	gops->bios.init = gm206_bios_init;
	gops->bios.get_perf_table_ptrs = gm206_bios_get_perf_table_ptrs;
}
