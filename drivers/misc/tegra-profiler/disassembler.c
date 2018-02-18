/*
 * drivers/misc/tegra-profiler/disassembler.c
 *
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

/* To debug this code, define DEBUG here and QM_DEBUG_DISASSEMBLER
   at the beginning of disassembler.h. */

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/tegra_profiler.h>

#include "tegra.h"
#include "disassembler.h"

/* FIXME: grossly duplicated */

#define read_user_data(addr, retval)				\
({								\
	long ret;						\
								\
	pagefault_disable();					\
	ret = __get_user(retval, addr);				\
	pagefault_enable();					\
								\
	if (ret) {						\
		pr_debug("%s: failed for address: %p\n",	\
			 __func__, addr);			\
		ret = -QUADD_URC_EACCESS;			\
	}							\
								\
	ret;							\
})

static long
quadd_arm_imm(u32 val)
{
	unsigned int rot = (val & 0xf00) >> 7, imm = (val & 0xff);

	return ((imm << (32 - rot)) | (imm >> rot)) & 0xffffffff;
}

static int
quadd_stack_found(struct quadd_disasm_data *qd)
{
	return qd->stackreg != -1 || qd->stacksize != 0;
}

static void
quadd_print_reg(char *buf, size_t size, int reg)
{
	if (reg != -1)
		snprintf(buf, size, "r%d", reg);
	else
		snprintf(buf, size, "<unused>");
}

/* Search interesting ARM insns in [qd->min...qd->max),
   may preliminary stop at unconditional branch or pop. */

static long
quadd_disassemble_arm(struct quadd_disasm_data *qd)
{
	unsigned long addr;

	for (addr = qd->min; addr < qd->max; addr += 4) {
		u32 val;

		if (read_user_data((const u32 __user *) addr, val) < 0)
			return -QUADD_URC_EACCESS;

		if (((val & 0x0def0ff0) == 0x01a00000) &&
		    !quadd_stack_found(qd)) {
			unsigned x = (val >> 12) & 0xf, y = (val & 0xf);

			if (y == 13 && x != y) {
				/* mov x, sp, where x != sp */
				qd->stackreg = x;
				qd->stackoff = 0;
#ifdef QM_DEBUG_DISASSEMBLER
				qd->stackmethod = 1;
#endif
			}
		} else if ((((val & 0x0fe00000) == 0x02800000) ||
			    ((val & 0x0fe00010) == 0x00800000) ||
			    ((val & 0x0fe00090) == 0x00800010)) &&
			   !quadd_stack_found(qd)) {
			unsigned x = (val >> 12) & 0xf, y = (val >> 16) & 0xf;

			if (y == 13 && x != y) {
				/* add x, sp, i, where x != sp */
				qd->stackreg = x;
				qd->stackoff = quadd_arm_imm(val);
#ifdef QM_DEBUG_DISASSEMBLER
				qd->stackmethod = 2;
#endif
			}
		} else if ((((val & 0x0fe00000) == 0x02400000) ||
			    ((val & 0x0fe00010) == 0x00400000)) &&
			   !quadd_stack_found(qd)) {
			unsigned x = (val >> 12) & 0xf, y = (val >> 16) & 0xf;

			if (x == 13 && y == 13) {
				/* sub sp, sp, imm */
				qd->stacksize += quadd_arm_imm(val);
#ifdef QM_DEBUG_DISASSEMBLER
				qd->stackmethod = 3;
#endif
			}
		} else if ((val & 0x0fff0000) == 0x092d0000) {
			/* push [regset] */
			qd->r_regset |= (val & 0xffff);
		} else if ((val & 0x0fff0fff) == 0x052d0004) {
			/* push [reg] */
			qd->r_regset |= (1 << ((val >> 12) & 0xf));
		} else if ((val & 0x0fbf0f01) == 0x0d2d0b00) {
			/* vpush [reg/reg+off-1] */
			int reg = ((val >> 12) & 0xf) | ((val >> 18) & 0x10);
			int off = (val >> 1) & 0x3f;

			if (off == 1)
				qd->d_regset |= (1 << reg);
			else if (reg + off <= 32) {
				int i;

				for (i = reg; i < reg + off; i++)
					qd->d_regset |= (1 << i);
			}
		} else if ((((val & 0x0e000000) == 0x0a000000) ||
			    ((val & 0x0ffffff0) == 0x012fff10)) &&
			   ((val >> 28) & 0xf) == 0xe) {
			/* b, bl, bx, blx */
			qd->max = addr + 4;
			break;
		} else if (((val & 0x0fff0fff) == 0x049d0004) ||
			   ((val & 0x0fff0000) == 0x08bd0000)) {
			/* pop [reg], pop [regset] */
			qd->max = addr + 4;
			break;
		}
	}
	return QUADD_URC_SUCCESS;
}

/* Likewise for Thumb. */

static long
quadd_disassemble_thumb(struct quadd_disasm_data *qd)
{
	unsigned long addr;

	for (addr = qd->min; addr < qd->max; addr += 2) {
		u16 val1;

		if (read_user_data((const u16 __user *) addr, val1) < 0)
			return -QUADD_URC_EACCESS;

		if ((val1 & 0xf800) == 0xa800 && !quadd_stack_found(qd)) {
			/* add x, sp, i */
			qd->stackreg = ((val1 >> 8) & 0x7);
			qd->stackoff = ((val1 & 0xff) * 4);
#ifdef QM_DEBUG_DISASSEMBLER
			qd->stackmethod = 1;
#endif
		} else if ((val1 & 0xff80) == 0xb080 &&
			   !quadd_stack_found(qd)) {
			/* sub sp, imm */
			qd->stacksize += (val1 & 0x7f) * 4;
#ifdef QM_DEBUG_DISASSEMBLER
			qd->stackmethod = 2;
#endif
		} else if ((val1 & 0xfe00) == 0xb400) {
			/* push [regset] */
			qd->r_regset |= (val1 & 0xff);
			if (val1 & (1 << 8))
				/* LR is special */
				qd->r_regset |= (1 << 14);
		} else if ((val1 & 0xff80) == 0x4700 ||
			   (val1 & 0xff87) == 0x4780 ||
			   ((val1 & 0xf000) == 0xd000 &&
			    ((val1 >> 8) & 0xf) == 0xe) ||
			   (val1 & 0xf800) == 0xe000) {
			/* bx, blx, b(1), b(2) */
			qd->max = addr + 2;
			break;
		} else if ((val1 & 0xfe00) == 0xbc00) {
			/* pop */
			qd->max = addr + 2;
			break;
		} else if (((val1 & 0xf800) == 0xf800
			    || (val1 & 0xf800) == 0xf000
			    || (val1 & 0xf800) == 0xe800)
			   && addr + 2 < qd->max) {
			/* 4-byte insn still in range */
			u16 val2;
			u32 val;

			if (read_user_data((const u16 __user *)
					   (addr + 2), val2) < 0)
				return -QUADD_URC_EACCESS;
			val = (val1 << 16) | val2;
			addr += 2;

			if ((val & 0xfbe08000) == 0xf1a00000
			    && ((val >> 8) & 0xf) == 13
			    && ((val >> 16) & 0xf) == 13) {
				/* sub.w sp, sp, imm */
				unsigned int bits = 0, imm, imm8, mod;

				bits |= (val & 0x000000ff);
				bits |= (val & 0x00007000) >> 4;
				bits |= (val & 0x04000000) >> 15;
				imm8 = (bits & 0x0ff);
				mod = (bits & 0xf00) >> 8;
				if (mod == 0)
					imm = imm8;
				else if (mod == 1)
					imm = ((imm8 << 16) | imm8);
				else if (mod == 2)
					imm = ((imm8 << 24) | (imm8 << 8));
				else if (mod == 3)
					imm = ((imm8 << 24) | (imm8 << 16) |
					       (imm8 << 8) | imm8);
				else {
					mod  = (bits & 0xf80) >> 7;
					imm8 = (bits & 0x07f) | 0x80;
					imm  = (((imm8 << (32 - mod)) |
						 (imm8 >> mod)) & 0xffffffff);
				}
				qd->stacksize += imm;
			} else if ((val & 0x0fbf0f01) == 0x0d2d0b00) {
				/* vpush [reg/reg+off-1] */
				int reg = (((val >> 12) & 0xf) |
					   ((val >> 18) & 0x10));
				int off = (val >> 1) & 0x3f;

				if (off == 1)
					qd->d_regset |= (1 << reg);
				else if (reg + off <= 32) {
					int i;

					for (i = reg; i < reg + off; i++)
						qd->d_regset |= (1 << i);
				}
			} else if ((val & 0xffd00000) == 0xe9000000
				   && ((val >> 16) & 0xf) == 13) {
				/* stmdb sp!,[regset] */
				qd->r_regset |= (val & 0xffff);
			} else if (((val & 0xf800d000) == 0xf0009000 ||
				    (val & 0xf800d001) == 0xf000c000 ||
				    (val & 0xf800d000) == 0xf000d000 ||
				    (val & 0xf800d000) == 0xf0008000) &&
				   ((val >> 28) & 0xf) >= 0xe) {
				/* b.w, bl, blx */
				qd->max = addr + 2;
				break;
			} else if ((val & 0xffd00000) == 0xe8900000) {
				/* ldmia.w */
				qd->max = addr + 2;
				break;
			}
		}
	}
	return QUADD_URC_SUCCESS;
}

/* Wrapper for the two above, depend on arm/thumb mode. */

long
quadd_disassemble(struct quadd_disasm_data *qd, unsigned long min,
		  unsigned long max, int thumbflag)
{
	qd->thumb = thumbflag;
	qd->min = min;
	qd->max = max;
	qd->stackreg = qd->ustackreg = -1;
	qd->stackoff = qd->stacksize = qd->r_regset = qd->d_regset = 0;
#ifdef QM_DEBUG_DISASSEMBLER
	qd->stackmethod = 0;
#endif
	return thumbflag ? quadd_disassemble_thumb(qd) :
		quadd_disassemble_arm(qd);
}

#ifdef QM_DEBUG_DISASSEMBLER

static void
quadd_disassemble_debug(unsigned long pc, struct quadd_disasm_data *qd)
{
	int i;
	char msg[256], *p = msg, *end = p + sizeof(msg);

	pr_debug("  pc %#lx: disassembled in %#lx..%#lx as %s:\n",
		 pc, qd->min, qd->max, (qd->thumb ? "thumb" : "arm"));

	if (quadd_stack_found(qd)) {
		char regname[16];

		quadd_print_reg(regname, sizeof(regname), qd->stackreg);

		p += snprintf(p, end - p, "  method %d", qd->stackmethod);
		p += snprintf(p, end - p,
			      ", stackreg %s, stackoff %ld, stacksize %ld",
			      regname, qd->stackoff, qd->stacksize);
	} else
		p += snprintf(p, end - p, "  stack is not used");

	if (qd->r_regset) {
		p += snprintf(p, end - p, ", core registers:");
		for (i = 0; i < 16; i++)
			if (qd->r_regset & (1 << i))
				p += snprintf(p, end - p, " r%d", i);
	} else
		p += snprintf(p, end - p, ", core registers are not saved");

	if (qd->d_regset) {
		p += snprintf(p, end - p, ", fp registers:");
		for (i = 0; i < 32; i++)
			if (qd->d_regset & (1 << i))
				p += snprintf(p, end - p, " d%d", i);
	} else
		p += snprintf(p, end - p, ", fp registers are not saved");
	pr_debug("%s\n", msg);
}

#endif /* QM_DEBUG_DISASSEMBLER */

/* If we unwind less stack space than found, or don't unwind a register,
   or restore sp from wrong register with wrong offset, something is bad. */

long
quadd_check_unwind_result(unsigned long pc, struct quadd_disasm_data *qd)
{
	if (qd->stacksize > 0 || qd->r_regset != 0 || qd->d_regset != 0 ||
	    (qd->stackreg != qd->ustackreg) || qd->stackoff != 0) {
		int i;
		char regname[16], uregname[16];

		pr_debug("in %s code at %#lx, unwind %#lx..%#lx mismatch:",
			 (qd->thumb ? "thumb" : "arm"), pc, qd->min, qd->max);

		quadd_print_reg(regname, sizeof(regname), qd->stackreg);
		quadd_print_reg(uregname, sizeof(uregname), qd->ustackreg);

		pr_debug("  stackreg %s/%s, stackoff %ld\n",
			 regname, uregname, qd->stackoff);

		if (qd->stacksize > 0)
			pr_debug("  %ld stack bytes was not unwound\n",
				 qd->stacksize);
		if (qd->r_regset != 0) {
			for (i = 0; i < 16; i++)
				if (qd->r_regset & (1 << i))
					pr_debug("  r%d was not unwound\n", i);
		}
		if (qd->d_regset != 0) {
			for (i = 0; i < 32; i++)
				if (qd->d_regset & (1 << i))
					pr_debug("  d%d was not unwound\n", i);
		}
#ifdef QM_DEBUG_DISASSEMBLER
		quadd_disassemble_debug(pc, qd->orig);
#endif
		return -QUADD_URC_UNWIND_MISMATCH;
	}
	return QUADD_URC_SUCCESS;
}
