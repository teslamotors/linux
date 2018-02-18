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

#include <asm/cpu.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/traps.h>
#include <linux/debugfs.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/tegra-mce.h>
#include <linux/platform/tegra/ari_mca.h>
#include <linux/ioport.h>
#include <linux/tegra-soc.h>

#define ARI_BANK_PRINTF		0
#define ARI_MCA_SAVE_PREBOOT	0
#define ARI_MCA_CONTROL_ACCESS	0

/* MCA bank handling functions */

static int read_bank_info(u64 *data)
{
	u32 error;
	int e;
	mca_cmd_t mca_cmd = {.cmd = MCA_ARI_CMD_RD_SERR,
			     .idx = MCA_ARI_IDX_BANKINFO,
			     .subidx = 0};

	e = tegra_mce_read_uncore_mca(mca_cmd, data, &error);
	if (e != 0) {
		pr_err("%s: ARI call failed\n", __func__);
		return -EINVAL;
	}

	*data &= 0xff;
	return 0;
}

static int read_bank_template(u64 bank, u64 *data)
{
	u32 error;
	int e;
	mca_cmd_t mca_cmd = {.cmd = MCA_ARI_CMD_RD_SERR,
			     .idx = MCA_ARI_IDX_BANKTEMPLATE,
			     .subidx = 0};

	e = tegra_mce_read_uncore_mca(mca_cmd, data, &error);
	if (e != 0) {
		pr_err("%s: ARI call failed\n", __func__);
		return -EINVAL;
	}

	*data = (*data >> (8 * bank)) & 0xff;
	return 0;
}

#if ARI_MCA_CONTROL_ACCESS
static int read_bank_control(struct ari_mca_bank *mca_bank, u64 *data,
			     int preboot)
{
	u32 error;
	int e;
	mca_cmd_t mca_cmd = {.cmd = MCA_ARI_CMD_RD_SERR,
			     .idx = mca_bank->bank,
			     .subidx = MCA_ARI_RW_SUBIDX_CTRL};

#if ARI_BANK_PRINTF
	pr_crit("%s: CPU%d mca_cmd = 0x%llx\n", __func__,
		smp_processor_id(), mca_cmd.data);
#endif
	e = tegra_mce_read_uncore_mca(mca_cmd, data, &error);
	if (e != 0) {
		pr_err("%s: ARI call failed\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int write_bank_control(struct ari_mca_bank *mca_bank, u64 *data)
{
	u32 error;
	int e;
	mca_cmd_t mca_cmd = {.cmd = MCA_ARI_CMD_WR_SERR,
			     .idx = mca_bank->bank,
			     .subidx = MCA_ARI_RW_SUBIDX_CTRL};

#if ARI_BANK_PRINTF
	pr_crit("%s: CPU%d mca_cmd = 0x%llx\n", __func__,
		smp_processor_id(), mca_cmd.data);
#endif
	e = tegra_mce_read_uncore_mca(mca_cmd, data, &error);
	if (e != 0) {
		pr_err("%s: ARI call failed\n", __func__);
		return -EINVAL;
	}
	return 0;
}
#endif

static int read_bank_status(struct ari_mca_bank *mca_bank, u64 *data,
			    int preboot)
{
	u32 error;
	int e;
	mca_cmd_t mca_cmd = {.cmd = MCA_ARI_CMD_RD_SERR,
			     .idx = mca_bank->bank,
			     .subidx = MCA_ARI_RW_SUBIDX_STAT};

	mca_cmd.cmd = preboot ? MCA_ARI_CMD_RD_PREBOOT_SERR :
		      MCA_ARI_CMD_RD_SERR;

#if ARI_BANK_PRINTF
	pr_crit("%s: CPU%d mca_cmd = 0x%llx\n", __func__,
		smp_processor_id(), mca_cmd.data);
#endif
	e = tegra_mce_read_uncore_mca(mca_cmd, data, &error);
	if (e != 0) {
		pr_err("%s: ARI call failed\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int read_bank_address(struct ari_mca_bank *mca_bank, u64 *data,
			     int preboot)
{
	u32 error;
	int e;
	mca_cmd_t mca_cmd = {.cmd = MCA_ARI_CMD_RD_SERR,
			     .idx = mca_bank->bank,
			     .subidx = MCA_ARI_RW_SUBIDX_ADDR};

	mca_cmd.cmd = preboot ? MCA_ARI_CMD_RD_PREBOOT_SERR :
		      MCA_ARI_CMD_RD_SERR;

#if ARI_BANK_PRINTF
	pr_crit("%s: CPU%d mca_cmd = 0x%llx\n", __func__,
		smp_processor_id(), mca_cmd.data);
#endif
	e = tegra_mce_read_uncore_mca(mca_cmd, data, &error);
	if (e != 0) {
		pr_err("%s: ARI call failed\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int read_bank_misc1(struct ari_mca_bank *mca_bank, u64 *data,
			   int preboot)
{
	u32 error;
	int e;
	mca_cmd_t mca_cmd = {.cmd = MCA_ARI_CMD_RD_SERR,
			     .idx = mca_bank->bank,
			     .subidx = MCA_ARI_RW_SUBIDX_MSC1};

	mca_cmd.cmd = preboot ? MCA_ARI_CMD_RD_PREBOOT_SERR :
		      MCA_ARI_CMD_RD_SERR;

#if ARI_BANK_PRINTF
	pr_crit("%s: CPU%d mca_cmd = 0x%llx\n", __func__,
		smp_processor_id(), mca_cmd.data);
#endif
	e = tegra_mce_read_uncore_mca(mca_cmd, data, &error);
	if (e != 0) {
		pr_err("%s: ARI call failed\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int read_bank_misc2(struct ari_mca_bank *mca_bank, u64 *data,
			   int preboot)
{
	u32 error;
	mca_cmd_t mca_cmd = {.cmd = MCA_ARI_CMD_RD_SERR,
			     .idx = mca_bank->bank,
			     .subidx = MCA_ARI_RW_SUBIDX_MSC2};

	mca_cmd.cmd = preboot ? MCA_ARI_CMD_RD_PREBOOT_SERR :
		      MCA_ARI_CMD_RD_SERR;

#if ARI_BANK_PRINTF
	pr_crit("%s: CPU%d mca_cmd = 0x%llx\n", __func__,
		smp_processor_id(), mca_cmd.data);
#endif
	if (tegra_mce_read_uncore_mca(mca_cmd, data, &error) != 0) {
		pr_err("%s: ARI call failed\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static void save_bank(struct ari_mca_bank *mca_bank, int preboot)
{
	if (mca_bank->reg_mask & MCA_ARI_STAT_REG_MASK)
		read_bank_status(mca_bank, &mca_bank->saved_status, preboot);
	else
		mca_bank->saved_status = 0ULL;

	if (mca_bank->reg_mask & MCA_ARI_ADDR_REG_MASK)
		read_bank_address(mca_bank, &mca_bank->saved_addr, preboot);
	else
		mca_bank->saved_addr = 0ULL;

	if (mca_bank->reg_mask & MCA_ARI_MSC1_REG_MASK)
		read_bank_misc1(mca_bank, &mca_bank->saved_msc1, preboot);
	else
		mca_bank->saved_msc1 = 0ULL;

	if (mca_bank->reg_mask & MCA_ARI_MSC2_REG_MASK)
		read_bank_misc2(mca_bank, &mca_bank->saved_msc2, preboot);
	else
		mca_bank->saved_msc2 = 0ULL;
}

static char *cqx_rd_cmds[] = {
	"CRd",
	"NCRd",
	"ICRd",
	"IORd",
	"RSVD0",
	"RSVD1",
	"RSVD2",
	"RSVD4",
};
#define NUM_CQX_RD_CMDS	(sizeof(cqx_rd_cmds)/sizeof(char *))

static char *cqx_wr_cmds[] = {
	"CWb",
	"CBarrier",
	"NCWr",
	"PWr",
	"RSVD0",
	"ICWr",
	"ICBarrier",
	"IOWr",
	"ICWrBE",
	"RSVD4",
};
#define NUM_CQX_WR_CMDS	(sizeof(cqx_wr_cmds)/sizeof(char *))

static char *cqx_cids[] = {
	"L2_0 (Denver)",
	"L2_1 (A57)",
	"DMA",
	"MISC",
};
#define NUM_CQX_CIDS (sizeof(cqx_cids)/sizeof(char *))

static char *cce_cqx_cmds[] = {
	"CRdEx",		/* 0 */
	"CRdExOptData",		/* 1 */
	"CRdExNoData",		/* 2 */
	"CRdSh",		/* 3 */
	"CRdShUp",		/* 4 */
	"CRdExUp",		/* 5 */
	"CRdClean",		/* 6 */
	"CRdCleanInv",		/* 7 */
	"CRdCleanInvNoTR",	/* 8 */
	"CRdMigration",		/* 9 */
	"CUpgrade",		/* 10 */
	"DUpgrade",		/* 11 */
	"NCRd",			/* 12 */
	"ICRd",			/* 13 */
	"IORd",			/* 14 */
	"CWb",			/* 15 */
	"CWbND",		/* 16 */
	"CWbDown",		/* 17 */
	"CWbMod",		/* 18 */
	"CWbModDown",		/* 19 */
	"CWbDataOnly",		/* 20 */
	"CBarrier",		/* 21 */
	"NCWr",			/* 22 */
	"IOWr",			/* 23 */
	"PWr",			/* 24 */
	"ICWr",			/* 25 */
	"ICWrBE",		/* 26 */
	"ICBarrier",		/* 27 */
	"NCRdExOptData",	/* 28 */
	"WCFlushPA",		/* 29 */
	"IOIOWrFlush",		/* 30 */
	"DMACRd",		/* 31 */
	"DMACWr",		/* 32 */
};
#define NUM_CCE_CQX_CMDS (sizeof(cce_cqx_cmds)/sizeof(char *))

static char *axi_responses[] = {
	"OKAY",
	"Exclusive Access OKAY",
	"Slave Error",
	"Decode Error",
};
#define NUM_AXI_RESPONSES (sizeof(axi_responses)/sizeof(char *))

static char *ctu_cmds[] = {
	"NewFG",
	"NewCG",
	"SetFG",
	"SetCG",
	"Clear",
	"ClearLP",
	"Promote",
	"ClearAll",
	"GacCG",
	"ClearNFetch",
	"FetchFG",
	"FetchCG",
};
#define NUM_CTU_CMDS (sizeof(ctu_cmds)/sizeof(char *))

static struct ari_mca_error roc_mcb_mc_errors[] = {
	{.name = "READ Error", .error_code = MCA_ARI_ROC_MCB_READ_ERROR },
	{.name = "Write Error", .error_code = MCA_ARI_ROC_MCB_WRITE_ERROR },
	{}
};

static struct ari_bits sys_dpmu_stat_bits[] = {
	{.name = "DMCE Error", .mask = MCA_ARI_SYS_DPMU_STAT_DMCE_ERR},
	{.name = "CRAB Access Timeout", .mask = MCA_ARI_SYS_DPMU_STAT_CRAB_ERR},
	{.name = "UCode Error", .mask = MCA_ARI_SYS_DPMU_STAT_UCODE_ERR},
	{}
};

static struct ari_bits roc_iob_stat_bits[] = {
	{.name = "MSI Error", .mask = MCA_ARI_ROC_IOB_STAT_MSI_ERR},
	{.name = "IHI Error", .mask = MCA_ARI_ROC_IOB_STAT_IHI_ERR},
	{.name = "CTU RAM Interface Error",
	 .mask = MCA_ARI_ROC_IOB_STAT_CRI_ERR},
	{.name = "MMCRAB Error", .mask = MCA_ARI_ROC_IOB_STAT_MMCRAB_ERR},
	{.name = "CSI Error", .mask = MCA_ARI_ROC_IOB_STAT_CSI_ERR},
	{}
};

static struct ari_bits roc_cce_stat_bits[] = {
	{.name = "Timeout Error", .mask = MCA_ARI_ROC_CCE_STAT_TO_ERR},
	{.name = "Directory State Error",
	 .mask = MCA_ARI_ROC_CCE_STAT_STAT_ERR},
	{.name = "Destination Error", .mask = MCA_ARI_ROC_CCE_STAT_DST_ERR},
	{.name = "Unsupported Command Error",
	 .mask = MCA_ARI_ROC_CCE_STAT_UNC_ERR},
	{.name = "Mutli-Hit Error", .mask = MCA_ARI_ROC_CCE_STAT_MH_ERR},
	{.name = "Parity Error", .mask = MCA_ARI_ROC_CCE_STAT_PERR},
	{.name = "Poison Error", .mask = MCA_ARI_ROC_CCE_STAT_PSN_ERR},
	{}
};

static struct ari_bits roc_cce_msc1_bits[] = {
	{.name = "Timeout", .mask = MCA_ARI_ROC_CCE_MSC1_TO},
	{.name = "Divide by 4", .mask = MCA_ARI_ROC_CCE_MSC1_DIV4},
	{}
};

static struct ari_bits roc_cce_to_err_art_bits[] = {
	{.name = "\tART Pending Chaining",
	 .mask = MCA_ARI_ROC_CCE_ART_PENDING_CHAINING},
	{.name = "\tART Pending Tag Replay",
	 .mask = MCA_ARI_ROC_CCE_ART_PENDING_TAG_REPLAY},
	{.name = "\tART Pending Full Replay",
	 .mask = MCA_ARI_ROC_CCE_ART_PENDING_FULL_REPLAY},
	{.name = "\tART Pending Write ACK",
	 .mask = MCA_ARI_ROC_CCE_ART_PENDING_WRITE_ACK},
	{.name = "\tART Pending Fill to Own",
	 .mask = MCA_ARI_ROC_CCE_ART_PENDING_FILL_OWN},
	{.name = "\tART Pending Write",
	 .mask = MCA_ARI_ROC_CCE_ART_PENDING_WRITE},
	{.name = "\tART Pending Read",
	 .mask = MCA_ARI_ROC_CCE_ART_PENDING_READ},
	{}
};

static struct ari_bits roc_cce_to_err_vt_bits[] = {
	{.name = "\tVT Pending Chaining",
	 .mask = MCA_ARI_ROC_CCE_VT_PENDING_CHAINING},
	{.name = "\tVT Pending Write",
	 .mask = MCA_ARI_ROC_CCE_VT_PENDING_WRITE},
	{.name = "\tVT Pending Write Ack",
	 .mask = MCA_ARI_ROC_CCE_VT_PENDING_WRITE_ACK},
	{}
};

static struct ari_bits roc_cce_more_info_bits[] = {
	{.name = "\tDDIR Hit", .mask = MCA_ARI_ROC_CCE_DDIR_HIT},
	{.name = "\tVDIR Hit", .mask = MCA_ARI_ROC_CCE_VDIR_HIT},
	{}
};

static struct ari_bits roc_cce_psn_bits[] = {
	{.name = "\tRequest not going to SYSRAM or DRAM",
	 .mask = MCA_ARI_ROC_CCE_PSN_NOT_DRAM},
	{.name = "\tDRAM request poisoned at source or WB MMIO",
	 .mask = MCA_ARI_ROC_CCE_PSN_WB_MMIO},
	{.name = "\tDRAM request out of range for SYSRAM",
	 .mask = MCA_ARI_ROC_CCE_PSN_SYSRAM_RANGE},
	{.name = "\tIllegal MTS access",
	 .mask = MCA_ARI_ROC_CCE_PSN_ILL_MTS_ACCESS},
	{.name = "\tTZ-DRAM and VPR carve outs overlap",
	 .mask = MCA_ARI_ROC_CCE_PSN_TZDRAM_VPR_OVERLAP},
	{.name = "\tVPR and GSC carve outs overlap",
	 .mask = MCA_ARI_ROC_CCE_PSN_VPR_GSC_OVERLAP},
	{.name = "\tGSC and TZ-DRRAM carve outs overlap",
	 .mask = MCA_ARI_ROC_CCE_PSN_GSC_TZDRAM_OVERLAP},
	{.name = "\tRead Request failed VPR checks",
	 .mask = MCA_ARI_ROC_CCE_PSN_VPR_READ_FAIL},
	{.name = "\tWrite Request failed VPR checks",
	 .mask = MCA_ARI_ROC_CCE_PSN_VPR_WRITE_FAIL},
	{.name = "\tRead Request failed TZ-DRAM checks",
	 .mask = MCA_ARI_ROC_CCE_PSN_TZDRAM_READ_FAIL},
	{.name = "\tWrite Request failed TZ-DRAM checks",
	 .mask = MCA_ARI_ROC_CCE_PSN_TZDRAM_WRITE_FAIL},
	{.name = "\tRead Request failed GSC checks",
	 .mask = MCA_ARI_ROC_CCE_PSN_GSC_READ_FAIL},
	{.name = "\tWrite Request failed GSC checks",
	 .mask = MCA_ARI_ROC_CCE_PSN_GSC_WRITE_FAIL},
	{}
};

static struct ari_bits roc_ctu_stat_bits[] = {
	{.name = "CTU Parity Error", .mask = MCA_ARI_ROC_CTU_STAT_CTU_PAR},
	{.name = "Multiple Errors", .mask = MCA_ARI_ROC_CTU_STAT_MULTI},
	{}
};

static void print_mca(struct seq_file *file, const char *fmt, ...)
{
	va_list args;
	struct va_format vaf;

	va_start(args, fmt);

	if (file) {
		seq_vprintf(file, fmt, args);
	} else {
		vaf.fmt = fmt;
		vaf.va = &args;
		pr_crit("%pV", &vaf);
	}

	va_end(args);
}

static void print_mca_bits(struct seq_file *file, struct ari_bits *table,
			   u64 value)
{
	int i;

	for (i = 0; table[i].name; i += 1)
		if (value & table[i].mask)
			print_mca(file, "\t%s\n", table[i].name);
}

static void print_mca_table(struct seq_file *file, char *msg, char *table[],
			    u64 table_len, u64 value)
{
	if (value < table_len)
		print_mca(file, "\t%s = %s (0x%llx)\n",
			  msg, table[value], value);
	else
		print_mca(file, "\t%s = 0x%llx\n",
			  msg, value);
}

static void _print_snoop(struct seq_file *file, char *source,
			 char *msg, int ack)
{
	print_mca(file, "%s %s Pending Snoop %s\n",
		  source, msg, ack ? "Ack" : "");
}

static void print_snoop(struct seq_file *file, char *msg, u64 snoop, int ack)
{
	if (snoop & 0x01)
		_print_snoop(file, "Denver", msg, ack);
	if (snoop & 0x02)
		_print_snoop(file, "A57", msg, ack);
}

static void print_mca_status(struct seq_file *file, u64 status)
{
	if (status & SERRi_STATUS_OVF)
		print_mca(file, "\tOverflow (there may be more errors)\n");
	if (status & SERRi_STATUS_UC)
		print_mca(file, "\tUncorrected (this is fatal)\n");
	else
		print_mca(file, "\tCorrectable (but, not corrected)\n");
	if (status & SERRi_STATUS_EN)
		print_mca(file,
			  "\tError reporting enabled when error arrived\n");
	else
		print_mca(file,
			  "\tError reporting not enabled when error arrived\n");
}

static void print_mca_error_code(struct seq_file *file,
				 struct ari_mca_error *errors, u16 code)
{
	u64 i;
	int found = 0;

	print_mca(file, "\tError Code = 0x%x\n", code);

	if (errors) {
		for (i = 0; errors[i].name; i++) {
			if (errors[i].error_code  == code) {
				print_mca(file, "\t%s: 0x%x\n",
					  errors[i].name, code);
				found = 1;
				break;
			}
		}
		if (!found)
			print_mca(file, "\tUnknown error: 0x%x\n", code);
	}
}

static void print_address(struct seq_file *file, u64 addr)
{
	u64 addr_type;
	u64 phys_addr;
	struct resource *res;

	addr_type = get_mca_addr_type(addr);

	print_mca(file, "\tAddress Type = %sSecure %s\n",
		  (addr_type & 0x02) ? "Non-" : "",
		  (addr_type & 0x01) ? "MMIO" : "DRAM");

	phys_addr = get_mca_addr_addr(addr);
	res = locate_resource(&iomem_resource, phys_addr);
	if (res == NULL)
		print_mca(file, "\tAddress = 0x%llx (Unknown Device)\n",
			  phys_addr);
	else
		print_mca(file, "\tAddress = 0x%llx -- %s + 0x%llx\n",
			  phys_addr, res->name, phys_addr - res->start);
}

static void print_axi_id(struct seq_file *file, u64 axi_id)
{
	if (axi_id & 0x40) {
		print_mca(file, "\tAXI_ID: 0x%x -- DPMU\n", axi_id);
	} else {
		if (axi_id < 0x04)
			print_mca(file,
				  "\tAXI_ID: 0x%x -- Denver Core %d\n",
				  axi_id, axi_id);
		else if (axi_id < 0x08)
			print_mca(file,
				  "\tAXI_ID: 0x%x -- A57 Core %d\n",
				  axi_id, axi_id - 0x4);
		else
			print_mca(file,
				  "\tAXI_ID: 0x%x -- Pool B\n", axi_id);
	}
}

/* SYS:DPMU Decoders */

static void print_mca_sys_dpmu(struct seq_file *file,
			       struct ari_mca_bank *mca_bank)
{
	u16 error;
	u64 status;
	u64 addr;
	u64 msc1;
	u64 msc2;
	u64 dmce_err;
	u64 crab_err;
	u64 ucode_err;

	status = mca_bank->saved_status;
	addr = mca_bank->saved_addr;
	msc1 = mca_bank->saved_msc1;
	msc2 = mca_bank->saved_msc2;

	print_mca_status(file, status);

	error = get_mca_status_error_code(status);
	print_mca_error_code(file, mca_bank->errors, error);

	dmce_err = status & MCA_ARI_SYS_DPMU_STAT_DMCE_ERR;
	crab_err = status & MCA_ARI_SYS_DPMU_STAT_CRAB_ERR;
	ucode_err = status & MCA_ARI_SYS_DPMU_STAT_UCODE_ERR;

	print_mca_bits(file, sys_dpmu_stat_bits, status);

	if (dmce_err) {
		print_mca(file, "\tDMCE Index = 0x%llx\n",
			  get_mca_sys_dpmu_stat_dmce(status));
		print_mca(file, "\tADDR_H = 0x%llx\n",
			  get_mca_sys_dpmu_addr_addr_h(addr));
	}

	if (dmce_err || crab_err) {
		print_mca(file, "\tRequest = %s\n",
			  (status & MCA_ARI_ROC_IOB_STAT_RD_WR_N) ?
			  "WRITE" : "READ");

		print_mca(file, "\tADDR_L = 0x%llx\n",
			  get_mca_sys_dpmu_addr_addr_l(addr));
	}

	print_mca(file, "\tUCode Error Code = 0x%llx\n",
		  get_mca_sys_dpmu_addr_ucode_errcd(addr));
}

/* ROC:IOB MCA Decoders */

static void print_mca_roc_iob(struct seq_file *file,
			      struct ari_mca_bank *mca_bank)
{
	u16 error;
	u64 status;
	u64 addr;
	u64 msc1;
	u64 msc2;
	u64 msi_err;
	u64 ihi_err;
	u64 cri_err;
	u64 cqx_cmd;

	status = mca_bank->saved_status;
	addr = mca_bank->saved_addr;
	msc1 = mca_bank->saved_msc1;
	msc2 = mca_bank->saved_msc2;

	print_mca_status(file, status);

	error = get_mca_status_error_code(status);
	print_mca_error_code(file, mca_bank->errors, error);

	msi_err = status & MCA_ARI_ROC_IOB_STAT_MSI_ERR;
	ihi_err = status & MCA_ARI_ROC_IOB_STAT_IHI_ERR;
	cri_err = status & MCA_ARI_ROC_IOB_STAT_CRI_ERR;

	print_mca_bits(file, roc_iob_stat_bits, status);

	if (msi_err || ihi_err || cri_err) {
		cqx_cmd = get_mca_roc_iob_addr_cqx_cmd(addr);

		if (status & MCA_ARI_ROC_IOB_STAT_RD_WR_N) {
			print_mca(file, "\tRequest = READ\n");
			print_mca_table(file, "CQX Request Command",
					cqx_rd_cmds, NUM_CQX_RD_CMDS, cqx_cmd);
		} else {
			print_mca(file, "\tRequest = WRITE\n");
			print_mca_table(file, "CQX Request Command",
					cqx_wr_cmds, NUM_CQX_WR_CMDS, cqx_cmd);
		}

		print_mca_table(file, "Request CQX CID",
				cqx_cids, NUM_CQX_CIDS,
				get_mca_roc_iob_addr_cqx_cid(addr));
		print_mca(file, "\tCQX ID of Request = 0x%llx\n",
			   get_mca_roc_iob_addr_cqx_id(addr));
	}

	if (msi_err || ihi_err) {
		print_mca(file, "\tRequest Error Type = 0x%llx\n",
			   get_mca_roc_iob_stat_req_errt(status));
		print_mca_table(file, "Response Error Type",
				axi_responses, NUM_AXI_RESPONSES,
				get_mca_roc_iob_stat_resp_errt(status));
		print_axi_id(file, get_mca_roc_iob_addr_axi_id(addr));
	}

	print_address(file, get_mca_roc_iob_msc1_addr(msc1));
}

/* ROC:MCB Decoders */

static void print_mca_roc_mcb(struct seq_file *file,
			      struct ari_mca_bank *mca_bank)
{
	u16 error;
	u64 status;
	u64 addr;
	u64 msc1;
	u64 msc2;

	status = mca_bank->saved_status;
	addr = mca_bank->saved_addr;
	msc1 = mca_bank->saved_msc1;
	msc2 = mca_bank->saved_msc2;

	print_mca_status(file, status);

	error = get_mca_status_error_code(status);

	if (status & MCA_ARI_ROC_MCB_STAT_MC_ERR) {
		print_mca_error_code(file, roc_mcb_mc_errors, error);
		print_mca(file, "\tMemory Controller Bridge Error\n");
	} else
		print_mca_error_code(file, NULL, error);

	if (status & MCA_ARI_ROC_MCB_STAT_SYSRAM_ERR)
		print_mca(file, "\tSYSRAM Parity Error\n");

	print_mca_table(file, "Client ID", cqx_cids, NUM_CQX_CIDS,
			get_mca_roc_mcb_stat_client_id(status));
	print_mca(file, "\tTransaction ID = 0x%llx\n",
		  get_mca_roc_mcb_addr_id(addr));

	if (status & MCA_ARI_ROC_MCB_STAT_MC_ERR) {
		if (error == MCA_ARI_ROC_MCB_READ_ERROR)
			print_mca_table(file, "Command", cqx_rd_cmds,
					NUM_CQX_RD_CMDS,
					get_mca_roc_mcb_addr_cmd(addr));
		if (error == MCA_ARI_ROC_MCB_WRITE_ERROR)
			print_mca_table(file, "Command", cqx_wr_cmds,
					NUM_CQX_WR_CMDS,
					get_mca_roc_mcb_addr_cmd(addr));
	} else
		print_mca(file, "\tCommand = 0x%llx\n",
			  get_mca_roc_mcb_addr_cmd(addr));

	print_mca(file, "\tAddress = 0x%llx\n",
		  get_mca_roc_mcb_addr_addr(addr));
}

/* ROC:CCE Decoders */

static void print_mca_roc_cce(struct seq_file *file,
			      struct ari_mca_bank *mca_bank)
{
	u16 error;
	u64 status;
	u64 addr;
	u64 msc1;
	u64 msc2;
	u64 psn_err;
	u64 perr;
	u64 mh_err;
	u64 stat_err;
	u64 dst_err;
	u64 to_err;
	u64 to_info;
	u64 snoop;
	u64 more_info;
	u64 l2_present;
	u64 psn_info;

	status = mca_bank->saved_status;
	addr = mca_bank->saved_addr;
	msc1 = mca_bank->saved_msc1;
	msc2 = mca_bank->saved_msc2;

	print_mca_status(file, status);

	error = get_mca_status_error_code(status);
	print_mca_error_code(file, mca_bank->errors, error);

	to_err = status & MCA_ARI_ROC_CCE_STAT_TO_ERR;
	stat_err = status & MCA_ARI_ROC_CCE_STAT_STAT_ERR;
	dst_err = status & MCA_ARI_ROC_CCE_STAT_DST_ERR;
	mh_err = status & MCA_ARI_ROC_CCE_STAT_MH_ERR;
	perr = status & MCA_ARI_ROC_CCE_STAT_PERR;
	psn_err = status & MCA_ARI_ROC_CCE_STAT_PSN_ERR;

	print_mca_bits(file, roc_cce_stat_bits, status);
	print_mca_bits(file, roc_cce_msc1_bits, msc1);

	print_mca_table(file, "Command", cce_cqx_cmds, NUM_CCE_CQX_CMDS,
			get_mca_roc_cce_addr_cmd(addr));
	print_address(file, get_mca_roc_cce_addr_addr(addr));
	print_mca(file, "\tTLimit = 0x%llx\n",
		   get_mca_roc_cce_msc1_tlimit(msc1));
	print_mca(file, "\tPoison Error Mask = 0x%llx\n",
		   get_mca_roc_cce_msc1_psn_err(msc1));

	more_info = get_mca_roc_cce_msc2_more_info(msc2);
	print_mca(file, "\tMore Info = 0x%llx\n", more_info);

	to_info = get_mca_roc_cce_msc2_to_info(msc2);
	print_mca(file, "\tTimeout Info = 0x%llx\n", to_info);
	if (to_err) {
		if (more_info & MCA_ARI_ROC_CCE_ART_VLD) {
			print_mca_bits(file, roc_cce_to_err_art_bits,
				       more_info);

			snoop = get_mca_roc_cce_to_info_art_snoop(to_info);
			print_snoop(file, "ART", snoop, 0);

			snoop = get_mca_roc_cce_to_info_art_snoop_ack(to_info);
			print_snoop(file, "ART", snoop, 1);
		}

		if (more_info & MCA_ARI_ROC_CCE_VID_VLD) {
			print_mca_bits(file, roc_cce_to_err_vt_bits, more_info);

			snoop = get_mca_roc_cce_to_info_vt_snoop(to_info);
			print_snoop(file, "VT", snoop, 0);

			snoop = get_mca_roc_cce_to_info_vt_snoop_ack(to_info);
			print_snoop(file, "VT", snoop, 1);
		}
	}

	if (psn_err) {
		psn_info = get_mca_roc_cce_more_info_poison_info_d1(more_info);
		print_mca(file, "\t\tPoison Info = 0x%llx\n", psn_info);
		print_mca_bits(file, roc_cce_psn_bits, psn_info);
	}

	if (perr || mh_err || stat_err) {
		if ((more_info & MCA_ARI_ROC_CCE_DDIR_HIT) ||
		    (more_info & MCA_ARI_ROC_CCE_VDIR_HIT)) {
			print_mca(file, "\t\tDirectory State = %s\n",
				  (more_info & MCA_ARI_ROC_CCE_DIR_STATE) ?
				  "Exclusive" : "Shared");
			print_mca_bits(file, roc_cce_more_info_bits, more_info);

			l2_present = get_mca_roc_cce_more_info_l2_present(more_info);
			if (l2_present & 0x01)
				print_mca(file, "\t\tL2 Present: Denver\n");
			if (l2_present & 0x02)
				print_mca(file, "\t\tL2 Present: A57\n");

			print_mca(file, "\t\tL2 Directory Way Hit = 0x%llx\n",
				  get_mca_roc_cce_more_info_l2dir_way_hit(more_info));
		}
	}


	if (dst_err || to_err) {
		if (more_info & MCA_ARI_ROC_CCE_ART_VLD)
			print_mca(file, "\t\tART ID = 0x%llx\n",
				   get_mca_roc_cce_more_info_aid(more_info));
		if (more_info & MCA_ARI_ROC_CCE_VID_VLD)
			print_mca(file, "\t\tVT ID = 0x%llx\n",
				   get_mca_roc_cce_more_info_vid(more_info));
	}

	print_mca_table(file, "Source", cqx_cids, NUM_CQX_CIDS,
		       get_mca_roc_cce_msc2_src(msc2));
	print_mca(file, "\tTID = 0x%llx\n",
		   get_mca_roc_cce_msc2_tid(msc2));
}

/* ROC:CQX Decoders */

static void print_mca_roc_cqx(struct seq_file *file,
			      struct ari_mca_bank *mca_bank)
{
	u16 error;
	u64 status;
	u64 addr;
	u64 msc1;
	u64 msc2;

	status = mca_bank->saved_status;
	addr = mca_bank->saved_addr;
	msc1 = mca_bank->saved_msc1;
	msc2 = mca_bank->saved_msc2;

	print_mca_status(file, status);

	error = get_mca_status_error_code(status);
	print_mca_error_code(file, mca_bank->errors, error);

	if (status & MCA_ARI_ROC_CQX_STAT_SRC_ERR)
		print_mca(file, "\tSource Error\n");
	if (status & MCA_ARI_ROC_CQX_STAT_DST_ERR)
		print_mca(file, "\tDestination Error\n");
	if (status & MCA_ARI_ROC_CQX_STAT_REQ_ERR)
		print_mca(file, "\tRequest Error\n");
	if (status & MCA_ARI_ROC_CQX_STAT_RSP_ERR)
		print_mca(file, "\tResponse Error\n");
}

/* ROC:CTU Decoders */

static void print_mca_roc_ctu(struct seq_file *file,
			      struct ari_mca_bank *mca_bank)
{
	u16 error;
	u64 status;
	u64 addr;
	u64 msc1;
	u64 msc2;
	u64 src;
	u64 ctu_addr;
	u64 data;

	status = mca_bank->saved_status;
	addr = mca_bank->saved_addr;
	msc1 = mca_bank->saved_msc1;
	msc2 = mca_bank->saved_msc2;

	print_mca_status(file, status);

	error = get_mca_status_error_code(status);
	print_mca_error_code(file, mca_bank->errors, error);

	print_mca_bits(file, roc_ctu_stat_bits, status);

	print_mca_table(file, "CTU Command", ctu_cmds, NUM_CTU_CMDS,
			get_mca_roc_ctu_addr_cmd(addr));
	print_mca(file, "\tTransaction ID = 0x%llx\n",
		  get_mca_roc_ctu_addr_id(addr));

	src = get_mca_roc_ctu_addr_src(addr);
	ctu_addr = get_mca_roc_ctu_addr_addr(addr);
	data = get_mca_roc_ctu_addr_data(addr);

	print_mca(file, "\tSource Field = 0x%llx\n", src);
	print_mca(file, "\tData = 0x%llx\n", data);
	print_mca(file, "\tAddress = 0x%llx\n", ctu_addr);

	if (is_roc_ctu_src_cga_data_array(src)) {
		print_mca(file, "\tCGA Data Array Error:\n");
		print_mca(file, "\t\tError Data: 0x%llx\n", data & 0xff);
		print_mca(file, "\t\tParity Bit: %lld\n",
			  MCA_ARI_EXTRACT(data, 8, 8));
		print_mca(file, "\t\tTag Index:  0x%llx\n",
			  MCA_ARI_EXTRACT(ctu_addr, 9, 2));
		print_mca(file, "\t\tWay Group:  0x%llx\n",
			  MCA_ARI_EXTRACT(ctu_addr, 1, 0));
		print_mca(file, "\t\tByte Num:   0x%llx\n", src & 0x0f);
	}

	if (is_roc_ctu_src_cga_tag_array(src)) {
		print_mca(file, "\tCGA Tag Array Error:\n");
		print_mca(file, "\t\tParity Bit: %lld\n",
			  MCA_ARI_EXTRACT(data, 10, 10));
		if ((src & 0x01) == 0)
			print_mca(file, "\t\tTag[9:0]:   0x%llx\n",
				  MCA_ARI_EXTRACT(data, 9, 0));
		else
			print_mca(file, "\t\tTag[17:10]:     0x%llx\n",
				  MCA_ARI_EXTRACT(data, 8, 0));

		print_mca(file, "\t\tCGA Tag Index: 0x%llx\n",
			  MCA_ARI_EXTRACT(ctu_addr, 7, 0));
		print_mca(file, "\t\tError in Field: 0x%llx\n",
			  MCA_ARI_EXTRACT(data, 5, 0));
	}
}

static void print_bank_info(struct seq_file *file,
			    struct ari_mca_bank *mca_bank)
{
	print_mca(file, "%s Registers:\n", mca_bank->name);

	if (mca_bank->reg_mask & MCA_ARI_STAT_REG_MASK)
		print_mca(file, "\tSTAT: 0x%llx\n", mca_bank->saved_status);

	if (mca_bank->reg_mask & MCA_ARI_ADDR_REG_MASK)
		print_mca(file, "\tADDR: 0x%llx\n", mca_bank->saved_addr);

	if (mca_bank->reg_mask & MCA_ARI_MSC1_REG_MASK)
		print_mca(file, "\tMSC1: 0x%llx\n", mca_bank->saved_msc1);

	if (mca_bank->reg_mask & MCA_ARI_MSC2_REG_MASK)
		print_mca(file, "\tMSC2: 0x%llx\n", mca_bank->saved_msc2);

	if (mca_bank->print_mca &&
	    (mca_bank->reg_mask & MCA_ARI_STAT_REG_MASK) &&
	    (mca_bank->saved_status & SERRi_STATUS_VAL)) {
		print_mca(file, "--------------------------------------\n");
		print_mca(file, "Decoded %s Machine Check:\n", mca_bank->name);
		mca_bank->print_mca(file, mca_bank);
	}
}

static struct ari_mca_bank ari_mca_banks[] = {
	{.name = "SYS:DPMU",	.bank = 0,
	 .print_mca = print_mca_sys_dpmu,
	},
	{.name = "ROC:IOB",	.bank = 1,
	 .print_mca = print_mca_roc_iob,
	},
	{.name = "ROC:MCB",	.bank = 2,
	 .print_mca = print_mca_roc_mcb,
	},
	{.name = "ROC:CCE",	.bank = 3,
	 .print_mca = print_mca_roc_cce,
	},
	{.name = "ROC:CQX",	.bank = 4,
	 .print_mca = print_mca_roc_cqx,
	},
	{.name = "ROC:CTU",	.bank = 5,
	 .print_mca = print_mca_roc_ctu,
	},
	{}
};

static DEFINE_MUTEX(ari_mca_mutex);
static struct dentry *mca_root;

static int ari_mca_show(struct seq_file *file, void *data)
{
	int bank;
	struct ari_mca_bank *mca_bank;

	mutex_lock(&ari_mca_mutex);

	bank = *(int *)file->private;
	if (ari_mca_banks[bank].reg_mask == 0x0) {
		mutex_unlock(&ari_mca_mutex);
		return -EINVAL;
	}

	mca_bank = &ari_mca_banks[bank];

	print_bank_info(file, mca_bank);

	mutex_unlock(&ari_mca_mutex);
	return 0;
}

static int ari_mca_open(struct inode *inode, struct file *file)
{
	return single_open(file, ari_mca_show, inode->i_private);
}

static const struct file_operations tegra18_ari_mca_fops = {
	.open = ari_mca_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int ari_mca_dbgfs_init(void)
{
	struct dentry *d;
	struct dentry *ari_dir;
	int i;

	d = debugfs_create_dir("tegra_mca", NULL);
	if (IS_ERR_OR_NULL(d)) {
		pr_err("%s: could not create 'tegra_mca' node\n", __func__);
		goto clean;
	}

	mca_root = d;

	d = debugfs_create_dir("ari_prev_boot", mca_root);
	if (IS_ERR_OR_NULL(d)) {
		pr_err("%s: could not create 'ari' node\n", __func__);
		goto clean;
	}
	ari_dir = d;

	for (i = 0; ari_mca_banks[i].name; i++) {
		if (ari_mca_banks[i].reg_mask == 0x0)
			continue;

		d = debugfs_create_file(ari_mca_banks[i].name,
					S_IRUGO, ari_dir,
					&ari_mca_banks[i].bank,
					&tegra18_ari_mca_fops);
		if (IS_ERR_OR_NULL(d)) {
			pr_err("%s: could not create '%s' node\n",
			       __func__, ari_mca_banks[i].name);
			goto clean;
		}
	}

	return 0;

clean:
	debugfs_remove_recursive(mca_root);
	return PTR_ERR(d);
}

static LIST_HEAD(ari_mca_list);
static DEFINE_RAW_SPINLOCK(ari_mca_lock);

static int register_ari_mca_bank(struct ari_mca_bank *bank)
{
	unsigned long flags;
	u64 data;
	u64 nbanks;
	int rc = 0;

	raw_spin_lock_irqsave(&ari_mca_lock, flags);

	rc = read_bank_info(&nbanks);
	if (rc)
		goto out;

	if (bank->bank > nbanks) {
		pr_err("%s: Attempted to register bank %lld.  Max bank = %lld\n",
		       __func__, bank->bank, nbanks);
		rc = -EINVAL;
		goto out;
	}

	rc = read_bank_template(bank->bank, &data);
	if (rc)
		goto out;

	bank->reg_mask = data;

	if (bank->reg_mask != 0) {
#if ARI_MCA_SAVE_PREBOOT
		save_bank(bank, 1);
#endif

		list_add(&bank->node, &ari_mca_list);
	} else
		rc = -EINVAL;

out:
	raw_spin_unlock_irqrestore(&ari_mca_lock, flags);
	return rc;
}

static void unregister_ari_mca_bank(struct ari_mca_bank *bank)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&ari_mca_lock, flags);
	list_del(&bank->node);
	raw_spin_unlock_irqrestore(&ari_mca_lock, flags);
}

static void register_ari_mca_banks(void)
{
	int i;

	for (i = 0; ari_mca_banks[i].name; i++) {
		if (register_ari_mca_bank(&ari_mca_banks[i]) == 0)
			pr_info("%s: Registered MCA %s\n",
				__func__, ari_mca_banks[i].name);
	}
}

static void unregister_ari_mca_banks(void)
{
	int i;

	for (i = 0; ari_mca_banks[i].name; i++) {
		if (ari_mca_banks[i].reg_mask != 0)
			unregister_ari_mca_bank(&ari_mca_banks[i]);
	}
}

/* MCA assert register dump */

static void print_bank(struct ari_mca_bank *mca_bank)
{

	pr_crit("**************************************\n");
	pr_crit("%s Machine Check Error:\n", mca_bank->name);
	print_bank_info(NULL, mca_bank);
	pr_crit("**************************************\n");
}

static int ari_serr_hook(struct pt_regs *regs, int reason,
			unsigned int esr, void *priv)
{
	u64 status;
	struct ari_mca_bank *bank;
	unsigned long flags;

	/* Iterate through the banks looking for one with an error */
	raw_spin_lock_irqsave(&ari_mca_lock, flags);
	list_for_each_entry(bank, &ari_mca_list, node) {
	    if (read_bank_status(bank, &status, 0) != 0)
			continue;
		if ((status & SERRi_STATUS_VAL) &&
		    !bank->processed) {
			save_bank(bank, 0);
			print_bank(bank);
			bank->processed = 1;
		}
	}
	raw_spin_unlock_irqrestore(&ari_mca_lock, flags);
	return 0;	/* Not handled */
}

static struct serr_hook hook = {
	.fn = ari_serr_hook
};

static int __init ari_serr_init(void)
{
	int rc;

	/*
	 * ARI is not supported on the simulator
	 */
	if (tegra_cpu_is_asim())
		return 0;

	/* Register the SError hook so that this driver is called on SError */
	register_serr_hook(&hook);

	/* Register the ARI MCA banks */
	register_ari_mca_banks();

	rc = ari_mca_dbgfs_init();
	if (rc)
		return rc;

	return 0;
}

module_init(ari_serr_init);

static void __exit ari_serr_exit(void)
{
	unregister_ari_mca_banks();
	unregister_serr_hook(&hook);
	debugfs_remove_recursive(mca_root);
}
module_exit(ari_serr_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ARI Machine Check / SError handler");
