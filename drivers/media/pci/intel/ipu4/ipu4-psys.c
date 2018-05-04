// SPDX-License_Identifier: GPL-2.0
// Copyright (C) 2018 Intel Corporation

#include "ipu.h"
#include "ipu-psys.h"
#include "ipu-platform-regs.h"
#include "ipu-trace.h"

struct ipu_trace_block psys_trace_blocks[] = {
	{
	 .offset = TRACE_REG_PS_TRACE_UNIT_BASE,
	 .type = IPU_TRACE_BLOCK_TUN,
	},
	{
	 .offset = TRACE_REG_PS_SPC_EVQ_BASE,
	 .type = IPU_TRACE_BLOCK_TM,
	},
	{
	 .offset = TRACE_REG_PS_SPP0_EVQ_BASE,
	 .type = IPU_TRACE_BLOCK_TM,
	},
	{
	 .offset = TRACE_REG_PS_SPP1_EVQ_BASE,
	 .type = IPU_TRACE_BLOCK_TM,
	},
	{
	 .offset = TRACE_REG_PS_ISP0_EVQ_BASE,
	 .type = IPU_TRACE_BLOCK_TM,
	},
	{
	 .offset = TRACE_REG_PS_ISP1_EVQ_BASE,
	 .type = IPU_TRACE_BLOCK_TM,
	},
	{
	 .offset = TRACE_REG_PS_ISP2_EVQ_BASE,
	 .type = IPU_TRACE_BLOCK_TM,
	},
	{
	 .offset = TRACE_REG_PS_ISP3_EVQ_BASE,
	 .type = IPU_TRACE_BLOCK_TM,
	},
	{
	 .offset = TRACE_REG_PS_SPC_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_SPP0_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_SPP1_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_MMU_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_ISL_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_ISP0_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_ISP1_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_ISP2_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_ISP3_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_GPREG_TRACE_TIMER_RST_N,
	 .type = IPU_TRACE_TIMER_RST,
	},
	{
	 .type = IPU_TRACE_BLOCK_END,
	}
};

static void set_sp_info_bits(void *base)
{
	int i;

	writel(IPU_INFO_REQUEST_DESTINATION_PRIMARY,
		   base + IPU_REG_PSYS_INFO_SEG_0_CONFIG_ICACHE_MASTER);

	for (i = 0; i < 4; i++)
		writel(IPU_INFO_REQUEST_DESTINATION_PRIMARY,
			   base + IPU_REG_PSYS_INFO_SEG_CMEM_MASTER(i));
	for (i = 0; i < 4; i++)
		writel(IPU_INFO_REQUEST_DESTINATION_PRIMARY,
			   base + IPU_REG_PSYS_INFO_SEG_XMEM_MASTER(i));
}

static void set_isp_info_bits(void *base)
{
	int i;

	writel(IPU_INFO_REQUEST_DESTINATION_PRIMARY,
		   base + IPU_REG_PSYS_INFO_SEG_0_CONFIG_ICACHE_MASTER);

	for (i = 0; i < 4; i++)
		writel(IPU_INFO_REQUEST_DESTINATION_PRIMARY,
			   base + IPU_REG_PSYS_INFO_SEG_DATA_MASTER(i));
}

void psys_setup_hw(struct ipu_psys *psys)
{
	void __iomem *base = psys->pdata->base;
	void __iomem *spc_regs_base =
	    base + psys->pdata->ipdata->hw_variant.spc_offset;
	void *psys_iommu0_ctrl = base +
	    psys->pdata->ipdata->hw_variant.mmu_hw[0].offset +
		IPU_PSYS_MMU0_CTRL_OFFSET;
	const u8 *thd = psys->pdata->ipdata->hw_variant.cdc_fifo_threshold;
	u32 irqs;
	unsigned int i;

	/* Configure PSYS info bits */
	writel(IPU_INFO_REQUEST_DESTINATION_PRIMARY, psys_iommu0_ctrl);

	set_sp_info_bits(spc_regs_base + IPU_PSYS_REG_SPC_STATUS_CTRL);
	set_sp_info_bits(spc_regs_base + IPU_PSYS_REG_SPP0_STATUS_CTRL);
	set_sp_info_bits(spc_regs_base + IPU_PSYS_REG_SPP1_STATUS_CTRL);
	set_isp_info_bits(spc_regs_base + IPU_PSYS_REG_ISP0_STATUS_CTRL);
	set_isp_info_bits(spc_regs_base + IPU_PSYS_REG_ISP1_STATUS_CTRL);
	set_isp_info_bits(spc_regs_base + IPU_PSYS_REG_ISP2_STATUS_CTRL);
	set_isp_info_bits(spc_regs_base + IPU_PSYS_REG_ISP3_STATUS_CTRL);

	/* Enable FW interrupt #0 */
	writel(0, base + IPU_REG_PSYS_GPDEV_FWIRQ(0));
	irqs = IPU_PSYS_GPDEV_IRQ_FWIRQ(0);
	writel(irqs, base + IPU_REG_PSYS_GPDEV_IRQ_EDGE);
	/*
	 * With pulse setting, driver misses interrupts. IUNIT integration
	 * HAS(v1.26) suggests to use pulse, but this seem to be error in
	 * documentation.
	 */
	writel(irqs, base + IPU_REG_PSYS_GPDEV_IRQ_LEVEL_NOT_PULSE);
	writel(irqs, base + IPU_REG_PSYS_GPDEV_IRQ_CLEAR);
	writel(irqs, base + IPU_REG_PSYS_GPDEV_IRQ_MASK);
	writel(irqs, base + IPU_REG_PSYS_GPDEV_IRQ_ENABLE);

	/* Write CDC FIFO threshold values for psys */
	for (i = 0; i < psys->pdata->ipdata->hw_variant.cdc_fifos; i++)
		writel(thd[i], base + IPU_REG_PSYS_CDC_THRESHOLD(i));
}
