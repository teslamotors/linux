/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */


#ifndef __INCLUDE_MCERR_ECC_T18X_H
#define __INCLUDE_MCERR_ECC_T18X_H

#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>

#define MAX_CHANNELS                              4


#define DRAM_ECC_DISABLE                          0
#define DRAM_ECC_ENABLE                           1

#define HW_SCRUBBER_CGID                        170
#define ERROR_INJ_CGID                          157

#define MC_ECC_LOG_RING_MODE                      0
#define MC_ECC_LOG_WRITE_STOP_MODE                1

#define MC_ECC_LOG_BUFF_DEPTH                    32

extern void __iomem *emc;
extern void __iomem *emc_regs[MAX_CHANNELS];
extern u32 ecc_int_mask;

struct mc_ecc_err_log {
	u32 emc_ecc_err_req;
	u32 ecc_err_cgid;
	u32 ecc_err_ch;
	u32 emc_ecc_err_sp0;
	u32 emc_ecc_err_sp1;
	u32 ecc_eerr_par_sp0;
	u32 ecc_derr_par_sp0;
	u32 ecc_err_poison_sp0;
	u32 ecc_err_bit_sp0;
	u32 ecc_eerr_par_sp1;
	u32 ecc_derr_par_sp1;
	u32 ecc_err_poison_sp1;
	u32 ecc_err_bit_sp1;
	u32 ecc_err_addr;
	u32 ecc_err_dev;
	u32 ecc_err_size;
	u32 ecc_err_swap;
	u32 row;
	u32 bank;
	u32 col;
	u32 col_sp0;
	u32 col_sp1;
	u32 gob;
	u32 err_seq;
	u32 subp;
};

#define DT_REG_INDEX_EMC_BROADCAST	5


void mc_ecc_config_read(void);
int mc_ecc_config_dump(struct seq_file *s, void *v);

u64 mc_addr_translate(u32 device, u32 ch, u32 row, u32 bank, u32 col, u32 subp,
								u32 lsb);

#endif
