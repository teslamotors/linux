/*
 * nvadsp_app.c
 *
 * ADSP OS App management
 *
 * Copyright (C) 2014-2015 NVIDIA Corporation. All rights reserved.
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

#include <linux/tegra_nvadsp.h>
#include <linux/elf.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/kernel.h>

#include "os.h"
#include "dram_app_mem_manager.h"
#include "adsp_shared_struct.h"

#ifdef CONFIG_DEBUG_SET_MODULE_RONX
# define debug_align(X) ALIGN(X, PAGE_SIZE)
#else
# define debug_align(X) (X)
#endif


#ifndef ARCH_SHF_SMALL
#define ARCH_SHF_SMALL 0
#endif

#define BITS_PER_INT 32
#define INIT_OFFSET_MASK (1U < (BITS_PER_INT-1))


#define HWCAP_SWP	(1 << 0)
#define HWCAP_HALF	(1 << 1)
#define HWCAP_THUMB	(1 << 2)
#define HWCAP_26BIT	(1 << 3)	/* Play it safe */
#define HWCAP_FAST_MULT	(1 << 4)
#define HWCAP_FPA	(1 << 5)
#define HWCAP_VFP	(1 << 6)
#define HWCAP_EDSP	(1 << 7)
#define HWCAP_JAVA	(1 << 8)
#define HWCAP_IWMMXT	(1 << 9)
#define HWCAP_CRUNCH	(1 << 10)
#define HWCAP_THUMBEE	(1 << 11)
#define HWCAP_NEON	(1 << 12)
#define HWCAP_VFPv3	(1 << 13)
#define HWCAP_VFPv3D16	(1 << 14)	/* also set for VFPv4-D16 */
#define HWCAP_TLS	(1 << 15)
#define HWCAP_VFPv4	(1 << 16)
#define HWCAP_IDIVA	(1 << 17)
#define HWCAP_IDIVT	(1 << 18)
#define HWCAP_VFPD32	(1 << 19)	/* set if VFP has 32 regs (not 16) */
#define HWCAP_IDIV	(HWCAP_IDIVA | HWCAP_IDIVT)

#define HWCAP_LPAE	(1 << 20)
#define HWCAP_EVTSTRM_32	(1 << 21)


#define EF_ARM_EABI_MASK	0xff000000
#define EF_ARM_EABI_UNKNOWN	0x00000000
#define EF_ARM_EABI_VER1	0x01000000
#define EF_ARM_EABI_VER2	0x02000000
#define EF_ARM_EABI_VER3	0x03000000
#define EF_ARM_EABI_VER4	0x04000000
#define EF_ARM_EABI_VER5	0x05000000

#define EF_ARM_BE8		0x00800000	/* ABI 4,5 */
#define EF_ARM_LE8		0x00400000	/* ABI 4,5 */
#define EF_ARM_MAVERICK_FLOAT	0x00000800	/* ABI 0 */
#define EF_ARM_VFP_FLOAT	0x00000400	/* ABI 0 */
#define EF_ARM_SOFT_FLOAT	0x00000200	/* ABI 0 */
#define EF_ARM_OLD_ABI		0x00000100	/* ABI 0 */
#define EF_ARM_NEW_ABI		0x00000080	/* ABI 0 */
#define EF_ARM_ALIGN8		0x00000040	/* ABI 0 */
#define EF_ARM_PIC		0x00000020	/* ABI 0 */
#define EF_ARM_MAPSYMSFIRST	0x00000010	/* ABI 2 */
#define EF_ARM_APCS_FLOAT	0x00000010	/* ABI 0, floats in fp regs */
#define EF_ARM_DYNSYMSUSESEGIDX	0x00000008	/* ABI 2 */
#define EF_ARM_APCS_26		0x00000008	/* ABI 0 */
#define EF_ARM_SYMSARESORTED	0x00000004	/* ABI 1,2 */
#define EF_ARM_INTERWORK	0x00000004	/* ABI 0 */
#define EF_ARM_HASENTRY		0x00000002	/* All */
#define EF_ARM_RELEXEC		0x00000001	/* All */


#define R_ARM_NONE		0
#define R_ARM_PC24		1
#define R_ARM_ABS32		2
#define R_ARM_CALL		28
#define R_ARM_JUMP24		29
#define R_ARM_TARGET1		38
#define R_ARM_V4BX		40
#define R_ARM_PREL31		42
#define R_ARM_MOVW_ABS_NC	43
#define R_ARM_MOVT_ABS		44

#define R_ARM_THM_CALL		10
#define R_ARM_THM_JUMP24	30
#define R_ARM_THM_MOVW_ABS_NC	47
#define R_ARM_THM_MOVT_ABS	48


struct load_info {
	const char *name;
	struct elf32_hdr *hdr;
	unsigned long len;
	struct elf32_shdr *sechdrs;
	char *secstrings, *strtab;
	unsigned long symoffs, stroffs;
	unsigned int num_debug;
	bool sig_ok;
	struct device *dev;
	struct {
		unsigned int sym, str, mod, vers, info, pcpu;
	} index;
};

static int
apply_relocate(const struct load_info *info, Elf32_Shdr *sechdrs,
		const char *strtab, unsigned int symindex,
		unsigned int relindex, struct adsp_module *module)
{
	Elf32_Shdr *symsec = sechdrs + symindex;
	Elf32_Shdr *relsec = sechdrs + relindex;
	Elf32_Shdr *dstsec = sechdrs + relsec->sh_info;
	Elf32_Rel *rel = (void *)info->hdr + relsec->sh_offset;
	struct device *dev = info->dev;
	unsigned int i;

	dev_dbg(dev, "the relative section is %s dst %s sym %s\n",
				info->secstrings + relsec->sh_name,
				info->secstrings + dstsec->sh_name,
				info->secstrings + symsec->sh_name);

	for (i = 0; i < relsec->sh_size / sizeof(Elf32_Rel); i++, rel++) {
		void *loc;
		Elf32_Sym *sym;
		const char *symname;
		s32 offset;
		u32 upper, lower, sign, j1, j2;
		uint32_t adsp_loc;
		bool switch_mode = false;
		int h_bit = 0;

		offset = ELF32_R_SYM(rel->r_info);
		if (offset < 0 || (offset >
			(symsec->sh_size / sizeof(Elf32_Sym)))) {
			dev_err(dev, "%s: section %u reloc %u: bad relocation sym offset\n",
				module->name, relindex, i);
			return -ENOEXEC;
		}

		sym = ((Elf32_Sym *)(module->module_ptr
					+ symsec->sh_addr)) + offset;
		symname = info->strtab + sym->st_name;

		dev_dbg(dev, "%s\n", symname);

		if (rel->r_offset < 0 ||
		rel->r_offset > dstsec->sh_size - sizeof(u32)) {
			dev_err(dev,
			"%s: section %u reloc %u sym '%s': out of bounds relocation, offset %d size %u\n",
			       module->name, relindex, i, symname,
			       rel->r_offset, dstsec->sh_size);
			return -ENOEXEC;
		}

		loc = module->module_ptr + dstsec->sh_addr + rel->r_offset;
		adsp_loc = module->adsp_module_ptr +
				dstsec->sh_addr + rel->r_offset;
		dev_dbg(dev, "%p 0x%x\n", loc, adsp_loc);

		if (ELF_ST_BIND(sym->st_info) == STB_WEAK
				&& sym->st_shndx == SHN_UNDEF) {
			dev_dbg(dev, "STB_WEAK %s\n", symname);
			continue;
		}

		switch (ELF32_R_TYPE(rel->r_info)) {
		case R_ARM_NONE:
			dev_dbg(dev, "R_ARM_NONE\n");
			/* ignore */
			break;

		case R_ARM_ABS32:
		case R_ARM_TARGET1:
			dev_dbg(dev, "R_ARM_ABS32\n");
			*(u32 *)loc += sym->st_value;
			dev_dbg(dev, "addrs: 0x%x %p  values: 0x%x 0x%x\n",
					adsp_loc, loc, sym->st_value,
							*(u32 *)loc);
			break;

		case R_ARM_PC24:
		case R_ARM_CALL:
		case R_ARM_JUMP24:
			dev_dbg(dev, "R_ARM_CALL R_ARM_JUMP24\n");
			offset = (*(u32 *)loc & 0x00ffffff) << 2;
			if (offset & 0x02000000)
				offset -= 0x04000000;

			offset += sym->st_value - adsp_loc;
			if ((ELF32_ST_TYPE(sym->st_info) == STT_FUNC)
							    && (offset & 3)) {
				dev_dbg(dev, "switching the mode from ARM to THUMB\n");
				switch_mode = true;
				h_bit = (offset & 2);
				dev_dbg(dev,
					"%s offset 0x%x hbit %d",
						symname, offset, h_bit);
			}

			if (offset <= (s32)0xfe000000 ||
			    offset >= (s32)0x02000000) {
				dev_err(dev,
				"%s: section %u reloc %u sym '%s': relocation %u out of range (%p -> %#x)\n",
				       module->name, relindex, i, symname,
				       ELF32_R_TYPE(rel->r_info), loc,
				       sym->st_value);
				return -ENOEXEC;
			}

			offset >>= 2;

			*(u32 *)loc &= 0xff000000;
			*(u32 *)loc |= offset & 0x00ffffff;
			if (switch_mode) {
				*(u32 *)loc &= ~(0xff000000);
				if (h_bit)
					*(u32 *)loc |= 0xfb000000;
				else
					*(u32 *)loc |= 0xfa000000;
			}

			dev_dbg(dev,
				"%s address 0x%x instruction 0x%x\n",
					symname, adsp_loc, *(u32 *)loc);
			break;

		case R_ARM_V4BX:
			dev_dbg(dev, "R_ARM_V4BX\n");
		       /* Preserve Rm and the condition code. Alter
			* other bits to re-code instruction as
			* MOV PC,Rm.
			*/
		       *(u32 *)loc &= 0xf000000f;
		       *(u32 *)loc |= 0x01a0f000;
		       break;

		case R_ARM_PREL31:
			dev_dbg(dev, "R_ARM_PREL31\n");
			offset = *(u32 *)loc + sym->st_value - adsp_loc;
			*(u32 *)loc = offset & 0x7fffffff;
			break;

		case R_ARM_MOVW_ABS_NC:
		case R_ARM_MOVT_ABS:
			dev_dbg(dev, "R_ARM_MOVT_ABS\n");
			offset = *(u32 *)loc;
			offset = ((offset & 0xf0000) >> 4) | (offset & 0xfff);
			offset = (offset ^ 0x8000) - 0x8000;

			offset += sym->st_value;
			if (ELF32_R_TYPE(rel->r_info) == R_ARM_MOVT_ABS)
				offset >>= 16;

			*(u32 *)loc &= 0xfff0f000;
			*(u32 *)loc |= ((offset & 0xf000) << 4) |
					(offset & 0x0fff);
			break;

		case R_ARM_THM_CALL:
		case R_ARM_THM_JUMP24:
			dev_dbg(dev, "R_ARM_THM_CALL R_ARM_THM_JUMP24\n");
			upper = *(u16 *)loc;
			lower = *(u16 *)(loc + 2);

			/*
			 * 25 bit signed address range (Thumb-2 BL and B.W
			 * instructions):
			 *   S:I1:I2:imm10:imm11:0
			 * where:
			 *   S     = upper[10]   = offset[24]
			 *   I1    = ~(J1 ^ S)   = offset[23]
			 *   I2    = ~(J2 ^ S)   = offset[22]
			 *   imm10 = upper[9:0]  = offset[21:12]
			 *   imm11 = lower[10:0] = offset[11:1]
			 *   J1    = lower[13]
			 *   J2    = lower[11]
			 */
			sign = (upper >> 10) & 1;
			j1 = (lower >> 13) & 1;
			j2 = (lower >> 11) & 1;
			offset = (sign << 24) | ((~(j1 ^ sign) & 1) << 23) |
				((~(j2 ^ sign) & 1) << 22) |
				((upper & 0x03ff) << 12) |
				((lower & 0x07ff) << 1);
			if (offset & 0x01000000)
				offset -= 0x02000000;
			offset += sym->st_value - adsp_loc;

			/*
			 * For function symbols, only Thumb addresses are
			 * allowed (no interworking).
			 *
			 * For non-function symbols, the destination
			 * has no specific ARM/Thumb disposition, so
			 * the branch is resolved under the assumption
			 * that interworking is not required.
			 */
			if (ELF32_ST_TYPE(sym->st_info) == STT_FUNC &&
				!(offset & 1)) {
				dev_dbg(dev,
				"switching the mode from THUMB to ARM\n");
				switch_mode = true;
				offset = ALIGN(offset, 4);
			}

			if (offset <= (s32)0xff000000 ||
			    offset >= (s32)0x01000000) {
				dev_err(dev,
				"%s: section %u reloc %u sym '%s': relocation %u out of range (%p -> %#x)\n",
				       module->name, relindex, i, symname,
				       ELF32_R_TYPE(rel->r_info), loc,
				       sym->st_value);
				return -ENOEXEC;
			}

			sign = (offset >> 24) & 1;
			j1 = sign ^ (~(offset >> 23) & 1);
			j2 = sign ^ (~(offset >> 22) & 1);
			*(u16 *)loc = (u16)((upper & 0xf800) | (sign << 10) |
					    ((offset >> 12) & 0x03ff));
			*(u16 *)(loc + 2) = (u16)((lower & 0xd000) |
						  (j1 << 13) | (j2 << 11) |
						  ((offset >> 1) & 0x07ff));

			if (switch_mode) {
				lower = *(u16 *)(loc + 2);
				lower &= (~(1 << 12));
				*(u16 *)(loc + 2) = lower;
			}

			dev_dbg(dev,
				"%s address 0x%x upper instruction 0x%x\n",
					symname, adsp_loc, *(u16 *)loc);
			dev_dbg(dev,
				"%s address 0x%x lower instruction 0x%x\n",
					symname, adsp_loc, *(u16 *)(loc + 2));
			break;

		case R_ARM_THM_MOVW_ABS_NC:
		case R_ARM_THM_MOVT_ABS:
			dev_dbg(dev, "in R_ARM_THM_MOVT_ABS\n");
			upper = *(u16 *)loc;
			lower = *(u16 *)(loc + 2);

			/*
			 * MOVT/MOVW instructions encoding in Thumb-2:
			 *
			 * i	= upper[10]
			 * imm4	= upper[3:0]
			 * imm3	= lower[14:12]
			 * imm8	= lower[7:0]
			 *
			 * imm16 = imm4:i:imm3:imm8
			 */
			offset = ((upper & 0x000f) << 12) |
				((upper & 0x0400) << 1) |
				((lower & 0x7000) >> 4) | (lower & 0x00ff);
			offset = (offset ^ 0x8000) - 0x8000;
			offset += sym->st_value;

			if (ELF32_R_TYPE(rel->r_info) == R_ARM_THM_MOVT_ABS)
				offset >>= 16;

			*(u16 *)loc = (u16)((upper & 0xfbf0) |
					    ((offset & 0xf000) >> 12) |
					    ((offset & 0x0800) >> 1));
			*(u16 *)(loc + 2) = (u16)((lower & 0x8f00) |
						  ((offset & 0x0700) << 4) |
						  (offset & 0x00ff));
			break;

		default:
			dev_err(dev, "%s: unknown relocation: %u\n",
			       module->name, ELF32_R_TYPE(rel->r_info));
			return -ENOEXEC;
		}
	}
	return 0;
}

static int
apply_relocations(struct adsp_module *mod,
			const struct load_info *info)
{
	unsigned int i;
	int err = 0;

	/* Now do relocations. */
	for (i = 1; i < info->hdr->e_shnum; i++) {
		unsigned int infosec = info->sechdrs[i].sh_info;

		/* Not a valid relocation section? */
		if (infosec >= info->hdr->e_shnum)
			continue;

		/* Don't bother with non-allocated sections */
		if (!(info->sechdrs[infosec].sh_flags & SHF_ALLOC))
			continue;

		if (info->sechdrs[i].sh_type == SHT_REL)
			err = apply_relocate(info, info->sechdrs, info->strtab,
					     info->index.sym, i, mod);
		else if (info->sechdrs[i].sh_type == SHT_RELA)
			return -EINVAL;
		if (err < 0)
			break;
	}
	return err;
}

static int
simplify_symbols(struct adsp_module *mod,
				const struct load_info *info)
{
	Elf32_Shdr *symsec = &info->sechdrs[info->index.sym];
	Elf32_Sym *sym = mod->module_ptr + symsec->sh_addr;
	unsigned int secbase;
	unsigned int i;
	int ret = 0;
	struct global_sym_info *sym_info;
	struct device *dev = info->dev;

	for (i = 1; i < symsec->sh_size / sizeof(Elf32_Sym); i++) {
		const char *name = info->strtab + sym[i].st_name;
		dev_dbg(dev, "%s\n", name);
		switch (sym[i].st_shndx) {
		case SHN_COMMON:
			/* We compiled with -fno-common.  These are not
			   supposed to happen.  */
			dev_err(dev, "Common symbol: '%s'\n", name);
			dev_err(dev,
			    "please compile module %s with -fno-common\n",
								    mod->name);
			ret = -ENOEXEC;
			goto end;

		case SHN_ABS:
			/* Don't need to do anything */
			dev_dbg(dev, "Absolute symbol: 0x%08lx\n",
			       (long)sym[i].st_value);
			break;

		case SHN_UNDEF:
			sym_info = find_global_symbol(name);

			/* Ok if resolved.  */
			if (sym_info) {
				dev_dbg(dev, "SHN_UNDEF sym '%s':0x%x\n",
							name, sym_info->addr);
				sym[i].st_value = sym_info->addr;
				sym[i].st_info = sym_info->info;
				break;
			}

			if (ELF_ST_BIND(sym[i].st_info) == STB_WEAK) {
				dev_dbg(dev, "WEAK SYM %s not resolved\n",
						name);
				break;
			}

			dev_err(dev, "No symbol '%s' found\n", name);
			ret = -ENOEXEC;
			goto end;

		default:
			/* Divert to percpu allocation if a percpu var. */
			dev_dbg(dev, "default\n");
			secbase = info->sechdrs[sym[i].st_shndx].sh_addr;
			sym[i].st_value += secbase + mod->adsp_module_ptr;
			dev_dbg(dev, "symbol %s is 0x%x\n",
						name, sym[i].st_value);
			break;
		}
	}
end:
	return ret;
}

static int move_module(struct adsp_module *mod, struct load_info *info)
{
	struct device *dev = info->dev;
	int i;

	mod->handle = dram_app_mem_request(info->name, mod->size);
	if (!mod->handle) {
		dev_err(dev, "cannot allocate memory for app %s\n", info->name);
		return -ENOMEM;
	}
	mod->adsp_module_ptr = dram_app_mem_get_address(mod->handle);
	mod->module_ptr = nvadsp_da_to_va_mappings(mod->adsp_module_ptr,
			mod->size);
	dev_info(dev, "module %s Load address %p 0x%x\n", info->name,
					mod->module_ptr, mod->adsp_module_ptr);
	/* Transfer each section which specifies SHF_ALLOC */
	dev_dbg(dev, "final section addresses:\n");
	for (i = 0; i < info->hdr->e_shnum; i++) {
		void *dest;
		struct elf32_shdr *shdr = &info->sechdrs[i];

		if (!(shdr->sh_flags & SHF_ALLOC))
			continue;

		if (shdr->sh_entsize & INIT_OFFSET_MASK) {
			dev_dbg(dev, "%s %d\n",
				info->secstrings + shdr->sh_name,
							shdr->sh_entsize);
			dest = mod->module_ptr
				+ (shdr->sh_entsize & ~INIT_OFFSET_MASK);
		} else {
			dev_dbg(dev, "%s %d\n",
				info->secstrings + shdr->sh_name,
							shdr->sh_entsize);
			dest = mod->module_ptr + shdr->sh_entsize;
		}

		if (shdr->sh_type != SHT_NOBITS)
			memcpy(dest,
				(void *)info->hdr + shdr->sh_offset,
							shdr->sh_size);
		/* Update sh_addr to point to copy in image. */
		shdr->sh_addr = (uint32_t)(dest - mod->module_ptr);
		dev_dbg(dev, "name %s 0x%x %p 0x%x 0x%x\n",
			info->secstrings + shdr->sh_name, shdr->sh_addr,
			dest, shdr->sh_addr + mod->adsp_module_ptr,
			shdr->sh_size);
	}

	return 0;
}

static int get_offset(struct adsp_module *mod, size_t *size,
		       struct elf32_shdr *sechdr, unsigned int section)
{
	int ret;

	ret = ALIGN(*size, sechdr->sh_addralign ?: 1);
	*size = ret + sechdr->sh_size;
	return ret;
}

static bool
is_core_symbol(const struct elf32_sym *src,
		const struct elf32_shdr *sechdrs, unsigned int shnum)
{
	const struct elf32_shdr *sec;

	if (src->st_shndx == SHN_UNDEF
	    || src->st_shndx >= shnum
	    || !src->st_name)
		return false;

	sec = sechdrs + src->st_shndx;
	if (!(sec->sh_flags & SHF_ALLOC)
#ifndef CONFIG_KALLSYMS_ALL
	    || !(sec->sh_flags & SHF_EXECINSTR)
#endif
	    || (sec->sh_entsize & INIT_OFFSET_MASK))
		return false;

	return true;
}

static void layout_sections(struct adsp_module *mod, struct load_info *info)
{
	static unsigned long const masks[][2] = {
		/* NOTE: all executable code must be the first section
		 * in this array; otherwise modify the text_size
		 * finder in the two loops below */
		{ SHF_EXECINSTR | SHF_ALLOC, ARCH_SHF_SMALL },
		{ SHF_ALLOC, SHF_WRITE | ARCH_SHF_SMALL },
		{ SHF_WRITE | SHF_ALLOC, ARCH_SHF_SMALL },
		{ ARCH_SHF_SMALL | SHF_ALLOC, 0 }
	};
	unsigned int m, i;
	struct device *dev = info->dev;

	for (i = 0; i < info->hdr->e_shnum; i++)
		info->sechdrs[i].sh_entsize = ~0U;

	dev_dbg(dev, "Core section allocation order:\n");
	for (m = 0; m < ARRAY_SIZE(masks); ++m) {
		for (i = 0; i < info->hdr->e_shnum; ++i) {
			struct elf32_shdr *s = &info->sechdrs[i];
			const char *sname = info->secstrings + s->sh_name;

			if ((s->sh_flags & masks[m][0]) != masks[m][0]
			    || (s->sh_flags & masks[m][1])
			    || s->sh_entsize != ~0U
			    || strstarts(sname, ".init"))
				continue;

			s->sh_entsize = get_offset(mod, &mod->size, s, i);
			dev_dbg(dev, "\t%s %d\n", sname, s->sh_entsize);
		}
	}

	dev_dbg(dev, "Init section allocation order:\n");
	for (m = 0; m < ARRAY_SIZE(masks); ++m) {
		for (i = 0; i < info->hdr->e_shnum; ++i) {
			struct elf32_shdr *s = &info->sechdrs[i];
			const char *sname = info->secstrings + s->sh_name;

			if ((s->sh_flags & masks[m][0]) != masks[m][0]
			    || (s->sh_flags & masks[m][1])
			    || s->sh_entsize != ~0U
			    || !strstarts(sname, ".init"))
				continue;
			s->sh_entsize = (get_offset(mod, &mod->size, s, i)
					 | INIT_OFFSET_MASK);
			dev_dbg(dev, "\t%s %d\n", sname, s->sh_entsize);
		}
	}
}

static int rewrite_section_headers(struct load_info *info)
{
	unsigned int i;
	struct device *dev = info->dev;

	/* This should always be true, but let's be sure. */
	info->sechdrs[0].sh_addr = 0;

	for (i = 1; i < info->hdr->e_shnum; i++) {
		struct elf32_shdr *shdr = &info->sechdrs[i];
		if (shdr->sh_type != SHT_NOBITS
		    && info->len < shdr->sh_offset + shdr->sh_size) {
			dev_err(dev, "Module len %lu truncated\n", info->len);
			return -ENOEXEC;
		}

		/* Mark all sections sh_addr with their address in the
		   temporary image. */
		shdr->sh_addr = shdr->sh_offset;

	}
	return 0;
}


static struct adsp_module *setup_load_info(struct load_info *info)
{
	unsigned int i;
	int err;
	struct adsp_module *mod;
	struct device *dev = info->dev;

	/* Set up the convenience variables */
	info->sechdrs = (void *)info->hdr + info->hdr->e_shoff;
	info->secstrings = (void *)info->hdr
		+ info->sechdrs[info->hdr->e_shstrndx].sh_offset;

	err = rewrite_section_headers(info);
	if (err)
		return ERR_PTR(err);

	/* Find internal symbols and strings. */
	for (i = 1; i < info->hdr->e_shnum; i++) {
		if (info->sechdrs[i].sh_type == SHT_SYMTAB) {
			info->index.sym = i;
			info->index.str = info->sechdrs[i].sh_link;
			info->strtab = (char *)info->hdr
				+ info->sechdrs[info->index.str].sh_offset;
			break;
		}
	}

	/* This is temporary: point mod into copy of data. */
	mod = kzalloc(sizeof(struct adsp_module), GFP_KERNEL);
	if (!mod) {
		dev_err(dev, "Unable to create module\n");
		return ERR_PTR(-ENOMEM);
	}

	if (info->index.sym == 0) {
		dev_warn(dev, "%s: module has no symbols (stripped?)\n",
								mod->name);
		kfree(mod);
		return ERR_PTR(-ENOEXEC);
	}

	return mod;
}


static void layout_symtab(struct adsp_module *mod, struct load_info *info)
{
	struct elf32_shdr *symsect = info->sechdrs + info->index.sym;
	struct elf32_shdr *strsect = info->sechdrs + info->index.str;
	const struct elf32_sym *src;
	unsigned int i, nsrc, ndst, strtab_size = 0;
	struct device *dev = info->dev;

	/* Put symbol section at end of init part of module. */
	symsect->sh_flags |= SHF_ALLOC;
	symsect->sh_entsize = get_offset(mod, &mod->size, symsect,
					 info->index.sym) | INIT_OFFSET_MASK;
	dev_dbg(dev, "\t%s %d\n", info->secstrings + symsect->sh_name,
							symsect->sh_entsize);

	src = (void *)info->hdr + symsect->sh_offset;
	nsrc = symsect->sh_size / sizeof(*src);

	/* Compute total space required for the core symbols' strtab. */
	for (ndst = i = 0; i < nsrc; i++) {
		if (i == 0 ||
		is_core_symbol(src+i, info->sechdrs, info->hdr->e_shnum)) {
			strtab_size += strlen(&info->strtab[src[i].st_name])+1;
			ndst++;
		}
	}

	/* Append room for core symbols at end of core part. */
	info->symoffs = ALIGN(mod->size, symsect->sh_addralign ?: 1);
	info->stroffs = mod->size = info->symoffs + ndst * sizeof(Elf32_Sym);
	mod->size += strtab_size;

	/* Put string table section at end of init part of module. */
	strsect->sh_flags |= SHF_ALLOC;
	strsect->sh_entsize = get_offset(mod, &mod->size, strsect,
					 info->index.str) | INIT_OFFSET_MASK;
	dev_dbg(dev, "\t%s %d\n",
		info->secstrings + strsect->sh_name,
					symsect->sh_entsize);
}

static struct adsp_module *layout_and_allocate(struct load_info *info)
{
	/* Module within temporary copy. */
	struct adsp_module *mod;
	int err;

	mod = setup_load_info(info);
	if (IS_ERR(mod))
		return mod;

	mod->name = info->name;

	/* Determine total sizes, and put offsets in sh_entsize.  For now
	   this is done generically; there doesn't appear to be any
	   special cases for the architectures. */
	layout_sections(mod, info);
	layout_symtab(mod, info);

	/* Allocate and move to the final place */
	err = move_module(mod, info);
	if (err) {
		/* TODO: need to handle error path more genericly */
		kfree(mod);
		return ERR_PTR(err);
	}

	return mod;
}

static int elf_check_arch_arm32(const struct elf32_hdr *x)
{
	unsigned int eflags;

	/* Make sure it's an ARM executable */
	if (x->e_machine != EM_ARM)
		return 0;

	/* Make sure the entry address is reasonable */
	if (x->e_entry & 1) {
		if (!(elf_hwcap & HWCAP_THUMB))
			return 0;
	} else if (x->e_entry & 3)
		return 0;

	eflags = x->e_flags;
	if ((eflags & EF_ARM_EABI_MASK) == EF_ARM_EABI_UNKNOWN) {
		unsigned int flt_fmt;

		/* APCS26 is only allowed if the CPU supports it */
		if ((eflags & EF_ARM_APCS_26) && !(elf_hwcap & HWCAP_26BIT))
			return 0;

		flt_fmt = eflags & (EF_ARM_VFP_FLOAT | EF_ARM_SOFT_FLOAT);

		/* VFP requires the supporting code */
		if (flt_fmt == EF_ARM_VFP_FLOAT && !(elf_hwcap & HWCAP_VFP))
			return 0;
	}
	return 1;
}

static int elf_header_check(struct load_info *info)
{
	if (info->len < sizeof(*(info->hdr)))
		return -ENOEXEC;

	if (memcmp(info->hdr->e_ident, ELFMAG, SELFMAG) != 0
	    || info->hdr->e_type != ET_REL
	    || !elf_check_arch_arm32(info->hdr)
	    || info->hdr->e_shentsize != sizeof(Elf32_Shdr))
		return -ENOEXEC;

	if (info->hdr->e_shoff >= info->len
	    || (info->hdr->e_shnum * sizeof(Elf32_Shdr) >
		info->len - info->hdr->e_shoff))
		return -ENOEXEC;

	return 0;
}

struct adsp_module *load_adsp_static_module(const char *appname,
	struct adsp_shared_app *shared_app, struct device *dev)
{
	struct adsp_module *mod = NULL;

	mod = kzalloc(sizeof(struct adsp_module), GFP_KERNEL);
	if (!mod)
		return NULL;

	memcpy((struct app_mem_size *)&mod->mem_size,
		&shared_app->mem_size, sizeof(shared_app->mem_size));

	mod->adsp_module_ptr = shared_app->mod_ptr;
	mod->dynamic = false;

	return mod;
}

struct adsp_module *load_adsp_dynamic_module(const char *appname,
	const char *appfile, struct device *dev)
{
	struct load_info info = { };
	struct adsp_module *mod = NULL;
	const struct firmware *fw;
	struct elf32_shdr *data_shdr;
	struct elf32_shdr *shared_shdr;
	struct elf32_shdr *shared_wc_shdr;
	struct elf32_shdr *aram_shdr;
	struct elf32_shdr *aram_x_shdr;
	struct app_mem_size *mem_size;
	int ret;

	ret = request_firmware(&fw, appfile, dev);
	if (ret < 0) {
		dev_err(dev,
			"request firmware for %s(%s) failed with %d\n",
							appname, appfile, ret);
		return ERR_PTR(ret);
	}

	info.hdr = (struct elf32_hdr *)fw->data;
	info.len = fw->size;
	info.dev = dev;
	info.name = appname;

	ret = elf_header_check(&info);
	if (ret) {
		dev_err(dev,
			"%s is not an elf file\n", appfile);
		goto error_release_fw;
	}

	/* Figure out module layout, and allocate all the memory. */
	mod = layout_and_allocate(&info);
	if (IS_ERR(mod))
		goto error_release_fw;

	/* update adsp specific sections */
	data_shdr = nvadsp_get_section(fw, ".dram_data");
	shared_shdr = nvadsp_get_section(fw, ".dram_shared");
	shared_wc_shdr = nvadsp_get_section(fw, ".dram_shared_wc");
	aram_shdr = nvadsp_get_section(fw, ".aram_data");
	aram_x_shdr = nvadsp_get_section(fw, ".aram_x_data");

	mem_size = (void *)&mod->mem_size;

	if (data_shdr) {
		dev_dbg(dev, "mem_size.dram_data %d\n",
					data_shdr->sh_size);
		mem_size->dram = data_shdr->sh_size;
	}

	if (shared_shdr) {
		dev_dbg(dev, "mem_size.dram_shared %d\n",
				shared_shdr->sh_size);
		mem_size->dram_shared =
				shared_shdr->sh_size;
	}

	if (shared_wc_shdr) {
		dev_dbg(dev, "shared_wc_shdr->sh_size %d\n",
				shared_wc_shdr->sh_size);
		mem_size->dram_shared_wc =
				shared_wc_shdr->sh_size;
	}

	if (aram_shdr) {
		dev_dbg(dev, "aram_shdr->sh_size %d\n", aram_shdr->sh_size);
		mem_size->aram = aram_shdr->sh_size;
	}

	if (aram_x_shdr) {
		dev_dbg(dev,
			"aram_x_shdr->sh_size %d\n", aram_x_shdr->sh_size);
		mem_size->aram_x = aram_x_shdr->sh_size;
	}

	/* Fix up syms, so that st_value is a pointer to location. */
	ret = simplify_symbols(mod, &info);
	if (ret) {
		dev_err(dev, "Unable to simplify symbols\n");
		goto unload_module;
	}

	dev_dbg(dev, "applying relocation\n");
	ret = apply_relocations(mod, &info);
	if (ret) {
		dev_err(dev, "relocation failed\n");
		goto unload_module;
	}

	mod->dynamic = true;

error_release_fw:
	release_firmware(fw);
	return IS_ERR_VALUE(ret) ? ERR_PTR(ret) : mod;

unload_module:
	unload_adsp_module(mod);
	release_firmware(fw);
	return ERR_PTR(ret);
}

void unload_adsp_module(struct adsp_module *mod)
{
	dram_app_mem_release(mod->handle);
	kfree(mod);
}
