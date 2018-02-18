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

struct a57_ramid {
	char *name;
	u64 id;
};

/*
 * Register CPUMERRSR_EL1 definitions
 */
#define A57_CPUMERRSR_FATAL		MCA_ARI_BIT(63)
#define get_a57_cpumerrsr_other(_x_)	MCA_ARI_EXTRACT(_x_, 47, 40)
#define get_a57_cpumerrsr_repeat(_x_)	MCA_ARI_EXTRACT(_x_, 39, 32)
#define A57_CPUMERRSR_VALID		MCA_ARI_BIT(31)
#define get_a57_cpumerrsr_ramid(_x_)	MCA_ARI_EXTRACT(_x_, 30, 24)
#define get_a57_cpumerrsr_bank(_x_)	MCA_ARI_EXTRACT(_x_, 22, 18)
#define get_a57_cpumerrsr_index(_x_)	MCA_ARI_EXTRACT(_x_, 17, 0)

/*
 * Register L2MERRSR_EL1 definitions
 */
#define A57_L2MERRSR_FATAL		MCA_ARI_BIT(63)
#define get_a57_l2merrsr_other(_x_)	MCA_ARI_EXTRACT(_x_, 47, 40)
#define get_a57_l2merrsr_repeat(_x_)	MCA_ARI_EXTRACT(_x_, 39, 32)
#define A57_L2MERRSR_VALID		MCA_ARI_BIT(31)
#define get_a57_l2merrsr_ramid(_x_)	MCA_ARI_EXTRACT(_x_, 30, 24)
#define get_a57_l2merrsr_cpuid(_x_)	MCA_ARI_EXTRACT(_x_, 21, 19)
#define get_a57_l2merrsr_way(_x_)	MCA_ARI_EXTRACT(_x_, 18, 18)
#define get_a57_l2merrsr_index(_x_)	MCA_ARI_EXTRACT(_x_, 17, 0)

/*
 * Register L2CTLR_EL1 definitions
 */
#define A57_L2CTLR_ECC_EN		MCA_ARI_BIT(21)
