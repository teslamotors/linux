/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef INTEL_IPU5_REGS_H
#define INTEL_IPU5_REGS_H

#define INTEL_IPU5_MRFLD_DATA_IOMMU_OFFSET		0x00070000
#define INTEL_IPU5_MRFLD_ICACHE_IOMMU_OFFSET		0x000a0000

/* IPU5 A0 offsets */
#define INTEL_IPU5_A0_ISYS_IOMMU0_OFFSET		0x000e0000
#define INTEL_IPU5_A0_ISYS_IOMMU1_OFFSET		0x000e0100

#define INTEL_IPU5_A0_ISYS_OFFSET			0x00100000
#define INTEL_IPU5_A0_PSYS_OFFSET			0x00100000

#define INTEL_IPU5_A0_PSYS_IOMMU0_OFFSET		0x00140000
#define INTEL_IPU5_A0_PSYS_IOMMU1_OFFSET		0x00140100
#define INTEL_IPU5_A0_PSYS_IOMMU1R_OFFSET		0x00140600

#define INTEL_IPU5_A0_TPG0_ADDR_OFFSET		0x69c00
#define INTEL_IPU5_A0_TPG1_ADDR_OFFSET		0x6bc00
#define INTEL_IPU5_A0_TPG2_ADDR_OFFSET		0x6dc00
#define INTEL_IPU5_A0_CSI2BE_ADDR_OFFSET		0xba000

/* Common for both A0 and B0 */
#define INTEL_IPU5_PSYS_MMU0_CTRL_OFFSET		0x08

#define INTEL_IPU5_GP0OFFSET			0x69800
#define INTEL_IPU5_GP1OFFSET			0x6B800
#define INTEL_IPU5_GP2OFFSET			0x6D800

#define INTEL_IPU5_ISYS_SPC_OFFSET			0x000000
#define INTEL_IPU5_PSYS_SPC_OFFSET			0x180000
#define INTEL_IPU5_ISYS_DMEM_OFFSET			0x008000
#define INTEL_IPU5_PSYS_DMEM_OFFSET			0x188000

/* PKG DIR OFFSET in IMR in secure mode */
#define INTEL_IPU5_PKG_DIR_IMR_OFFSET			0x40

#define INTEL_IPU5_GPREG_MIPI_PKT_GEN0_SEL		0x1c
#define INTEL_IPU5_GPREG_MIPI_PKT_GEN1_SEL		0x1c
#define INTEL_IPU5_GPREG_MIPI_PKT_GEN2_SEL		0x1c

/* IPU5 PCI config registers */
#define INTEL_IPU5_REG_PCI_PCIECAPHDR_PCIECAP		0x70
#define INTEL_IPU5_REG_PCI_DEVICECAP			0x74
#define INTEL_IPU5_REG_PCI_DEVICECTL_DEVICESTS		0x78
#define INTEL_IPU5_REG_PCI_MSI_CAPID			0xac
#define INTEL_IPU5_REG_PCI_MSI_ADDRESS_LO			0xb0
#define INTEL_IPU5_REG_PCI_MSI_ADDRESS_HI			0xb4
#define INTEL_IPU5_REG_PCI_MSI_DATA			0xb8
#define INTEL_IPU5_REG_PCI_PMCAP				0xd0
#define INTEL_IPU5_REG_PCI_PMCS				0xd4
#define INTEL_IPU5_REG_PCI_MANUFACTURING_ID		0xf8
#define INTEL_IPU5_REG_PCI_IUNIT_ACCESS_CTRL_VIOL		0xfc

/* IPU5 ISYS registers */
/* Isys DMA CIO info register */
#define INTEL_IPU5_REG_ISYS_INFO_CIO_DMA0(a)		(0x81810 + (a) * 0x40)
#define INTEL_IPU5_REG_ISYS_INFO_CIO_DMA1(a)		(0x93010 + (a) * 0x40)
#define INTEL_IPU5_REG_ISYS_INFO_CIO_DMA_IS(a)		(0xb0610 + (a) * 0x40)
#define INTEL_IPU5_ISYS_NUM_OF_DMA0_CHANNELS		16
#define INTEL_IPU5_ISYS_NUM_OF_DMA1_CHANNELS		32
#define INTEL_IPU5_ISYS_NUM_OF_IS_CHANNELS			4
/*Isys Info register offsets*/
#define INTEL_IPU5_REG_ISYS_INFO_SEG_0_CONFIG_ICACHE_MASTER	0x14
#define INTEL_IPU5_REG_ISYS_INFO_SEG_CMEM_MASTER(a)	(0x2C + (a * 12))
#define INTEL_IPU5_REG_ISYS_INFO_SEG_XMEM_MASTER(a)	(0x5C + (a * 12))

/* IRQ-related registers relative to ISYS_OFFSET */
#define INTEL_IPU5_REG_ISYS_UNISPART_IRQ_EDGE			0x7c000
#define INTEL_IPU5_REG_ISYS_UNISPART_IRQ_MASK			0x7c004
#define INTEL_IPU5_REG_ISYS_UNISPART_IRQ_STATUS			0x7c008
#define INTEL_IPU5_REG_ISYS_UNISPART_IRQ_CLEAR			0x7c00c
#define INTEL_IPU5_REG_ISYS_UNISPART_IRQ_ENABLE			0x7c010
#define INTEL_IPU5_REG_ISYS_UNISPART_IRQ_LEVEL_NOT_PULSE	0x7c014

/* CSI HUB TOP IRQ-related registers offset*/
#define INTEL_IPU5_REG_ISYS_CSI_TOP_IRQ_EDGE			0x64200
#define INTEL_IPU5_REG_ISYS_CSI_TOP_IRQ_MASK			0x64204
#define INTEL_IPU5_REG_ISYS_CSI_TOP_IRQ_STATUS			0x64208
#define INTEL_IPU5_REG_ISYS_CSI_TOP_IRQ_CLEAR			0x6420c
#define INTEL_IPU5_REG_ISYS_CSI_TOP_IRQ_ENABLE			0x64210
#define INTEL_IPU5_REG_ISYS_CSI_TOP_IRQ_LEVEL_NOT_PULSE		0x64214

/* CDC Burst collector thresholds for isys - 3 FIFOs i = 0..2 */
#define INTEL_IPU5_REG_ISYS_CDC_THRESHOLD(i)		(0x7c400 + ((i) * 4))
#define INTEL_IPU5_REG_ISYS_UNISPART_SW_IRQ_REG		0x7c414
#define INTEL_IPU5_REG_ISYS_UNISPART_SW_IRQ_MUX_REG	0x7c418

#define INTEL_IPU5_CSI_PIPE_NUM_PER_TOP				0x2
#define INTEL_IPU5_CSI_IRQ_NUM_PER_PIPE				0x3

#define INTEL_IPU5_ISYS_CSI_TOP_IRQ_A0(irq_num)			\
	(0x1 << irq_num)

#define INTEL_IPU5_ISYS_UNISPART_IRQ_CSI2_A0(port)		\
	(0x1 << (port / INTEL_IPU5_ISYS_MAX_CSI2_COMBO_PORTS *	\
	INTEL_IPU5_CSI_IRQ_NUM_PER_PIPE *			\
	INTEL_IPU5_CSI_PIPE_NUM_PER_TOP +			\
	port % INTEL_IPU5_ISYS_MAX_CSI2_COMBO_PORTS /		\
	INTEL_IPU5_CSI_PIPE_NUM_PER_TOP  +			\
	port % INTEL_IPU5_ISYS_MAX_CSI2_COMBO_PORTS %		\
	INTEL_IPU5_CSI_PIPE_NUM_PER_TOP * INTEL_IPU5_CSI_IRQ_NUM_PER_PIPE))

#define INTEL_IPU5_ISYS_UNISPART_IRQ_SW				(1 << 22)

/*Iunit Info bits*/
#define INTEL_IPU5_REG_PSYS_INFO_SEG_CMEM_MASTER(a)	(0x2C + ((a) * 12))
#define INTEL_IPU5_REG_PSYS_INFO_SEG_XMEM_MASTER(a)	(0x5C + ((a) * 12))
#define INTEL_IPU5_REG_PSYS_INFO_SEG_DATA_MASTER(a)	(0x8C + ((a) * 12))


#define INTEL_IPU5_PSYS_REG_SPC_STATUS_CTRL			0x00000

#define INTEL_IPU5_PSYS_SPC_STATUS_CTRL_ICACHE_INVALIDATE	(1 << 12)

#define INTEL_IPU5_PSYS_REG_SPC_START_PC		0x4
#define INTEL_IPU5_PSYS_REG_SPC_ICACHE_BASE		0x10
#define INTEL_IPU5_PSYS_REG_SPP0_STATUS_CTRL		0x20000
#define INTEL_IPU5_PSYS_REG_SPP1_STATUS_CTRL		0x30000
#define INTEL_IPU5_PSYS_REG_ISP0_STATUS_CTRL		0x180000
#define INTEL_IPU5_PSYS_REG_ISP1_STATUS_CTRL		0x200000
#define INTEL_IPU5_PSYS_REG_ISP2_STATUS_CTRL		0x280000
#define INTEL_IPU5_REG_PSYS_INFO_SEG_0_CONFIG_ICACHE_MASTER	0x14

/* IRQ-related registers in PSYS, relative to INTEL_IPU5_BXT_xx_PSYS_OFFSET */
#define INTEL_IPU5_REG_PSYS_GPDEV_IRQ_EDGE		0x128200
#define INTEL_IPU5_REG_PSYS_GPDEV_IRQ_MASK		0x128204
#define INTEL_IPU5_REG_PSYS_GPDEV_IRQ_STATUS		0x128208
#define INTEL_IPU5_REG_PSYS_GPDEV_IRQ_CLEAR		0x12820c
#define INTEL_IPU5_REG_PSYS_GPDEV_IRQ_ENABLE		0x128210
#define INTEL_IPU5_REG_PSYS_GPDEV_IRQ_LEVEL_NOT_PULSE	0x128214
/* There are 8 FW interrupts, n = 0..7 */
#define INTEL_IPU5_PSYS_GPDEV_IRQ_FWIRQ(n)		(BIT(17) << (n))
#define INTEL_IPU5_REG_PSYS_GPDEV_FWIRQ(n)		(4 * (n) + 0x128100)
/* CDC Burst collector thresholds for psys - 4 FIFOs i= 0..3 */
#define INTEL_IPU5_REG_PSYS_CDC_THRESHOLD(i)           (0x128700 + ((i) * 4))

/*VCO*/
#define INTEL_IPU5_INFO_ENABLE_SNOOP			BIT(0)
#define INTEL_IPU5_INFO_IMR_DESTINED			BIT(1)
#define INTEL_IPU5_INFO_REQUEST_DESTINATION_BUT_REGS	0
#define INTEL_IPU5_INFO_REQUEST_DESTINATION_PRIMARY	BIT(4)
#define INTEL_IPU5_INFO_REQUEST_DESTINATION_P2P		(BIT(4) | BIT(5))
/*VC1*/
#define INTEL_IPU5_INFO_DEADLINE_PTR                      BIT(1)
#define INTEL_IPU5_INFO_ZLW                               BIT(2)
#define INTEL_IPU5_INFO_STREAM_ID_SET(a)	((a & 0xF) << 4)
#define INTEL_IPU5_INFO_ADDRESS_SWIZZ                     BIT(8)

/* ISYS trace registers - offsets to isys base address */
/* Trace unit base offset */
#define INTEL_IPU5_TRACE_REG_IS_TRACE_UNIT_BASE			0x07d000
/* Trace monitors */
#define INTEL_IPU5_TRACE_REG_IS_SP_EVQ_BASE			0x001000
/* GPC blocks */
#define INTEL_IPU5_TRACE_REG_IS_SP_GPC_BASE			0x000800
#define INTEL_IPU5_TRACE_REG_IS_ISL_GPC_BASE			0x0aca00
#define INTEL_IPU5_TRACE_REG_IS_MMU_GPC_BASE			0x0e0b00
/* CSI2 receivers */
#define INTEL_IPU5_TRACE_REG_CSI2_TM_BASE			0x064400

/* Trace timers */
#define INTEL_IPU5_TRACE_REG_PS_GPREG_TRACE_TIMER_RST_N		0x124714
#define INTEL_IPU5_TRACE_REG_IS_GPREG_TRACE_TIMER_RST_N		0x07c410
#define INTEL_IPU5_TRACE_REG_GPREG_TRACE_TIMER_RST_OFF		BIT(0)

/* SIG2CIO */
/* 0 < n <= 10 */
#define INTEL_IPU5_TRACE_REG_CSI2_SIG2SIO_GRn_BASE(n)		\
			(0x064600 + (n) * 0x20)
#define INTEL_IPU5_TRACE_REG_CSI2_SIG2SIO_GR_NUM		11

/* PSYS trace registers - offsets to isys base address */
/* Trace unit base offset */
#define INTEL_IPU5_TRACE_REG_PS_TRACE_UNIT_BASE			0x12c000
/* Trace monitors */
#define INTEL_IPU5_TRACE_REG_PS_SPC_EVQ_BASE			0x181000
#define INTEL_IPU5_TRACE_REG_PS_SPP0_EVQ_BASE			0x1a1000
#define INTEL_IPU5_TRACE_REG_PS_SPP1_EVQ_BASE			0x1b1000

#define INTEL_IPU5_TRACE_REG_PS_ISP0_EVQ_BASE			0x301000
#define INTEL_IPU5_TRACE_REG_PS_ISP1_EVQ_BASE			0x381000
#define INTEL_IPU5_TRACE_REG_PS_ISP2_EVQ_BASE			0x401000

/* GPC blocks */
#define INTEL_IPU5_TRACE_REG_PS_SPC_GPC_BASE			0x180800
#define INTEL_IPU5_TRACE_REG_PS_SPP0_GPC_BASE			0x1a0800
#define INTEL_IPU5_TRACE_REG_PS_SPP1_GPC_BASE			0x1b0800
#define INTEL_IPU5_TRACE_REG_PS_MMU_GPC_BASE			0x140b00
#define INTEL_IPU5_TRACE_REG_PS_ISL_GPC_BASE			0x271800
#define INTEL_IPU5_TRACE_REG_PS_ISP0_GPC_BASE			0x300800
#define INTEL_IPU5_TRACE_REG_PS_ISP1_GPC_BASE			0x380800
#define INTEL_IPU5_TRACE_REG_PS_ISP2_GPC_BASE			0x400800

/*
*this structure is only for ipu5 fpga, and the cpd fw file not ready
This would be removed until cpd ready
*/
struct ia_css_cell_program_s {
	unsigned int magic_number;

	unsigned int blob_offset;
	unsigned int blob_size;

	unsigned int start[3];

	unsigned int icache_source;
	unsigned int icache_target;
	unsigned int icache_size;

	unsigned int pmem_source;
	unsigned int pmem_target;
	unsigned int pmem_size;

	unsigned int data_source;
	unsigned int data_target;
	unsigned int data_size;

	unsigned int bss_target;
	unsigned int bss_size;

	unsigned int cell_id;
	unsigned int regs_addr;

	unsigned int cell_pmem_data_bus_address;
	unsigned int cell_dmem_data_bus_address;
	unsigned int cell_pmem_control_bus_address;
	unsigned int cell_dmem_control_bus_address;

	unsigned int next;
	unsigned int dummy[2];
};


#endif /* INTEL_IPU5_REGS_H */
