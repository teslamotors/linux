/*
 * cyttsp6_regs.h
 * Cypress TrueTouch(TM) Standard Product V4 Registers.
 * For use with Cypress touchscreen controllers.
 * Supported parts include:
 * CY8CTMA46X
 * CY8CTMA1036/768
 * CYTMA445
 *
 * Copyright (C) 2012-2015 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#ifndef _CYTTSP6_REGS_H
#define _CYTTSP6_REGS_H

#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/device.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#elif defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/version.h>
#include <linux/cyttsp6_core.h>

/* Timeout in ms. */
#define CY_COMMAND_COMPLETE_TIMEOUT		1000
#define CY_CALIBRATE_COMPLETE_TIMEOUT		10000
#define CY_WATCHDOG_TIMEOUT			1000
#define CY_CORE_REQUEST_EXCLUSIVE_TIMEOUT	1000
#define CY_PROXIMITY_REQUEST_EXCLUSIVE_TIMEOUT	2000
#define CY_DA_REQUEST_EXCLUSIVE_TIMEOUT		1000
#define CY_LDR_REQUEST_EXCLUSIVE_TIMEOUT	10000
#define CY_CORE_SLEEP_REQUEST_EXCLUSIVE_TIMEOUT	10000
#define CY_CORE_WAIT_SYSINFO_MODE_TIMEOUT	4000
#define CY_CORE_MODE_CHANGE_TIMEOUT		2000
#define CY_CORE_RESET_AND_WAIT_TIMEOUT		1000
#define CY_CORE_WAKEUP_TIMEOUT			500
#define CY_LDR_CMD_TIMEOUT			1000
#define CY_LDR_CMD_INIT_TIMEOUT			20000
#define CY_CORE_CMD_WAIT_FOR_EVENT_TIMEOUT	50

/* helpers */
#define GET_NUM_TOUCH_RECORDS(x)	((x) & 0x1F)
#define IS_LARGE_AREA(x)		((x) & 0x20)
#define IS_BAD_PKT(x)			((x) & 0x20)
#define IS_TTSP_VER_GE(p, maj, min) \
		((p)->si_ptrs.cydata == NULL ? \
		0 : \
		((p)->si_ptrs.cydata->ttsp_ver_major < (maj) ? \
			0 : \
			((p)->si_ptrs.cydata->ttsp_ver_minor < (min) ? \
				0 : \
				1)))

#define IS_BOOTLOADER(hst_mode, reset_detect) \
		((hst_mode) & 0x01 || (reset_detect) != 0)
#define IS_BOOTLOADER_IDLE(hst_mode, reset_detect) \
		((hst_mode) & 0x01 && (reset_detect) & 0x01)

#define GET_HSTMODE(reg)		((reg & 0x70) >> 4)
#define GET_TOGGLE(reg)			((reg & 0x80) >> 7)

#define IS_LITTLEENDIAN(reg)		((reg & 0x01) == 1)
#define GET_PANELID(reg)		(reg & 0x07)

#define HI_BYTE(x)			(u8)(((x) >> 8) & 0xFF)
#define LO_BYTE(x)			(u8)((x) & 0xFF)

#define IS_DEEP_SLEEP_CONFIGURED(x)	((x) == 0 || (x) == 0xFF)

#define IS_TMO(t)			((t) == 0)

#define IS_GEST_EXTD(gest_id)		(((u8)gest_id) >> 7)

#define PUT_FIELD16(si, val, addr) \
do { \
	if (IS_LITTLEENDIAN((si)->si_ptrs.cydata->device_info)) \
		put_unaligned_le16(val, addr); \
	else \
		put_unaligned_be16(val, addr); \
} while (0)

#define GET_FIELD16(si, addr) \
({ \
	u16 __val; \
	if (IS_LITTLEENDIAN((si)->si_ptrs.cydata->device_info)) \
		__val = get_unaligned_le16(addr); \
	else \
		__val = get_unaligned_be16(addr); \
	__val; \
})

#define CY_BL_MAX_STATUS_SIZE		32

/* DEVICE REGISTERS */
/* OP MODE REGISTERS */
#define CY_REG_BASE			0x00

/* Enables cyttsp6_detect() to be used during the probe stage */
#define CYTTSP6_DETECT_HW

enum cyttsp6_hst_mode_bits {
	CY_HST_TOGGLE      = (1 << 7),
	CY_HST_MODE_CHANGE = (1 << 3),
	CY_HST_DEVICE_MODE = (7 << 4),
	CY_HST_OPERATE     = (0 << 4),
	CY_HST_SYSINFO     = (1 << 4),
	CY_HST_CAT         = (2 << 4),
	CY_HST_LOWPOW      = (1 << 2),
	CY_HST_SLEEP       = (1 << 1),
	CY_HST_RESET       = (1 << 0),
};

/* Touch record registers */
enum cyttsp6_object_id {
	CY_OBJ_STANDARD_FINGER,
	CY_OBJ_PROXIMITY,
	CY_OBJ_STYLUS,
	CY_OBJ_HOVER,
	CY_OBJ_GLOVE,
};

enum cyttsp6_event_id {
	CY_EV_NO_EVENT,
	CY_EV_TOUCHDOWN,
	CY_EV_MOVE,		/* significant displacement (> act dist) */
	CY_EV_LIFTOFF,		/* record reports last position */
};

/* CAT MODE REGISTERS */
#define CY_REG_CAT_CMD			2
#define CY_REG_CAT_CMD_DATA_MAX		511
#define CY_CMD_COMPLETE_MASK		(1 << 6)
#define CY_CMD_MASK			0x3F

enum cyttsp_cmd_bits {
	CY_CMD_COMPLETE    = (1 << 6),
};

/* SYSINFO REGISTERS */
#define CY_NUM_REVCTRL			8

#define CY_POST_CODEL_WDG_RST           0x01
#define CY_POST_CODEL_CFG_DATA_CRC_FAIL 0x02
#define CY_POST_CODEL_PANEL_TEST_FAIL   0x04
#define CY_POST_CODEL_SCAN_STATUS       0x08

#define CY_ERR_OFFSET			0xFE

/* touch record system information offset masks and shifts */
#define CY_BYTE_OFS_MASK		0x1F
#define CY_BOFS_MASK			0xE0
#define CY_BOFS_SHIFT			5

/* x-axis resolution of panel in pixels */
#define CY_PCFG_RESOLUTION_X_MASK 0x7F

/* y-axis resolution of panel in pixels */
#define CY_PCFG_RESOLUTION_Y_MASK 0x7F

/* x-axis, 0:origin is on left side of panel, 1: right */
#define CY_PCFG_ORIGIN_X_MASK 0x80

/* y-axis, 0:origin is on top side of panel, 1: bottom */
#define CY_PCFG_ORIGIN_Y_MASK 0x80

#define CY_NORMAL_ORIGIN		0	/* upper, left corner */
#define CY_INVERT_ORIGIN		1	/* lower, right corner */

/* TTSP System Information interface definitions */
struct cyttsp6_cydata {
	u8 ttpidh;
	u8 ttpidl;
	u8 fw_ver_major;
	u8 fw_ver_minor;
	u8 revctrl[CY_NUM_REVCTRL];
	u8 blver_major;
	u8 blver_minor;
	u8 jtag_si_id3;
	u8 jtag_si_id2;
	u8 jtag_si_id1;
	u8 jtag_si_id0;
	u8 mfgid_sz;
	u8 cyito_idh;
	u8 cyito_idl;
	u8 cyito_verh;
	u8 cyito_verl;
	u8 ttsp_ver_major;
	u8 ttsp_ver_minor;
	u8 device_info;
	u8 mfg_id[];
} __packed;

struct cyttsp6_test {
	u8 post_codeh;
	u8 post_codel;
} __packed;

struct cyttsp6_pcfg {
	u8 electrodes_x;
	u8 electrodes_y;
	u8 len_xh;
	u8 len_xl;
	u8 len_yh;
	u8 len_yl;
	u8 res_xh;
	u8 res_xl;
	u8 res_yh;
	u8 res_yl;
	u8 max_zh;
	u8 max_zl;
	u8 panel_info0;
} __packed;

#define CY_NUM_TCH_FIELDS		7
#define CY_NUM_EXT_TCH_FIELDS		3

struct cyttsp6_tch_rec_params {
	u8 loc;
	u8 size;
} __packed;

struct cyttsp6_opcfg {
	u8 cmd_ofs;
	u8 rep_ofs;
	u8 rep_szh;
	u8 rep_szl;
	u8 num_btns;
	u8 tt_stat_ofs;
	u8 obj_cfg0;
	u8 max_tchs;
	u8 tch_rec_size;
	struct cyttsp6_tch_rec_params tch_rec_old[CY_NUM_TCH_FIELDS];
	u8 btn_rec_size;/* btn record size (in bytes) */
	u8 btn_diff_ofs;/* btn data loc ,diff counts, (Op-Mode byte ofs) */
	u8 btn_diff_size;/* btn size of diff counts (in bits) */
	struct cyttsp6_tch_rec_params tch_rec_new[CY_NUM_EXT_TCH_FIELDS];
	u8 noise_data_ofs;
	u8 noise_data_sz;
} __packed;

struct cyttsp6_sysinfo_data {
	u8 hst_mode;
	u8 reserved;
	u8 map_szh;
	u8 map_szl;
	u8 cydata_ofsh;
	u8 cydata_ofsl;
	u8 test_ofsh;
	u8 test_ofsl;
	u8 pcfg_ofsh;
	u8 pcfg_ofsl;
	u8 opcfg_ofsh;
	u8 opcfg_ofsl;
	u8 ddata_ofsh;
	u8 ddata_ofsl;
	u8 mdata_ofsh;
	u8 mdata_ofsl;
} __packed;

/* FLASH BLOCKS */
enum cyttsp6_ic_ebid {
	CY_TCH_PARM_EBID,
	CY_MDATA_EBID,
	CY_DDATA_EBID,
};

/* ttconfig block */
#define CY_TTCONFIG_VERSION_OFFSET	8
#define CY_TTCONFIG_VERSION_SIZE	2
#define CY_TTCONFIG_VERSION_ROW		0

#define CY_CONFIG_LENGTH_INFO_OFFSET	0
#define CY_CONFIG_LENGTH_INFO_SIZE	4
#define CY_CONFIG_LENGTH_OFFSET		0
#define CY_CONFIG_LENGTH_SIZE		2
#define CY_CONFIG_MAXLENGTH_OFFSET	2
#define CY_CONFIG_MAXLENGTH_SIZE	2
#define CY_CONFIG_GESTURE_ENABLE_ADDR	(0x04F1)

/* GESTURE_ENABLED */
#define CY_GEST_DISABLED	0
#define CY_GEST_ENABLED_STD	1
#define CY_GEST_ENABLED_EXT	2
#define CY_GEST_EXT_ID_MASK	(0x0F)
/* DEBUG */
/* drv_debug commands */
#define CY_DBG_SUSPEND			4
#define CY_DBG_RESUME			5
#define CY_DBG_SOFT_RESET		97
#define CY_DBG_RESET			98

/* Debug buffer */
#define CY_MAX_PRBUF_SIZE		PIPE_BUF
#define CY_PR_TRUNCATED			" truncated..."

/* TMA400 HOST SYNC BYTE */
#define CY_CMD_LDR_HOST_SYNC 0xFF

#define CY_START_OF_PACKET				0x01
#define CY_END_OF_PACKET				0x17

/* CMD */
enum cyttsp6_cmd_bl {
	CY_CMD_LDR_VERIFY_CHKSUM = 0x31,
	CY_CMD_LDR_ERASE_ROW = 0x34,
	CY_CMD_LDR_SEND_DATA = 0x37,
	CY_CMD_LDR_ENTER,
	CY_CMD_LDR_PROG_ROW,
	CY_CMD_LDR_VERIFY_ROW,
	CY_CMD_LDR_EXIT,
	CY_CMD_LDR_FAST_EXIT,
	CY_CMD_LDR_INIT = 0x48,
};

enum cyttsp6_cmd_cat {
	CY_CMD_CAT_NULL,
	CY_CMD_CAT_RESERVED_1,
	CY_CMD_CAT_GET_CFG_ROW_SZ,
	CY_CMD_CAT_READ_CFG_BLK,
	CY_CMD_CAT_WRITE_CFG_BLK,
	CY_CMD_CAT_RESERVED_2,
	CY_CMD_CAT_LOAD_SELF_TEST_DATA,
	CY_CMD_CAT_RUN_SELF_TEST,
	CY_CMD_CAT_GET_SELF_TEST_RESULT,
	CY_CMD_CAT_CALIBRATE_IDACS,
	CY_CMD_CAT_INIT_BASELINES,
	CY_CMD_CAT_EXEC_PANEL_SCAN,
	CY_CMD_CAT_RETRIEVE_PANEL_SCAN,
	CY_CMD_CAT_START_SENSOR_DATA_MODE,
	CY_CMD_CAT_STOP_SENSOR_DATA_MODE,
	CY_CMD_CAT_INT_PIN_MODE,
	CY_CMD_CAT_RETRIEVE_DATA_STRUCTURE,
	CY_CMD_CAT_VERIFY_CFG_BLK_CRC,
	CY_CMD_CAT_RESERVED_N,
};

enum cyttsp6_cmd_op {
	CY_CMD_OP_NULL,
	CY_CMD_OP_RESERVED_1,
	CY_CMD_OP_GET_PARAM,
	CY_CMD_OP_SET_PARAM,
	CY_CMD_OP_RESERVED_2,
	CY_CMD_OP_GET_CRC,
	CY_CMD_OP_WAIT_FOR_EVENT,
};

enum cyttsp6_cmd_status {
	CY_CMD_STATUS_SUCCESS,
	CY_CMD_STATUS_FAILURE,
};

#define CY_CMD_LDR_ENTER_STAT_SIZE			15

/* Operational Mode Command Sizes */
/* NULL Command */
#define CY_CMD_OP_NULL_CMD_SZ			1
#define CY_CMD_OP_NULL_RET_SZ			0
/* Get Parameter */
#define CY_CMD_OP_GET_PARAM_CMD_SZ		2
#define CY_CMD_OP_GET_PARAM_RET_SZ		6
/* Set Parameter */
#define CY_CMD_OP_SET_PARAM_CMD_SZ		7
#define CY_CMD_OP_SET_PARAM_RET_SZ		2
/* Get Config Block CRC */
#define CY_CMD_OP_GET_CFG_BLK_CRC_CMD_SZ	2
#define CY_CMD_OP_GET_CFG_BLK_CRC_RET_SZ	3
/* Wait For Event */
#define CY_CMD_OP_WAIT_FOR_EVENT_CMD_SZ		2

/* CaT Mode Command Sizes */
/* NULL Command */
#define CY_CMD_CAT_NULL_CMD_SZ			1
#define CY_CMD_CAT_NULL_RET_SZ			0
/* Get Config Row Size */
#define CY_CMD_CAT_GET_CFG_ROW_SIZE_CMD_SZ	1
#define CY_CMD_CAT_GET_CFG_ROW_SIZE_RET_SZ	2
/* Read Config Block */
#define CY_CMD_CAT_READ_CFG_BLK_CMD_SZ		6
#define CY_CMD_CAT_READ_CFG_BLK_RET_SZ		7 /* + Data */
#define CY_CMD_CAT_READ_CFG_BLK_RET_HDR_SZ	5
/* Write Config Block */
#define CY_CMD_CAT_WRITE_CFG_BLK_CMD_SZ		8 /* + Data + Security Key */
#define CY_CMD_CAT_WRITE_CFG_BLK_RET_SZ		5
#define CY_CMD_CAT_WRITE_CFG_BLK_CMD_HDR_SZ	6
/* Load BIST Self-Test Data */
#define CY_CMD_CAT_LOAD_BIST_ST_DATA_CMD_SZ	6 /* + Data */
#define CY_CMD_CAT_LOAD_BIST_ST_DATA_RET_SZ	4
/* Load Shorts Self-Test Data */
#define CY_CMD_CAT_LOAD_SHORTS_ST_DATA_CMD_SZ	6 /* + Data */
#define CY_CMD_CAT_LOAD_SHORTS_ST_DATA_RET_SZ	4
/* Run BIST Self-Test */
#define CY_CMD_CAT_RUN_BIST_ST_CMD_SZ		2
#define CY_CMD_CAT_RUN_BIST_ST_RET_SZ		3
/* Run Shorts Self-Test */
#define CY_CMD_CAT_RUN_SHORTS_ST_CMD_SZ		2
#define CY_CMD_CAT_RUN_BIST_ST_RET_SZ		3
/* Run Opens Self-Test */
#define CY_CMD_CAT_RUN_OPENS_ST_CMD_SZ		2
#define CY_CMD_CAT_RUN_OPENS_ST_RET_SZ		3
/* Run Capacitance Self-Test */
#define CY_CMD_CAT_RUN_CAP_ST_CMD_SZ		2
#define CY_CMD_CAT_RUN_CAP_ST_RET_SZ		3
/* Run Auto Short Self-Test */
#define CY_CMD_CAT_RUN_AUTOSHORTS_ST_CMD_SZ	2
#define CY_CMD_CAT_RUN_AUTOSHORTS_ST_RET_SZ	3
/* Get BIST Self-Test Results */
#define CY_CMD_CAT_GET_BIST_ST_RES_CMD_SZ	6
#define CY_CMD_CAT_GET_SELFTEST_RES_RET_SZ	9
/* Get Shorts Self-Test Results */
#define CY_CMD_CAT_GET_SHORTS_ST_RES_CMD_SZ	6
#define CY_CMD_CAT_GET_SHORTS_ST_RES_RET_SZ	6 /* + Data */
/* Get Opens Self-Test Results */
#define CY_CMD_CAT_GET_OPENS_ST_RES_CMD_SZ	6
#define CY_CMD_CAT_GET_OPENS_ST_RES_RET_SZ	5 /* + Data */
/* Get Capacitance Self-Test Results */
#define CY_CMD_CAT_GET_CAP_ST_RES_CMD_SZ	6
#define CY_CMD_CAT_GET_CAP_ST_RES_RET_SZ	5 /* + Data */
/* Get Auto Shorts Self-Test Results */
#define CY_CMD_CAT_GET_AUTOSHORTS_ST_RES_CMD_SZ	6
#define CY_CMD_CAT_GET_AUTOSHORTS_ST_RES_RET_SZ	6 /* + Data */
/* Calibrate IDACs */
#define CY_CMD_CAT_CALIBRATE_IDAC_CMD_SZ	2
#define CY_CMD_CAT_CALIBRATE_IDAC_RET_SZ	1
/* Initialize Baselines */
#define CY_CMD_CAT_INIT_BASELINE_CMD_SZ		2
#define CY_CMD_CAT_INIT_BASELINE_RET_SZ		1
/* Execute Panel Scan */
#define CY_CMD_CAT_EXECUTE_PANEL_SCAN_CMD_SZ	1
#define CY_CMD_CAT_EXECUTE_PANEL_SCAN_RET_SZ	1
/* Retrieve Panel Scan */
#define CY_CMD_CAT_RETRIEVE_PANEL_SCAN_CMD_SZ	6
#define CY_CMD_CAT_RETRIEVE_PANEL_SCAN_RET_SZ	5 /* + Data */
/* Start Sensor Data Mode */
#define CY_CMD_CAT_START_SENSOR_MODE_CMD_SZ	1 /* + Data */
#define CY_CMD_CAT_START_SENSOR_MODE_RET_SZ	0 /* + Data */
/* Stop Sensor Data Mode */
#define CY_CMD_CAT_STOP_SENSOR_MODE_CMD_SZ	1
#define CY_CMD_CAT_STOP_SENSOR_MODE_RET_SZ	0
/* Interrupt Pin Override */
#define CY_CMD_CAT_INT_PIN_OVERRIDE_CMD_SZ	2
#define CY_CMD_CAT_INT_PIN_OVERRIDE_RET_SZ	1
/* Retrieve Data Structure */
#define CY_CMD_CAT_RETRIEVE_DATA_STRUCT_CMD_SZ	6
#define CY_CMD_CAT_RETRIEVE_DATA_STRUCT_RET_SZ	5 /* + Data */
/* Verify Config Block CRC */
#define CY_CMD_CAT_VERIFY_CFG_BLK_CRC_CMD_SZ	2
#define CY_CMD_CAT_VERIFY_CFG_BLK_CRC_RET_SZ	5

/* RAM ID */
#define CY_RAM_ID_SCAN_TYPE			0x4B
#define CY_RAM_ID_REFRESH_INTERVAL		0x1B
#define CY_RAM_ID_TOUCHMODE_ENABLED		0x02 /* Enable proximity */

enum cyttsp6_calibrate_idacs_sensing_mode {
	CY_CI_SM_MUTCAP_FINE,
	CY_CI_SM_MUTCAP_BUTTON,
	CY_CI_SM_SELFCAP,
};

enum cyttsp6_initialize_baselines_sensing_mode {
	CY_IB_SM_MUTCAP = 1,
	CY_IB_SM_BUTTON = 2,
	CY_IB_SM_SELFCAP = 4,
};

enum cyttsp6_retrieve_data_structure_data_id {
	CY_RDS_DATAID_MUTCAP_SCAN,
	CY_RDS_DATAID_SELFCAP_SCAN,
	CY_RDS_DATAID_BUTTON_SCAN = 3,
};

enum cyttsp6_self_test_id {
	CY_ST_ID_NULL,
	CY_ST_ID_BIST,
	CY_ST_ID_SHORTS,
	CY_ST_ID_OPENS,
	CY_ST_ID_AUTOSHORTS,
	CY_ST_ID_CM_PANEL_TEST,
	CY_ST_ID_CM_BUTTON_TEST,
	CY_ST_ID_CP_PANEL_TEST,
	CY_ST_ID_CP_BUTTON_TEST,
};

enum cyttsp6_self_test_result {
	CY_ST_RESULT_PASS,
	CY_ST_RESULT_FAIL,
	CY_ST_RESULT_HOST_MUST_INTERPRET = 0xFF,
};

/* TOUCH PARSE */
/* abs signal capabilities offsets in the frameworks array */
enum cyttsp6_sig_caps {
	CY_SIGNAL_OST,
	CY_MIN_OST,
	CY_MAX_OST,
	CY_FUZZ_OST,
	CY_FLAT_OST,
	CY_NUM_ABS_SET	/* number of signal capability fields */
};

/* helpers */
#define NUM_SIGNALS(frmwrk)		((frmwrk)->size / CY_NUM_ABS_SET)
#define PARAM(frmwrk, sig_ost, cap_ost) \
		((frmwrk)->abs[((sig_ost) * CY_NUM_ABS_SET) + (cap_ost)])

#define PARAM_SIGNAL(frmwrk, sig_ost)	PARAM(frmwrk, sig_ost, CY_SIGNAL_OST)
#define PARAM_MIN(frmwrk, sig_ost)	PARAM(frmwrk, sig_ost, CY_MIN_OST)
#define PARAM_MAX(frmwrk, sig_ost)	PARAM(frmwrk, sig_ost, CY_MAX_OST)
#define PARAM_FUZZ(frmwrk, sig_ost)	PARAM(frmwrk, sig_ost, CY_FUZZ_OST)
#define PARAM_FLAT(frmwrk, sig_ost)	PARAM(frmwrk, sig_ost, CY_FLAT_OST)

/* abs axis signal offsets in the framworks array  */
enum cyttsp6_sig_ost {
	CY_ABS_X_OST,
	CY_ABS_Y_OST,
	CY_ABS_P_OST,
	CY_ABS_W_OST,
	CY_ABS_ID_OST,
	CY_ABS_MAJ_OST,
	CY_ABS_MIN_OST,
	CY_ABS_OR_OST,
	CY_ABS_TOOL_OST,
	CY_ABS_D_OST,
	CY_NUM_ABS_OST	/* number of abs signals */
};

enum cyttsp6_tch_abs {	/* for ordering within the extracted touch data array */
	CY_TCH_X,	/* X */
	CY_TCH_Y,	/* Y */
	CY_TCH_P,	/* P (Z) */
	CY_TCH_T,	/* TOUCH ID */
	CY_TCH_E,	/* EVENT ID */
	CY_TCH_O,	/* OBJECT ID */
	CY_TCH_W,	/* SIZE */
	CY_TCH_MAJ,	/* TOUCH_MAJOR */
	CY_TCH_MIN,	/* TOUCH_MINOR */
	CY_TCH_OR,	/* ORIENTATION */
	CY_TCH_NUM_ABS
};

static const char * const cyttsp6_tch_abs_string[] = {
	[CY_TCH_X]	= "X",
	[CY_TCH_Y]	= "Y",
	[CY_TCH_P]	= "P",
	[CY_TCH_T]	= "T",
	[CY_TCH_E]	= "E",
	[CY_TCH_O]	= "O",
	[CY_TCH_W]	= "W",
	[CY_TCH_MAJ]	= "MAJ",
	[CY_TCH_MIN]	= "MIN",
	[CY_TCH_OR]	= "OR",
	[CY_TCH_NUM_ABS] = "INVALID"
};

/* scan_type ram id, scan values */
#define CY_SCAN_TYPE_GLOVE		0x8
#define CY_SCAN_TYPE_STYLUS		0x10
#define CY_SCAN_TYPE_PROXIMITY		0x40
#define CY_SCAN_TYPE_APA_MC		0x80

enum cyttsp6_scan_type {
	CY_ST_APA_MC,
	CY_ST_GLOVE,
	CY_ST_STYLUS,
	CY_ST_PROXIMITY,
};


/* DRIVER STATES */
enum cyttsp6_mode {
	CY_MODE_UNKNOWN      = 0,
	CY_MODE_BOOTLOADER   = (1 << 0),
	CY_MODE_OPERATIONAL  = (1 << 1),
	CY_MODE_SYSINFO      = (1 << 2),
	CY_MODE_CAT          = (1 << 3),
	CY_MODE_STARTUP      = (1 << 4),
	CY_MODE_LOADER       = (1 << 5),
	CY_MODE_CHANGE_MODE  = (1 << 6),
	CY_MODE_CHANGED      = (1 << 7),
	CY_MODE_CMD_COMPLETE = (1 << 8),
};

enum cyttsp6_int_state {
	CY_INT_NONE,
	CY_INT_IGNORE      = (1 << 0),
	CY_INT_MODE_CHANGE = (1 << 1),
	CY_INT_EXEC_CMD    = (1 << 2),
	CY_INT_AWAKE       = (1 << 3),
};

enum cyttsp6_sleep_state {
	SS_SLEEP_OFF,
	SS_SLEEP_ON,
	SS_SLEEPING,
	SS_WAKING,
};

enum cyttsp6_startup_state {
	STARTUP_NONE,
	STARTUP_QUEUED,
	STARTUP_RUNNING,
};

enum cyttsp6_atten_type {
	CY_ATTEN_IRQ,
	CY_ATTEN_STARTUP,
	CY_ATTEN_EXCLUSIVE,
	CY_ATTEN_WAKE,
	CY_ATTEN_LOADER,
	CY_ATTEN_SUSPEND,
	CY_ATTEN_RESUME,
	CY_ATTEN_NUM_ATTEN,
};

enum cyttsp6_module_id {
	CY_MODULE_MT,
	CY_MODULE_BTN,
	CY_MODULE_PROX,
	CY_MODULE_DEBUG,
	CY_MODULE_LOADER,
	CY_MODULE_DEVICE_ACCESS,
	CY_MODULE_LAST,
};

enum cyttsp6_fb_state {
	FB_ON,
	FB_OFF,
};

struct cyttsp6_sysinfo_ptr {
	struct cyttsp6_cydata *cydata;
	struct cyttsp6_test *test;
	struct cyttsp6_pcfg *pcfg;
	struct cyttsp6_opcfg *opcfg;
	struct cyttsp6_ddata *ddata;
	struct cyttsp6_mdata *mdata;
};

struct cyttsp6_touch {
	int abs[CY_TCH_NUM_ABS];
};

struct cyttsp6_tch_abs_params {
	size_t ofs;	/* abs byte offset */
	size_t size;	/* size in bytes */
	size_t max;	/* max value */
	size_t bofs;	/* bit offset */
};

struct cyttsp6_sysinfo_ofs {
	size_t chip_type;
	size_t cmd_ofs;
	size_t rep_ofs;
	size_t rep_sz;
	size_t num_btns;
	size_t num_btn_regs;	/* ceil(num_btns/4) */
	size_t tt_stat_ofs;
	size_t tch_rec_size;
	size_t obj_cfg0;
	size_t max_tchs;
	size_t mode_size;
	size_t data_size;
	size_t rep_hdr_size;
	size_t map_sz;
	size_t max_x;
	size_t x_origin;	/* left or right corner */
	size_t max_y;
	size_t y_origin;	/* upper or lower corner */
	size_t max_p;
	size_t len_x;
	size_t cydata_ofs;
	size_t test_ofs;
	size_t pcfg_ofs;
	size_t opcfg_ofs;
	size_t ddata_ofs;
	size_t mdata_ofs;
	size_t cydata_size;
	size_t test_size;
	size_t pcfg_size;
	size_t opcfg_size;
	size_t ddata_size;
	size_t mdata_size;
	size_t btn_keys_size;
	struct cyttsp6_tch_abs_params tch_abs[CY_TCH_NUM_ABS];
	size_t btn_rec_size; /* btn record size (in bytes) */
	size_t btn_diff_ofs;/* btn data loc ,diff counts, (Op-Mode byte ofs) */
	size_t btn_diff_size;/* btn size of diff counts (in bits) */
	size_t noise_data_ofs;
	size_t noise_data_sz;
};

/* button to keycode support */
#define CY_NUM_BTN_PER_REG		4
#define CY_BITS_PER_BTN			2

enum cyttsp6_btn_state {
	CY_BTN_RELEASED,
	CY_BTN_PRESSED,
	CY_BTN_NUM_STATE
};

struct cyttsp6_btn {
	bool enabled;
	int state;	/* CY_BTN_PRESSED, CY_BTN_RELEASED */
	int key_code;
};

struct cyttsp6_ttconfig {
	u16 version;
	u16 length;
	u16 max_length;
	u8  gesture_enable;
	u16 crc;
};

struct cyttsp6_sysinfo {
	bool ready;
	struct cyttsp6_sysinfo_data si_data;
	struct cyttsp6_sysinfo_ptr si_ptrs;
	struct cyttsp6_sysinfo_ofs si_ofs;
	struct cyttsp6_ttconfig ttconfig;
	struct cyttsp6_btn *btn;	/* button states */
	u8 *btn_rec_data;		/* button diff count data */
	u8 *xy_mode;			/* operational mode and status regs */
	u8 *xy_data;			/* operational touch regs */
};

/* device_access */
enum cyttsp6_ic_grpnum {
	CY_IC_GRPNUM_RESERVED,
	CY_IC_GRPNUM_CMD_REGS,
	CY_IC_GRPNUM_TCH_REP,
	CY_IC_GRPNUM_DATA_REC,
	CY_IC_GRPNUM_TEST_REC,
	CY_IC_GRPNUM_PCFG_REC,
	CY_IC_GRPNUM_TCH_PARM_VAL,
	CY_IC_GRPNUM_TCH_PARM_SIZE,
	CY_IC_GRPNUM_RESERVED1,
	CY_IC_GRPNUM_RESERVED2,
	CY_IC_GRPNUM_OPCFG_REC,
	CY_IC_GRPNUM_DDATA_REC,
	CY_IC_GRPNUM_MDATA_REC,
	CY_IC_GRPNUM_TEST_REGS,
	CY_IC_GRPNUM_BTN_KEYS,
	CY_IC_GRPNUM_TTHE_REGS,
	CY_IC_GRPNUM_NUM
};

/* test mode NULL command driver codes */
enum cyttsp6_null_test_cmd_code {
	CY_NULL_CMD_NULL,
	CY_NULL_CMD_MODE,
	CY_NULL_CMD_STATUS_SIZE,
	CY_NULL_CMD_HANDSHAKE,
	CY_NULL_CMD_LOW_POWER,
};

struct cyttsp6_test_mode_params {
	int cur_mode;
	int cur_cmd;
	size_t cur_status_size;
};

/* FW file name */
#define CY_FW_FILE_NAME			"cyttsp6_fw.bin"

/* Communication bus values */
#define CY_DEFAULT_ADAP_MAX_XFER	512
#define CY_ADAP_MIN_XFER		64

/* Core module */
#define CY_DEFAULT_CORE_ID		"cyttsp6_core0"
#define CY_MAX_NUM_CORE_DEVS		5

struct cyttsp6_mt_data;
struct cyttsp6_mt_function {
	int (*mt_release)(struct device *dev);
	int (*mt_probe)(struct device *dev, struct cyttsp6_mt_data *md);
	void (*report_slot_liftoff)(struct cyttsp6_mt_data *md, int max_slots);
	void (*input_sync)(struct input_dev *input);
	void (*input_report)(struct input_dev *input, int sig, int t, int type);
	void (*final_sync)(struct input_dev *input, int max_slots,
			int mt_sync_count, unsigned long *ids);
	int (*input_register_device)(struct input_dev *input, int max_slots);
};

struct cyttsp6_mt_data {
	struct device *dev;
	struct cyttsp6_mt_platform_data *pdata;
	struct cyttsp6_sysinfo *si;
	struct input_dev *input;
	struct cyttsp6_mt_function mt_function;
	struct mutex mt_lock;
	bool is_suspended;
	bool input_device_registered;
	char phys[NAME_MAX];
	int num_prv_rec; /* Number of previous touch records */
	int or_min;
	int or_max;
	int t_min;
	int t_max;
};

struct cyttsp6_btn_data {
	struct device *dev;
	struct cyttsp6_btn_platform_data *pdata;
	struct cyttsp6_sysinfo *si;
	struct input_dev *input;
	struct mutex btn_lock;
	bool is_suspended;
	bool input_device_registered;
	char phys[NAME_MAX];
};

struct cyttsp6_proximity_data {
	struct device *dev;
	struct cyttsp6_proximity_platform_data *pdata;
	struct cyttsp6_sysinfo *si;
	struct input_dev *input;
	struct mutex prox_lock;
	struct mutex sysfs_lock;
	int enable_count;
	bool input_device_registered;
	char phys[NAME_MAX];
};

typedef int (*cyttsp6_atten_func) (struct device *);

struct cyttsp6_core_commands {
	int (*subscribe_attention)(struct device *dev,
		enum cyttsp6_atten_type type, char id, cyttsp6_atten_func func,
		int flags);
	int (*unsubscribe_attention)(struct device *dev,
		enum cyttsp6_atten_type type, char id, cyttsp6_atten_func func,
		int flags);
	int (*request_exclusive)(struct device *dev, int timeout_ms);
	int (*release_exclusive)(struct device *dev);
	int (*request_reset)(struct device *dev);
	int (*request_restart)(struct device *dev, bool wait);
	int (*request_set_mode)(struct device *dev, int mode);
	struct cyttsp6_sysinfo * (*request_sysinfo)(struct device *dev);
	struct cyttsp6_loader_platform_data
		*(*request_loader_pdata)(struct device *dev);
	int (*request_handshake)(struct device *dev, u8 mode);
	int (*request_exec_cmd)(struct device *dev, u8 mode, u8 *cmd_buf,
		size_t cmd_size, u8 *return_buf, size_t return_buf_size,
		int timeout_ms);
	int (*request_stop_wd)(struct device *dev);
	int (*request_toggle_lowpower)(struct device *dev, u8 mode);
	int (*request_config_row_size)(struct device *dev,
		u16 *config_row_size);
	int (*request_write_config)(struct device *dev, u8 ebid, u16 offset,
		u8 *data, u16 length);
	int (*request_enable_scan_type)(struct device *dev, u8 scan_type);
	int (*request_disable_scan_type)(struct device *dev, u8 scan_type);
	const u8 * (*get_security_key)(struct device *dev, int *size);
	void (*get_touch_record)(struct device *dev, int rec_no, int *rec_abs);
	int (*write)(struct device *dev, int mode, u16 addr, const void *buf,
			int size);
	int (*read)(struct device *dev, int mode, u16 addr, void *buf,
			int size);
	u16 (*calc_app_crc)(const u8 *data, int size);
};

#define NEED_SUSPEND_NOTIFIER \
	((LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)) \
	&& defined(CONFIG_PM_SLEEP) && defined(CONFIG_PM_RUNTIME))

struct cyttsp6_core_data {
	struct list_head node;
	char core_id[20];
	struct device *dev;
	struct list_head atten_list[CY_ATTEN_NUM_ATTEN];
	struct mutex system_lock;
	struct mutex atten_lock;
	enum cyttsp6_mode mode;
	enum cyttsp6_sleep_state sleep_state;
	enum cyttsp6_startup_state startup_state;
	int int_status;
	int cmd_toggle;
	struct cyttsp6_mt_data md;
	struct cyttsp6_btn_data bd;
	struct cyttsp6_proximity_data pd;
	int phys_num;
	void *cyttsp6_dynamic_data[CY_MODULE_LAST];
	struct cyttsp6_platform_data *pdata;
	struct cyttsp6_core_platform_data *cpdata;
	const struct cyttsp6_bus_ops *bus_ops;
	wait_queue_head_t wait_q;
	int irq;
#if NEED_SUSPEND_NOTIFIER
	/*
	 * This notifier is used to receive suspend prepare events
	 * When device is PM runtime suspended, pm_generic_suspend()
	 * does not call our PM suspend callback for kernels with
	 * version less than 3.3.0.
	 */
	struct notifier_block pm_notifier;
#endif
	struct work_struct startup_work;
	struct cyttsp6_sysinfo sysinfo;
	void *exclusive_dev;
	int exclusive_waits;
	atomic_t ignore_irq;
	bool irq_enabled;
	bool irq_wake;
	bool wake_initiated_by_device;
	bool invalid_touch_app;
	bool bl_fast_exit;
	int max_xfer;
	int apa_mc_en;
	int glove_en;
	int stylus_en;
	int proximity_en;
	u8 default_scantype;
	u8 easy_wakeup_gesture;
	unsigned int active_refresh_cycle_ms;
	u8 heartbeat_count;
	struct work_struct watchdog_work;
	struct timer_list watchdog_timer;
	u8 wr_buf[CY_DEFAULT_ADAP_MAX_XFER];
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend es;
#elif defined(CONFIG_FB)
	struct notifier_block fb_notifier;
	enum cyttsp6_fb_state fb_state;
#endif
#ifdef VERBOSE_DEBUG
	u8 pr_buf[CY_MAX_PRBUF_SIZE];
	u8 rep_stat_counter;
#endif
};

struct cyttsp6_bus_ops {
	int (*write)(struct device *dev, u16 addr, u8 *wr_buf, const void *buf,
		int size, int max_xfer);
	int (*read)(struct device *dev, u16 addr, void *buf,
		int size, int max_xfer);
};

static inline void *cyttsp6_get_dynamic_data(struct device *dev, int id)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	return cd->cyttsp6_dynamic_data[id];
}

void cyttsp6_get_touch_record_(struct device *dev, int rec_no, int *rec_abs);
int cyttsp6_read_(struct device *dev, int mode, u16 addr, void *buf, int size);
int cyttsp6_write_(struct device *dev, int mode, u16 addr, const void *buf,
	int size);
int cyttsp6_request_exclusive(struct device *dev, int timeout_ms);
int cyttsp6_release_exclusive(struct device *dev);

static inline void cyttsp6_get_touch_record(struct device *dev, int rec_no,
		int *rec_abs)
{
	cyttsp6_get_touch_record_(dev, rec_no, rec_abs);
}

static inline int cyttsp6_read(struct device *dev, int mode, u16 addr,
	void *buf, int size)
{
	return cyttsp6_read_(dev, mode, addr, buf, size);
}

static inline int cyttsp6_write(struct device *dev, int mode, u16 addr,
	const void *buf, int size)
{
	return cyttsp6_write_(dev, mode, addr, buf, size);
}

#ifdef VERBOSE_DEBUG
extern void cyttsp6_pr_buf(struct device *dev, u8 *pr_buf, u8 *dptr, int size,
			   const char *data_name);
#else
#define cyttsp6_pr_buf(a, b, c, d, e) do { } while (0)
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_DEVICETREE_SUPPORT
int cyttsp6_devtree_create_and_get_pdata(struct device *adap_dev);
int cyttsp6_devtree_clean_pdata(struct device *adap_dev);
#else
static inline int cyttsp6_devtree_create_and_get_pdata(struct device *adap_dev)
{
	return 0;
}

static inline int cyttsp6_devtree_clean_pdata(struct device *adap_dev)
{
	return 0;
}
#endif

int cyttsp6_probe(const struct cyttsp6_bus_ops *ops, struct device *dev,
		u16 irq, size_t xfer_buf_size);
int cyttsp6_release(struct cyttsp6_core_data *cd);

struct cyttsp6_core_commands *cyttsp6_get_commands(void);
struct cyttsp6_core_data *cyttsp6_get_core_data(char *id);

int cyttsp6_mt_probe(struct device *dev);
int cyttsp6_mt_release(struct device *dev);

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_BUTTON
int cyttsp6_btn_probe(struct device *dev);
int cyttsp6_btn_release(struct device *dev);
#else
static inline int cyttsp6_btn_probe(struct device *dev) { return 0; }
static inline int cyttsp6_btn_release(struct device *dev) { return 0; }
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_PROXIMITY
int cyttsp6_proximity_probe(struct device *dev);
int cyttsp6_proximity_release(struct device *dev);
#else
static inline int cyttsp6_proximity_probe(struct device *dev) { return 0; }
static inline int cyttsp6_proximity_release(struct device *dev) { return 0; }
#endif

void cyttsp6_init_function_ptrs(struct cyttsp6_mt_data *md);
int cyttsp6_subscribe_attention_(struct device *dev,
	enum cyttsp6_atten_type type, char id, int (*func)(struct device *),
	int mode);
int cyttsp6_unsubscribe_attention_(struct device *dev,
	enum cyttsp6_atten_type type, char id, int (*func)(struct device *),
	int mode);
struct cyttsp6_sysinfo *cyttsp6_request_sysinfo_(struct device *dev);
int cyttsp6_request_disable_scan_type_(struct device *dev, u8 scan_type);
int cyttsp6_request_enable_scan_type_(struct device *dev, u8 scan_type);

static inline int is_crc_stat_failed(struct cyttsp6_sysinfo *si)
{
	int crc_stat;

	/* Check if device POST config CRC test failed */
	crc_stat = si->si_ptrs.test->post_codel &
			CY_POST_CODEL_CFG_DATA_CRC_FAIL;

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_TTSP20_MODE
	if (crc_stat)
		return 1;
#else
	if (!crc_stat)
		return 1;
#endif

	return 0;
}

extern const struct dev_pm_ops cyttsp6_pm_ops;

#endif /* _CYTTSP6_REGS_H */
