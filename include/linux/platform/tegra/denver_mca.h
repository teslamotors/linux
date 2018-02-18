/*
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
 */

/* Template for MCA bank register accessors */
#define DENVER_MCA_OP(bank, reg, crm, op2)				\
static u64 denver_mca_ ##reg##_##bank(void)				\
{									\
	u64 ret;							\
	asm volatile ("mrs %0, s3_0_c15_c" #crm "_" #op2 : "=r"(ret));	\
	return ret;							\
}

/* A macro to instanciate all of the register accessors in the bank */
#define DEFINE_DENVER_MCA_OPS(bank,					\
        crm_ctrl, op2_ctrl,						\
        crm_stat, op2_stat,						\
        crm_addr, op2_addr,						\
        crm_msc1, op2_msc1,						\
        crm_msc2, op2_msc2)						\
        DENVER_MCA_OP(bank, ctrl, crm_ctrl, op2_ctrl)			\
        DENVER_MCA_OP(bank, stat, crm_stat, op2_stat)			\
        DENVER_MCA_OP(bank, addr, crm_addr, op2_addr)			\
        DENVER_MCA_OP(bank, msc1, crm_msc1, op2_msc1)			\
        DENVER_MCA_OP(bank, msc2, crm_msc2, op2_msc2)

struct denver_mca_error {
	char *name;
	u16 error_code;
};

struct denver_mca_bank {
	struct list_head node;
	char *name;
	u64 bank;
	struct denver_mca_error *errors;
	u64 (*ctrl)(void);
	u64 (*stat)(void);
	u64 (*addr)(void);
	u64 (*msc1)(void);
	u64 (*msc2)(void);
	u8 processed;
};

/* Helper macro for filling in struct denver_mca_bank */
#define DENVER_MCA_OP_ENTRY(_bank)					\
	.bank = _bank,							\
	.ctrl = denver_mca_ctrl_ ## _bank,				\
	.stat = denver_mca_stat_ ## _bank,				\
	.addr = denver_mca_addr_ ## _bank,				\
	.msc1 = denver_mca_msc1_ ## _bank,				\
	.msc2 = denver_mca_msc2_ ## _bank

/* Helper macro for a denver_mca_bank without a denver_mca_error */
#define SIMPLE_DENVER_MCA_OP_ENTRY(_name, _bank)			\
	.name = _name,							\
	DENVER_MCA_OP_ENTRY(_bank)

void register_denver_mca_bank(struct denver_mca_bank *bank);
void unregister_denver_mca_bank(struct denver_mca_bank *bank);
