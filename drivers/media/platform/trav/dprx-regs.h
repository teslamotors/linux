#ifndef _DPRX_LINK_SFR_H_
#define _DPRX_LINK_SFR_H_

#define DPRX_NUM_MST						8
#define DPRX_NUM_FRAME_ADDR					8

#define SLAVE_ID_TOP_REG					0
#define SLAVE_ID_CONTROL_REG					1
#define SLAVE_ID_MAINLINK_REG_0					2
#define SLAVE_ID_MAINLINK_REG_1					3
#define SLAVE_ID_MAINLINK_REG_2					4
#define SLAVE_ID_MAINLINK_REG_3					5
#define SLAVE_ID_MAINLINK_REG_4					6
#define SLAVE_ID_MAINLINK_REG_5					7
#define SLAVE_ID_MAINLINK_REG_6					8
#define SLAVE_ID_MAINLINK_REG_7					9
#define SLAVE_ID_SERDES_REG					10

#define SLAVE_ID_HDCP_22_REG					0

#define dprx_cfg_read(dev, slave_id, offset)			readl((dev->base) + ((slave_id << 12) | offset))
#define dprx_cfg_write(dev, slave_id, offset, wdata)		writel((wdata), (dev->base) + ((slave_id << 12) | (offset)))

#define dprx_cfg_read_auth(id, slave_id, offset)		readl((dev->base) + ((slave_id << 12) | offset))
#define dprx_cfg_write_auth(id, slave_id, offset, wdata)	writel(wdata, (dev->base) + ((slave_id << 12) | offset))

typedef enum {
	LS_162,
	LS_270,
	LS_540,
	LS_810,
} link_rate;

typedef enum {
	LT_PATTERN_1 = 1,
	LT_PATTERN_2,
	LT_PATTERN_3,
	LT_PATTERN_4 = 7,
} tps_pattern;

/* TOP_REG */
#define DPRX_ADDR_PWD_CTRL_0					0x000
#define DPRX_PWD_ALL						(0x1 << 6)
#define DPRX_PWD_HDCP2						(0x1 << 5)
#define DPRX_PWD_AUX_CH						(0x1 << 4)
#define DPRX_PWD_LANE3						(0x1 << 3)
#define DPRX_PWD_LANE2						(0x1 << 2)
#define DPRX_PWD_LANE1						(0x1 << 1)
#define DPRX_PWD_LANE0						(0x1 << 0)

#define DPRX_ADDR_PWD_CTRL_1					0x004
#define DPRX_PWD_MAINLINK_7					(1 << 7)
#define DPRX_PWD_MAINLINK_6					(1 << 6)
#define DPRX_PWD_MAINLINK_5					(1 << 5)
#define DPRX_PWD_MAINLINK_4					(1 << 4)
#define DPRX_PWD_MAINLINK_3					(1 << 3)
#define DPRX_PWD_MAINLINK_2					(1 << 2)
#define DPRX_PWD_MAINLINK_1					(1 << 1)
#define DPRX_PWD_MAINLINK_0					(1 << 0)

#define DPRX_ADDR_RST_SEL					0x010
#define DPRX_MAINLINK_AUTO_RST					(0x1 << 6)
#define DPRX_MAIN_VID_CHANGE_RST_EN				(0x1 << 5)
#define DPRX_HDCP2_AUTO_RST					(0x1 << 4)
#define DPRX_MST_EXT_AUTO_RST					(0x1 << 3)
#define DPRX_RXTOP_AUTP_RST					(0x1 << 2)
#define DPRX_NULL_MTP_RST_EN					(0x1 << 1)
#define DPRX_ALIGN_STATUS_RST_EN				(0x1 << 0)

#define DPRX_ADDR_RST_CTRL_0					0x018
#define DPRX_RST_SW_RESET					(0x1 << 1)
#define DPRX_RST_LOGIC_RESET					(0x1 << 0)

#define DPRX_ADDR_RST_CTRL_1					0x01C
#define DPRX_RST_SERDESCTRL_RESET				(0x1 << 5)
#define DPRX_RST_FEC_DEC_RESET					(0x1 << 4)
#define DPRX_RST_HDCP2_RESET					(0x1 << 3)
#define DPRX_RST_MST_EXT_RESET					(0x1 << 2)
#define DPRX_RST_RXTOP_RESET					(0x1 << 1)
#define DPRX_RST_AUX_CH_RESET					(0x1 << 0)

#define DPRX_ADDR_RST_CTRL_2					0x020
#define DPRX_RST_MAINLINK_7_RST					(0x1 << 7)
#define DPRX_RST_MAINLINK_6_RST					(0x1 << 6)
#define DPRX_RST_MAINLINK_5_RST					(0x1 << 5)
#define DPRX_RST_MAINLINK_4_RST					(0x1 << 4)
#define DPRX_RST_MAINLINK_3_RST					(0x1 << 3)
#define DPRX_RST_MAINLINK_2_RST					(0x1 << 2)
#define DPRX_RST_MAINLINK_1_RST					(0x1 << 1)
#define DPRX_RST_MAINLINK_0_RST					(0x1 << 0)

#define DPRX_ADDR_MISC_CTRL					0x030
#define DPRX_REF_CLK_SEL_MASK					~(0x1 << 2)
#define DPRX_REF_CLK_SEL_24MHZ					(0x0 << 2)
#define DPRX_REF_CLK_SEL_27MHZ					(0x1 << 2)
#define DPRX_RX_CLK_SEL_MASK					~(0x3 << 0)
#define DPRX_RX_CLK_SEL_LANE0					(0x0 << 0)
#define DPRX_RX_CLK_SEL_LANE1					(0x1 << 0)
#define DPRX_RX_CLK_SEL_LANE2					(0x2 << 0)
#define DPRX_RX_CLK_SEL_LANE3					(0x3 << 0)

#define DPRX_ADDR_SW_INTR_CTRL					0x044
#define DPRX_SOFT_INTR						(0x1 << 0)

#define DPRX_ADDR_INTR_0					0x050
#define DPRX_HPD_INTR						(0x1 << 2)
#define DPRX_SERDES_INTR					(0x1 << 1)
#define DPRX_DPIP_INTR						(0x1 << 0)

#define DPRX_ADDR_INTR_1					0x054
#define DPRX_MAINLINK_7_INTR					(0x1 << 7)
#define DPRX_MAINLINK_6_INTR					(0x1 << 6)
#define DPRX_MAINLINK_5_INTR					(0x1 << 5)
#define DPRX_MAINLINK_4_INTR					(0x1 << 4)
#define DPRX_MAINLINK_3_INTR					(0x1 << 3)
#define DPRX_MAINLINK_2_INTR					(0x1 << 2)
#define DPRX_MAINLINK_1_INTR					(0x1 << 1)
#define DPRX_MAINLINK_0_INTR					(0x1 << 0)

#define DPRX_ADDR_INTR_2					0x058
#define DPRX_HOT_PLUG_DETECT_INT				(0x1 << 3)
#define DPRX_HPD_CHANGE_INT					(0x1 << 2)
#define DPRX_HPD_LOST_INT					(0x1 << 1)
#define DPRX_HPD_PLUG_INT					(0x1 << 0)

#define DPRX_ADDR_INTR_0_MASK					0x060
#define DPRX_ADDR_INTR_1_MASK					0x064
#define DPRX_ADDR_INTR_2_MASK					0x068

#define DPRX_ADDR_DP_HPD_DEGLITCH_L				0x070
#define DPRX_HPD_DEGLITCH_L(_v)					((_v & 0xff) << 0)

#define DPRX_ADDR_DP_HPD_DEGLITCH_H				0x074
#define DPRX_FORCE_HOT_PLUG_DETECET				(0x1 << 7)
#define DPRX_HOT_PLUG_DETECT_CONTROL				(0x1 << 6)
#define DPRX_HPD_DEGLITCH_H(_v)					((_v & 0x3f) << 0)

#define DPRX_ADDR_DP_HPD_DEBUG					0x078
#define DPRX_HPD						(0x1 << 1)
#define DPRX_HOT_PLUG_DETECT					(0x1 << 0)

/* CONTROL_REG */
#define DPRX_ADDR_LINK_BW_SET					0x000
#define DPRX_LINK_BW_SET_1_62					6
#define DPRX_LINK_BW_SET_2_70					10
#define DPRX_LINK_BW_SET_5_40					20
#define DPRX_LINK_BW_SET_8_10					30

#define DPRX_ADDR_LANE_COUNT_SET				0x004
#define DPRX_ENHANCED_FRAME_EN					(0x1 << 7)
#define DPRX_LANE_COUNT_SET(_n)					(_n << 0)

#define DPRX_ADDR_RCD_PN_CONVERTE				0x008
#define DPRX_ADDR_LANE_USE_AS					0x00C
#define DPRX_LANE_USE_AS_3(_n)					(_n << 6)
#define DPRX_LANE_USE_AS_2(_n)					(_n << 4)
#define DPRX_LANE_USE_AS_1(_n)					(_n << 2)
#define DPRX_LANE_USE_AS_0(_n)					(_n << 6)

#define DPRX_ADDR_SCRAMBLE_CTRL					0x030
#define DPRX_POL_MAKER_SEL_SW					(0x1 << 3)
#define DPRX_POL_MAKER_SEL_LOGIC				(0x0 << 3)
#define DPRX_UPDATE_SEL						(0x1 << 2)
#define DPRX_HARD_LOST_VALID_LESS_THAN_1			(0x1 << 1)
#define DPRX_HARD_LOST_VALID_LESS_THAN_2			(0x0 << 1)
#define DPRX_ENHANCE_MODE_DETECT_FROM_HW			(0x1 << 0)
#define DPRX_ENHANCE_MODE_FROM_DPCD_REG				(0x0 << 0)

#define DPRX_ADDR_DOWNSPREAD_CTRL				0x034
#define DPRX_MSA_TIMING_PAR_SRC_TIMING				(0x0 << 6)
#define DPRX_MSA_TIMING_PAR_INVALID_SRC_TIMING			(0x1 << 6)

#define DPRX_ADDR_SYSTEM_CTRL					0x038
#define DPRX_INV_SEED						(0x1 << 2)
#define DPRX_DESCRAMBLE_AUTOSYNC_EN				(0x1 << 1)
#define DPRX_CHK_DU_EN						(0x1 << 0)

#define DPRX_ADDR_SYSTEM_STATUS					0x040
#define DPRX_EN_FRM_DETECT					(1 << 0)

#define DPRX_ADDR_EDP_CONFIG_SET				0x0F0
#define DPRX_FRAMING_CHANGE_ENABLE				(0x1 << 1)
#define DPRX_ALTERNATE_SCRAMBLER_RESET_ENABLE_FFFE		(0x1 << 0)
#define DPRX_ALTERNATE_SCRAMBLER_RESET_ENABLE_FFFF		(0x0 << 0)

#define DPRX_ADDR_AUX_CH_CTRL_0					0x100
#define DPRX_DEFER_CTRL_EN					(0x1 << 7)
#define DPRX_DEFER_COUNT(_v)					((_v & 0x7f) << 0)

#define DPRX_ADDR_AUX_CH_CTRL_1					0x104
#define DPRX_REPLY_TIMER_DONE_THRD_11_8(_v)			((_v & 0xf) << 4)
#define DPRX_AUX_PN_INV						(0x1 << 3)
#define DPRX_AUX_RETRY_TIMER(_v)				((_v & 0x7) << 0)

#define DPRX_ADDR_AUX_CH_CTRL_2					0x108
#define DPRX_REPLY_TIMER_DONE_THRD_7_0(_v)			((_v & 0xff) << 0)

#define DPRX_ADDR_AUX_STATUS_0					0x110
#define DPRX_AUX_BUSY						(1 << 1)
#define DPRX_AUX_CMD_STATUS					(0xf << 0)
#define DPRX_AUX_NACK_ERROR					(0x1 << 0)
#define DPRX_AUX_TIMEOUT_ERROR					(0x2 << 0)
#define DPRX_AUX_UNKNOWN_ERROR					(0x3 << 0)
#define DPRX_AUX_MUCH_DEFER_ERROR				(0x4 << 0)
#define DPRX_AUX_TX_SHORT_ERROR					(0x5 << 0)
#define DPRX_AUX_RX_SHORT_ERROR					(0x6 << 0)
#define DPRX_AUX_NACK_WITHOUT_M_ERROR				(0x7 << 0)
#define DPRX_AUX_I2C_NACK_ERROR					(0x8 << 0)

#define DPRX_ADDR_AUX_STATUS_1					0x114
#define DPRX_AUX_ERR_NUM					(0xff << 0)

#define DPRX_ADDR_AUX_RX_COMM					0x120
#define DPRX_AUX_RX_COMM					(0xff << 0)

#define DPRX_ADDR_REG_AUX_CMD_LEN				0x130
#define DPRX_AUX_LENGTH(_v)					(_v << 4)
#define DPRX_AUX_COMM(_v)					(_v << 0)

#define DPRX_ADDR_REG_AUX_ADDR_0				0x134
#define DPRX_AUX_ADDR_7_0(_v)					(_v << 0)

#define DPRX_ADDR_REG_AUX_ADDR_1				0x138
#define DPRX_AUX_ADDR_15_8(_v)					(_v << 0)

#define DPRX_ADDR_REG_AUX_ADDR_2				0x13C
#define DPRX_AUX_ADDR_19_16(_v)					(_v << 0)
#define DPRX_AUX_ADDRESS_ONLY_COMM				(0x1 << 4)
#define DPRX_AUX_NORMAL_COMM					(0x0 << 4)

#define DPRX_ADDR_REG_AUX_CTRL_1				0x140
#define DPRX_AUX_OP_EN						(0x1 << 0)

#define DPRX_ADDR_REG_AUX_BUF_CLR				0x150
#define DPRX_AUX_BUF_CLR					(0x1 << 0)

#define DPRX_ADDR_REG_AUX_BUF_DATA				0x1A0
#define DPRX_AUX_BUF_HAVE_DATA					(0x1 << 4)
#define DPRX_AUX_BUF_DATA_NUM					(0xf << 0)

#define DPRX_ADDR_CH_TEST_CTRL					0x1B0
#define DPRX_AUX_DATA_FORCE					(0x1 << 4)
#define DPRX_AUX_SEND_0_1_EN					(0x1 << 3)
#define DPRX_AUX_TEST_MODE					(0x1 << 2)
#define DPRX_AUX_NORMAL_MODE					(0x0 << 2)
#define DPRX_AUX_T_TEST						(0x1 << 1)
#define DPRX_AUX_EN_TEST					(0x1 << 0)

#define DPRX_ADDR_CH_TEST_DBG					0x1B4
#define DPRX_AUX_CH_DATA_IN					(0x1 << 4)

#define DPRX_ADDR_REG_AUX_BUFFER(_n)				(0x1C0 + (_n * 4))

#define DPRX_ADDR_LINK_QUAL_LANE0_SET				0x200
#define DPRX_ADDR_LINK_QUAL_LANE1_SET				0x204
#define DPRX_ADDR_LINK_QUAL_LANE2_SET				0x208
#define DPRX_ADDR_LINK_QUAL_LANE3_SET				0x20C
#define DPRX_LINK_QUAL_NOT_TRANSMITTED				(0x0 << 0)
#define DPRX_LINK_QUAL_D10_2_TEST_PATTERN			(0x1 << 0)
#define DPRX_LINK_QUAL_SYM_ERR_MEASURE_PATTERN			(0x2 << 0)
#define DPRX_LINK_QUAL_80_BIT_CUSTOM_PATTERN			(0x4 << 0)
#define DPRX_LINK_QUAL_CP2520_PATTERN_1				(0x5 << 0)
#define DPRX_LINK_QUAL_CP2520_PATTERN_2				(0x6 << 0)
#define DPRX_LINK_QUAL_CP2520_PATTERN_3				(0x7 << 0)

#define DPRX_ADDR_TRAINING_PATTERN_SET				0x300
#define DPRX_SYMBOL_ERROR_CNT_BOTH				(0x0 << 6)
#define DPRX_SYMBOL_ERROR_CNT_DISPARITY_ONLY			(0x1 << 6)
#define DPRX_SYMBOL_ERROR_CNT_ILLEGAL_SYM_ONLY			(0x2 << 6)

#define DPRX_SCRAMBLING_DISABLE					(0x1 << 5)

#define DPRX_TRAINING_PATTERN_SEL_MASK				~(0xf << 0)
#define DPRX_NO_TRAINING					(0x0 << 0)
#define DPRX_LINK_TRAINING_PATTERN_1				(0x1 << 0)
#define DPRX_LINK_TRAINING_PATTERN_2				(0x2 << 0)
#define DPRX_LINK_TRAINING_PATTERN_3				(0x3 << 0)
#define DPRX_LINK_TRAINING_PATTERN_4				(0x7 << 0)

#define DPRX_ADDR_TX_SWING_EMP_REQ				0x304
#define DPRX_TX_SWING_CHK_EN					(0x1 << 6)
#define DPRX_TX_SWING_SET(_v)					((_v & 0x3) << 3)
#define DPRX_TX_EMP_REQ_EN					(0x1 << 2)
#define DPRX_TX_EMP_REQ(_v)					((_v & 0x3) << 0)

#define DPRX_ADDR_TRAINING_LANE0_SET				0x310
#define DPRX_ADDR_TRAINING_LANE1_SET				0x314
#define DPRX_ADDR_TRAINING_LANE2_SET				0x318
#define DPRX_ADDR_TRAINING_LANE3_SET				0x31C
#define DPRX_MAX_PRE_EMPHASIS_REACHED				(0x1 << 5)
#define DPRX_PRE_EMPHASIS_SET_LEVEL0				(0x0 << 3)
#define DPRX_PRE_EMPHASIS_SET_LEVEL1				(0x1 << 3)
#define DPRX_PRE_EMPHASIS_SET_LEVEL2				(0x2 << 3)
#define DPRX_PRE_EMPHASIS_SET_LEVEL3				(0x3 << 3)
#define DPRX_MAX_CURRENT_REACHED				(0x1 << 2)
#define DPRX_VOLTAGE_SWING_LEVEL_0				(0x0 << 0)
#define DPRX_VOLTAGE_SWING_LEVEL_1				(0x1 << 0)
#define DPRX_VOLTAGE_SWING_LEVEL_2				(0x2 << 0)
#define DPRX_VOLTAGE_SWING_LEVEL_3				(0x3 << 0)

#define DPRX_ADDR_LANE0_1_STATUS				0x320
#define DPRX_LANE1_SYMBOL_LOCKED				(0x1 << 6)
#define DPRX_LANE1_CHANNEL_EQ_DONE				(0x1 << 5)
#define DPRX_LANE1_CR_DONE					(0x1 << 4)
#define DPRX_LANE0_SYMBOL_LOCKED				(0x1 << 2)
#define DPRX_LANE0_CHANNEL_EQ_DONE				(0x1 << 1)
#define DPRX_LANE0_CR_DONE					(0x1 << 0)

#define DPRX_ADDR_LANE2_3_STATUS				0x324
#define DPRX_LANE3_SYMBOL_LOCKED				(0x1 << 6)
#define DPRX_LANE3_CHANNEL_EQ_DONE				(0x1 << 5)
#define DPRX_LANE3_CR_DONE					(0x1 << 4)
#define DPRX_LANE2_SYMBOL_LOCKED				(0x1 << 2)
#define DPRX_LANE2_CHANNEL_EQ_DONE				(0x1 << 1)
#define DPRX_LANE2_CR_DONE					(0x1 << 0)

#define DPRX_ADDR_LANE_ALIGN_STATUS				0x328
#define DPRX_LINK_STATUS_UPDATED				(0x1 << 7)
#define DPRX_INTERLANE_ALIGN_DONE				(0x1 << 0)

#define DPRX_ADDR_ADJUST_REQUEST_LANE0_1			0x330
#define DPRX_PRE_EMPHASIS_LANE1					(0x3 << 6)
#define DPRX_VOLTAGE_SWING_LANE1				(0x3 << 4)
#define DPRX_PRE_EMPHASIS_LANE0					(0x3 << 2)
#define DPRX_VOLTAGE_SWING_LANE0				(0x3 << 0)

#define DPRX_ADDR_ADJUST_REQUEST_LANE2_3			0x334
#define DPRX_PRE_EMPHASIS_LANE3					(0x3 << 6)
#define DPRX_VOLTAGE_SWING_LANE3				(0x3 << 4)
#define DPRX_PRE_EMPHASIS_LANE2					(0x3 << 2)
#define DPRX_VOLTAGE_SWING_LANE2				(0x3 << 0)

#define DPRX_ADDR_RC_TRAINING_STATUS				0x340
#define DPRX_RC_TRAINING_DONE_3					(0x1 << 3)
#define DPRX_RC_TRAINING_DONE_2					(0x1 << 2)
#define DPRX_RC_TRAINING_DONE_1					(0x1 << 1)
#define DPRX_RC_TRAINING_DONE_0					(0x1 << 0)

#define DPRX_ADDR_EQ_TRAINING_STATUS				0x344
#define DPRX_ALIGN_STATUS					(0x1 << 4)
#define DPRX_LANE_SYNC_STATUS_3					(0x1 << 3)
#define DPRX_LANE_SYNC_STATUS_2					(0x1 << 2)
#define DPRX_LANE_SYNC_STATUS_1					(0x1 << 1)
#define DPRX_LANE_SYNC_STATUS_0					(0x1 << 0)

#define DPRX_ADDR_FEC_CONFIG_0					0x400
#define DPRX_FEC_LANE_SELECT_MASK				~(0x3 << 4)
#define DPRX_FEC_LANE_SELECT_0					(0x0 << 4)
#define DPRX_FEC_LANE_SELECT_1					(0x1 << 4)
#define DPRX_FEC_LANE_SELECT_2					(0x2 << 4)
#define DPRX_FEC_LANE_SELECT_3					(0x3 << 4)
#define DPRX_FEC_ERROR_COUNT_SEL_MASK				~(0x7 << 1)
#define DPRX_FEC_ERROR_COUNT_DIS				(0x0 << 1)
#define DPRX_UNCORRECTED_BLOCK_ERROR_COUNT			(0x1 << 1)
#define DPRX_CORRECTED_BLOCK_ERROR_COUNT			(0x2 << 1)
#define DPRX_BIT_ERROR_COUNT					(0x3 << 1)
#define DPRX_PARITY_BLOCK_ERROR_COUNT				(0x4 << 1)
#define DPRX_PARITY_BIT_ERROR_COUNT				(0x5 << 1)
#define DPRX_FEC_READY						(0x01 << 0)
#define DPRX_FEC_NOT_READY					(0x00 << 0)

#define DPRX_ADDR_FEC_CONFIG_1					0x404
#define DPRX_STOP_CIPHER_EN					(0x1 << 1)
#define DPRX_FEC_CORRECT_BYPASS					(0x1 << 0)

#define DPRX_ADDR_FEC_STATUS					0x408
#define DPRX_FEC_DECODE_DIS_DETECTED				(0x1 << 1)
#define DPRX_FEC_DECODE_EN_DETECTED				(0x1 << 0)

#define DPRX_ADDR_FEC_ERROR_COUNT_L				0x420
#define DPRX_FEC_ERROR_COUNT_7_0				(0xff << 0)

#define DPRX_ADDR_FEC_ERROR_COUNT_H				0x420
#define DPRX_FEC_ERROR_COUNT_VALID				(0x1 << 7)
#define DPRX_FEC_ERROR_COUNT_14_8				(0x7f << 0)

#define DPRX_ADDR_DSC_ENABLE					0x500
#define DPRX_DSC_3_EN						(0x1 << 3)
#define DPRX_DSC_2_EN						(0x1 << 2)
#define DPRX_DSC_1_EN						(0x1 << 1)
#define DPRX_DSC_0_EN						(0x1 << 0)

#define DPRX_ADDR_DSC_STATUS(_n)				(0x510 + (_n * 4))
#define DPRX_DSC_CHUNK_LEN_ERROR				(0x1 << 2)
#define DPRX_DSC_RC_BUF_OVERFLOW				(0x1 << 1)
#define DPRX_DSC_RC_BUF_UNDERRUN				(0x1 << 0)

#define DPRX_ADDR_MSTM_CTRL					0x600
#define DPRX_MST_EN						(0x1 << 0)

#define DPRX_ADDR_STRM_SEL					0x604
#define DPRX_MST_STRM_EN(_n)					(0x1 << _n)

#define DPRX_ADDR_MSTM_CONFIG_0					0x608
#define DPRX_BYPASS_UPSTRM_ACT					(0x1 << 1)
#define DPRX_ID_CHANGE_DETECT					(0x1 << 0)

#define DPRX_ADDR_MSTM_CONFIG_1					0x60C
#define DPRX_LOAD_VC_PL_TABLE					(0x1 << 1)

#define DPRX_ADDR_MSTM_DBG_0					0x610
#define DPRX_VCPF_LOCK(_n)					(0x1 << _n)

#define DPRX_ADDR_STRM_PL_ID(_n)				(0x620 + (_n * 4))
#define DPRX_STRM_PL_ID						(0x7f << 0)

#define DPRX_ADDR_MST_PL_ID(_n)					(0x700 + (_n * 4))
#define DPRX_MST_PL_ID						(0x7f << 0)

#define DPRX_ADDR_HDCP_CTRL_0					0x800
#define DPRX_DP_HDCP_INT					(0x1 << 0)

#define DPRX_ADDR_HDCP_CTRL_1					0x804
#define DPRX_HDCP_DISABLE_XTOR_ON_BS				(0x1 << 2)
#define DPRX_HDCP_STOP_DEC					(0x1 << 1)
#define DPRX_HDCP_BYPASS					(0x1 << 0)

#define DPRX_ADDR_RX_HDCP_MST_ECF(_n)				(0x810 + (_n * 4))

#define DPRX_ADDR_DPIP_INTR_0					0x900
#define DPRX_HDCP_DECRYPT_INTR					(0x1 << 6)
#define DPRX_HDCP_LINK_FAIL_INTR				(0x1 << 5)
#define DPRX_FEC_PM_INTERVAL_ERR_INTR				(0x1 << 4)
#define DPRX_MST_BOUNDARY_ERR_INTR				(0x1 << 3)
#define DPRX_MST_LANE_ALIGN_UNLOCK_INTR				(0x1 << 2)
#define DPRX_ACT_IS_DETECT					(0x1 << 1)
#define DPRX_INT_AUX_ERROR					(0x1 << 0)

#define DPRX_ADDR_DPIP_INTR_1					0x904
#define DPRX_NULL_MTP_STRM(_n)					(0x1 << n)

#define DPRX_ADDR_DPIP_INTR_2					0x908
#define DPRX_VCPF_UNLOCK_S(_n)					(0x1 << n)

#define DPRX_ADDR_DPIP_INTR_MASK_0				0x910
#define DPRX_ADDR_DPIP_INTR_MASK_1				0x914
#define DPRX_ADDR_DPIP_INTR_MASK_2				0x918

/* MAIN_LINK_REG */
#define DPRX_ADDR_RST_ML_CTRL					0x000
#define DPRX_RST_AUD_RESET					(0x1 << 1)
#define DPRX_RST_VID_RESET					(0x1 << 0)

#define DPRX_ADDR_BS_JITTER_THRE				0x010
#define DPRX_BS_JITTER_THRE(_v)					((_v & 0xff) << 0)

#define DPRX_ADDR_BE_JITTER_THRE				0x014
#define DPRX_BE_JITTER_THRE(_v)					((_v & 0xff) << 0)

#define DPRX_ADDR_HTOTAL_BYTE_NUM_H				0x020
#define DPRX_HTOTAL_BYTE_NUM_H(_v)				((_v & 0xff) << 0)

#define DPRX_ADDR_HTOTAL_BYTE_NUM_L				0x024
#define DPRX_HTOTAL_BYTE_NUM_L(_v)				((_v & 0xff) << 0)

#define DPRX_ADDR_RX_VBID					0x060
#define DPRX_RX_VBID						(0xff << 0)

#define DPRX_ADDR_MAINLINK_STATUS				0x070
#define DPRX_DATA_MUX_STATE					(0x1f << 0)

#define DPRX_ADDR_MSA_CTRL_0					0x100
#define DPRX_DIS_WAIT_MSA_RECOVER				(0x1 << 6)
#define DPRX_BYPASS_MSA_COMP					(0x1 << 5)
#define DPRX_MSA_WRITE_EN					(0x1 << 4)
#define DPRX_M_STABLE						(0x1 << 3)
#define DPRX_FRAME_SAME(_v)					((_v & 0x7) << 0)

#define DPRX_ADDR_MSA_CTRL_1					0x104
#define DPRX_MSA_CHANGE_RESET_EN				(0x1 << 3)
#define DPRX_MSA_CHANGE_CNT(_v)					((_v & 0x7) << 0)

#define DPRX_ADDR_FRAME_SAME_MASK_0				0x110
#define DPRX_FRAME_SAME_MASK_10_8(_v)				((_v & 0x7) << 0)

#define DPRX_ADDR_FRAME_SAME_MASK_1				0x114
#define DPRX_FRAME_SAME_MASK_7_0(_v)				((_v & 0xff) << 0)

#define DPRX_ADDR_RX_HSTART_H					0x1A0
#define DPRX_ADDR_RX_HSTART_L					0x1A4

#define DPRX_ADDR_RX_HSW_H					0x1A8
#define DPRX_ADDR_RX_HSW_L					0x1AC

#define DPRX_ADDR_RX_HTOTAL_H					0x1B0
#define DPRX_ADDR_RX_HTOTAL_L					0x1B4

#define DPRX_ADDR_RX_HWIDTH_H					0x1B8
#define DPRX_ADDR_RX_HWIDTH_L					0x1BC

#define DPRX_ADDR_RX_VTOTAL_H					0x1C0
#define DPRX_ADDR_RX_VTOTAL_L					0x1C4

#define DPRX_ADDR_RX_VHEIGHT_H					0x1C8
#define DPRX_ADDR_RX_VHEIGHT_L					0x1CC

#define DPRX_ADDR_RX_VSTART_H					0x1D0
#define DPRX_ADDR_RX_VSTART_L					0x1D4

#define DPRX_ADDR_RX_VSW_H					0x1D8
#define DPRX_ADDR_RX_VSW_L					0x1DC

#define DPRX_ADDR_RX_MISC0					0x1E0
#define DPRX_ADDR_RX_MISC1					0x1E4

#define DPRX_ADDR_MSA_STATUS					0x1F0
#define DPRX_MSA_IS_STABLE					(0x1 << 0)

#define DPRX_ADDR_VID_PARA_SEL					0x200
#define DPRX_VTOTAL_SELECT					(0x1 << 4)
#define DPRX_VHEIGHT_SELECT					(0x1 << 3)
#define DPRX_HWIDTH_SELECT					(0x1 << 2)
#define DPRX_MISC_1_SELECT					(0x1 << 1)
#define DPRX_MISC_0_SELECT					(0x1 << 0)

#define DPRX_ADDR_ACTIVE_SYMBOL_DIFF				0x204
#define DPRX_CHANGED_TU_EN					(0x1 << 7)
#define DPRX_ACTIVE_SYMBOL_DIFF(_v)				((_v & 0x7f) << 0)

#define DPRX_ADDR_VID_MIS_CTRL					0x208
#define DPRX_FRAME_END_CNT(_v)					((_v & 0x3) << 0)

#define DPRX_ADDR_CFG_VTOTAL_H					0x210
#define DPRX_CFG_VTOTAL_H(_v)					((_v & 0xff) << 0)

#define DPRX_ADDR_CFG_VTOTAL_L					0x214
#define DPRX_CFG_VTOTAL_L(_v)					((_v & 0xff) << 0)

#define DPRX_ADDR_CFG_VHEIGHT_H					0x220
#define DPRX_CFG_VHEIGHT_H(_v)					((_v & 0xff) << 0)

#define DPRX_ADDR_CFG_VHEIGHT_L					0x224
#define DPRX_CFG_VHEIGHT_L(_v)					((_v & 0xff) << 0)

#define DPRX_ADDR_CFG_HWIDTH_H					0x230
#define DPRX_CFG_HWIDTH_H(_v)					((_v & 0xff) << 0)

#define DPRX_ADDR_CFG_HWIDTH_L					0x234
#define DPRX_CFG_HWIDTH_L(_v)					((_v & 0xff) << 0)

#define DPRX_ADDR_CFG_MISC0					0x240
#define DPRX_CFG_MISC0(_v)					((_v & 0xff) << 0)

#define DPRX_ADDR_CFG_MISC1					0x244
#define DPRX_CFG_MISC1(_v)					((_v & 0xff) << 0)

#define DPRX_ADDR_RX_TU_NUM					0x280
#define DPRX_RX_TU_NUM						(0x3f << 0)

#define DPRX_ADDR_RX_ACTIVE_SYMBOL				0x284
#define DPRX_RX_ACTIVE_SYMBOL					(0x7f << 0)

#define DPRX_ADDR_RX_MVID(_h)					(0x290 + (_h * 4))

#define DPRX_ADDR_RX_NVID(_h)					(0x2A0 + (_h * 4))

#define DPRX_ADDR_VID_STATUS					0x2B0
#define DPRX_VID_HWIDTH_MORE					(0x1 << 3)
#define DPRX_VID_HWIDTH_LESS					(0x1 << 2)
#define DPRX_VID_VHEIGHT_MORE					(0x1 << 1)
#define DPRX_VID_VHEIGHT_LESS					(0x1 << 0)

#define DPRX_ADDR_AUD_CTRL					0x300
#define DPRX_AUD_AUTO_MUTE_EN					(0x1 << 4)
#define DPRX_AUD_OVWR_ALL_CH_ST					(0x1 << 3)
#define DPRX_AUD_OVWR_CH_ST					(0x1 << 2)
#define DPRX_AUD_UNMUTE_ON_DE					(0x1 << 1)
#define DPRX_CFG_AUD_MUTE					(0x1 << 0)

#define DPRX_ADDR_AUD_M_N_CRITERIA				0x304
#define DPRX_AUD_M_N_CRITERIA(_v)				((_v & 0xff) << 0)

#define DPRX_ADDR_AUD_CH01_MAP					0x310
#define DPRX_ADDR_AUD_CH23_MAP					0x314
#define DPRX_ADDR_AUD_CH45_MAP					0x318
#define DPRX_ADDR_AUD_CH67_MAP					0x31C

#define DPRX_ADDR_CFG_AUD_CH_STATUS(_n)				(0x320 + (_n * 4))
#define DPRX_CFG_AUD_CH_STATUS(_n)				((_n % 8) << 0)

#define DPRX_ADDR_RX_AUD_CH_STATUS(_n)				(0x360 + (_n * 4))

#define DPRX_ADDR_RX_MAUD(_h)					(0x380 + (_h * 4))

#define DPRX_ADDR_RX_NAUD(_h)					(0x390 + (_h * 4))

#define DPRX_ADDR_RX_AUD_PACK_STATUS				0x3A0
#define DPRX_RX_AUD_HBR						(0x1 << 0)
#define DPRX_RX_AUD_CHANNEL_COUNT				(0x7 << 0)

#define DPRX_ADDR_SDP_CTRL					0x400
#define DPRX_UPDATE_SDP_POSEDGE					(0x0 << 5)
#define DPRX_UPDATE_SDP_NEGEDGE					(0x1 << 5)
#define DPRX_BYPASS_SDP_CAM_HEADER				(0x1 << 4)
#define DPRX_SDP_RS_ERROR_EN					(0x1 << 3)
#define DPRX_SDP_CAM_TRANSPARENT_EN				(0x1 << 2)
#define DPRX_SDP_CAM_GEN_EOF_EN					(0x1 << 1)
#define DPRX_SDP_CAM_GEN_SOF_EN					(0x1 << 1)

#define DPRX_ADDR_SDP_CAM_CTRL					0x404
#define DPRX_SDP_CAM_GEN_EN(_n)					(0x1 << _n)

#define DPRX_ADDR_SPD_HEADER					0x408
#define DPRX_SPD_HEADER(_v)					((_v & 0xff) << 0)

#define DPRX_ADDR_PPS_HEADER					0x40C
#define DPRX_PPS_HEADER(_v)					((_v & 0xff) << 0)

#define DPRX_ADDR_SDP_CAM_GEN_EOF_TYPE				0x410
#define DPRX_SDP_CAM_GEN_EOF_TYPE(_v)				((_v & 0xff) << 0)

#define DPRX_ADDR_SDP_CAM_GEN_SOF_TYPE				0x414
#define DPRX_SDP_CAM_GEN_SOF_TYPE(_v)				((_v & 0xff) << 0)

#define DPRX_ADDR_SDP_BIT_ERR_THRE				0x418
#define DPRX_SDP_BIT_ERROR_THRE(_v)				((_v & 0xff) << 0)

#define DPRX_ADDR_SDP_CAM_GEN_TYPE(_n)				(0x420 + (_n * 4))

#define DPRX_ADDR_SDP_MPEG_INFO_HBR3				0x450
#define DPRX_ADDR_SDP_SPD_INFO_HBR3				0x454
#define DPRX_ADDR_SDP_AVI_INFO_HBR3				0x458
#define DPRX_ADDR_SDP_AUDIO_INFO_HBR3				0x45C

#define DPRX_ADDR_SDP_VSC_PKG_HB2				0x460
#define DPRX_ADDR_SDP_VSC_PKG_HB3				0x464

#define DPRX_ADDR_SDP_PKG_READ_EN				0x470
#define DPRX_AUDIO_INFO_READ_EN					(0x1 << 5)
#define DPRX_PPS_PKG_READ_EN					(0x1 << 4)
#define DPRX_VSC_PKG_READ_EN					(0x1 << 3)
#define DPRX_MPEG_INFO_READ_EN					(0x1 << 2)
#define DPRX_SPD_INFO_READ_EN					(0x1 << 1)
#define DPRX_AVI_INFO_READ_EN					(0x1 << 0)

#define DPRX_ADDR_SDP_PKG_ERR					0x474
#define DPRX_AVI_FAIL_FLAG					(0x1 << 7)
#define DPRX_AUDIO_FAIL_FLAG					(0x1 << 6)
#define DPRX_AUDIO_INFO_ERR					(0x1 << 5)
#define DPRX_PPS_PKG_ERR					(0x1 << 4)
#define DPRX_VSC_PKG_ERR					(0x1 << 3)
#define DPRX_MPEG_INFO_ERR					(0x1 << 2)
#define DPRX_SPD_INFO_ERR					(0x1 << 1)
#define DPRX_AVI_INFO_ERR					(0x1 << 0)

#define DPRX_ADDR_SDP_AUDIO_INFO(_nb)				(0x500 + (_nb * 4))

#define DPRX_ADDR_SDP_VSC_PKG_DATA(_nb)				(0x570 + (_nb * 4))

#define DPRX_ADDR_SDP_MPEG_INFO(_nb)				(0x600 + (_nb * 4))

#define DPRX_ADDR_SDP_SPD_INFO(_nb)				(0x640 + (_nb * 4))

#define DPRX_ADDR_SDP_AVI_INFO(_nb)				(0x6B0 + (_nb * 4))

#define DPRX_ADDR_SDP_PPS_PKG_DATA(_nb)				(0x700 + (_nb * 4))

#define DPRX_ADDR_TEST_CRC_R_CR_L				0x900
#define DPRX_ADDR_TEST_CRC_R_CR_H				0x904
#define DPRX_ADDR_TEST_CRC_G_Y_L				0x908
#define DPRX_ADDR_TEST_CRC_G_Y_H				0x90C
#define DPRX_ADDR_TEST_CRC_B_CB_L				0x910
#define DPRX_ADDR_TEST_CRC_B_CB_H				0x914
#define DPRX_ADDR_TEST_CRC_COUNT				0x918
#define DPRX_ADDR_TEST_CRC_CTRL					0x91C
#define DPRX_TEST_CRC_EN					(0x1 << 0)

#define DPRX_ADDR_MAINLINK_INTR0				0xA00
#define DPRX_VOTING_ERROR_INTR					(0x1 << 4)
#define DPRX_BE_CNT_ERR_IN_LINE_INTR				(0x1 << 3)
#define DPRX_BS_CNT_ERR_IN_LINE_INTR				(0x1 << 2)
#define DPRX_BE_CNT_ERR_IN_FRAME_INTR				(0x1 << 1)
#define DPRX_BS_CNT_ERR_IN_FRAME_INTR				(0x1 << 0)

#define DPRX_ADDR_MAINLINK_INTR1				0xA04
#define DPRX_VID_CHANGE_INTR					(0x1 << 7)
#define DPRX_VID_FRAME_ERR_INTR					(0x1 << 6)
#define DPRX_VID_MN_RDY_INTR					(0x1 << 5)
#define DPRX_VID_UNMUTE_INTR					(0x1 << 4)
#define DPRX_VID_MUTE_INTR					(0x1 << 3)
#define DPRX_MSA_RDY_INTR					(0x1 << 2)
#define DPRX_MSA_CHANGE_INTR					(0x1 << 1)
#define DPRX_MSA_UPDATE_INTR					(0x1 << 0)

#define DPRX_ADDR_MAINLINK_INTR2				0xA08
#define DPRX_AUD_M_N_CHANGE_INTR				(0x1 << 6)
#define DPRX_AUD_MN_RDY_INTR					(0x1 << 5)
#define DPRX_AUD_CH_STATUS_CHG_INTR				(0x1 << 4)
#define DPRX_AUD_MUTE_INTR					(0x1 << 3)
#define DPRX_AUD_LINKERR_INTR					(0x1 << 2)
#define DPRX_AUD_DECERR_INTR					(0x1 << 1)
#define DPRX_AUD_PARITYERR_INTR					(0x1 << 0)

#define DPRX_ADDR_MAINLINK_INTR3				0xA0C
#define DPRX_OTHER_INFO_RECEIVED_INTR				(0x1 << 6)
#define DPRX_AUDIO_INFO_CHANGE_INTR				(0x1 << 5)
#define DPRX_PPS_PKG_INFO_CHANGE_INTR				(0x1 << 4)
#define DPRX_VSC_PKG_INFO_CHANGE_INTR				(0x1 << 3)
#define DPRX_MPEG_INFO_CHANGE_INTR				(0x1 << 2)
#define DPRX_SPD_INFO_CHANGE_INTR				(0x1 << 1)
#define DPRX_AVI_INFO_CHANGE_INTR				(0x1 << 0)

#define DPRX_ADDR_MAINLINK_INTR4				0xA10
#define DPRX_SDP_LEN_ERR_INTR					(0x1 << 2)
#define DPRX_2BIT_LEN_ERR_INTR					(0x1 << 1)
#define DPRX_1BIT_LEN_ERR_INTR					(0x1 << 0)

#define DPRX_ADDR_MAINLINK_INTR0_MASK				0xA20
#define DPRX_ADDR_MAINLINK_INTR1_MASK				0xA24
#define DPRX_ADDR_MAINLINK_INTR2_MASK				0xA28
#define DPRX_ADDR_MAINLINK_INTR3_MASK				0xA2C
#define DPRX_ADDR_MAINLINK_INTR4_MASK				0xA30

/* SERDES_REG */

#define DPRX_ADDR_DPRX_REG0					0x000
#define DPRX_ADDR_DPRX_REG1					0x004
#define DPRX_ADDR_DPRX_REG2					0x008
#define DPRX_ADDR_DPRX_REG3					0x00C
#define DPRX_ADDR_DPRX_REG4					0x010
#define DPRX_ADDR_DPRX_REG5					0x014
#define DPRX_ADDR_DPRX_REG6					0x018
#define DPRX_ADDR_DPRX_REG7					0x01C
#define DPRX_ADDR_DPRX_REG8					0x020
#define DPRX_ADDR_DPRX_REG9					0x024
#define DPRX_ADDR_DPRX_REG10					0x028
#define DPRX_ADDR_DPRX_REG11					0x02C
#define DPRX_ADDR_DPRX_REG12					0x030
#define DPRX_ADDR_DPRX_REG13					0x034
#define DPRX_ADDR_DPRX_REG14					0x038
#define DPRX_ADDR_DPRX_REG15					0x03C
#define DPRX_ADDR_DPRX_REG16					0x040
#define DPRX_ADDR_DPRX_REG17					0x044
#define DPRX_ADDR_DPRX_REG18					0x048
#define DPRX_ADDR_DPRX_REG19					0x04C
#define DPRX_ADDR_DPRX_REG20					0x050

#define DPRX_ADDR_DP_CHn_CTRL0(_n)				(0x06C + (_n * 4))
#define DPRX_DP_CH_RX_RDY					(0x1 << 7)
#define DPRX_DP_CH_LANE_OK					(0x1 << 6)
#define DPRX_DP_CH_PD_RX					(0x1 << 5)
#define DPRX_DP_CH_RESET_RX					(0x1 << 4)
#define DPRX_DP_CH_EN_RTERM					(0x1 << 3)
#define DPRX_DP_CH_SIG_NOT_CR					(0x1 << 2)
#define DPRX_DP_CH_EN_ADPT_DIGITAL				(0x1 << 1)
#define DPRX_DP_CH_FRZ_ADPT					(0x1 << 0)

#define DPRX_ADDR_DP_RXn_REGv_O(_n, _v)				(0x07C + (_n * 4) + (_v * 4))

#define DPRX_ADDR_SERDES_LANE_0_1_STATE				0x0CC
#define DPRX_SERDES_LANE_1_STATE				(0xf << 4)
#define DPRX_SERDES_LANE_0_STATE				(0xf << 0)

#define DPRX_ADDR_SERDES_LANE_2_3_STATE				0x0D0
#define DPRX_SERDES_LANE_3_STATE				(0xf << 4)
#define DPRX_SERDES_LANE_2_STATE				(0xf << 0)

#define DPRX_ADDR_SERDES_WAIT_DONE_T				0x0D8
#define DPRX_SERDES_WAIT_DONE_(t)				((_t & 0xff) << 0)

#define DPRX_ADDR_SERDES_CDR_RESET_T				0x0DC
#define DPRX_SERDES_CDR_RESET(_t)				((_t & 0xff) << 0)

#define DPRX_ADDR_SERDES_PLL_LOCK_TIMEOUT			0x0E0
#define DPRX_PLL_LOCK_TIMEOUT(_t)				((_t & 0xff) << 0)

#define DPRX_ADDR_SERDES_PLL_RESET_T				0x0E4
#define DPRX_PLL_PLL_RESET(_t)					((_t & 0xff) << 0)

#define DPRX_ADDR_SERDES_CAL_EN_T				0x0E8
#define DPRX_SERDES_CAL_EN(_t)					((_t & 0xff) << 0)

#define DPRX_ADDR_SERDES_RC_LOST_THRESHOLD			0x0EC
#define DPRX_SERDES_RC_LOST_THRESHOLD(_t)			((_t & 0xff) << 0)

#define DPRX_ADDR_SERDES_LANE_SWAP				0x0F4
#define DPRX_SERDES_LANE3_SWAP(_v)				(0x6 << _v)
#define DPRX_SERDES_LANE2_SWAP(_v)				(0x4 << _v)
#define DPRX_SERDES_LANE1_SWAP(_v)				(0x2 << _v)
#define DPRX_SERDES_LANE0_SWAP(_v)				(0x0 << _v)

#define DPRX_ADDR_SERDES_STATUS0				0x0F8
#define DPRX_PLL_LOCK_DET					(0x1 << 5)
#define DPRX_ALL_LANE_OK					(0x1 << 4)
#define DPRX_SERDES_LANE_OK(_n)					(0x1 << _n)

#define DPRX_ADDR_SERDES_INTERRUPT				0x0FC
#define DPRX_ADDR_SERDES_UNMASK(_v)				(_v << 4)
#define DPRX_ADDR_SERDES_INTP					(0xf << 0)

#define DPRX_ADDR_SERDES_CR_OK_CNT				0x104
#define DPRX_ADDR_CR_OK_CNT(_c)					((_c & 0xff) << 0)

#define DPRX_ADDR_SERDES_FILTER_CNT				0x108
#define DPRX_ADDR_FILTER_CNT(_c)				((_c & 0xff) << 0)

#define DPRX_ADDR_DP_AUX_REG					0x10C
#define DPRX_SWAP_AUXP_AUXN_TX					(0x1 << 5)
#define DPRX_SWAP_AUXP_AUXN_RX					(0x1 << 4)
#define DPRX_AUX_RX_VOLTAGE_1V					(0x1 << 3)
#define DPRX_AUX_RX_VOLTAGE_0_5V				(0x0 << 3)
#define DPRX_AUX_DOUBLE_OUTPUT					(0x1 << 2)
#define DPRX_AUX_SINGLE_OUTPUT					(0x0 << 2)
#define DPRX_AUX_CHANNEL_TERMINATION_600_OHM			(0x0 << 0)
#define DPRX_AUX_CHANNEL_TERMINATION_300_OHM			(0x1 << 0)
#define DPRX_AUX_CHANNEL_TERMINATION_100_OHM			(0x2 << 0)
#define DPRX_AUX_CHANNEL_TERMINATION_50_OHM			(0x3 << 0)

#define DPRX_ADDR_DP_PLL_REG1					0x114
#define DPRX_DP_PLL_MANUAL_KVCO(_v)				(_v << 4)
#define DPRX_DP_PLL_REF_CLK_SEL_24M				(0x0 << 0)
#define DPRX_DP_PLL_REF_CLK_SEL_25M				(0x1 << 0)
#define DPRX_DP_PLL_REF_CLK_SEL_27M				(0x2 << 0)
#define DPRX_DP_PLL_REF_CLK_SEL_38M				(0x3 << 0)

#define DPRX_ADDR_DP_PLL_REG2					0x118
#define DPRX_ADDR_DP_PLL_REG3					0x11C
#define DPRX_ADDR_DP_PLL_REG4					0x120
#define DPRX_ADDR_DP_PLL_REG5					0x124

#define DPRX_ADDR_DP_TEST_REG					0x12C

#define DPRX_ADDR_SERDES_CTRL0					0x130
#define DPRX_DIS_SIG_NOT_CR					(0x1 << 6)
#define DPRX_CR_SEL_FULL_LINK_TRAINING				(0x0 << 4)
#define DPRX_CR_SEL_FAST_LINK_TRAINING				(0x1 << 4)
#define DPRX_RESET_STATE					(0x1 << 3)
#define DPRX_BYPASS_PLL_LOCK					(0x1 << 2)
#define DPRX_BYPASS_SIGNAL_DET					(0x1 << 1)
#define DPRX_BYPASS_STATE					(0x1 << 0)

#define DPRX_ADDR_SERDES_CTRL1					0x134
#define DPRX_DP_PD_PLL_POWER_UP					(0x1 << 6)
#define DPRX_DP_PD_PLL_POWER_DOWN				(0x0 << 6)
#define DPRX_RESET_PLL						(0x1 << 5)
#define DPRX_DP_RX_CAL_EN					(0x1 << 4)
#define DPRX_DP_POWERDOWN_AUX					(0x1 << 3)
#define DPRX_DP_POWERUP_AUX					(0x0 << 3)
#define DPRX_POWERDOWN_PHY					(0x1 << 2)
#define DPRX_POWERUP_PHY					(0x0 << 2)
#define DPRX_PLL_SPEED_1_62					(0x0 << 0)
#define DPRX_PLL_SPEED_2_70					(0x1 << 0)
#define DPRX_PLL_SPEED_5_40					(0x2 << 0)
#define DPRX_PLL_SPEED_8_10					(0x3 << 0)

/* DPCD */
#define DPCD_ADDR_LINK_BW_SET					0x100
#define DPCD_ADDR_LANE_COUNT_SET				0x101
#define DPCD_ADDR_TRAINING_PATTERN_SET				0x102
#define DPCD_ADDR_PAYLOAD_ALLOCATE_SET				0x1C0
#define DPCD_ADDR_PAYLOAD_START_TIME_SLOT			0x1C1
#define DPCD_ADDR_PAYLOAD_TIME_SLOT_COUNT			0x1C2
#define DPCD_ADDR_RATE_GOVERNING_X(n)				(0x1CB - n)
#define DPCD_ADDR_RATE_GOVERNING_Y(n)				(0x1D3 - n)
#define DPCD_ADDR_PAYLOAD_TABLE_UPDATE_STATUS			0x2C0
#define DPCD_ADDR_VC_PAYLOAD_ID(n)				(0x2C1 + n)
#define DPCD_ADDR_VENDOR_SPECIFIC				0x410
#endif
