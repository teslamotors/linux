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

struct ari_mca_error {
	char *name;
	u16 error_code;
};

struct ari_mca_bank {
	struct list_head node;
	char *name;
	u64 bank;
	u8 reg_mask;
	u8 processed;
	u64 saved_status;
	u64 saved_addr;
	u64 saved_msc1;
	u64 saved_msc2;
	void (*print_mca)(struct seq_file *file, struct ari_mca_bank *bank);
	struct ari_mca_error *errors;
};

struct ari_bits {
	char *name;
	u64 mask;
};

#define MCA_ARI_BIT(_bit_) (1ULL << (_bit_))
#define MCA_ARI_MASK(_msb_, _lsb_) \
	((MCA_ARI_BIT(_msb_+1) - 1ULL) & ~(MCA_ARI_BIT(_lsb_) - 1ULL))
#define MCA_ARI_EXTRACT(_x_, _msb_, _lsb_)	\
	((_x_ & MCA_ARI_MASK(_msb_, _lsb_)) >> _lsb_)

#define SERRi_STATUS_VAL	MCA_ARI_BIT(63)
#define SERRi_STATUS_OVF	MCA_ARI_BIT(62)
#define SERRi_STATUS_UC		MCA_ARI_BIT(61)
#define SERRi_STATUS_EN		MCA_ARI_BIT(60)
#define SERRi_STATUS_MV		MCA_ARI_BIT(59)
#define SERRi_STATUS_AV		MCA_ARI_BIT(58)

#define get_mca_status_error_code(_x_)	MCA_ARI_EXTRACT(_x_, 15, 0)

enum {
	MCA_ARI_CMD_NOP = 0x00,
	MCA_ARI_CMD_RD_SERR = 0x01,
	MCA_ARI_CMD_WR_SERR = 0x02,
	MCA_ARI_CMD_CLEAR_SERR = 0x04,
	MCA_ARI_CMD_REPORT_SERR = 0x05,
	MCA_ARI_CMD_RD_INTSTS = 0x06,
	MCA_ARI_CMD_WR_INTSTS = 0x07,
	MCA_ARI_CMD_RD_PREBOOT_SERR = 0x08,
};

#define MCA_ARI_SERR_IDX_OFF	6

enum {
	MCA_ARI_IDX_ASERR0 = 0x00,
	MCA_ARI_IDX_ASERR1 = 0x01,
	MCA_ARI_IDX_ASERR2 = 0x02,
	MCA_ARI_IDX_ASERR3 = 0x03,
	MCA_ARI_IDX_ASERR4 = 0x04,
	MCA_ARI_IDX_ASERR5 = 0x05,
	MCA_ARI_IDX_BANKINFO = 0x0f,
	MCA_ARI_IDX_BANKTEMPLATE = 0x10,
	MCA_ARI_IDX_SECURE_ACCESS = 0x11
};

enum {
	MCA_ARI_RW_SUBIDX_CTRL = 0,
	MCA_ARI_RW_SUBIDX_STAT = 1,
	MCA_ARI_RW_SUBIDX_ADDR = 2,
	MCA_ARI_RW_SUBIDX_MSC1 = 3,
	MCA_ARI_RW_SUBIDX_MSC2 = 4,
};

#define MCA_ARI_CTRL_REG_MASK (1 << MCA_ARI_RW_SUBIDX_CTRL)
#define MCA_ARI_STAT_REG_MASK (1 << MCA_ARI_RW_SUBIDX_STAT)
#define MCA_ARI_ADDR_REG_MASK (1 << MCA_ARI_RW_SUBIDX_ADDR)
#define MCA_ARI_MSC1_REG_MASK (1 << MCA_ARI_RW_SUBIDX_MSC1)
#define MCA_ARI_MSC2_REG_MASK (1 << MCA_ARI_RW_SUBIDX_MSC2)

/*
 * SYS:DPMU Register layout
 */
#define MCA_ARI_SYS_DPMU_STAT_DMCE_ERR	MCA_ARI_BIT(16)
#define MCA_ARI_SYS_DPMU_STAT_CRAB_ERR	MCA_ARI_BIT(17)
#define MCA_ARI_SYS_DPMU_STAT_RD_WR_N	MCA_ARI_BIT(18)
#define MCA_ARI_SYS_DPMU_STAT_UCODE_ERR	MCA_ARI_BIT(19)

#define get_mca_sys_dpmu_stat_dmce(_x_)		MCA_ARI_EXTRACT(_x_, 23, 20)
#define get_mca_sys_dpmu_addr_addr_l(_x_)	MCA_ARI_EXTRACT(_x_, 26, 0)
#define get_mca_sys_dpmu_addr_addr_h(_x_)	MCA_ARI_EXTRACT(_x_, 41, 27)
#define get_mca_sys_dpmu_addr_ucode_errcd(_x_)	MCA_ARI_EXTRACT(_x_, 52, 42)

/*
 * ROC:IOB Register layout
 */
#define MCA_ARI_ROC_IOB_STAT_MSI_ERR	MCA_ARI_BIT(16)
#define MCA_ARI_ROC_IOB_STAT_IHI_ERR	MCA_ARI_BIT(17)
#define MCA_ARI_ROC_IOB_STAT_CRI_ERR	MCA_ARI_BIT(18)
#define MCA_ARI_ROC_IOB_STAT_MMCRAB_ERR	MCA_ARI_BIT(19)
#define MCA_ARI_ROC_IOB_STAT_CSI_ERR	MCA_ARI_BIT(20)
#define MCA_ARI_ROC_IOB_STAT_RD_WR_N	MCA_ARI_BIT(21)

#define get_mca_roc_iob_stat_req_errt(_x_)	MCA_ARI_EXTRACT(_x_, 23, 22)
#define get_mca_roc_iob_stat_resp_errt(_x_)	MCA_ARI_EXTRACT(_x_, 25, 24)
#define get_mca_roc_iob_addr_cqx_cmd(_x_)	MCA_ARI_EXTRACT(_x_, 35, 32)
#define get_mca_roc_iob_addr_cqx_cid(_x_)	MCA_ARI_EXTRACT(_x_, 31, 28)
#define get_mca_roc_iob_addr_cqx_id(_x_)	MCA_ARI_EXTRACT(_x_, 27, 8)
#define get_mca_roc_iob_addr_axi_id(_x_)	MCA_ARI_EXTRACT(_x_, 7,  0)
#define get_mca_roc_iob_msc1_addr(_x_)		MCA_ARI_EXTRACT(_x_, 41, 0)

/*
 * ROC:MCB Register layout
 */
#define MCA_ARI_ROC_MCB_STAT_MC_ERR	MCA_ARI_BIT(16)
#define MCA_ARI_ROC_MCB_STAT_SYSRAM_ERR	MCA_ARI_BIT(17)
#define MCA_ARI_ROC_MCB_READ_ERROR	0x081E
#define MCA_ARI_ROC_MCB_WRITE_ERROR	0x082E

#define get_mca_roc_mcb_stat_client_id(_x_)	MCA_ARI_EXTRACT(_x_, 18, 17)
#define get_mca_roc_mcb_addr_id(_x_)		MCA_ARI_EXTRACT(_x_, 17, 0)
#define get_mca_roc_mcb_addr_cmd(_x_)		MCA_ARI_EXTRACT(_x_, 21, 18)
#define get_mca_roc_mcb_addr_addr(_x_)		MCA_ARI_EXTRACT(_x_, 53, 22)

/*
 * ROC:CCE Register layout
 */
#define MCA_ARI_ROC_CCE_STAT_TO_ERR	MCA_ARI_BIT(16)
#define MCA_ARI_ROC_CCE_STAT_STAT_ERR	MCA_ARI_BIT(17)
#define MCA_ARI_ROC_CCE_STAT_DST_ERR	MCA_ARI_BIT(18)
#define MCA_ARI_ROC_CCE_STAT_UNC_ERR	MCA_ARI_BIT(19)
#define MCA_ARI_ROC_CCE_STAT_MH_ERR	MCA_ARI_BIT(20)
#define MCA_ARI_ROC_CCE_STAT_PERR	MCA_ARI_BIT(21)
#define MCA_ARI_ROC_CCE_STAT_PSN_ERR	MCA_ARI_BIT(22)

#define MCA_ARI_ROC_CCE_MSC1_TO		MCA_ARI_BIT(0)
#define MCA_ARI_ROC_CCE_MSC1_DIV4	MCA_ARI_BIT(1)

#define get_mca_roc_cce_addr_cmd(_x_)		MCA_ARI_EXTRACT(_x_, 5, 0)
#define get_mca_roc_cce_addr_addr(_x_)		MCA_ARI_EXTRACT(_x_, 47, 6)
#define get_mca_roc_cce_msc1_tlimit(_x_)	MCA_ARI_EXTRACT(_x_, 11, 2)
#define get_mca_roc_cce_msc1_psn_err(_x_)	MCA_ARI_EXTRACT(_x_, 25, 12)
#define get_mca_roc_cce_msc2_more_info(_x_)	MCA_ARI_EXTRACT(_x_, 17, 0)
#define get_mca_roc_cce_msc2_to_info(_x_)	MCA_ARI_EXTRACT(_x_, 43, 18)
#define get_mca_roc_cce_msc2_src(_x_)		MCA_ARI_EXTRACT(_x_, 45, 44)
#define get_mca_roc_cce_msc2_tid(_x_)		MCA_ARI_EXTRACT(_x_, 52, 46)

/*
 * more_info bits
 */
/* PSN_ERR */
#define get_mca_roc_cce_more_info_poison_info_d1(_x_) MCA_ARI_EXTRACT(_x_, 13, 0)
#define MCA_ARI_ROC_CCE_PSN_NOT_DRAM		MCA_ARI_BIT(0)
#define MCA_ARI_ROC_CCE_PSN_WB_MMIO		MCA_ARI_BIT(1)
#define MCA_ARI_ROC_CCE_PSN_SYSRAM_RANGE	MCA_ARI_BIT(2)
#define MCA_ARI_ROC_CCE_PSN_ILL_MTS_ACCESS	MCA_ARI_BIT(3)
#define MCA_ARI_ROC_CCE_PSN_TZDRAM_VPR_OVERLAP	MCA_ARI_BIT(4)
#define MCA_ARI_ROC_CCE_PSN_VPR_GSC_OVERLAP	MCA_ARI_BIT(5)
#define MCA_ARI_ROC_CCE_PSN_GSC_TZDRAM_OVERLAP	MCA_ARI_BIT(6)
#define MCA_ARI_ROC_CCE_PSN_VPR_READ_FAIL	MCA_ARI_BIT(7)
#define MCA_ARI_ROC_CCE_PSN_VPR_WRITE_FAIL	MCA_ARI_BIT(8)
#define MCA_ARI_ROC_CCE_PSN_TZDRAM_READ_FAIL	MCA_ARI_BIT(9)
#define MCA_ARI_ROC_CCE_PSN_TZDRAM_WRITE_FAIL	MCA_ARI_BIT(10)
#define MCA_ARI_ROC_CCE_PSN_GSC_READ_FAIL	MCA_ARI_BIT(11)
#define MCA_ARI_ROC_CCE_PSN_GSC_WRITE_FAIL	MCA_ARI_BIT(12)

/* PERR, MH_ERR, STAT_ERR */
#define MCA_ARI_ROC_CCE_DIR_STATE	MCA_ARI_BIT(16)
#define MCA_ARI_ROC_CCE_DDIR_HIT	MCA_ARI_BIT(13)
#define MCA_ARI_ROC_CCE_VDIR_HIT	MCA_ARI_BIT(12)

#define get_mca_roc_cce_more_info_l2_present(_x_) MCA_ARI_EXTRACT(_x_, 15, 14)
#define get_mca_roc_cce_more_info_l2dir_way_hit(_x_) MCA_ARI_EXTRACT(_x_, 11, 0)

/* DST_ERR, TO_ERR */
#define MCA_ARI_ROC_CCE_ART_VLD		MCA_ARI_BIT(17)
#define MCA_ARI_ROC_CCE_VID_VLD		MCA_ARI_BIT(8)

#define get_mca_roc_cce_more_info_aid(_x_)	MCA_ARI_EXTRACT(_x_, 15, 9)
#define get_mca_roc_cce_more_info_vid(_x_)	MCA_ARI_EXTRACT(_x_, 5, 0)

/*
 * to_info bits
 */
#define MCA_ARI_ROC_CCE_ART_PENDING_CHAINING	MCA_ARI_BIT(25)
#define MCA_ARI_ROC_CCE_ART_PENDING_TAG_REPLAY	MCA_ARI_BIT(24)
#define MCA_ARI_ROC_CCE_ART_PENDING_FULL_REPLAY	MCA_ARI_BIT(23)
#define get_mca_roc_cce_to_info_art_snoop_ack(_x_) MCA_ARI_EXTRACT(_x_, 22, 20)
#define get_mca_roc_cce_to_info_art_snoop(_x_)	MCA_ARI_EXTRACT(_x_, 19, 17)
#define MCA_ARI_ROC_CCE_ART_PENDING_WRITE_ACK	MCA_ARI_BIT(16)
#define MCA_ARI_ROC_CCE_ART_PENDING_FILL_OWN	MCA_ARI_BIT(15)
#define MCA_ARI_ROC_CCE_ART_PENDING_WRITE	MCA_ARI_BIT(14)
#define MCA_ARI_ROC_CCE_ART_PENDING_READ	MCA_ARI_BIT(13)
#define MCA_ARI_ROC_CCE_VT_PENDING_CHAINING	MCA_ARI_BIT(12)
#define MCA_ARI_ROC_CCE_TO_RESERVED_1		MCA_ARI_BIT(11)
#define MCA_ARI_ROC_CCE_TO_RESERVED_2		MCA_ARI_BIT(10)
#define MCA_ARI_ROC_CCE_TO_RESERVED_3		MCA_ARI_BIT(9)
#define get_mca_roc_cce_to_info_vt_snoop_ack(_x_) MCA_ARI_EXTRACT(_x_, 8, 7)
#define MCA_ARI_ROC_CCE_TO_RESERVED_4		MCA_ARI_BIT(6)
#define get_mca_roc_cce_to_info_vt_snoop(_x_)	MCA_ARI_EXTRACT(_x_, 5, 4)
#define MCA_ARI_ROC_CCE_VT_PENDING_WRITE_ACK	MCA_ARI_BIT(3)
#define MCA_ARI_ROC_CCE_TO_RESERVED_5		MCA_ARI_BIT(2)
#define MCA_ARI_ROC_CCE_VT_PENDING_WRITE	MCA_ARI_BIT(1)
#define MCA_ARI_ROC_CCE_TO_RESERVED_6		MCA_ARI_BIT(0)

/*
 * ROC:CQX Register layout
 */
#define MCA_ARI_ROC_CQX_STAT_SRC_ERR	MCA_ARI_BIT(16)
#define MCA_ARI_ROC_CQX_STAT_DST_ERR	MCA_ARI_BIT(17)
#define MCA_ARI_ROC_CQX_STAT_REQ_ERR	MCA_ARI_BIT(18)
#define MCA_ARI_ROC_CQX_STAT_RSP_ERR	MCA_ARI_BIT(19)

/*
 * ROC:CTU Register layout
 */
#define MCA_ARI_ROC_CTU_STAT_CTU_PAR	MCA_ARI_BIT(16)
#define MCA_ARI_ROC_CTU_STAT_MULTI	MCA_ARI_BIT(17)

#define get_mca_roc_ctu_addr_src(_x_)	MCA_ARI_EXTRACT(_x_, 7, 0)
#define get_mca_roc_ctu_addr_id(_x_)	MCA_ARI_EXTRACT(_x_, 15, 8)
#define get_mca_roc_ctu_addr_data(_x_)	MCA_ARI_EXTRACT(_x_, 26, 16)
#define get_mca_roc_ctu_addr_cmd(_x_)	MCA_ARI_EXTRACT(_x_, 35, 32)
#define get_mca_roc_ctu_addr_addr(_x_)	MCA_ARI_EXTRACT(_x_, 45, 36)

#define is_roc_ctu_src_cga_data_array(_x_) ((_x_ & 0xf0) == 0)
#define is_roc_ctu_src_cga_tag_array(_x_) ((_x_ >= 0x40) && (_x_ < 0x7f))

/*
 * Decode a 42 bit address
 */
#define get_mca_addr_type(_x_)		MCA_ARI_EXTRACT(_x_, 41, 40)
#define get_mca_addr_addr(_x_)		MCA_ARI_EXTRACT(_x_, 39, 0)
