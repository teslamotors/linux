/*
 * TRAV CSIS camera interface driver
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#ifndef _TRAV_CSIS_H_
#define _TRAV_CSIS_H_

#define CMU_CAM_CSI    0	//0x12610000

#define PLL_LOCKTIME_PLL_CAM_CSI               (CMU_CAM_CSI + (0x0000))
#define PLL_CON0_PLL_CAM_CSI                   (CMU_CAM_CSI + (0x0100))
#define CLK_CON_DIV_DIV_CAM_CSI0_ACLK          (CMU_CAM_CSI + (0x1800))
#define CLK_CON_DIV_DIV_CAM_CSI1_ACLK          (CMU_CAM_CSI + (0x1804))
#define CLK_CON_DIV_DIV_CAM_CSI2_ACLK          (CMU_CAM_CSI + (0x1808))
#define CLK_CON_DIV_DIV_CAM_CSI_BUSD           (CMU_CAM_CSI + (0x180c))
#define CLK_CON_DIV_DIV_CAM_CSI_BUSP           (CMU_CAM_CSI + (0x1810))

#define MIPI_PHY_M4S4_CONTROL			0x70C
#define MIPI_PHY_M4S4_MOD_CONTROL		0x710

#define PLL_CON0_LOCKTIME_VAL	(0Xff)
#define PLL_CON0_LOCKTIME_EN	(1 << 31)
#define PLL_CON0_MUX_SEL	(0x01 << 4)
#define PLL_CON0_DIV_P_VAL	(0x0C << 8)
#define PLL_CON0_DIV_M_VAL	(0x215 << 16)
#define PLL_CON0_DIV_S_VAL	(~(0x03))

#define PLL_CON_BUSD_DIV_RATIO		(0x01)
#define PLL_CON_CSI0_ACLK_DIV_RATIO	(0x02)
#define PLL_CON_CSI1_ACLK_DIV_RATIO	(0x02)
#define PLL_CON_CSI2_ACLK_DIV_RATIO	(0x02)
#define PLL_CON_BUSP_DIV_RATIO		(0x03)

#define TRAV_CSI_1_6_Gbps
/*
#define TRAV_CSI_2_1_Gbps
#define TRAV_CSI_1_5_Gbps
#define TRAV_CSI_1_0_Gbps
#define TRAV_CSI_800_Mbps
*/

#if	defined (TRAV_CSI_2_1_Gbps)
#define S_CLKSETTLECTL_VAL	(0x00)
#define S_HSSETTLECTL_VAL	(0x2E)
#elif	defined (TRAV_CSI_1_6_Gbps)
#define S_CLKSETTLECTL_VAL	(0x00)
#define S_HSSETTLECTL_VAL	(0x23)
#elif	defined (TRAV_CSI_1_5_Gbps)
#define S_CLKSETTLECTL_VAL	(0x00)
#define S_HSSETTLECTL_VAL	(0x21)
#elif	defined (TRAV_CSI_1_0_Gbps)
#define S_CLKSETTLECTL_VAL	(0x00)
#define S_HSSETTLECTL_VAL	(0x16)
#elif	defined (TRAV_CSI_800_Mbps)
#define S_CLKSETTLECTL_VAL	(0x00)
#define S_HSSETTLECTL_VAL	(0x11)
#else
#define S_CLKSETTLECTL_VAL     (0x01)
#define S_HSSETTLECTL_VAL      (0x0f)
#endif

#define	HSYNC_LINTV		(0x20)
#define CLKGATE_TAIL_VAL	(0x07)
#define DMA_CLK_GATE_TRAIL	(0x07)

#define CSIS_NUM_INPUT		1
#define CSIS_NUM_VC		1
#define CSIS_NUM_CH		4
#define CSIS_NUM_DATA_LANE	4
#define CSIS_NUM_MIN_CH		1
#define CSIS_NUM_DMA_OUT_CH	8

#define CSIS_WMIN	48
#define CSIS_WMAX	1920
#define CSIS_HMIN	32
#define CSIS_HMAX	1200
#define CSIS_WALIGN	2
#define CSIS_HALIGN	0
#define CSIS_SALIGN	0

#define V4L2_CID_USER_CSIS_NO_OF_LANE           (V4L2_CID_USER_TRAV_CSIS_BASE + 0)
#define V4L2_CID_USER_CSIS_LANE_SPEED           (V4L2_CID_USER_TRAV_CSIS_BASE + 1)
#define V4L2_CID_USER_CSIS_TPS_PATTERN          (V4L2_CID_USER_TRAV_CSIS_BASE + 2)
#define V4L2_CID_USER_CSIS_ENABLE_HDCP          (V4L2_CID_USER_TRAV_CSIS_BASE + 3)
#define V4L2_CID_USER_CSIS_ENABLE_DSC           (V4L2_CID_USER_TRAV_CSIS_BASE + 4)
#define V4L2_CID_USER_CSIS_MST_CONFIG           (V4L2_CID_USER_TRAV_CSIS_BASE + 5)
#define V4L2_CID_USER_CSIS_POWER_STATE          (V4L2_CID_USER_TRAV_CSIS_BASE + 6)
#define V4L2_CID_CSIS_REG_CTRL			(V4L2_CID_USER_TRAV_CSIS_BASE + 7)

#define CSIS_DMA_VID_BITS_WIDTH_8                      (0x0 << 0)
#define CSIS_DMA_VID_BITS_WIDTH_8                       (0x0 << 0)
#define CSIS_DMA_VID_BITS_WIDTH_10                      (0x1 << 0)
#define CSIS_DMA_VID_BITS_WIDTH_12                      (0x10 << 0)
#define CSIS_DMA_VID_BITS_WIDTH_14                      (0x11 << 0)
#define CSIS_DMA_VID_BITS_WIDTH_16                      (0x100 << 0)
#define CSIS_DMA_VID_BITS_WIDTH_MASK                    ~(0xfff << 0)

#define CSIS_DMA_CH0_MASK	0x4111U // Samsung MIPI CSI-2-Slave V4.3.0 section 6.7

struct csis_fmt {
	char name[32];
	u32 fourcc;
	u32 colorspace;
	u32 code;
	u32 depth;
};

typedef enum {
	CSIS_1ST_CH = 0,
	CSIS_2ND_CH = 1,
	CSIS_3RD_CH = 2,
	CSIS_4TH_CH = 3,
} CSIS_CHANNEL;

typedef enum {
	DATALANE0 = 0,
	DATALANE1,
	DATALANE2,
	DATALANE3,
	DATALANE4
} CSIS_DATA_LANE_NUM;

typedef enum {
	VCI_disable,
	VCI_enable
} CSIS_INTERLEAVE_for_VCI;

typedef struct {
	unsigned int CSIS_INTV_Lintv;           // 0x0~0x3F
} CSIS_INTV_SETTING;

typedef enum {
	VC_CH0,
	VC_CH1,
	VC_CH2,
	VC_CH3,
	VC_MAX_NUM,
} CSIS_VC;

typedef enum {
	VC_DT_NONE = 0,
	DT_ONLY,
	VC_ONLY,
	VC_DT_BOTH
} CSIS_INTERLEAVE;

typedef enum {
	MIPI_24BIT_ALIGN,
	MIPI_32BIT_ALIGN
} MIPI_DATA_ALIGN;

typedef enum {
	EXPECT_FRAME_STATE_NONE,
	EXPECT_FRAME_START,
	EXPECT_FRAME_END
} CSIS_CTX_SW_STATE;

typedef enum {
	CSIS_VERSION            = 0x0000,
	CSIS_CMN_CTRL           = 0x0004,
	CSIS_CLK_CTRL           = 0x0008,
	CSIS_INT_MSK0           = 0x0010,
	CSIS_INT_SRC0           = 0x0014,
	CSIS_INT_MSK1           = 0x0018,
	CSIS_INT_SRC1           = 0x001C,
	DPHY_STATUS             = 0x0020,
	DPHY_CMN_CTRL           = 0x0024,
	DPHY_BCTRL_L            = 0x0030,
	DPHY_BCTRL_H            = 0x0034,
	DPHY_SCTRL_L            = 0x0038,
	DPHY_SCTRL_H            = 0x003C,
	ISP_CONFIG_CH0          = 0x0040,
	ISP_RESOL_CH0           = 0x0044,
	ISP_SYNC_CH0            = 0x0048,
	ISP_CONFIG_CH1          = 0x0050,
	ISP_RESOL_CH1           = 0x0054,
	ISP_SYNC_CH1            = 0x0058,
	ISP_CONFIG_CH2          = 0x0060,
	ISP_RESOL_CH2           = 0x0064,
	ISP_SYNC_CH2            = 0x0068,
	ISP_CONFIG_CH3          = 0x0070,
	ISP_RESOL_CH3           = 0x0074,
	ISP_SYNC_CH3            = 0x0078,
	SDW_CONFIG_CH0          = 0x0080,
	SDW_RESOL_CH0           = 0x0084,
	SDW_SYNC_CH0            = 0x0088,
	SDW_CONFIG_CH1          = 0x0090,
	SDW_RESOL_CH1           = 0x0094,
	SDW_SYNC_CH1            = 0x0098,
	SDW_CONFIG_CH2          = 0x00A0,
	SDW_RESOL_CH2           = 0x00A4,
	SDW_SYNC_CH2            = 0x00A8,
	SDW_CONFIG_CH3          = 0x00B0,
	SDW_RESOL_CH3           = 0x00b4,
	SDW_SYNC_CH3            = 0x00B8,
	DBG_CTRL                = 0x00C0,
	DBG_INTR_MSK            = 0x00C4,
	DBG_INTR_SRC            = 0x00C8,
	FRM_CNT_CH0             = 0x0100,
	FRM_CNT_CH1             = 0x0104,
	FRM_CNT_CH2             = 0x0108,
	FRM_CNT_CH3             = 0x010C,
	LINE_INTR_CH0           = 0x0110,
	LINE_INTR_CH1           = 0x0114,
	LINE_INTR_CH2           = 0x0118,
	LINE_INTR_CH3           = 0x011C,
	VC_PASSING		= 0x0120,
	CSIS_INT_MSK0_CLONE     = 0x0910,
	CSIS_INT_SRC0_CLONE     = 0x0914,
	CSIS_INT_MSK1_CLONE     = 0x0918,
	CSIS_INT_SRC1_CLONE     = 0x091C,
	DBG_INTR_MSK_CLONE      = 0x09C4,
	DBG_INTR_SRC_CLONE      = 0x09C8,
} CSIS_SFR_OFFSET;

typedef enum {
	DMA0_CTRL               = 0x0000,
	DMA0_FMT                = 0x0004,
	DMA0_SKIP               = 0x0008,
	DMA0_ADDR1              = 0x0010,
	DMA0_ADDR2              = 0x0014,
	DMA0_ADDR3              = 0x0018,
	DMA0_ADDR4              = 0x001C,
	DMA0_ADDR5              = 0x0020,
	DMA0_ADDR6              = 0x0024,
	DMA0_ADDR7              = 0x0028,
	DMA0_ADDR8              = 0x002C,
	DMA0_ACT_CTRL           = 0x0030,
	DMA0_ACT_FMT            = 0x0034,
	DMA0_ACT_SKIP           = 0x0038,
	DMA0_BYTE_CNT           = 0x0040,
	DMA1_CTRL               = 0x0100,
	DMA1_FMT                = 0x0104,
	DMA1_SKIP               = 0x0108,
	DMA1_ADDR1              = 0x0110,
	DMA1_ADDR2              = 0x0114,
	DMA1_ADDR3              = 0x0118,
	DMA1_ADDR4              = 0x011C,
	DMA1_ADDR5              = 0x0120,
	DMA1_ADDR6              = 0x0124,
	DMA1_ADDR7              = 0x0128,
	DMA1_ADDR8              = 0x012C,
	DMA1_ACT_CTRL           = 0x0130,
	DMA1_ACT_FMT            = 0x0134,
	DMA1_ACT_SKIP           = 0x0138,
	DMA1_BYTE_CNT           = 0x0140,
	DMA2_CTRL               = 0x0200,
	DMA2_FMT                = 0x0204,
	DMA2_SKIP               = 0x0208,
	DMA2_ADDR1              = 0x0210,
	DMA2_ADDR2              = 0x0214,
	DMA2_ADDR3              = 0x0218,
	DMA2_ADDR4              = 0x021C,
	DMA2_ADDR5              = 0x0220,
	DMA2_ADDR6              = 0x0224,
	DMA2_ADDR7              = 0x0228,
	DMA2_ADDR8              = 0x022C,
	DMA2_ACT_CTRL           = 0x0230,
	DMA2_ACT_FMT            = 0x0234,
	DMA2_ACT_SKIP           = 0x0238,
	DMA2_BYTE_CNT           = 0x0240,
	DMA3_CTRL               = 0x0300,
	DMA3_FMT                = 0x0304,
	DMA3_SKIP               = 0x0308,
	DMA3_ADDR1              = 0x0310,
	DMA3_ADDR2              = 0x0314,
	DMA3_ADDR3              = 0x0318,
	DMA3_ADDR4              = 0x031C,
	DMA3_ADDR5              = 0x0320,
	DMA3_ADDR6              = 0x0324,
	DMA3_ADDR7              = 0x0328,
	DMA3_ADDR8              = 0x032C,
	DMA3_ACT_CTRL           = 0x0330,
	DMA3_ACT_FMT            = 0x0334,
	DMA3_ACT_SKIP           = 0x0338,
	DMA3_BYTE_CNT           = 0x0340,
	DMA_CMN_CTRL            = 0x0400,
	DMA_ERR_CODE            = 0x0404,
	DMA_CLK_CTRL            = 0x0408,
	DMA_AWUSER              = 0x040C,
	DBG_AXIM_INFO           = 0x0440,
	DBG_TRXFIFO_INFO        = 0x0444,
	DBG_DMAFIFO_INFO        = 0x0448,
	CSIS_NONIMG             = 0x1000,
} CSIS_DMA_SFR_OFFSET;

enum{
	SW_RESETN_DPHY		= 0x040C,
} CSIS_SYSREG_SFR_OFFSET;

#define DPHYON_DATA3        (1 << 4)
#define DPHYON_DATA2        (1 << 3)
#define DPHYON_DATA1        (1 << 2)
#define DPHYON_DATA0        (1 << 1)
#define DPHYON_CLK          (1 << 0)

#endif
