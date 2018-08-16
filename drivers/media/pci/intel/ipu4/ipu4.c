// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Intel Corporation

#include <linux/device.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>

#include "ipu.h"
#include "ipu-cpd.h"
#include "ipu-isys.h"
#include "ipu-buttress.h"
#include "ipu-psys.h"
#include "ipu-platform.h"
#include "ipu-platform-regs.h"
#include "ipu-platform-buttress-regs.h"

#ifdef CONFIG_VIDEO_INTEL_IPU4
static struct ipu_receiver_electrical_params ipu4_ev_params[] = {
	{0, 1500000000ul / 2, IPU_PCI_ID, IPU_HW_BXT_P_B1_REV,
	 .rcomp_val_combo = 11,
	 .rcomp_val_legacy = 11,
	 .ports[0].crc_val = 18,
	 .ports[0].drc_val = 29,
	 .ports[0].drc_val_combined = 29,
	 .ports[0].ctle_val = 4,
	 .ports[1].crc_val = 18,
	 .ports[1].drc_val = 29,
	 .ports[1].drc_val_combined = 31,
	 .ports[1].ctle_val = 4
	},
	{0, 1500000000ul / 2, IPU_PCI_ID, IPU_HW_BXT_P_D0_REV,
	 .rcomp_val_combo = 11,
	 .rcomp_val_legacy = 11,
	 .ports[0].crc_val = 18,
	 .ports[0].drc_val = 29,
	 .ports[0].drc_val_combined = 29,
	 .ports[0].ctle_val = 4,
	 .ports[1].crc_val = 18,
	 .ports[1].drc_val = 29,
	 .ports[1].drc_val_combined = 31,
	 .ports[1].ctle_val = 4
	},
	{0, 1500000000ul / 2, IPU_PCI_ID, IPU_HW_BXT_P_E0_REV,
	 .rcomp_val_combo = 11,
	 .rcomp_val_legacy = 11,
	 .ports[0].crc_val = 18,
	 .ports[0].drc_val = 29,
	 .ports[0].drc_val_combined = 29,
	 .ports[0].ctle_val = 4,
	 .ports[1].crc_val = 18,
	 .ports[1].drc_val = 29,
	 .ports[1].drc_val_combined = 31,
	 .ports[1].ctle_val = 4
	},
	{},
};

static unsigned int ipu4_csi_offsets[] = {
	0x64000, 0x65000, 0x66000, 0x67000, 0x6C000, 0x6C800
};

static unsigned char ipu4_csi_evlanecombine[] = {
	0, 0, 0, 0, 2, 0
};

static unsigned int ipu4_tpg_offsets[] = {
	IPU_TPG0_ADDR_OFFSET,
	IPU_TPG1_ADDR_OFFSET
};

static unsigned int ipu4_tpg_sels[] = {
	IPU_GPOFFSET + IPU_GPREG_MIPI_PKT_GEN0_SEL,
	IPU_COMBO_GPOFFSET + IPU_GPREG_MIPI_PKT_GEN1_SEL
};

const struct ipu_isys_internal_pdata isys_ipdata = {
	.csi2 = {
		 .nports = ARRAY_SIZE(ipu4_csi_offsets),
		 .offsets = ipu4_csi_offsets,
		 .evparams = ipu4_ev_params,
		 .evlanecombine = ipu4_csi_evlanecombine,
		 .evsetmask0 = 1 << 4,	/* CSI port 4  */
		 .evsetmask1 = 1 << 5,	/* CSI port 5 */
	},
	.tpg = {
		.ntpgs = ARRAY_SIZE(ipu4_tpg_offsets),
		.offsets = ipu4_tpg_offsets,
		.sels = ipu4_tpg_sels,
	},
	.hw_variant = {
		       .offset = IPU_ISYS_OFFSET,
		       .nr_mmus = 2,
		       .mmu_hw = {
			{
				.offset = IPU_ISYS_IOMMU0_OFFSET,
				.info_bits =
				IPU_INFO_REQUEST_DESTINATION_PRIMARY,
				.nr_l1streams = 0,
				.nr_l2streams = 0,
				.insert_read_before_invalidate = true,
			},
			{
				.offset = IPU_ISYS_IOMMU1_OFFSET,
				.info_bits = IPU_INFO_STREAM_ID_SET(0),
				.nr_l1streams = IPU_MMU_MAX_TLB_L1_STREAMS,
				.l1_block_sz = {
						8, 16, 16, 16, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 8
				},
				.l1_zlw_en = {
						1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0
				},
				.l1_zlw_1d_mode = {
						0, 1, 1, 1, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, 0
				},
				.l1_ins_zlw_ahead_pages = {
							0, 3, 3, 3, 0, 0,
							0, 0, 0, 0, 0, 0,
							0, 0, 0, 0
				},
				.l1_zlw_2d_mode = {
						0, 0, 0, 0, 0, 0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0
				},
				.nr_l2streams = IPU_MMU_MAX_TLB_L2_STREAMS,
				.l2_block_sz = {
						2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
						2, 2, 2, 2, 2, 2
				},
				.insert_read_before_invalidate = false,
				.zlw_invalidate = true,
				.l1_stream_id_reg_offset =
					IPU_MMU_L1_STREAM_ID_REG_OFFSET,
				.l2_stream_id_reg_offset =
					IPU_MMU_L2_STREAM_ID_REG_OFFSET,
			},
		},
		.dmem_offset = IPU_ISYS_DMEM_OFFSET,
		.spc_offset = IPU_ISYS_SPC_OFFSET,
	},
	.num_parallel_streams = IPU_ISYS_NUM_STREAMS,
	.isys_dma_overshoot = IPU_ISYS_OVERALLOC_MIN,
};

const struct ipu_psys_internal_pdata psys_ipdata = {
	.hw_variant = {
		       .offset = IPU_PSYS_OFFSET,
		       .nr_mmus = 3,
		       .mmu_hw = {
				{
				   .offset = IPU_PSYS_IOMMU0_OFFSET,
				   .info_bits =
				   IPU_INFO_REQUEST_DESTINATION_PRIMARY,
				   .nr_l1streams = 0,
				   .nr_l2streams = 0,
				   .insert_read_before_invalidate = true,
				},
				{
				   .offset = IPU_PSYS_IOMMU1_OFFSET,
				   .info_bits = IPU_INFO_STREAM_ID_SET(0),
				   .nr_l1streams = IPU_MMU_MAX_TLB_L1_STREAMS,
				   .l1_block_sz = {
						   0, 0, 0, 0, 10, 8, 10, 8, 0,
						   4, 4, 12, 0, 0, 0, 8
				   },
				   .l1_zlw_en = {
						 0, 0, 0, 0, 1, 1, 1, 1, 0, 1,
						 1, 1, 0, 0, 0, 0
				   },
				   .l1_zlw_1d_mode = {
						      0, 0, 0, 0, 1, 1, 1, 1, 0,
						      1, 1, 1, 0, 0, 0, 0
				   },
				   .l1_ins_zlw_ahead_pages = {
							      0, 0, 0, 0, 3, 3,
							      3, 3, 0, 3, 1, 3,
							      0, 0, 0, 0
				   },
				   .l1_zlw_2d_mode = {
						      0, 0, 0, 0, 0, 0, 0, 0, 0,
						      0, 0, 0, 0, 0, 0, 0
				   },
				   .nr_l2streams = IPU_MMU_MAX_TLB_L2_STREAMS,
				   .l2_block_sz = {
						   2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
						   2, 2, 2, 2, 2, 2
				   },
				   .insert_read_before_invalidate = false,
				   .zlw_invalidate = true,
				   .l1_stream_id_reg_offset =
				   IPU_MMU_L1_STREAM_ID_REG_OFFSET,
				   .l2_stream_id_reg_offset =
				   IPU_MMU_L2_STREAM_ID_REG_OFFSET,
				},
				{
				   .offset = IPU_PSYS_IOMMU1R_OFFSET,
				   .info_bits = IPU_INFO_STREAM_ID_SET(0),
				   .nr_l1streams = IPU_MMU_MAX_TLB_L1_STREAMS,
				   .l1_block_sz = {
						   0, 0, 0, 0, 0, 0, 0, 0, 8, 0,
						   0, 0, 16, 12, 12, 16
				   },
				   .l1_zlw_en = {
						 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
						 0, 0, 1, 1, 1, 1
				   },
				   .l1_zlw_1d_mode = {
						      0, 0, 0, 0, 0, 0, 0, 0, 1,
						      0, 0, 0, 0, 1, 1, 1
				   },
				   .l1_ins_zlw_ahead_pages = {
							      0, 0, 0, 0, 0, 0,
							      0, 0, 3, 0, 0, 0,
							      0, 0, 0, 0
				   },
				   .l1_zlw_2d_mode = {
						      0, 0, 0, 0, 0, 0, 0, 0, 0,
						      0, 0, 0, 0, 1, 1, 1
				   },
				   .nr_l2streams = IPU_MMU_MAX_TLB_L2_STREAMS,
				   .l2_block_sz = {
						   2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
						   2, 2, 2, 2, 2, 2
				   },
				   .insert_read_before_invalidate = false,
				   .zlw_invalidate = true,
				   .l1_stream_id_reg_offset =
				   IPU_MMU_L1_STREAM_ID_REG_OFFSET,
				   .l2_stream_id_reg_offset =
				   IPU_MMU_L2_STREAM_ID_REG_OFFSET,
				},
			},
		       .dmem_offset = IPU_PSYS_DMEM_OFFSET,
		       .spc_offset = IPU_PSYS_SPC_OFFSET,
	},
};

/*
 * This is meant only as reference for initialising the buttress control,
 * because the different HW stepping can have different initial values
 *
 * There is a HW bug and IS_PWR and PS_PWR fields cannot be used to
 * detect if power on/off is ready. Using IS_PWR_FSM and PS_PWR_FSM
 * fields instead.
 */
const struct ipu_buttress_ctrl isys_buttress_ctrl = {
	.divisor = IS_FREQ_CTL_DIVISOR,
	.qos_floor = 0,
	.freq_ctl = BUTTRESS_REG_IS_FREQ_CTL,
	.pwr_sts_shift = BUTTRESS_PWR_STATE_IS_PWR_FSM_SHIFT,
	.pwr_sts_mask = BUTTRESS_PWR_STATE_IS_PWR_FSM_MASK,
	.pwr_sts_on = BUTTRESS_PWR_STATE_IS_PWR_FSM_IS_RDY,
	.pwr_sts_off = BUTTRESS_PWR_STATE_IS_PWR_FSM_IDLE,
};

/*
 * This is meant only as reference for initialising the buttress control,
 * because the different HW stepping can have different initial values
 */

const struct ipu_buttress_ctrl psys_buttress_ctrl = {
	.divisor = PS_FREQ_CTL_DEFAULT_RATIO,
	.qos_floor = PS_FREQ_CTL_DEFAULT_RATIO,
	.freq_ctl = BUTTRESS_REG_PS_FREQ_CTL,
	.pwr_sts_shift = BUTTRESS_PWR_STATE_PS_PWR_FSM_SHIFT,
	.pwr_sts_mask = BUTTRESS_PWR_STATE_PS_PWR_FSM_MASK,
	.pwr_sts_on = BUTTRESS_PWR_STATE_PS_PWR_FSM_PS_PWR_UP,
	.pwr_sts_off = BUTTRESS_PWR_STATE_PS_PWR_FSM_IDLE,
};
#endif

#ifdef CONFIG_VIDEO_INTEL_IPU4P

/*
 * ipu4p available hw ports start from sip0 port3
 * available ports are:
 * s0p3, s1p0, s1p1, s1p2, s1p3
 */
static unsigned int ipu4p_csi_offsets[] = {
	0x64300, 0x6c000, 0x6c100, 0x6c200, 0x6c300
};

static unsigned char ipu4p_csi_evlanecombine[] = {
	0, 0, 0, 0, 0, 0
};

static unsigned int ipu4p_tpg_offsets[] = {
	IPU_TPG0_ADDR_OFFSET,
	IPU_TPG1_ADDR_OFFSET
};

static unsigned int ipu4p_tpg_sels[] = {
	IPU_GPOFFSET + IPU_GPREG_MIPI_PKT_GEN0_SEL,
	IPU_COMBO_GPOFFSET + IPU_GPREG_MIPI_PKT_GEN1_SEL
};

const struct ipu_isys_internal_pdata isys_ipdata = {
	.csi2 = {
		 .nports = ARRAY_SIZE(ipu4p_csi_offsets),
		 .offsets = ipu4p_csi_offsets,
		 .evlanecombine = ipu4p_csi_evlanecombine,
	},
	.tpg = {
		.ntpgs = ARRAY_SIZE(ipu4p_tpg_offsets),
		.offsets = ipu4p_tpg_offsets,
		.sels = ipu4p_tpg_sels,
	},
	.hw_variant = {
		       .offset = IPU_ISYS_OFFSET,
		       .nr_mmus = 2,
		       .mmu_hw = {
				{
				   .offset = IPU_ISYS_IOMMU0_OFFSET,
				   .info_bits =
				   IPU_INFO_REQUEST_DESTINATION_PRIMARY,
				   .nr_l1streams = 0,
				   .nr_l2streams = 0,
				   .insert_read_before_invalidate = true,
				},
				{
				   .offset = IPU_ISYS_IOMMU1_OFFSET,
				   .info_bits = IPU_INFO_STREAM_ID_SET(0),
				   .nr_l1streams = IPU_MMU_MAX_TLB_L1_STREAMS,
				   .l1_block_sz = {
						   5, 16, 6, 6, 6, 6, 6, 8, 0,
						   0, 0, 0, 0, 0, 0, 5
				   },
				   .l1_zlw_en = {
						 0, 1, 1, 1, 1, 1, 1, 1, 0, 0,
						 0, 0, 0, 0, 0, 0
				   },
				   .l1_zlw_1d_mode = {
						      0, 1, 1, 1, 1, 1, 1, 1, 0,
						      0, 0, 0, 0, 0, 0, 0
				   },
				   .l1_ins_zlw_ahead_pages = {
							      0, 3, 3, 3, 3, 3,
							      3, 3, 0, 0, 0, 0,
							      0, 0, 0, 0
				   },
				   .l1_zlw_2d_mode = {
						      0, 0, 0, 0, 0, 0, 0, 0, 0,
						      0, 0, 0, 0, 0, 0, 0
				   },
				   .nr_l2streams = IPU_MMU_MAX_TLB_L2_STREAMS,
				   .l2_block_sz = {
						   2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
						   2, 2, 2, 2, 2, 2
				   },
				   .insert_read_before_invalidate = false,
				   .zlw_invalidate = true,
				   .l1_stream_id_reg_offset =
				   IPU_MMU_L1_STREAM_ID_REG_OFFSET,
				   .l2_stream_id_reg_offset =
				   IPU_MMU_L2_STREAM_ID_REG_OFFSET,
				   },
			},
		       .dmem_offset = IPU_ISYS_DMEM_OFFSET,
		       .spc_offset = IPU_ISYS_SPC_OFFSET,
	},
	.num_parallel_streams = IPU_ISYS_NUM_STREAMS,
	.isys_dma_overshoot = IPU_ISYS_OVERALLOC_MIN,
};

const struct ipu_psys_internal_pdata psys_ipdata = {
	.hw_variant = {
		       .offset = IPU_PSYS_OFFSET,
		       .nr_mmus = 3,
		       .mmu_hw = {
				{
				   .offset = IPU_PSYS_IOMMU0_OFFSET,
				   .info_bits =
				   IPU_INFO_REQUEST_DESTINATION_PRIMARY,
				   .nr_l1streams = 0,
				   .nr_l2streams = 0,
				   .insert_read_before_invalidate = true,
				},
				{
				   .offset = IPU_PSYS_IOMMU1_OFFSET,
				   .info_bits = IPU_INFO_STREAM_ID_SET(0),
				   .nr_l1streams = IPU_MMU_MAX_TLB_L1_STREAMS,
				   .l1_block_sz = {
						   2, 5, 4, 2, 2, 10, 5, 16, 10,
						   5, 0, 0, 0, 0, 0, 3
				   },
				   .l1_zlw_en = {
						 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
						 0, 0, 0, 0, 0, 0
				   },
				   .l1_zlw_1d_mode = {
						      0, 0, 1, 1, 1, 1, 1, 1, 1,
						      1, 0, 0, 0, 0, 0, 0
				   },
				   .l1_ins_zlw_ahead_pages = {
							      0, 0, 3, 3, 3, 3,
							      3, 3, 3, 3, 0, 0,
							      0, 0, 0, 0
				   },
				   .l1_zlw_2d_mode = {
						      0, 0, 0, 0, 0, 0, 0, 0, 0,
						      0, 0, 0, 0, 0, 0, 0
				   },
				   .nr_l2streams = IPU_MMU_MAX_TLB_L2_STREAMS,
				   .l2_block_sz = {
						   2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
						   2, 2, 2, 2, 2, 2
				   },
				   .insert_read_before_invalidate = false,
				   .zlw_invalidate = true,
				   .l1_stream_id_reg_offset =
				   IPU_MMU_L1_STREAM_ID_REG_OFFSET,
				   .l2_stream_id_reg_offset =
				   IPU_MMU_L2_STREAM_ID_REG_OFFSET,
				},
				{
				   .offset = IPU_PSYS_IOMMU1R_OFFSET,
				   .info_bits = IPU_INFO_STREAM_ID_SET(0),
				   .nr_l1streams = IPU_MMU_MAX_TLB_L1_STREAMS,
				   .l1_block_sz = {
						   2, 6, 5, 16, 16, 8, 8, 0, 0,
						   0, 0, 0, 0, 0, 0, 3
				   },
				   .l1_zlw_en = {
						 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
						 0, 0, 0, 0, 0, 0
				   },
				   .l1_zlw_1d_mode = {
						      0, 0, 1, 1, 0, 0, 0, 0, 0,
						      0, 0, 0, 0, 0, 0, 0
				   },
				   .l1_ins_zlw_ahead_pages = {
							      0, 0, 3, 3, 0, 0,
							      0, 0, 0, 0, 0, 0,
							      0, 0, 0, 0
				   },
				   .l1_zlw_2d_mode = {
						      0, 0, 0, 0, 0, 0, 0, 0, 0,
						      0, 0, 0, 0, 0, 0, 0
				   },
				   .nr_l2streams = IPU_MMU_MAX_TLB_L2_STREAMS,
				   .l2_block_sz = {
						   2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
						   2, 2, 2, 2, 2, 2
				   },
				   .insert_read_before_invalidate = false,
				   .zlw_invalidate = true,
				   .l1_stream_id_reg_offset =
				   IPU_MMU_L1_STREAM_ID_REG_OFFSET,
				   .l2_stream_id_reg_offset =
				   IPU_MMU_L2_STREAM_ID_REG_OFFSET,
				},
			},
		       .dmem_offset = IPU_PSYS_DMEM_OFFSET,
		       .spc_offset = IPU_PSYS_SPC_OFFSET,
	},
};

const struct ipu_buttress_ctrl isys_buttress_ctrl = {
	.divisor = IS_FREQ_CTL_DIVISOR,
	.qos_floor = 0,
	.ovrd = 0,
	.freq_ctl = BUTTRESS_REG_IS_FREQ_CTL,
	.divisor_shift = BUTTRESS_REG_IS_FREQ_CTL_RATIO_SHIFT,
	.pwr_sts_shift = BUTTRESS_PWR_STATE_IS_PWR_FSM_SHIFT,
	.pwr_sts_mask = BUTTRESS_PWR_STATE_IS_PWR_FSM_MASK,
	.pwr_sts_on = BUTTRESS_PWR_STATE_IS_PWR_FSM_IS_RDY,
	.pwr_sts_off = BUTTRESS_PWR_STATE_IS_PWR_FSM_IDLE,
};

const struct ipu_buttress_ctrl psys_buttress_ctrl = {
	.divisor = PS_FREQ_CTL_DEFAULT_RATIO,
	.qos_floor = PS_FREQ_CTL_DEFAULT_RATIO,
	.ovrd = 1,
	.freq_ctl = BUTTRESS_REG_PS_FREQ_CTL,
	.divisor_shift = BUTTRESS_REG_PS_FREQ_CTL_RATIO_SHIFT,
	.ovrd_shift = BUTTRESS_REG_PS_FREQ_CTL_OVRD_SHIFT,
	.pwr_sts_shift = BUTTRESS_PWR_STATE_PS_PWR_FSM_SHIFT,
	.pwr_sts_mask = BUTTRESS_PWR_STATE_PS_PWR_FSM_MASK,
	.pwr_sts_on = BUTTRESS_PWR_STATE_PS_PWR_FSM_PS_PWR_UP,
	.pwr_sts_off = BUTTRESS_PWR_STATE_PS_PWR_FSM_IDLE,
};
#endif

void ipu_configure_spc(struct ipu_device *isp,
		       const struct ipu_hw_variants *hw_variant,
		       int pkg_dir_idx, void __iomem *base, u64 *pkg_dir,
		       dma_addr_t pkg_dir_dma_addr)
{
	u32 val;
	void __iomem *dmem_base = base + hw_variant->dmem_offset;
	void __iomem *spc_regs_base = base + hw_variant->spc_offset;

	val = readl(spc_regs_base + IPU_PSYS_REG_SPC_STATUS_CTRL);
	val |= IPU_PSYS_SPC_STATUS_CTRL_ICACHE_INVALIDATE;
	writel(val, spc_regs_base + IPU_PSYS_REG_SPC_STATUS_CTRL);

	if (isp->secure_mode) {
		writel(IPU_PKG_DIR_IMR_OFFSET, dmem_base);
	} else {
		u32 server_addr;

		server_addr = ipu_cpd_pkg_dir_get_address(pkg_dir, pkg_dir_idx);

		writel(server_addr +
			   ipu_cpd_get_pg_icache_base(isp, pkg_dir_idx,
						      isp->cpd_fw->data,
						      isp->cpd_fw->size),
			   spc_regs_base + IPU_PSYS_REG_SPC_ICACHE_BASE);
		writel(ipu_cpd_get_pg_entry_point(isp, pkg_dir_idx,
						      isp->cpd_fw->data,
						      isp->cpd_fw->size),
			   spc_regs_base + IPU_PSYS_REG_SPC_START_PC);
		writel(IPU_INFO_REQUEST_DESTINATION_PRIMARY,
			   spc_regs_base +
			   IPU_REG_PSYS_INFO_SEG_0_CONFIG_ICACHE_MASTER);
		writel(pkg_dir_dma_addr, dmem_base);
	}
}
EXPORT_SYMBOL(ipu_configure_spc);

int ipu_buttress_psys_freq_get(void *data, u64 *val)
{
	struct ipu_device *isp = data;
	u32 reg_val, ratio;
	int rval;

	rval = pm_runtime_get_sync(&isp->psys->dev);
	if (rval < 0) {
		pm_runtime_put(&isp->psys->dev);
		dev_err(&isp->pdev->dev, "Runtime PM failed (%d)\n", rval);
		return rval;
	}

	reg_val = readl(isp->base + BUTTRESS_REG_PS_FREQ_CAPABILITIES);

	pm_runtime_put(&isp->psys->dev);

	ratio = (reg_val &
		 BUTTRESS_PS_FREQ_CAPABILITIES_LAST_RESOLVED_RATIO_MASK) >>
	    BUTTRESS_PS_FREQ_CAPABILITIES_LAST_RESOLVED_RATIO_SHIFT;

	*val = BUTTRESS_PS_FREQ_STEP * ratio;

	return 0;
}

int ipu_buttress_isys_freq_get(void *data, u64 *val)
{
	struct ipu_device *isp = data;
	u32 reg_val;
	int rval;

	rval = pm_runtime_get_sync(&isp->isys->dev);
	if (rval < 0) {
		pm_runtime_put(&isp->isys->dev);
		dev_err(&isp->pdev->dev, "Runtime PM failed (%d)\n", rval);
		return rval;
	}

	reg_val = readl(isp->base + BUTTRESS_REG_IS_FREQ_CTL);

	pm_runtime_put(&isp->isys->dev);

	/* Input system frequency specified as 1600MHz/divisor */
	*val = 1600 / (reg_val & BUTTRESS_IS_FREQ_CTL_DIVISOR_MASK);

	return 0;
}
