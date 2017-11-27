/*
 * Utility functions for x86 operand and address decoding
 *
 * Copyright (C) Intel Corporation 2017
 */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ratelimit.h>
#include <linux/mmu_context.h>
#include <asm/desc_defs.h>
#include <asm/desc.h>
#include <asm/inat.h>
#include <asm/insn.h>
#include <asm/insn-eval.h>
#include <asm/ldt.h>
#include <asm/vm86.h>

#undef pr_fmt
#define pr_fmt(fmt) "insn: " fmt

enum reg_type {
	REG_TYPE_RM = 0,
	REG_TYPE_INDEX,
	REG_TYPE_BASE,
};

/**
 * is_string_insn() - Determine if instruction is a string instruction
 * @insn:	Instruction structure containing the opcode
 *
 * Return: true if the instruction, determined by the opcode, is any of the
 * string instructions as defined in the Intel Software Development manual.
 * False otherwise.
 */
static bool is_string_insn(struct insn *insn)
{
	insn_get_opcode(insn);

	/* All string instructions have a 1-byte opcode. */
	if (insn->opcode.nbytes != 1)
		return false;

	switch (insn->opcode.bytes[0]) {
	case 0x6c ... 0x6f:	/* INS, OUTS */
	case 0xa4 ... 0xa7:	/* MOVS, CMPS */
	case 0xaa ... 0xaf:	/* STOS, LODS, SCAS */
		return true;
	default:
		return false;
	}
}

/**
 * get_overridden_seg_reg() - obtain segment register to use from prefixes
 * @insn:	Instruction structure with segment override prefixes
 * @regs:	Structure with register values as seen when entering kernel mode
 * @regoff:	Operand offset, in pt_regs, used to deterimine segment register
 *
 * The segment register to which an effective address refers depends on
 * a) whether running in long mode (in such a case semgment override prefixes
 * are ignored. b) Whether segment override prefixes must be ignored for certain
 * registers: always use CS when the register is (R|E)IP; always use ES when
 * operand register is (E)DI with a string instruction as defined in the Intel
 * documentation. c) If segment overrides prefixes are found in the instruction
 * prefixes. d) Use the default segment register associated with the operand
 * register.
 *
 * This function returns the overridden segment register to use, if any, as per
 * the conditions described above. Please note that this function
 * does not return the value in the segment register (i.e., the segment
 * selector). The segment selector needs to be obtained using
 * get_segment_selector() and passing the segment register resolved by
 * this function.
 *
 * Return: A constant identifying the segment register to use, among CS, SS, DS,
 * ES, FS, or GS. INAT_SEG_REG_IGNORE is returned if running in long mode.
 * INAT_SEG_REG_DEFAULT is returned if no segment override prefixes were found
 * and the default segment register shall be used. -EINVAL in case of error.
 */
static int get_overridden_seg_reg(struct insn *insn, struct pt_regs *regs,
				  int regoff)
{
	int i;
	int sel_overrides = 0;
	int seg_register = INAT_SEG_REG_DEFAULT;

	/*
	 * Segment override prefixes should not be used for (E)IP. Check this
	 * case first as we might not have (and not needed at all) a
	 * valid insn structure to evaluate segment override prefixes.
	 */
	if (regoff == offsetof(struct pt_regs, ip)) {
		if (user_64bit_mode(regs))
			return INAT_SEG_REG_IGNORE;
		else
			return INAT_SEG_REG_DEFAULT;
	}

	if (!insn)
		return -EINVAL;

	insn_get_prefixes(insn);

	/* Look for any segment override prefixes. */
	for (i = 0; i < insn->prefixes.nbytes; i++) {
		insn_attr_t attr;

		attr = inat_get_opcode_attribute(insn->prefixes.bytes[i]);
		switch (attr) {
		case INAT_MAKE_PREFIX(INAT_PFX_CS):
			seg_register = INAT_SEG_REG_CS;
			sel_overrides++;
			break;
		case INAT_MAKE_PREFIX(INAT_PFX_SS):
			seg_register = INAT_SEG_REG_SS;
			sel_overrides++;
			break;
		case INAT_MAKE_PREFIX(INAT_PFX_DS):
			seg_register = INAT_SEG_REG_DS;
			sel_overrides++;
			break;
		case INAT_MAKE_PREFIX(INAT_PFX_ES):
			seg_register = INAT_SEG_REG_ES;
			sel_overrides++;
			break;
		case INAT_MAKE_PREFIX(INAT_PFX_FS):
			seg_register = INAT_SEG_REG_FS;
			sel_overrides++;
			break;
		case INAT_MAKE_PREFIX(INAT_PFX_GS):
			seg_register = INAT_SEG_REG_GS;
			sel_overrides++;
			break;
		/* No default action needed. */
		}
	}

	/*
	 * In long mode, segment override prefixes are ignored, except for
	 * overrides for FS and GS.
	 */
	if (user_64bit_mode(regs)) {
		if (seg_register != INAT_SEG_REG_FS &&
		    seg_register != INAT_SEG_REG_GS)
			return INAT_SEG_REG_IGNORE;
	/* More than one segment override prefix leads to undefined behavior. */
	} else if (sel_overrides > 1) {
		return -EINVAL;
	/*
	 * Segment override prefixes are always ignored for string instructions
	 * that involve the use the (E)DI register.
	 */
	} else if ((regoff == offsetof(struct pt_regs, di)) &&
		   is_string_insn(insn)) {
		return INAT_SEG_REG_DEFAULT;
	}

	return seg_register;
}

/**
 * resolve_seg_register() - obtain segment register
 * @insn:	Instruction structure with segment override prefixes
 * @regs:	Structure with register values as seen when entering kernel mode
 * @regoff:	Operand offset, in pt_regs, used to deterimine segment register
 *
 * Determine the segment register associated with the operands and, if
 * applicable, prefixes and the instruction pointed by insn. The function first
 * checks if the segment register shall be ignored or has been overridden in the
 * instruction prefixes. Otherwise, it resolves the segment register to use
 * based on the defaults described in the Intel documentation.
 *
 * The operand register, regoff, is represented as the offset from the base of
 * pt_regs. Also, regoff can be -EDOM for cases in which registers are not
 * used as operands (e.g., displacement-only memory addressing).
 *
 * Return: A constant identifying the segment register to use, among CS, SS, DS,
 * ES, FS, or GS. INAT_SEG_REG_IGNORE is returned if running in long mode.
 * -EINVAL in case of error.
 */
static int resolve_seg_register(struct insn *insn, struct pt_regs *regs,
				int regoff)
{
	int seg_reg;

	seg_reg = get_overridden_seg_reg(insn, regs, regoff);

	if (seg_reg < 0)
		return seg_reg;

	if (seg_reg == INAT_SEG_REG_IGNORE)
		return seg_reg;

	if (seg_reg != INAT_SEG_REG_DEFAULT)
		return seg_reg;

	/*
	 * If we are here, we use the default segment register as described
	 * in the Intel documentation:
	 *  + DS for all references involving (E)AX, (E)CX, (E)DX, (E)BX, and
	 * (E)SI.
	 *  + If used in a string instruction, ES for (E)DI. Otherwise, DS.
	 *  + AX, CX and DX are not valid register operands in 16-bit address
	 *    encodings but are valid for 32-bit and 64-bit encodings.
	 *  + -EDOM is reserved to identify for cases in which no register
	 *    is used (i.e., displacement-only addressing). Use DS.
	 *  + SS for (E)SP or (E)BP.
	 *  + CS for (E)IP.
	 */

	switch (regoff) {
	case offsetof(struct pt_regs, ax):
	case offsetof(struct pt_regs, cx):
	case offsetof(struct pt_regs, dx):
		/* Need insn to verify address size. */
		if (!insn || insn->addr_bytes == 2)
			return -EINVAL;
	case -EDOM:
	case offsetof(struct pt_regs, bx):
	case offsetof(struct pt_regs, si):
		return INAT_SEG_REG_DS;
	case offsetof(struct pt_regs, di):
		/* Need insn to see if insn is string instruction. */
		if (!insn)
			return -EINVAL;
		if (is_string_insn(insn))
			return INAT_SEG_REG_ES;
		return INAT_SEG_REG_DS;
	case offsetof(struct pt_regs, bp):
	case offsetof(struct pt_regs, sp):
		return INAT_SEG_REG_SS;
	case offsetof(struct pt_regs, ip):
		return INAT_SEG_REG_CS;
	default:
		return -EINVAL;
	}
}

/**
 * get_segment_selector() - obtain segment selector
 * @regs:	Structure with register values as seen when entering kernel mode
 * @seg_reg:	Segment register to use
 *
 * Obtain the segment selector from any of the CS, SS, DS, ES, FS, GS segment
 * registers. In CONFIG_X86_32, the segment is obtained from either pt_regs or
 * kernel_vm86_regs as applicable. In CONFIG_X86_64, CS and SS are obtained
 * from pt_regs. DS, ES, FS and GS are obtained by reading the actual CPU
 * registers. This done for only for completeness as in CONFIG_X86_64 segment
 * registers are ignored.
 *
 * Return: Value of the segment selector, including null when running in
 * long mode. -1 on error.
 */
static short get_segment_selector(struct pt_regs *regs, int seg_reg)
{
#ifdef CONFIG_X86_64
	unsigned short sel;

	switch (seg_reg) {
	case INAT_SEG_REG_IGNORE:
		return 0;
	case INAT_SEG_REG_CS:
		return (unsigned short)(regs->cs & 0xffff);
	case INAT_SEG_REG_SS:
		return (unsigned short)(regs->ss & 0xffff);
	case INAT_SEG_REG_DS:
		savesegment(ds, sel);
		return sel;
	case INAT_SEG_REG_ES:
		savesegment(es, sel);
		return sel;
	case INAT_SEG_REG_FS:
		savesegment(fs, sel);
		return sel;
	case INAT_SEG_REG_GS:
		savesegment(gs, sel);
		return sel;
	default:
		return -EINVAL;
	}
#else /* CONFIG_X86_32 */
	struct kernel_vm86_regs *vm86regs = (struct kernel_vm86_regs *)regs;

	if (v8086_mode(regs)) {
		switch (seg_reg) {
		case INAT_SEG_REG_CS:
			return (unsigned short)(regs->cs & 0xffff);
		case INAT_SEG_REG_SS:
			return (unsigned short)(regs->ss & 0xffff);
		case INAT_SEG_REG_DS:
			return vm86regs->ds;
		case INAT_SEG_REG_ES:
			return vm86regs->es;
		case INAT_SEG_REG_FS:
			return vm86regs->fs;
		case INAT_SEG_REG_GS:
			return vm86regs->gs;
		case INAT_SEG_REG_IGNORE:
			/* fall through */
		default:
			return -EINVAL;
		}
	}

	switch (seg_reg) {
	case INAT_SEG_REG_CS:
		return (unsigned short)(regs->cs & 0xffff);
	case INAT_SEG_REG_SS:
		return (unsigned short)(regs->ss & 0xffff);
	case INAT_SEG_REG_DS:
		return (unsigned short)(regs->ds & 0xffff);
	case INAT_SEG_REG_ES:
		return (unsigned short)(regs->es & 0xffff);
	case INAT_SEG_REG_FS:
		return (unsigned short)(regs->fs & 0xffff);
	case INAT_SEG_REG_GS:
		/*
		 * GS may or may not be in regs as per CONFIG_X86_32_LAZY_GS.
		 * The macro below takes care of both cases.
		 */
		return get_user_gs(regs);
	case INAT_SEG_REG_IGNORE:
		/* fall through */
	default:
		return -EINVAL;
	}
#endif /* CONFIG_X86_64 */
}

static int get_reg_offset(struct insn *insn, struct pt_regs *regs,
			  enum reg_type type)
{
	int regno = 0;

	static const int regoff[] = {
		offsetof(struct pt_regs, ax),
		offsetof(struct pt_regs, cx),
		offsetof(struct pt_regs, dx),
		offsetof(struct pt_regs, bx),
		offsetof(struct pt_regs, sp),
		offsetof(struct pt_regs, bp),
		offsetof(struct pt_regs, si),
		offsetof(struct pt_regs, di),
#ifdef CONFIG_X86_64
		offsetof(struct pt_regs, r8),
		offsetof(struct pt_regs, r9),
		offsetof(struct pt_regs, r10),
		offsetof(struct pt_regs, r11),
		offsetof(struct pt_regs, r12),
		offsetof(struct pt_regs, r13),
		offsetof(struct pt_regs, r14),
		offsetof(struct pt_regs, r15),
#endif
	};
	int nr_registers = ARRAY_SIZE(regoff);
	/*
	 * Don't possibly decode a 32-bit instructions as
	 * reading a 64-bit-only register.
	 */
	if (IS_ENABLED(CONFIG_X86_64) && !insn->x86_64)
		nr_registers -= 8;

	switch (type) {
	case REG_TYPE_RM:
		regno = X86_MODRM_RM(insn->modrm.value);

		/*
		 * ModRM.mod == 0 and ModRM.rm == 5 means a 32-bit displacement
		 * follows the ModRM byte.
		 */
		if (!X86_MODRM_MOD(insn->modrm.value) && regno == 5)
			return -EDOM;

		if (X86_REX_B(insn->rex_prefix.value))
			regno += 8;
		break;

	case REG_TYPE_INDEX:
		regno = X86_SIB_INDEX(insn->sib.value);
		if (X86_REX_X(insn->rex_prefix.value))
			regno += 8;

		/*
		 * If ModRM.mod != 3 and SIB.index = 4 the scale*index
		 * portion of the address computation is null. This is
		 * true only if REX.X is 0. In such a case, the SIB index
		 * is used in the address computation.
		 */
		if (X86_MODRM_MOD(insn->modrm.value) != 3 && regno == 4)
			return -EDOM;
		break;

	case REG_TYPE_BASE:
		regno = X86_SIB_BASE(insn->sib.value);
		/*
		 * If ModRM.mod is 0 and SIB.base == 5, the base of the
		 * register-indirect addressing is 0. In this case, a
		 * 32-bit displacement follows the SIB byte.
		 */
		if (!X86_MODRM_MOD(insn->modrm.value) && regno == 5)
			return -EDOM;

		if (X86_REX_B(insn->rex_prefix.value))
			regno += 8;
		break;

	default:
		pr_err_ratelimited("invalid register type: %d\n", type);
		return -EINVAL;
	}

	if (regno >= nr_registers) {
		WARN_ONCE(1, "decoded an instruction with an invalid register");
		return -EINVAL;
	}
	return regoff[regno];
}

/**
 * get_reg_offset_16() - Obtain offset of register indicated by instruction
 * @insn:	Instruction structure containing ModRM and SIB bytes
 * @regs:	Structure with register values as seen when entering kernel mode
 * @offs1:	Offset of the first operand register
 * @offs2:	Offset of the second opeand register, if applicable
 *
 * Obtain the offset, in pt_regs, of the registers indicated by the ModRM byte
 * within insn. This function is to be used with 16-bit address encodings. The
 * offs1 and offs2 will be written with the offset of the two registers
 * indicated by the instruction. In cases where any of the registers is not
 * referenced by the instruction, the value will be set to -EDOM.
 *
 * Return: 0 on success, -EINVAL on failure.
 */
static int get_reg_offset_16(struct insn *insn, struct pt_regs *regs,
			     int *offs1, int *offs2)
{
	/*
	 * 16-bit addressing can use one or two registers. Specifics of
	 * encodings are given in Table 2-1. "16-Bit Addressing Forms with the
	 * ModR/M Byte" of the Intel Software Development Manual.
	 */
	static const int regoff1[] = {
		offsetof(struct pt_regs, bx),
		offsetof(struct pt_regs, bx),
		offsetof(struct pt_regs, bp),
		offsetof(struct pt_regs, bp),
		offsetof(struct pt_regs, si),
		offsetof(struct pt_regs, di),
		offsetof(struct pt_regs, bp),
		offsetof(struct pt_regs, bx),
	};

	static const int regoff2[] = {
		offsetof(struct pt_regs, si),
		offsetof(struct pt_regs, di),
		offsetof(struct pt_regs, si),
		offsetof(struct pt_regs, di),
		-EDOM,
		-EDOM,
		-EDOM,
		-EDOM,
	};

	if (!offs1 || !offs2)
		return -EINVAL;

	/* Operand is a register, use the generic function. */
	if (X86_MODRM_MOD(insn->modrm.value) == 3) {
		*offs1 = insn_get_modrm_rm_off(insn, regs);
		*offs2 = -EDOM;
		return 0;
	}

	*offs1 = regoff1[X86_MODRM_RM(insn->modrm.value)];
	*offs2 = regoff2[X86_MODRM_RM(insn->modrm.value)];

	/*
	 * If ModRM.mod is 0 and ModRM.rm is 110b, then we use displacement-
	 * only addressing. This means that no registers are involved in
	 * computing the effective address. Thus, ensure that the first
	 * register offset is invalild. The second register offset is already
	 * invalid under the aforementioned conditions.
	 */
	if ((X86_MODRM_MOD(insn->modrm.value) == 0) &&
	    (X86_MODRM_RM(insn->modrm.value) == 6))
		*offs1 = -EDOM;

	return 0;
}

/**
 * get_desc() - Obtain address of segment descriptor
 * @sel:	Segment selector
 *
 * Given a segment selector, obtain a pointer to the segment descriptor.
 * Both global and local descriptor tables are supported.
 *
 * Return: pointer to segment descriptor on success. NULL on error.
 */
static struct desc_struct *get_desc(unsigned short sel)
{
	struct desc_ptr gdt_desc = {0, 0};
	struct desc_struct *desc = NULL;
	unsigned long desc_base;

#ifdef CONFIG_MODIFY_LDT_SYSCALL
	if ((sel & SEGMENT_TI_MASK) == SEGMENT_LDT) {
		/* Bits [15:3] contain the index of the desired entry. */
		sel >>= 3;

		mutex_lock(&current->active_mm->context.lock);
		/* The size of the LDT refers to the number of entries. */
		if (!current->active_mm->context.ldt ||
		    sel >= current->active_mm->context.ldt->nr_entries) {
			mutex_unlock(&current->active_mm->context.lock);
			return NULL;
		}

		desc = &current->active_mm->context.ldt->entries[sel];
		mutex_unlock(&current->active_mm->context.lock);
		return desc;
	}
#endif
	native_store_gdt(&gdt_desc);

	/*
	 * Segment descriptors have a size of 8 bytes. Thus, the index is
	 * multiplied by 8 to obtain the memory offset of the desired descriptor
	 * from the base of the GDT. As bits [15:3] of the segment selector
	 * contain the index, it can be regarded as multiplied by 8 already.
	 * All that remains is to clear bits [2:0].
	 */
	desc_base = sel & ~(SEGMENT_RPL_MASK | SEGMENT_TI_MASK);

	if (desc_base > gdt_desc.size)
		return NULL;

	desc = (struct desc_struct *)(gdt_desc.address + desc_base);
	return desc;
}

/**
 * insn_get_seg_base() - Obtain base address of segment descriptor.
 * @regs:	Structure with register values as seen when entering kernel mode
 * @insn:	Instruction structure with selector override prefixes
 * @regoff:	Operand offset, in pt_regs, of which the selector is needed
 *
 * Obtain the base address of the segment descriptor as indicated by either
 * any segment override prefixes contained in insn or the default segment
 * applicable to the register indicated by regoff. regoff is specified as the
 * offset in bytes from the base of pt_regs.
 *
 * Return: In protected mode, base address of the segment. Zero in long mode,
 * except when FS or GS are used. In virtual-8086 mode, the segment
 * selector shifted 4 positions to the right. -1L in case of
 * error.
 */
unsigned long insn_get_seg_base(struct pt_regs *regs, struct insn *insn,
				int regoff)
{
	struct desc_struct *desc;
	int seg_reg;
	short sel;

	seg_reg = resolve_seg_register(insn, regs, regoff);
	if (seg_reg < 0)
		return -1L;

	sel = get_segment_selector(regs, seg_reg);
	if (sel < 0)
		return -1L;

	if (v8086_mode(regs))
		/*
		 * Base is simply the segment selector shifted 4
		 * positions to the right.
		 */
		return (unsigned long)(sel << 4);

	if (user_64bit_mode(regs)) {
		/*
		 * Only FS or GS will have a base address, the rest of
		 * the segments' bases are forced to 0.
		 */
		unsigned long base;

		if (seg_reg == INAT_SEG_REG_FS)
			rdmsrl(MSR_FS_BASE, base);
		else if (seg_reg == INAT_SEG_REG_GS)
			/*
			 * swapgs was called at the kernel entry point. Thus,
			 * MSR_KERNEL_GS_BASE will have the user-space GS base.
			 */
			rdmsrl(MSR_KERNEL_GS_BASE, base);
		else if (seg_reg != INAT_SEG_REG_IGNORE)
			/* We should ignore the rest of segment registers. */
			base = -1L;
		else
			base = 0;
		return base;
	}

	/* In protected mode the segment selector cannot be null. */
	if (!sel)
		return -1L;

	desc = get_desc(sel);
	if (!desc)
		return -1L;

	return get_desc_base(desc);
}

/**
 * get_seg_limit() - Obtain the limit of a segment descriptor
 * @regs:	Structure with register values as seen when entering kernel mode
 * @insn:	Instruction structure with selector override prefixes
 * @regoff:	Operand offset, in pt_regs, of which the selector is needed
 *
 * Obtain the limit of the segment descriptor. The segment selector is obtained
 * from the relevant segment register determined by inspecting any segment
 * override prefixes or the default segment register associated with regoff.
 * regoff is specified as the offset in bytes from the base * of pt_regs.
 *
 * Return: In protected mode, the limit of the segment descriptor in bytes.
 * In long mode and virtual-8086 mode, segment limits are not enforced. Thus,
 * limit is returned as -1L to imply a limit-less segment. Zero is returned on
 * error.
 */
static unsigned long get_seg_limit(struct pt_regs *regs, struct insn *insn,
				   int regoff)
{
	struct desc_struct *desc;
	unsigned long limit;
	int seg_reg;
	short sel;

	seg_reg = resolve_seg_register(insn, regs, regoff);
	if (seg_reg < 0)
		return 0;

	sel = get_segment_selector(regs, seg_reg);
	if (sel < 0)
		return 0;

	if (user_64bit_mode(regs) || v8086_mode(regs))
		return -1L;

	if (!sel)
		return 0;

	desc = get_desc(sel);
	if (!desc)
		return 0;

	/*
	 * If the granularity bit is set, the limit is given in multiples
	 * of 4096. This also means that the 12 least significant bits are
	 * not tested when checking the segment limits. In practice,
	 * this means that the segment ends in (limit << 12) + 0xfff.
	 */
	limit = get_desc_limit(desc);
	if (desc->g)
		limit = (limit << 12) + 0xfff;

	return limit;
}

/**
 * insn_get_code_seg_defaults() - Obtain code segment default parameters
 * @regs:	Structure with register values as seen when entering kernel mode
 *
 * Obtain the default parameters of the code segment: address and operand sizes.
 * The code segment is obtained from the selector contained in the CS register
 * in regs. In protected mode, the default address is determined by inspecting
 * the L and D bits of the segment descriptor. In virtual-8086 mode, the default
 * is always two bytes for both address and operand sizes.
 *
 * Return: A signed 8-bit value containing the default parameters on success and
 * -EINVAL on error.
 */
char insn_get_code_seg_defaults(struct pt_regs *regs)
{
	struct desc_struct *desc;
	unsigned short sel;

	if (v8086_mode(regs))
		/* Address and operand size are both 16-bit. */
		return INSN_CODE_SEG_PARAMS(2, 2);

	sel = (unsigned short)regs->cs;

	desc = get_desc(sel);
	if (!desc)
		return -EINVAL;

	/*
	 * The most significant byte of the Type field of the segment descriptor
	 * determines whether a segment contains data or code. If this is a data
	 * segment, return error.
	 */
	if (!(desc->type & BIT(3)))
		return -EINVAL;

	switch ((desc->l << 1) | desc->d) {
	case 0: /*
		 * Legacy mode. CS.L=0, CS.D=0. Address and operand size are
		 * both 16-bit.
		 */
		return INSN_CODE_SEG_PARAMS(2, 2);
	case 1: /*
		 * Legacy mode. CS.L=0, CS.D=1. Address and operand size are
		 * both 32-bit.
		 */
		return INSN_CODE_SEG_PARAMS(4, 4);
	case 2: /*
		 * IA-32e 64-bit mode. CS.L=1, CS.D=0. Address size is 64-bit;
		 * operand size is 32-bit.
		 */
		return INSN_CODE_SEG_PARAMS(4, 8);
	case 3: /* Invalid setting. CS.L=1, CS.D=1 */
		/* fall through */
	default:
		return -EINVAL;
	}
}

/**
 * insn_get_modrm_rm_off() - Obtain register in r/m part of ModRM byte
 * @insn:	Instruction structure containing the ModRM byte
 * @regs:	Structure with register values as seen when entering kernel mode
 *
 * Return: The register indicated by the r/m part of the ModRM byte. The
 * register is obtained as an offset from the base of pt_regs. In specific
 * cases, the returned value can be -EDOM to indicate that the particular value
 * of ModRM does not refer to a register and shall be ignored.
 */
int insn_get_modrm_rm_off(struct insn *insn, struct pt_regs *regs)
{
	return get_reg_offset(insn, regs, REG_TYPE_RM);
}

/**
 * get_addr_ref_16() - Obtain the 16-bit address referred by instruction
 * @insn:	Instruction structure containing ModRM byte and displacement
 * @regs:	Structure with register values as seen when entering kernel mode
 *
 * This function is to be used with 16-bit address encodings. Obtain the memory
 * address referred by the instruction's ModRM and displacement bytes. Also, the
 * segment used as base is determined by either any segment override prefixes in
 * insn or the default segment of the registers involved in the address
 * computation. In protected mode, segment limits are enforced.
 *
 * Return: linear address referenced by instruction and registers on success.
 * -1L on error.
 */
static void __user *get_addr_ref_16(struct insn *insn, struct pt_regs *regs)
{
	unsigned long linear_addr = -1L, seg_base_addr, seg_limit;
	short eff_addr, addr1 = 0, addr2 = 0;
	int addr_offset1, addr_offset2;
	int ret;

	insn_get_modrm(insn);
	insn_get_displacement(insn);

	if (insn->addr_bytes != 2)
		goto out;

	/*
	 * If operand is a register, the layout is the same as in
	 * 32-bit and 64-bit addressing.
	 */
	if (X86_MODRM_MOD(insn->modrm.value) == 3) {
		addr_offset1 = get_reg_offset(insn, regs, REG_TYPE_RM);
		if (addr_offset1 < 0)
			goto out;

		eff_addr = regs_get_register(regs, addr_offset1);

		seg_base_addr = insn_get_seg_base(regs, insn, addr_offset1);
		if (seg_base_addr == -1L)
			goto out;

		seg_limit = get_seg_limit(regs, insn, addr_offset1);
	} else {
		ret = get_reg_offset_16(insn, regs, &addr_offset1,
					&addr_offset2);
		if (ret < 0)
			goto out;

		/*
		 * Don't fail on invalid offset values. They might be invalid
		 * because they cannot be used for this particular value of
		 * the ModRM. Instead, use them in the computation only if
		 * they contain a valid value.
		 */
		if (addr_offset1 != -EDOM)
			addr1 = 0xffff & regs_get_register(regs, addr_offset1);
		if (addr_offset2 != -EDOM)
			addr2 = 0xffff & regs_get_register(regs, addr_offset2);

		eff_addr = addr1 + addr2;

		/*
		 * The first operand register could indicate to use of either SS
		 * or DS registers to obtain the segment selector.  The second
		 * operand register can only indicate the use of DS. Thus, use
		 * the first register to obtain the segment selector.
		 */
		seg_base_addr = insn_get_seg_base(regs, insn, addr_offset1);
		if (seg_base_addr == -1L)
			goto out;

		seg_limit = get_seg_limit(regs, insn, addr_offset1);

		eff_addr += (insn->displacement.value & 0xffff);
	}

	/*
	 * Before computing the linear address, make sure the effective address
	 * is within the limits of the segment. In virtual-8086 mode, segment
	 * limits are not enforced. In such a case, the segment limit is -1L to
	 * reflect this fact.
	 */
	if ((unsigned long)(eff_addr & 0xffff) > seg_limit)
		goto out;

	linear_addr = (unsigned long)(eff_addr & 0xffff) + seg_base_addr;

	/* Limit linear address to 20 bits */
	if (v8086_mode(regs))
		linear_addr &= 0xfffff;

out:
	return (void __user *)linear_addr;
}

/**
 * get_addr_ref_32() - Obtain a 32-bit linear address
 * @insn:	Instruction struct with ModRM and SIB bytes and displacement
 * @regs:	Structure with register values as seen when entering kernel mode
 *
 * This function is to be used with 32-bit address encodings to obtain the
 * linear memory address referred by the instruction's ModRM, SIB,
 * displacement bytes and segment base address, as applicable. If in protected
 * mode, segment limits are enforced.
 *
 * Return: linear address referenced by instruction and registers on success.
 * -1L on error.
 */
static void __user *get_addr_ref_32(struct insn *insn, struct pt_regs *regs)
{
	int eff_addr, base, indx, addr_offset, base_offset, indx_offset;
	unsigned long linear_addr = -1L, seg_base_addr, seg_limit, tmp;
	insn_byte_t sib;

	insn_get_modrm(insn);
	insn_get_sib(insn);
	sib = insn->sib.value;

	if (insn->addr_bytes != 4)
		goto out;

	if (X86_MODRM_MOD(insn->modrm.value) == 3) {
		addr_offset = get_reg_offset(insn, regs, REG_TYPE_RM);
		if (addr_offset < 0)
			goto out;

		tmp = regs_get_register(regs, addr_offset);
		/* The 4 most significant bytes must be zero. */
		if (tmp & ~0xffffffffL)
			goto out;

		eff_addr = (int)(tmp & 0xffffffff);

		seg_base_addr = insn_get_seg_base(regs, insn, addr_offset);
		if (seg_base_addr == -1L)
			goto out;

		seg_limit = get_seg_limit(regs, insn, addr_offset);
	} else {
		if (insn->sib.nbytes) {
			/*
			 * Negative values in the base and index offset means
			 * an error when decoding the SIB byte. Except -EDOM,
			 * which means that the registers should not be used
			 * in the address computation.
			 */
			base_offset = get_reg_offset(insn, regs, REG_TYPE_BASE);
			if (base_offset == -EDOM) {
				base = 0;
			} else if (base_offset < 0) {
				goto out;
			} else {
				tmp = regs_get_register(regs, base_offset);
				/* The 4 most significant bytes must be zero. */
				if (tmp & ~0xffffffffL)
					goto out;

				base = (int)(tmp & 0xffffffff);
			}

			indx_offset = get_reg_offset(insn, regs, REG_TYPE_INDEX);
			if (indx_offset == -EDOM) {
				indx = 0;
			} else if (indx_offset < 0) {
				goto out;
			} else {
				tmp = regs_get_register(regs, indx_offset);
				/* The 4 most significant bytes must be zero. */
				if (tmp & ~0xffffffffL)
					goto out;

				indx = (int)(tmp & 0xffffffff);
			}

			eff_addr = base + indx * (1 << X86_SIB_SCALE(sib));

			seg_base_addr = insn_get_seg_base(regs, insn,
							  base_offset);
			if (seg_base_addr == -1L)
				goto out;

			seg_limit = get_seg_limit(regs, insn, base_offset);
		} else {
			addr_offset = get_reg_offset(insn, regs, REG_TYPE_RM);

			/*
			 * -EDOM means that we must ignore the address_offset.
			 * In such a case, in 64-bit mode the effective address
			 * relative to the RIP of the following instruction.
			 */
			if (addr_offset == -EDOM) {
				if (user_64bit_mode(regs))
					eff_addr = (long)regs->ip + insn->length;
				else
					eff_addr = 0;
			} else if (addr_offset < 0) {
				goto out;
			} else {
				tmp = regs_get_register(regs, addr_offset);
				/* The 4 most significant bytes must be zero. */
				if (tmp & ~0xffffffffL)
					goto out;

				eff_addr = (int)(tmp & 0xffffffff);
			}

			seg_base_addr = insn_get_seg_base(regs, insn,
							  addr_offset);
			if (seg_base_addr == -1L)
				goto out;

			seg_limit = get_seg_limit(regs, insn, addr_offset);
		}
		eff_addr += insn->displacement.value;
	}

	/*
	 * In protected mode, before computing the linear address, make sure
	 * the effective address is within the limits of the segment.
	 * 32-bit addresses can be used in long and virtual-8086 modes if an
	 * address override prefix is used. In such cases, segment limits are
	 * not enforced. When in virtual-8086 mode, the segment limit is -1L
	 * to reflect this situation.
	 *
	 * After computed, the effective address is treated as an unsigned
	 * quantity.
	 */
	if (!user_64bit_mode(regs) && ((unsigned int)eff_addr > seg_limit))
		goto out;

	/*
	 * Even though 32-bit address encodings are allowed in virtual-8086
	 * mode, the address range is still limited to [0x-0xffff].
	 */
	if (v8086_mode(regs) && (eff_addr & ~0xffff))
		goto out;

	/*
	 * Data type long could be 64 bits in size. Ensure that our 32-bit
	 * effective address is not sign-extended when computing the linear
	 * address.
	 */
	linear_addr = (unsigned long)(eff_addr & 0xffffffff) + seg_base_addr;

	/* Limit linear address to 20 bits */
	if (v8086_mode(regs))
		linear_addr &= 0xfffff;

out:
	return (void __user *)linear_addr;
}

/**
 * get_addr_ref_64() - Obtain a 64-bit linear address
 * @insn:	Instruction struct with ModRM and SIB bytes and displacement
 * @regs:	Structure with register values as seen when entering kernel mode
 *
 * This function is to be used with 64-bit address encodings to obtain the
 * linear memory address referred by the instruction's ModRM, SIB,
 * displacement bytes and segment base address, as applicable.
 *
 * Return: linear address referenced by instruction and registers on success.
 * -1L on error.
 */
#ifndef CONFIG_X86_64
static void __user *get_addr_ref_64(struct insn *insn, struct pt_regs *regs)
{
	return (void __user *)-1L;
}
#else
static void __user *get_addr_ref_64(struct insn *insn, struct pt_regs *regs)
{
	unsigned long linear_addr = -1L, seg_base_addr;
	int addr_offset, base_offset, indx_offset;
	long eff_addr, base, indx;
	insn_byte_t sib;

	insn_get_modrm(insn);
	insn_get_sib(insn);
	sib = insn->sib.value;

	if (insn->addr_bytes != 8)
		goto out;

	if (X86_MODRM_MOD(insn->modrm.value) == 3) {
		addr_offset = get_reg_offset(insn, regs, REG_TYPE_RM);
		if (addr_offset < 0)
			goto out;

		eff_addr = regs_get_register(regs, addr_offset);

		seg_base_addr = insn_get_seg_base(regs, insn, addr_offset);
		if (seg_base_addr == -1L)
			goto out;
	} else {
		if (insn->sib.nbytes) {
			/*
			 * Negative values in the base and index offset means
			 * an error when decoding the SIB byte. Except -EDOM,
			 * which means that the registers should not be used
			 * in the address computation.
			 */
			base_offset = get_reg_offset(insn, regs, REG_TYPE_BASE);
			if (base_offset == -EDOM)
				base = 0;
			else if (base_offset < 0)
				goto out;
			else
				base = regs_get_register(regs, base_offset);

			indx_offset = get_reg_offset(insn, regs, REG_TYPE_INDEX);

			if (indx_offset == -EDOM)
				indx = 0;
			else if (indx_offset < 0)
				goto out;
			else
				indx = regs_get_register(regs, indx_offset);

			eff_addr = base + indx * (1 << X86_SIB_SCALE(sib));

			seg_base_addr = insn_get_seg_base(regs, insn,
							  base_offset);
			if (seg_base_addr == -1L)
				goto out;
		} else {
			addr_offset = get_reg_offset(insn, regs, REG_TYPE_RM);
			/*
			 * -EDOM means that we must ignore the address_offset.
			 * In such a case, in 64-bit mode the effective address
			 * relative to the RIP of the following instruction.
			 */
			if (addr_offset == -EDOM) {
				if (user_64bit_mode(regs))
					eff_addr = (long)regs->ip + insn->length;
				else
					eff_addr = 0;
			} else if (addr_offset < 0) {
				goto out;
			} else {
				eff_addr = regs_get_register(regs, addr_offset);
			}

			seg_base_addr = insn_get_seg_base(regs, insn,
							  addr_offset);
			if (seg_base_addr == -1L)
				goto out;
		}

		eff_addr += insn->displacement.value;
	}

	linear_addr = (unsigned long)eff_addr + seg_base_addr;

out:
	return (void __user *)linear_addr;
}
#endif /* CONFIG_X86_64 */

/**
 * insn_get_addr_ref() - Obtain the linear address referred by instruction
 * @insn:	Instruction structure containing ModRM byte and displacement
 * @regs:	Structure with register values as seen when entering kernel mode
 *
 * Obtain the linear address referred by the instruction's ModRM, SIB and
 * displacement bytes, and segment base, as applicable. In protected mode,
 * segment limits are enforced.
 *
 * Return: linear address referenced by instruction and registers on success.
 * -1L on error.
 */
void __user *insn_get_addr_ref(struct insn *insn, struct pt_regs *regs)
{
	switch (insn->addr_bytes) {
	case 2:
		return get_addr_ref_16(insn, regs);
	case 4:
		return get_addr_ref_32(insn, regs);
	case 8:
		return get_addr_ref_64(insn, regs);
	default:
		return (void __user *)-1L;
	}
}
