/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_INTEL_H
#define __SOF_INTEL_H

/*
 * SHIM registers for BYT, BSW, CHT HSW, BDW
 */

#define SHIM_CSR		(SHIM_OFFSET + 0x00)
#define SHIM_PISR		(SHIM_OFFSET + 0x08)
#define SHIM_PIMR		(SHIM_OFFSET + 0x10)
#define SHIM_ISRX		(SHIM_OFFSET + 0x18)
#define SHIM_ISRD		(SHIM_OFFSET + 0x20)
#define SHIM_IMRX		(SHIM_OFFSET + 0x28)
#define SHIM_IMRD		(SHIM_OFFSET + 0x30)
#define SHIM_IPCX		(SHIM_OFFSET + 0x38)
#define SHIM_IPCD		(SHIM_OFFSET + 0x40)
#define SHIM_ISRSC		(SHIM_OFFSET + 0x48)
#define SHIM_ISRLPESC		(SHIM_OFFSET + 0x50)
#define SHIM_IMRSC		(SHIM_OFFSET + 0x58)
#define SHIM_IMRLPESC		(SHIM_OFFSET + 0x60)
#define SHIM_IPCSC		(SHIM_OFFSET + 0x68)
#define SHIM_IPCLPESC		(SHIM_OFFSET + 0x70)
#define SHIM_CLKCTL		(SHIM_OFFSET + 0x78)
#define SHIM_CSR2		(SHIM_OFFSET + 0x80)
#define SHIM_LTRC		(SHIM_OFFSET + 0xE0)
#define SHIM_HMDC		(SHIM_OFFSET + 0xE8)

#define SHIM_PWMCTRL		0x1000

/*
 * SST SHIM register bits for BYT, BSW, CHT HSW, BDW
 * Register bit naming and functionaility can differ between devices.
 */

/* CSR / CS */
#define SHIM_CSR_RST		(0x1 << 1)
#define SHIM_CSR_SBCS0		(0x1 << 2)
#define SHIM_CSR_SBCS1		(0x1 << 3)
#define SHIM_CSR_DCS(x)		(x << 4)
#define SHIM_CSR_DCS_MASK	(0x7 << 4)
#define SHIM_CSR_STALL		(0x1 << 10)
#define SHIM_CSR_S0IOCS		(0x1 << 21)
#define SHIM_CSR_S1IOCS		(0x1 << 23)
#define SHIM_CSR_LPCS		(0x1 << 31)
#define SHIM_CSR_24MHZ_LPCS \
	(SHIM_CSR_SBCS0 | SHIM_CSR_SBCS1 | SHIM_CSR_LPCS)
#define SHIM_CSR_24MHZ_NO_LPCS	(SHIM_CSR_SBCS0 | SHIM_CSR_SBCS1)
#define SHIM_BYT_CSR_RST	(0x1 << 0)
#define SHIM_BYT_CSR_VECTOR_SEL	(0x1 << 1)
#define SHIM_BYT_CSR_STALL	(0x1 << 2)
#define SHIM_BYT_CSR_PWAITMODE	(0x1 << 3)

/*  ISRX / ISC */
#define SHIM_ISRX_BUSY		(0x1 << 1)
#define SHIM_ISRX_DONE		(0x1 << 0)
#define SHIM_BYT_ISRX_REQUEST	(0x1 << 1)

/*  ISRD / ISD */
#define SHIM_ISRD_BUSY		(0x1 << 1)
#define SHIM_ISRD_DONE		(0x1 << 0)

/* IMRX / IMC */
#define SHIM_IMRX_BUSY		(0x1 << 1)
#define SHIM_IMRX_DONE		(0x1 << 0)
#define SHIM_BYT_IMRX_REQUEST	(0x1 << 1)

/* IMRD / IMD */
#define SHIM_IMRD_DONE		(0x1 << 0)
#define SHIM_IMRD_BUSY		(0x1 << 1)
#define SHIM_IMRD_SSP0		(0x1 << 16)
#define SHIM_IMRD_DMAC0		(0x1 << 21)
#define SHIM_IMRD_DMAC1		(0x1 << 22)
#define SHIM_IMRD_DMAC		(SHIM_IMRD_DMAC0 | SHIM_IMRD_DMAC1)

/*  IPCX / IPCC */
#define	SHIM_IPCX_DONE		(0x1 << 30)
#define	SHIM_IPCX_BUSY		(0x1 << 31)
#define SHIM_BYT_IPCX_DONE	((u64)0x1 << 62)
#define SHIM_BYT_IPCX_BUSY	((u64)0x1 << 63)

/*  IPCD */
#define	SHIM_IPCD_DONE		(0x1 << 30)
#define	SHIM_IPCD_BUSY		(0x1 << 31)
#define SHIM_BYT_IPCD_DONE	((u64)0x1 << 62)
#define SHIM_BYT_IPCD_BUSY	((u64)0x1 << 63)

/* CLKCTL */
#define SHIM_CLKCTL_SMOS(x)	(x << 24)
#define SHIM_CLKCTL_MASK	(3 << 24)
#define SHIM_CLKCTL_DCPLCG	BIT(18)
#define SHIM_CLKCTL_SCOE1	BIT(17)
#define SHIM_CLKCTL_SCOE0	BIT(16)

/* CSR2 / CS2 */
#define SHIM_CSR2_SDFD_SSP0	BIT(1)
#define SHIM_CSR2_SDFD_SSP1	BIT(2)

/* LTRC */
#define SHIM_LTRC_VAL(x)	(x << 0)

/* HMDC */
#define SHIM_HMDC_HDDA0(x)	(x << 0)
#define SHIM_HMDC_HDDA1(x)	(x << 7)
#define SHIM_HMDC_HDDA_E0_CH0	1
#define SHIM_HMDC_HDDA_E0_CH1	2
#define SHIM_HMDC_HDDA_E0_CH2	4
#define SHIM_HMDC_HDDA_E0_CH3	8
#define SHIM_HMDC_HDDA_E1_CH0	SHIM_HMDC_HDDA1(SHIM_HMDC_HDDA_E0_CH0)
#define SHIM_HMDC_HDDA_E1_CH1	SHIM_HMDC_HDDA1(SHIM_HMDC_HDDA_E0_CH1)
#define SHIM_HMDC_HDDA_E1_CH2	SHIM_HMDC_HDDA1(SHIM_HMDC_HDDA_E0_CH2)
#define SHIM_HMDC_HDDA_E1_CH3	SHIM_HMDC_HDDA1(SHIM_HMDC_HDDA_E0_CH3)
#define SHIM_HMDC_HDDA_E0_ALLCH	\
	(SHIM_HMDC_HDDA_E0_CH0 | SHIM_HMDC_HDDA_E0_CH1 | \
	 SHIM_HMDC_HDDA_E0_CH2 | SHIM_HMDC_HDDA_E0_CH3)
#define SHIM_HMDC_HDDA_E1_ALLCH	\
	(SHIM_HMDC_HDDA_E1_CH0 | SHIM_HMDC_HDDA_E1_CH1 | \
	 SHIM_HMDC_HDDA_E1_CH2 | SHIM_HMDC_HDDA_E1_CH3)

/* Audio DSP PCI registers */
#define PCI_VDRTCTL0		0xa0
#define PCI_VDRTCTL1		0xa4
#define PCI_VDRTCTL2		0xa8
#define PCI_VDRTCTL3		0xaC

/* VDRTCTL0 */
#define PCI_VDRTCL0_D3PGD		BIT(0)
#define PCI_VDRTCL0_D3SRAMPGD		BIT(1)
#define PCI_VDRTCL0_DSRAMPGE_SHIFT	12
#define PCI_VDRTCL0_DSRAMPGE_MASK	(0xfffff << PCI_VDRTCL0_DSRAMPGE_SHIFT)
#define PCI_VDRTCL0_ISRAMPGE_SHIFT	2
#define PCI_VDRTCL0_ISRAMPGE_MASK	(0x3ff << PCI_VDRTCL0_ISRAMPGE_SHIFT)

/* VDRTCTL2 */
#define PCI_VDRTCL2_DCLCGE		BIT(1)
#define PCI_VDRTCL2_DTCGE		BIT(10)
#define PCI_VDRTCL2_APLLSE_MASK		BIT(31)

/* PMCS */
#define PCI_PMCS		0x84
#define PCI_PMCS_PS_MASK	0x3

/* PCI registers */
#define PCI_TCSEL			0x44
#define PCI_CGCTL			0x48

/* PCI_CGCTL bits */
#define PCI_CGCTL_MISCBDCGE_MASK	BIT(6)

/* Legacy HDA registers and bits used - widths are variable */
#define SOF_HDA_GCAP			0x0
#define SOF_HDA_GCTL			0x8
/* accept unsol. response enable */
#define SOF_HDA_GCTL_UNSOL		BIT(8)
#define SOF_HDA_LLCH			0x14
#define SOF_HDA_INTCTL			0x20
#define SOF_HDA_INTSTS			0x24
#define SOF_HDA_WAKESTS			0x0E
#define SOF_HDA_WAKESTS_INT_MASK	((1 << 8) - 1)

/* SOF_HDA_GCTL register bist */
#define SOF_HDA_GCTL_RESET		BIT(0)

/* SOF_HDA_INCTL and SOF_HDA_INTSTS regs */
#define SOF_HDA_INT_GLOBAL_EN		BIT(31)
#define SOF_HDA_INT_CTRL_EN		BIT(30)
#define SOF_HDA_INT_ALL_STREAM		0xff

#define SOF_HDA_MAX_CAPS		10
#define SOF_HDA_CAP_ID_OFF		16
#define SOF_HDA_CAP_ID_MASK		(0xFFF << SOF_HDA_CAP_ID_OFF)
#define SOF_HDA_CAP_NEXT_MASK		0xFFFF

#define SOF_HDA_PP_CAP_ID		0x3
#define SOF_HDA_REG_PP_PPCH		0x10
#define SOF_HDA_REG_PP_PPCTL		0x04
#define SOF_HDA_PPCTL_PIE		BIT(31)
#define SOF_HDA_PPCTL_GPROCEN		BIT(30)

#define SOF_HDA_SPIB_CAP_ID		0x4
#define SOF_HDA_DRSM_CAP_ID		0x5

#define SOF_HDA_SPIB_BASE		0x08
#define SOF_HDA_SPIB_INTERVAL		0x08
#define SOF_HDA_SPIB_SPIB		0x00
#define SOF_HDA_SPIB_MAXFIFO		0x04

#define SOF_HDA_PPHC_BASE		0x10
#define SOF_HDA_PPHC_INTERVAL		0x10

#define SOF_HDA_PPLC_BASE		0x10
#define SOF_HDA_PPLC_MULTI		0x10
#define SOF_HDA_PPLC_INTERVAL		0x10

#define SOF_HDA_DRSM_BASE		0x08
#define SOF_HDA_DRSM_INTERVAL		0x08

/* Descriptor error interrupt */
#define SOF_HDA_CL_DMA_SD_INT_DESC_ERR		0x10

/* FIFO error interrupt */
#define SOF_HDA_CL_DMA_SD_INT_FIFO_ERR		0x08

/* Buffer completion interrupt */
#define SOF_HDA_CL_DMA_SD_INT_COMPLETE		0x04

#define SOF_HDA_CL_DMA_SD_INT_MASK \
	(SOF_HDA_CL_DMA_SD_INT_DESC_ERR | \
	SOF_HDA_CL_DMA_SD_INT_FIFO_ERR | \
	SOF_HDA_CL_DMA_SD_INT_COMPLETE)
#define SOF_HDA_SD_CTL_DMA_START		0x02 /* Stream DMA start bit */

/* Intel HD Audio Code Loader DMA Registers */
#define SOF_HDA_ADSP_LOADER_BASE		0x80
#define SOF_HDA_ADSP_DPLBASE			0x70
#define SOF_HDA_ADSP_DPUBASE			0x74
#define SOF_HDA_ADSP_DPLBASE_ENABLE		0x01

/* Stream Registers */
#define SOF_HDA_ADSP_REG_CL_SD_CTL		0x00
#define SOF_HDA_ADSP_REG_CL_SD_STS		0x03
#define SOF_HDA_ADSP_REG_CL_SD_LPIB		0x04
#define SOF_HDA_ADSP_REG_CL_SD_CBL		0x08
#define SOF_HDA_ADSP_REG_CL_SD_LVI		0x0C
#define SOF_HDA_ADSP_REG_CL_SD_FIFOW		0x0E
#define SOF_HDA_ADSP_REG_CL_SD_FIFOSIZE		0x10
#define SOF_HDA_ADSP_REG_CL_SD_FORMAT		0x12
#define SOF_HDA_ADSP_REG_CL_SD_FIFOL		0x14
#define SOF_HDA_ADSP_REG_CL_SD_BDLPL		0x18
#define SOF_HDA_ADSP_REG_CL_SD_BDLPU		0x1C

/* CL: Software Position Based FIFO Capability Registers */
#define SOF_DSP_REG_CL_SPBFIFO \
	(SOF_HDA_ADSP_LOADER_BASE + 0x20)
#define SOF_HDA_ADSP_REG_CL_SPBFIFO_SPBFCH	0x0
#define SOF_HDA_ADSP_REG_CL_SPBFIFO_SPBFCCTL	0x4
#define SOF_HDA_ADSP_REG_CL_SPBFIFO_SPIB	0x8
#define SOF_HDA_ADSP_REG_CL_SPBFIFO_MAXFIFOS	0xc

/* Stream Number */
#define SOF_HDA_CL_SD_CTL_STREAM_TAG_SHIFT	20
#define SOF_HDA_CL_SD_CTL_STREAM_TAG_MASK \
	(0xf << SOF_HDA_CL_SD_CTL_STREAM_TAG_SHIFT)

enum skl_cl_dma_wake_states {
	APL_CL_DMA_STATUS_NONE = 0,
	APL_CL_DMA_BUF_COMPLETE,
	APL_CL_DMA_ERR,	/* TODO: Expand the error states */
};

struct stream_sample_format {
	u32 sample_rate;
	u8 code;
};

static struct stream_sample_format sample_format[] = {
	{8000, 0x5},
	{9600, 0x4},
	{11025, 0x43},
	{16000, 0x2},
	{22050, 0x41},
	{24000, 0x1},
	{32000, 0xa},
	{44100, 0x40},
	{48000, 0x0},
	{88200, 0x48},
	{96000, 0x8},
	{144000, 0x10},
	{176400, 0x58},
	{192000, 0x18},
};

static inline uint8_t get_sample_code(uint32_t sample_rate)
{
	int i;

	for (i = 0; i < sizeof(sample_format)
		/ sizeof(struct stream_sample_format); i++) {
		if (sample_format[i].sample_rate == sample_rate)
			return sample_format[i].code;
	}

	return 0; /* use 48KHz if not found */
}

struct stream_bits_format {
	u32 bits;
	u8 code;
};

static struct stream_bits_format bits_format[] = {
	{8, 0x0},
	{16, 0x1},
	{20, 0x2},
	{24, 0x3},
	{32, 0x4},
};

/* get code for BITS(Bits per Sample) */
static inline uint8_t get_bits_code(uint32_t bits)
{
	int i;

	for (i = 0; i < sizeof(bits_format)
		/ sizeof(struct stream_bits_format); i++) {
		if (bits_format[i].bits == bits)
			return bits_format[i].code;
	}

	return 1; /* use 16bits format if not found */
}

#endif
