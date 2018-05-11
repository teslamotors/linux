/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2014 - 2018 Intel Corporation */

#ifndef IPU_PLATFORM_REGS_H
#define IPU_PLATFORM_REGS_H

#ifdef CONFIG_VIDEO_INTEL_IPU4P
#define IPU_ISYS_IOMMU0_OFFSET                  0x000e0000
#define IPU_ISYS_IOMMU1_OFFSET                  0x000e0100

#define IPU_ISYS_OFFSET                         0x00100000
#define IPU_PSYS_OFFSET                         0x00400000

#define IPU_PSYS_IOMMU0_OFFSET                  0x000b0000
#define IPU_PSYS_IOMMU1_OFFSET                  0x000b0100
#define IPU_PSYS_IOMMU1R_OFFSET                 0x000b0600

/* the offset from IOMMU base register */
#define IPU_MMU_L1_STREAM_ID_REG_OFFSET		0x0c
#define IPU_MMU_L2_STREAM_ID_REG_OFFSET		0x4c

#define IPU_TPG0_ADDR_OFFSET                    0x66c00
#define IPU_TPG1_ADDR_OFFSET                    0x6ec00
#define IPU_CSI2BE_ADDR_OFFSET                  0xba000

#define IPU_PSYS_MMU0_CTRL_OFFSET               0x08

#define IPU_GPOFFSET                            0x66800
#define IPU_COMBO_GPOFFSET                      0x6e800

#define IPU_GPREG_MIPI_PKT_GEN0_SEL             0x1c
#define IPU_GPREG_MIPI_PKT_GEN1_SEL             0x1c

#define IPU_REG_ISYS_ISA_ACC_IRQ_CTRL_BASE      0xb0c00
#define IPU_REG_ISYS_A_IRQ_CTRL_BASE            0xbe200
#define IPU_REG_ISYS_SIP0_IRQ_CTRL_BASE         0x66d00
#define IPU_REG_ISYS_SIP1_IRQ_CTRL_BASE         0x6ed00
#define IPU_REG_ISYS_SIP0_IRQ_CTRL_STATUS       0x66d08
#define IPU_REG_ISYS_SIP1_IRQ_CTRL_STATUS       0x6ed08
#define IPU_REG_ISYS_SIP0_IRQ_CTRL_CLEAR        0x66d0c
#define IPU_REG_ISYS_SIP1_IRQ_CTRL_CLEAR        0x6ed0c
#define IPU_REG_ISYS_CSI_IRQ_CTRL_BASE(p)       \
	({ typeof(p) __p = (p); \
		__p > 0 ? (0x6cb00 + 0x800 * (__p - 1)) : (0x66300); })
#define IPU_REG_ISYS_CSI_IRQ_CTRL0_BASE(p)      \
	({ typeof(p) __p = (p); \
		__p > 0 ? (0x6cc00 + 0x800 * (__p - 1)) : (0x66400); })
#define IPU_ISYS_CSI2_A_IRQ_MASK		GENMASK(0, 0)
#define IPU_ISYS_CSI2_B_IRQ_MASK		GENMASK(1, 1)
#define IPU_ISYS_CSI2_C_IRQ_MASK		GENMASK(2, 2)
#define IPU_ISYS_CSI2_D_IRQ_MASK		GENMASK(3, 3)

/* IRQ-related registers relative to ISYS_OFFSET */
#define IPU_REG_ISYS_UNISPART_IRQ_EDGE            0x7c000
#define IPU_REG_ISYS_UNISPART_IRQ_MASK            0x7c004
#define IPU_REG_ISYS_UNISPART_IRQ_STATUS          0x7c008
#define IPU_REG_ISYS_UNISPART_IRQ_CLEAR           0x7c00c
#define IPU_REG_ISYS_UNISPART_IRQ_ENABLE          0x7c010
#define IPU_REG_ISYS_UNISPART_IRQ_LEVEL_NOT_PULSE 0x7c014
#define IPU_REG_ISYS_UNISPART_SW_IRQ_REG          0x7c414
#define IPU_REG_ISYS_UNISPART_SW_IRQ_MUX_REG      0x7c418
#define IPU_ISYS_UNISPART_IRQ_SW                  BIT(22)
#endif

#ifdef CONFIG_VIDEO_INTEL_IPU4
#define IPU_ISYS_IOMMU0_OFFSET		0x000e0000
#define IPU_ISYS_IOMMU1_OFFSET		0x000e0100

#define IPU_ISYS_OFFSET			0x00100000
#define IPU_PSYS_OFFSET			0x00400000

#define IPU_PSYS_IOMMU0_OFFSET		0x000b0000
#define IPU_PSYS_IOMMU1_OFFSET		0x000b0100
#define IPU_PSYS_IOMMU1R_OFFSET		0x000b0600

/* the offset from IOMMU base register */
#define IPU_MMU_L1_STREAM_ID_REG_OFFSET		0x0c
#define IPU_MMU_L2_STREAM_ID_REG_OFFSET		0x4c

#define IPU_TPG0_ADDR_OFFSET		0x64800
#define IPU_TPG1_ADDR_OFFSET		0x6f400
#define IPU_CSI2BE_ADDR_OFFSET		0xba000

#define IPU_PSYS_MMU0_CTRL_OFFSET		0x08

#define IPU_GPOFFSET				0x67800
#define IPU_COMBO_GPOFFSET			0x6f000

#define IPU_GPREG_MIPI_PKT_GEN0_SEL		0x24
#define IPU_GPREG_MIPI_PKT_GEN1_SEL		0x1c

/* IRQ-related registers relative to ISYS_OFFSET */
#define IPU_REG_ISYS_UNISPART_IRQ_EDGE		0x7c000
#define IPU_REG_ISYS_UNISPART_IRQ_MASK		0x7c004
#define IPU_REG_ISYS_UNISPART_IRQ_STATUS		0x7c008
#define IPU_REG_ISYS_UNISPART_IRQ_CLEAR		0x7c00c
#define IPU_REG_ISYS_UNISPART_IRQ_ENABLE		0x7c010
#define IPU_REG_ISYS_UNISPART_IRQ_LEVEL_NOT_PULSE 0x7c014
#define IPU_REG_ISYS_UNISPART_SW_IRQ_REG		0x7c414
#define IPU_REG_ISYS_UNISPART_SW_IRQ_MUX_REG	0x7c418
#define IPU_ISYS_UNISPART_IRQ_SW			BIT(30)
#endif /* CONFIG_VIDEO_INTEL_IPU4 */

#define IPU_ISYS_SPC_OFFSET			0x000000
#define IPU_PSYS_SPC_OFFSET			0x000000
#define IPU_ISYS_DMEM_OFFSET			0x008000
#define IPU_PSYS_DMEM_OFFSET			0x008000

/* PKG DIR OFFSET in IMR in secure mode */
#define IPU_PKG_DIR_IMR_OFFSET			0x40

/* PCI config registers */
#define IPU_REG_PCI_PCIECAPHDR_PCIECAP		0x70
#define IPU_REG_PCI_DEVICECAP			0x74
#define IPU_REG_PCI_DEVICECTL_DEVICESTS		0x78
#define IPU_REG_PCI_MSI_CAPID			0xac
#define IPU_REG_PCI_MSI_ADDRESS_LO			0xb0
#define IPU_REG_PCI_MSI_ADDRESS_HI			0xb4
#define IPU_REG_PCI_MSI_DATA			0xb8
#define IPU_REG_PCI_PMCAP				0xd0
#define IPU_REG_PCI_PMCS				0xd4
#define IPU_REG_PCI_MANUFACTURING_ID		0xf8
#define IPU_REG_PCI_IUNIT_ACCESS_CTRL_VIOL		0xfc

/* ISYS registers */
/* Isys DMA CIO info register */
#define IPU_REG_ISYS_INFO_CIO_DMA0(a)		(0x81810 + (a) * 0x40)
#define IPU_REG_ISYS_INFO_CIO_DMA1(a)		(0x93010 + (a) * 0x40)
#define IPU_REG_ISYS_INFO_CIO_DMA_IS(a)		(0xb0610 + (a) * 0x40)
#define IPU_ISYS_NUM_OF_DMA0_CHANNELS		16
#define IPU_ISYS_NUM_OF_DMA1_CHANNELS		32
#define IPU_ISYS_NUM_OF_IS_CHANNELS			4
/*Isys Info register offsets*/
#define IPU_REG_ISYS_INFO_SEG_0_CONFIG_ICACHE_MASTER	0x14
#define IPU_REG_ISYS_INFO_SEG_CMEM_MASTER(a)	(0x2C + (a * 12))
#define IPU_REG_ISYS_INFO_SEG_XMEM_MASTER(a)	(0x5C + (a * 12))

/* CDC Burst collector thresholds for isys - 3 FIFOs i = 0..2 */
#define IPU_REG_ISYS_CDC_THRESHOLD(i)		(0x7c400 + ((i) * 4))

/*Iunit Info bits*/
#define IPU_REG_PSYS_INFO_SEG_CMEM_MASTER(a)	(0x2C + ((a) * 12))
#define IPU_REG_PSYS_INFO_SEG_XMEM_MASTER(a)	(0x5C + ((a) * 12))
#define IPU_REG_PSYS_INFO_SEG_DATA_MASTER(a)	(0x8C + ((a) * 12))

#define IPU_ISYS_REG_SPC_STATUS_CTRL			0x0

#define IPU_ISYS_SPC_STATUS_START			BIT(1)
#define IPU_ISYS_SPC_STATUS_RUN				BIT(3)
#define IPU_ISYS_SPC_STATUS_READY			BIT(5)
#define IPU_ISYS_SPC_STATUS_CTRL_ICACHE_INVALIDATE	BIT(12)
#define IPU_ISYS_SPC_STATUS_ICACHE_PREFETCH		BIT(13)

#define IPU_PSYS_REG_SPC_STATUS_CTRL			0x0

#define IPU_PSYS_SPC_STATUS_START			BIT(1)
#define IPU_PSYS_SPC_STATUS_RUN				BIT(3)
#define IPU_PSYS_SPC_STATUS_READY			BIT(5)
#define IPU_PSYS_SPC_STATUS_CTRL_ICACHE_INVALIDATE	BIT(12)
#define IPU_PSYS_SPC_STATUS_ICACHE_PREFETCH		BIT(13)

#define IPU_PSYS_REG_SPC_START_PC		0x4
#define IPU_PSYS_REG_SPC_ICACHE_BASE		0x10
#define IPU_PSYS_REG_SPP0_STATUS_CTRL		0x20000
#define IPU_PSYS_REG_SPP1_STATUS_CTRL		0x30000
#define IPU_PSYS_REG_SPF_STATUS_CTRL		0x40000
#define IPU_PSYS_REG_ISP0_STATUS_CTRL		0x1C0000
#define IPU_PSYS_REG_ISP1_STATUS_CTRL		0x240000
#define IPU_PSYS_REG_ISP2_STATUS_CTRL		0x2C0000
#define IPU_PSYS_REG_ISP3_STATUS_CTRL		0x340000
#define IPU_REG_PSYS_INFO_SEG_0_CONFIG_ICACHE_MASTER	0x14

/* VC0 */
#define IPU_INFO_ENABLE_SNOOP			BIT(0)
#define IPU_INFO_IMR_DESTINED			BIT(1)
#define IPU_INFO_REQUEST_DESTINATION_BUT_REGS	0
#define IPU_INFO_REQUEST_DESTINATION_PRIMARY	BIT(4)
#define IPU_INFO_REQUEST_DESTINATION_P2P		(BIT(4) | BIT(5))
/* VC1 */
#define IPU_INFO_DEADLINE_PTR                      BIT(1)
#define IPU_INFO_ZLW                               BIT(2)
#define IPU_INFO_STREAM_ID_SET(a)	((a & 0xF) << 4)
#define IPU_INFO_ADDRESS_SWIZZ                     BIT(8)

/* Trace unit related register definitions */
#define TRACE_REG_MAX_ISYS_OFFSET	0x0fffff
#define TRACE_REG_MAX_PSYS_OFFSET	0xffffff
/* ISYS trace registers - offsets to isys base address */
/* Trace unit base offset */
#define TRACE_REG_IS_TRACE_UNIT_BASE			0x07d000
/* Trace monitors */
#define TRACE_REG_IS_SP_EVQ_BASE			0x001000
/* GPC blocks */
#define TRACE_REG_IS_SP_GPC_BASE			0x000800
#define TRACE_REG_IS_ISL_GPC_BASE			0x0bd400
#define TRACE_REG_IS_MMU_GPC_BASE			0x0e0B00
/* CSI2 receivers */
#define TRACE_REG_CSI2_TM_BASE				0x067a00
#define TRACE_REG_CSI2_3PH_TM_BASE			0x06f200
/* Trace timers */
#define TRACE_REG_PS_GPREG_TRACE_TIMER_RST_N		0x060614
#define TRACE_REG_IS_GPREG_TRACE_TIMER_RST_N		0x07c410
#define TRACE_REG_GPREG_TRACE_TIMER_RST_OFF		BIT(0)
/* SIG2CIO */
/* 0 < n <= 8 */
#define TRACE_REG_CSI2_SIG2SIO_GR_BASE(n)		(0x067c00 + (n) * 0x20)
#define TRACE_REG_CSI2_SIG2SIO_GR_NUM			9
/* 0 < n <= 8 */
#define TRACE_REG_CSI2_PH3_SIG2SIO_GR_BASE(n)		(0x06f600 + (n) * 0x20)
#define TRACE_REG_CSI2_PH3_SIG2SIO_GR_NUM		9
/* PSYS trace registers - offsets to isys base address */
/* Trace unit base offset */
#define TRACE_REG_PS_TRACE_UNIT_BASE			0x3e0000
/* Trace monitors */
#define TRACE_REG_PS_SPC_EVQ_BASE			0x001000
#define TRACE_REG_PS_SPP0_EVQ_BASE			0x021000
#define TRACE_REG_PS_SPP1_EVQ_BASE			0x031000
#define TRACE_REG_PS_SPF_EVQ_BASE			0x041000
#define TRACE_REG_PS_ISP0_EVQ_BASE			0x1c1000
#define TRACE_REG_PS_ISP1_EVQ_BASE			0x241000
#define TRACE_REG_PS_ISP2_EVQ_BASE			0x2c1000
#define TRACE_REG_PS_ISP3_EVQ_BASE			0x341000
/* GPC blocks */
#define TRACE_REG_PS_SPC_GPC_BASE			0x000800
#define TRACE_REG_PS_SPP0_GPC_BASE			0x020800
#define TRACE_REG_PS_SPP1_GPC_BASE			0x030800
#define TRACE_REG_PS_SPF_GPC_BASE			0x040800
#define TRACE_REG_PS_MMU_GPC_BASE			0x0b0b00
#define TRACE_REG_PS_ISL_GPC_BASE			0x0fe800
#define TRACE_REG_PS_ISP0_GPC_BASE			0x1c0800
#define TRACE_REG_PS_ISP1_GPC_BASE			0x240800
#define TRACE_REG_PS_ISP2_GPC_BASE			0x2c0800
#define TRACE_REG_PS_ISP3_GPC_BASE			0x340800

/* common macros on each platform */
#ifdef CONFIG_VIDEO_INTEL_IPU4
#define IPU_ISYS_UNISPART_IRQ_CSI2(port)		\
	({ typeof(port) __port = (port); \
	__port < IPU_ISYS_MAX_CSI2_LEGACY_PORTS ?	\
	((0x8) << __port) :					\
	(0x800 << (__port - IPU_ISYS_MAX_CSI2_LEGACY_PORTS)); })
#define IPU_PSYS_GPDEV_IRQ_FWIRQ(n)		(BIT(17) << (n))
#endif
#ifdef CONFIG_VIDEO_INTEL_IPU4P
#define IPU_ISYS_UNISPART_IRQ_CSI2(port)		\
	((port) > 0 ? 0x10 : 0x8)
/* bit 20 for fw irqreg0 */
#define IPU_PSYS_GPDEV_IRQ_FWIRQ(n)		(BIT(20) << (n))
#endif
/* IRQ-related registers in PSYS, relative to IPU_xx_PSYS_OFFSET */
#define IPU_REG_PSYS_GPDEV_IRQ_EDGE		0x60200
#define IPU_REG_PSYS_GPDEV_IRQ_MASK		0x60204
#define IPU_REG_PSYS_GPDEV_IRQ_STATUS		0x60208
#define IPU_REG_PSYS_GPDEV_IRQ_CLEAR		0x6020c
#define IPU_REG_PSYS_GPDEV_IRQ_ENABLE		0x60210
#define IPU_REG_PSYS_GPDEV_IRQ_LEVEL_NOT_PULSE	0x60214
/* There are 8 FW interrupts, n = 0..7 */
#define IPU_PSYS_GPDEV_FWIRQ0			0
#define IPU_REG_PSYS_GPDEV_FWIRQ(n)		(4 * (n) + 0x60100)
/* CDC Burst collector thresholds for psys - 4 FIFOs i= 0..3 */
#define IPU_REG_PSYS_CDC_THRESHOLD(i)           (0x60600 + ((i) * 4))

#endif /* IPU_REGS_H */
