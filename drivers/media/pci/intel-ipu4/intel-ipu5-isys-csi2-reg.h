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

#ifndef INTEL_IPU5_ISYS_CSI2_REG_H
#define INTEL_IPU5_ISYS_CSI2_REG_H

#define INTEL_IPU5_CSI_REG_TOP0_RX_A_ADDR_OFFSET		0x68000
#define INTEL_IPU5_CSI_REG_TOP0_RX_B_ADDR_OFFSET		0x68100
#define INTEL_IPU5_CSI_REG_TOP0_CPHY_RX_0_ADDR_OFFSET		0x68200
#define INTEL_IPU5_CSI_REG_TOP0_CPHY_RX_1_ADDR_OFFSET		0x68300

#define INTEL_IPU5_CSI_REG_TOP1_RX_A_ADDR_OFFSET		0x6a000
#define INTEL_IPU5_CSI_REG_TOP1_RX_B_ADDR_OFFSET		0x6a100
#define INTEL_IPU5_CSI_REG_TOP1_CPHY_RX_0_ADDR_OFFSET		0x6a200
#define INTEL_IPU5_CSI_REG_TOP1_CPHY_RX_1_ADDR_OFFSET		0x6a300

#define INTEL_IPU5_CSI_REG_TOP2_RX_A_ADDR_OFFSET		0x6c000
#define INTEL_IPU5_CSI_REG_TOP2_RX_B_ADDR_OFFSET		0x6c100
#define INTEL_IPU5_CSI_REG_TOP2_CPHY_RX_0_ADDR_OFFSET		0x6c200
#define INTEL_IPU5_CSI_REG_TOP2_CPHY_RX_1_ADDR_OFFSET		0x6c300

#define INTEL_IPU5_CSI_REG_RX_ENABLE				0x00
#define INTEL_IPU5_CSI_REG_RX_ENABLE_ENABLE			0x01
#define INTEL_IPU5_CSI_REG_RX_NOF_ENABLED_LANES			0x04
#define INTEL_IPU5_CSI_REG_RX_CTL				0x08
#define INTEL_IPU5_CSI_REG_RX_CONFIG_RELEASE_LP11		0x1
#define INTEL_IPU5_CSI_REG_RX_CONFIG_DISABLE_BYTE_CLK_GATING	0x2
#define INTEL_IPU5_CSI_REG_RX_HBP_TESTMODE_ENABLE		0x0c

#define INTEL_IPU5_CSI_REG_RX_ERROR_HANDLING			0x10
#define INTEL_IPU5_CSI_REG_RX_SYNC_CNTR_SEL			0x14
#define INTEL_IPU5_CSI_REG_RX_SP_IF_CONFIG			0x18
#define INTEL_IPU5_CSI_REG_RX_LP_IF_CONFIG			0x1C

#define INTEL_IPU5_CSI_REG_RX_DLY_CNT_TERMEN_CLANE		0x30
#define INTEL_IPU5_CSI_REG_RX_DLY_CNT_SETTLE_CLANE		0x34
/* 0..3 */
#define INTEL_IPU5_CSI_REG_RX_DLY_CNT_TERMEN_DLANE(n)		(0x38 + (n) * 8)
#define INTEL_IPU5_CSI_REG_RX_DLY_CNT_SETTLE_DLANE(n)		(0x3C + (n) * 8)

/*
* IPU5 related IRQ registers not define here
* Will commit another new patch to implement later soon.
*/

#endif /* INTEL_IPU5_ISYS_CSI2_REG_H */
