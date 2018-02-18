/*
 * drivers/misc/tegra-profiler/disassembler.h
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

#ifndef __QUADD_DISASSEMBLER_H
#define __QUADD_DISASSEMBLER_H

/* By default we disassemble at most 128 bytes from the beginning
   of the function... */
#define QUADD_DISASM_MAX 128

/* ...but not less than 16 bytes to get meaningful results. */
#define QUADD_DISASM_MIN 16

struct quadd_disasm_data {
	int thumb;
	unsigned long min, max;
	int r_regset;
	int d_regset;
	/* Used if stack is adjusted with sub sp, sp, [stacksize] (ARM)
	   or sub sp, [stacksize] (Thumb). */
	long stacksize;
	/* Otherwise used if sp is saved with add [stackreg], sp, [stackoff]
	   or mov [stackreg], sp. */
	int stackreg;
	long stackoff;
	/* If the latter, actual register used to restore sp during unwind. */
	int ustackreg;
#ifdef QM_DEBUG_DISASSEMBLER
	int stackmethod;
	struct quadd_disasm_data *orig;
#endif
};

extern long quadd_disassemble(struct quadd_disasm_data *, unsigned long,
			      unsigned long, int);
extern long quadd_check_unwind_result(unsigned long,
				      struct quadd_disasm_data *);

#endif /* __QUADD_DISASSEMBLER_H */
