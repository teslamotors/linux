/* Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* The NVS = NVidia Sensor framework */
/* See nvs_iio.c and nvs.h for documentation */
/* See nvs_proximity.c and nvs_proximity.h for documentation */

/* !!!!!!!!!!!!!TODO: add IQS253 support */

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/nvs.h>
#include <linux/nvs_proximity.h>
#include <linux/nvs_iqs2x3.h>
#include <asm/atomic.h>


#define IQS_DRIVER_VERSION		(21)
#define IQS_VENDOR			"Azoteq"
#define IQS_NAME			"iqs2x3"
#define IQS_NAME_IQS253			"iqs253"
#define IQS_NAME_IQS263			"iqs263"
#define IQS_NAME_SAR_PROXIMITY		"SAR_proximity"
#define IQS_NAME_SAR_TOUCH		"SAR_touch"
#define IQS_NAME_SAR_DELTA		"SAR_delta"
#define IQS_NAME_SAR_DELTA_CH0		"SAR_delta_ch0"
#define IQS_NAME_SAR_DELTA_CH1		"SAR_delta_ch1"
#define IQS_NAME_SAR_DELTA_CH2		"SAR_delta_ch2"
#define IQS_NAME_SAR_DELTA_CH3		"SAR_delta_ch3"
#define IQS_GPIO_SAR_DBG_I2C_STR	"debug_i2c"
#define IQS_DEVID_IQS253		(0x29)
#define IQS_DEVID_IQS263		(0x3C)
#define IQS_PART_253			(0)
#define IQS_PART_263			(1)
#define IQS_PART_N			(2)
#define IQS_HW_DELAY_MS			(10)
#define IQS_START_DELAY_MS		(100)
#define IQS_PROX_MILLIAMP_MICRO		(180000)
#define IQS_PROX_THRESHOLD		(10)
#define IQS_MULTI_THRESHOLD		(5)
/* configuration */
#define IQS_POLL_DLY_MS_MIN		(1000)
#define IQS_POLL_DLY_MS_MAX		(1000)
#define IQS_POLL_DLY_MS_WATCHDOG	(30000)
#define IQS_I2C_STOP_DLY_NS		(1000000) /* must be >= 1ms */
#define IQS_I2C_RETRY_N			(10)
#define IQS_RDY_RETRY_N			(25)
#define IQS_RDY_DELAY_N			(2000)
#define IQS_RDY_DELAY_US_MIN		(100)
#define IQS_RDY_DELAY_US_MAX		(110)
/* proximity defines */
#define IQS_PROX_VERSION		(1)
/* binary proximity when max_range and resolution are 1.0 */
#define IQS_PROX_MAX_RANGE_IVAL		(1)
#define IQS_PROX_MAX_RANGE_MICRO	(0)
#define IQS_PROX_RESOLUTION_IVAL	(1)
#define IQS_PROX_RESOLUTION_MICRO	(0)
#define IQS_PROX_MILLIAMP_IVAL		(0)
/* devices */
#define IQS_DEV_PROX			(0)
#define IQS_DEV_TOUCH			(1)
#define IQS_DEV_HW_N			(2)
#define IQS_DEV_DELTA			(2)
#define IQS_DEV_DELTA0			(3)
#define IQS_DEV_DELTA1			(4)
#define IQS_DEV_DELTA2			(5)
#define IQS_DEV_DELTA3			(6)
#define IQS_DEV_N			(7)
#define IQS_CH_N			(4)
/* to allow code style rules */
#define IQS_FS_NANO			NVS_FLOAT_SIGNIFICANCE_NANO
#define IQS_FS_MICRO			NVS_FLOAT_SIGNIFICANCE_MICRO
/* delta test conditions */
#define IQS_DELTA_TEST0_N		(3)
#define IQS_DELTA_TEST1_N		(4)
/* debug message spew flags */
#define IQS_STS_GPIO_SAR		(1 << NVS_STS_EXT_N)
#define IQS_STS_EXT_STATE		(1 << (NVS_STS_EXT_N + 1))

/* regulator names in order of powering on */
static char *iqs_vregs[] = {
	"vddhi",
};

static unsigned short iqs_i2c_addrs[] = {
	0x44,
	0x45,
	0x46,
	0x47,
};

static const struct i2c_device_id iqs_i2c_device_id[] = {
	{ IQS_NAME_IQS253, 0 },
	{ IQS_NAME_IQS263, 0 },
	{ IQS_NAME, 0 },
	{}
};

static char * const iqs_snsr_names[] = {
	IQS_NAME_SAR_PROXIMITY,
	IQS_NAME_SAR_TOUCH,
	IQS_NAME_SAR_DELTA,
	IQS_NAME_SAR_DELTA_CH0,
	IQS_NAME_SAR_DELTA_CH1,
	IQS_NAME_SAR_DELTA_CH2,
	IQS_NAME_SAR_DELTA_CH3,
	IQS_GPIO_SAR_DBG_I2C_STR,
};

enum IQS_GPIO_SAR_DBG {
	IQS_GPIO_SAR_DBG_I2C = IQS_DEV_N, /* IQS_GPIO_SAR_DBG_I2C_STR align */
	/* Additional debug uses for the SAR GPIO can be added here.  Be sure
	 * to add the gpio_sar_dev_asrt enable string to iqs_snsr_names.
	 */
	IQS_GPIO_SAR_DBG_N,
};

enum IQS_OS {
	IQS_OS_NONE = 0,
	IQS_OS_VISIBLE,			/* OS sees sensors */
	IQS_OS_CONTROL,			/* OS has control of sensors */
};

enum IQS_INFO {
	IQS_INFO_STS = 0,
	IQS_INFO_EXT_STATE_SPEW = 14,
	IQS_INFO_GPIO_SAR_SPEW,
	IQS_INFO_GPIO_RDY_INPUT,
	IQS_INFO_GPIO_RDY_OUTPUT,
	IQS_INFO_GPIO_SAR_OUTPUT,
	IQS_INFO_REG_WR,
	IQS_INFO_DBG,
};

enum IQS_DB_CMD {
	IQS_DB_CMD_INIT = 1,
	IQS_DB_CMD_EN,
	IQS_DB_CMD_DIS,
	IQS_DB_CMD_EVNT,
	IQS_DB_CMD_SUSPND,
	IQS_DB_CMD_DELTA,
	IQS_DB_CMD_EXT_LO,
	IQS_DB_CMD_EXT_HI,
	IQS_DB_CMD_N,
};

enum IQS_STREAM {
	IQS_STREAM_OFF = 0,
	IQS_STREAM_ALWAYS,
	IQS_STREAM_AUTO,
	IQS_STREAM_N,
};

struct iqs_delta_tst {
	int ch;
	int lt;
	int gt;
};

static const char * const iqs_delta_tst_dt[] = {
	"SAR_delta_test",
	"SAR_delta_test_true",
	"SAR_delta_test_false",
};

static unsigned char iqs263_wr_stream[] = {
	2, 0x09, 0x00, 0x00, /* data */
		 0x18, 0x40, /* mask */
	0 /* end - done - exit */
};

static unsigned char iqs253_wr_stream[] = {
	16, 0xC4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x04, /* data */
		  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x04, /* mask */
	0, /* end - done - exit */
};

static unsigned char iqs263_wr_events[] = {
	2, 0x09, 0x00, 0x40, /* data */
		 0x18, 0x40, /* mask */
	0 /* end - done - exit */
};

static unsigned char iqs253_wr_events[] = {
	16, 0xC4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, /* data */
		  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x04, /* mask */
	0 /* end - done - exit */
};

static unsigned char iqs263_wr_ati_redo[] = {
	1, 0x09, 0x10, /* data */
		 0x18, /* mask */
	0 /* end - done - exit */
};

static unsigned char iqs253_wr_ati_redo[] = {
	14, 0xC4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10, /* data */
		  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x18, /* mask */
	0 /* end - done - exit */
};

static unsigned char iqs263_wr_reseed[] = {
	1, 0x09, 0x08, /* data */
		 0x18, /* mask */
	0 /* end - done - exit */
};

static unsigned char iqs253_wr_reseed[] = {
	14, 0xC4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x08, /* data */
		  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x18, /* mask */
	0 /* end - done - exit */
};

static unsigned char iqs263_wr_disable[] = {
	3, 0x09, 0x00, 0x00, 0xC0, /* data */
		 0xFF, 0xFF, 0xFF, /* mask */
	0 /* end - done - exit */
};

static unsigned char iqs253_wr_disable[] = {
	/* TODO */
	0 /* end - done - exit */
};

struct iqs_hal {
	u8 reg;
	u16 len;
	u8 ndx;
	bool wr;
};

/* RI = Register Init */
/* RL = Register Length */
/* BI = Buffer Index */
#define IQS_BI_DEVINF			(0)
#define IQS_RL_DEVINF			(2)
/* IQS263 */
#define IQS263_BI_SYSFLAGS		(IQS_BI_DEVINF + \
					 IQS_RL_DEVINF + 1)
#define IQS263_RL_SYSFLAGS		(2)
#define IQS263_BI_COORDINATES		(IQS263_BI_SYSFLAGS + \
					 IQS263_RL_SYSFLAGS + 1)
#define IQS263_RL_COORDINATES		(3)
#define IQS263_BI_TOUCH			(IQS263_BI_COORDINATES + \
					 IQS263_RL_COORDINATES + 1)
#define IQS263_RL_TOUCH			(2)
#define IQS263_BI_COUNTS		(IQS263_BI_TOUCH + \
					 IQS263_RL_TOUCH + 1)
#define IQS263_RL_COUNTS		(10)
#define IQS263_BI_LTA			(IQS263_BI_COUNTS + \
					 IQS263_RL_COUNTS + 1)
#define IQS263_RL_LTA			(10)
#define IQS263_BI_DELTAS		(IQS263_BI_LTA + \
					 IQS263_RL_LTA + 1)
#define IQS263_RL_DELTAS		(8)
#define IQS263_BI_MULTIPLIERS		(IQS263_BI_DELTAS + \
					 IQS263_RL_DELTAS + 1)
#define IQS263_RL_MULTIPLIERS		(5)
#define IQS263_BI_COMPENSATION		(IQS263_BI_MULTIPLIERS + \
					 IQS263_RL_MULTIPLIERS + 1)
#define IQS263_RL_COMPENSATION		(4)
#define IQS263_BI_PROXSETTINGS		(IQS263_BI_COMPENSATION + \
					 IQS263_RL_COMPENSATION + 1)
#define IQS263_RL_PROXSETTINGS		(5)
#define IQS263_BI_THRESHOLDS		(IQS263_BI_PROXSETTINGS + \
					 IQS263_RL_PROXSETTINGS + 1)
#define IQS263_RL_THRESHOLDS		(8)
#define IQS263_BI_TIMINGS		(IQS263_BI_THRESHOLDS + \
					 IQS263_RL_THRESHOLDS + 1)
#define IQS263_RL_TIMINGS		(3)
#define IQS263_BI_GESTURE_TIMERS	(IQS263_BI_TIMINGS + \
					 IQS263_RL_TIMINGS + 1)
#define IQS263_RL_GESTURE_TIMERS	(3)
#define IQS263_BI_ACTIVE_CH		(IQS263_BI_GESTURE_TIMERS + \
					 IQS263_RL_GESTURE_TIMERS + 1)
#define IQS263_RL_ACTIVE_CH		(1)
#define IQS263_BI_ENG			(IQS263_BI_ACTIVE_CH + \
					 IQS263_RL_ACTIVE_CH + 1)
#define IQS263_RL_ENG			(1)
#define IQS263_BI_N			(IQS263_BI_ENG + \
					 IQS263_RL_ENG + 1)
/* IQS253 */
#define IQS253_BI_SYSFLAGS		(IQS_BI_DEVINF + \
					 IQS_RL_DEVINF + 1)
#define IQS253_RL_SYSFLAGS		(1)
#define IQS253_BI_PROXSTS		(IQS253_BI_SYSFLAGS + \
					 IQS253_RL_SYSFLAGS + 1)
#define IQS253_RL_PROXSTS		(1)
#define IQS253_BI_TOUCHSTS		(IQS253_BI_PROXSTS + \
					 IQS253_RL_PROXSTS + 1)
#define IQS253_RL_TOUCHSTS		(3)
#define IQS253_BI_HALT			(IQS253_BI_TOUCHSTS + \
					 IQS253_RL_TOUCHSTS + 1)
#define IQS253_RL_HALT			(1)
#define IQS253_BI_ACTIVE_CH		(IQS253_BI_HALT + \
					 IQS253_RL_HALT + 1)
#define IQS253_RL_ACTIVE_CH		(1)
#define IQS253_BI_COUNTS		(IQS253_BI_ACTIVE_CH + \
					 IQS253_RL_ACTIVE_CH + 1)
#define IQS253_RL_COUNTS		(2)
#define IQS253_BI_LTA			(IQS253_BI_COUNTS + \
					 IQS253_RL_COUNTS + 1)
#define IQS253_RL_LTA			(2)
#define IQS253_BI_SETTINGS		(IQS253_BI_LTA + \
					 IQS253_RL_LTA + 1)
#define IQS253_RL_SETTINGS		(26)
#define IQS253_BI_N			(IQS253_BI_SETTINGS + \
					 IQS253_RL_SETTINGS + 1)

#if IQS263_BI_N < IQS253_BI_N
#define IQS_BI_N			(IQS253_BI_N)
#else
#define IQS_BI_N			(IQS263_BI_N)
#endif /* IQS263_BI_N < IQS253_BI_N */

static const struct iqs_hal const iqs263_hal_tbl[] = {
	{ 0x00, IQS_RL_DEVINF, IQS_BI_DEVINF, false, },
	{ 0x01, IQS263_RL_SYSFLAGS, IQS263_BI_SYSFLAGS, true, },
	{ 0x02, IQS263_RL_COORDINATES, IQS263_BI_COORDINATES, false, },
	{ 0x03, IQS263_RL_TOUCH, IQS263_BI_TOUCH, false, },
	{ 0x04, IQS263_RL_COUNTS, IQS263_BI_COUNTS, false, },
	{ 0x05, IQS263_RL_LTA, IQS263_BI_LTA, false, },
	{ 0x06, IQS263_RL_DELTAS, IQS263_BI_DELTAS, false, },
	{ 0x07, IQS263_RL_MULTIPLIERS, IQS263_BI_MULTIPLIERS, true, },
	{ 0x08, IQS263_RL_COMPENSATION, IQS263_BI_COMPENSATION, true, },
	{ 0x09, IQS263_RL_PROXSETTINGS, IQS263_BI_PROXSETTINGS, true, },
	{ 0x0A, IQS263_RL_THRESHOLDS, IQS263_BI_THRESHOLDS, true, },
	{ 0x0B, IQS263_RL_TIMINGS, IQS263_BI_TIMINGS, true, },
	{ 0x0C, IQS263_RL_GESTURE_TIMERS, IQS263_BI_GESTURE_TIMERS, true, },
	{ 0x0D, IQS263_RL_ACTIVE_CH, IQS263_BI_ACTIVE_CH, true, },
	{ 0x48, IQS263_RL_ENG, IQS263_BI_ENG, true, },
};

static const struct iqs_hal const iqs253_hal_tbl[] = {
	{ 0x00, IQS_RL_DEVINF, IQS_BI_DEVINF, false, },
	{ 0x10, IQS253_RL_SYSFLAGS, IQS253_BI_SYSFLAGS, false, },
	{ 0x31, IQS253_RL_PROXSTS, IQS253_BI_PROXSTS, false, },
	{ 0x35, IQS253_RL_TOUCHSTS, IQS253_BI_TOUCHSTS, false, },
	{ 0x39, IQS253_RL_HALT, IQS253_BI_HALT, false, },
	{ 0x3D, IQS253_RL_ACTIVE_CH, IQS253_BI_ACTIVE_CH, false, },
	{ 0x42, IQS253_RL_COUNTS, IQS253_BI_COUNTS, false, },
	{ 0x83, IQS253_RL_LTA, IQS253_BI_LTA, true, },
	{ 0xC4, IQS253_RL_SETTINGS, IQS253_BI_SETTINGS, true, },
};

#define IQS_DT_INIT_N			(96) /* max DT init bytes */
#define IQS_DT_ABLE_N			(32) /* max DT en/dis-able bytes */
#define IQS_DT_EVNT_N			(32) /* max DT event bytes */
#define IQS_DT_EXT_N			(32) /* max DT external state bytes */
#define IQS_DT_SUSPND_N			(64) /* max DT suspend bytes */
#define IQS263_MSG_N			(15)
#define IQS253_MSG_N			(9)

#if IQS263_MSG_N < IQS253_MSG_N
#define IQS_MSG_N			(IQS253_MSG_N * 2)
#else
#define IQS_MSG_N			(IQS263_MSG_N * 2)
#endif /* IQS263_MSG_N < IQS253_MSG_N */

struct iqs_hal_iom {
	u8 hal_i;
	u8 offset;
	u8 mask;
};

struct iqs_hal_bit {
	struct iqs_hal_iom devinf_id;
	struct iqs_hal_iom sysflag_reset;
	struct iqs_hal_iom ati_err;
	struct iqs_hal_iom ati_busy;
	struct iqs_hal_iom event_mode;
	struct iqs_hal_iom ati_partial;
	struct iqs_hal_iom active_ch;
	struct iqs_hal_iom lta;
	struct iqs_hal_iom delta;
	struct iqs_hal_iom multi_comp;
	struct iqs_hal_iom multi_sens;
	struct iqs_hal_iom touch_prx;
	struct iqs_hal_iom touch_tch;
	struct iqs_hal_iom count_prx;
	struct iqs_hal_iom count_tch;
	struct iqs_hal_iom thresh_prx;
	struct iqs_hal_iom thresh_tch;
};

static const struct iqs_hal_bit iqs263_hal_bit = {
	.devinf_id			= {
		.hal_i			= 0,
		.offset			= 0,
		.mask			= 0xFF,
	},
	.sysflag_reset			= {
		.hal_i			= 1,
		.offset			= 0,
		.mask			= 0x80,
	},
	.ati_err			= {
		.hal_i			= 1,
		.offset			= 0,
		.mask			= 0x20,
	},
	.ati_busy			= {
		.hal_i			= 1,
		.offset			= 0,
		.mask			= 0x04,
	},
	.event_mode			= {
		.hal_i			= 9,
		.offset			= 1,
		.mask			= 0x40,
	},
	.ati_partial			= {
		.hal_i			= 9,
		.offset			= 0,
		.mask			= 0x40,
	},
	.active_ch			= {
		.hal_i			= 0x0D,
		.offset			= 0,
		.mask			= 0x0F,
	},
	.lta				= {
		.hal_i			= 5,
		.offset			= 0,
		.mask			= 0xFF,
	},
	.delta				= {
		.hal_i			= 6,
		.offset			= 0,
		.mask			= 0xFF,
	},
	.multi_comp			= {
		.hal_i			= 7,
		.offset			= 0,
		.mask			= 0x0F,
	},
	.multi_sens			= {
		.hal_i			= 7,
		.offset			= 0,
		.mask			= 0x30,
	},
	.touch_prx			= {
		.hal_i			= 3,
		.offset			= 0,
		.mask			= 0x01,
	},
	.touch_tch			= {
		.hal_i			= 3,
		.offset			= 0,
		.mask			= 0x0E,
	},
	.count_prx			= {
		.hal_i			= 4,
		.offset			= 0,
		.mask			= 1,
	},
	.count_tch			= {
		.hal_i			= 4,
		.offset			= 4,
		.mask			= 3,
	},
	.thresh_prx			= {
		.hal_i			= 10,
		.offset			= 0,
		.mask			= 1,
	},
	.thresh_tch			= {
		.hal_i			= 10,
		.offset			= 1,
		.mask			= 3,
	},
};

static const struct iqs_hal_bit iqs253_hal_bit = {
	.devinf_id			= {
		.hal_i			= 0,
		.offset			= 0,
		.mask			= 0xFF,
	},
	.sysflag_reset			= {
		.hal_i			= 1,
		.offset			= 0,
		.mask			= 0x20,
	},
	.thresh_prx			= {
		.hal_i			= 10,
		.offset			= 0,
		.mask			= 1,
	},
	.thresh_tch			= {
		.hal_i			= 10,
		.offset			= 1,
		.mask			= 3,
	},
	.count_prx			= {
		.hal_i			= 4,
		.offset			= 0,
		.mask			= 1,
	},
	.count_tch			= {
		.hal_i			= 4,
		.offset			= 4,
		.mask			= 3,
	},
};

struct iqs_state {
	struct i2c_client *i2c;
	struct nvs_fn_if *nvs;
	struct sensor_cfg cfg[IQS_DEV_N];
	struct delayed_work dw;
	struct regulator_bulk_data vreg[ARRAY_SIZE(iqs_vregs)];
	struct nvs_proximity prox[IQS_DEV_N];
	unsigned int sts;		/* status flags */
	unsigned int errs;		/* error count */
	unsigned int enabled;		/* enable status */
	unsigned int susrsm_en;		/* suspend/resume enable status */
	unsigned int dfr_rsm_ms;	/* ms to wait before resuming */
	u16 i2c_addr;			/* I2C address */
	u8 dev_id;			/* device ID */
	bool irq_dis;			/* interrupt disable flag */
	bool irq_set_irq_wake;		/* if irq_set_irq_wake is enabled */
	bool irq_trigger_edge;		/* if irq set for edge trigger */
	bool suspend_dis;		/* active during suspend */
	bool susrsm;			/* suspend/resume - exit I2C early */
	bool resume;			/* resume action needed */
	int op_i;			/* operational index */
	int op_read_n;			/* operational register read count */
	int op_read_reg[IQS_DEV_N + 2];	/* operational registers to read */
	unsigned int part_i;		/* part index */
	unsigned int dbg;		/* nvs debug interface */
	unsigned int os;		/* OS options */
	unsigned int stream;		/* configured for stream mode only */
	unsigned int delta_ch_msk;	/* delta sensors enable channel mask */
	unsigned int delta_avg_n;	/* delta sensors moving average cnt */
	unsigned int dbnc_lo[IQS_DEV_HW_N]; /* binary low debounce */
	unsigned int dbnc_hi[IQS_DEV_HW_N]; /* binary high debounce */
	unsigned int dbnc_lo_n[IQS_DEV_HW_N]; /* binary low debounce count */
	unsigned int dbnc_hi_n[IQS_DEV_HW_N]; /* binary high debounce count */
	unsigned int ati_redo_n;	/* ATI redo count */
	unsigned int wd_to_ms;		/* watchdog timeout ms */
	unsigned int gpio_rdy_dly_n;	/* GPIO RDY delay loop count */
	unsigned int gpio_rdy_dly_min;	/* GPIO RDY delay us min */
	unsigned int gpio_rdy_dly_max;	/* GPIO RDY delay us max */
	unsigned int gpio_rdy_retry;	/* GPIO RDY assert loop limit */
	unsigned int i2c_retry;		/* I2C transaction loop limit */
	s64 i2c_ss_war_ns;		/* I2C stop/start delay WAR */
	s64 i2c_stop_ts;		/* see IQS_I2C_STOP_DLY_NS */
	int gpio_rdy;			/* GPIO */
	int gpio_sar;			/* GPIO */
	int gpio_sar_val;		/* current GPIO state */
	int gpio_sar_sus_asrt;		/* GPIO assertion when suspending */
	unsigned int gpio_sar_asrt_pol;	/* GPIO SAR assert polarity */
	unsigned int gpio_sar_dev_asrt;	/* device that asserts SAR GPIO */
	unsigned int gpio_sar_dev_dasrt; /* device that deasserts SAR GPIO */
	unsigned int msg_n;		/* I2C transaction count */
	struct i2c_msg msg[IQS_MSG_N];	/* max possible I2C transactions */
	const struct iqs_hal *hal_tbl;	/* HAL register table */
	unsigned int hal_tbl_n;		/* HAL register table count */
	const struct iqs_hal_bit *hal_bit; /* HAL for specific bits */
	unsigned char *wr_disable;	/* byte stream to disable device */
	unsigned char *wr_stream;	/* byte stream for stream mode */
	unsigned char *wr_events;	/* byte stream for event mode */
	unsigned char *wr_ati_redo;	/* byte stream to do ATI redo */
	unsigned char *wr_reseed;	/* byte stream to do reseed */
	unsigned char dt_init[IQS_PART_N][IQS_DT_INIT_N]; /* DT byte stream */
	unsigned char dt_en[IQS_PART_N][IQS_DEV_HW_N][IQS_DT_ABLE_N]; /* " */
	unsigned char dt_dis[IQS_PART_N][IQS_DEV_HW_N][IQS_DT_ABLE_N]; /* " */
	unsigned char dt_evnt[IQS_PART_N][IQS_DT_EVNT_N]; /* DT byte stream */
	unsigned char dt_suspnd[IQS_PART_N][IQS_DT_SUSPND_N]; /* " */
	unsigned char dt_ext_lo[IQS_PART_N][IQS_DT_EXT_N]; /* DT byte stream */
	unsigned char dt_ext_hi[IQS_PART_N][IQS_DT_EXT_N]; /* DT byte stream */
	u8 rc[IQS_BI_N];		/* register cache */
	u16 *delta_avg[IQS_CH_N];	/* delta moving average data */
	struct iqs_delta_tst delta_tst[IQS_DELTA_TEST0_N][IQS_DELTA_TEST1_N];
};


static int iqs_ext_sts;
static atomic_t iqs_ext_sts_chg = ATOMIC_INIT(0);


static void iqs_err(struct iqs_state *st)
{
	st->errs++;
	if (!st->errs)
		st->errs--;
}

static void iqs_mutex_lock(struct iqs_state *st)
{
	unsigned int i;

	if (st->nvs) {
		for (i = 0; i < IQS_DEV_N; i++) {
			if (st->prox[i].nvs_st)
				st->nvs->nvs_mutex_lock(st->prox[i].nvs_st);
		}
	}
}

static void iqs_mutex_unlock(struct iqs_state *st)
{
	unsigned int i;

	if (st->nvs) {
		for (i = 0; i < IQS_DEV_N; i++) {
			if (st->prox[i].nvs_st)
				st->nvs->nvs_mutex_unlock(st->prox[i].nvs_st);
		}
	}
}

static unsigned int iqs_i2c_stop_ms(struct iqs_state *st)
{
	s64 i2c_stop_t;
	unsigned int ms = 0;

	i2c_stop_t = nvs_timestamp() - st->i2c_stop_ts;
	if (i2c_stop_t < st->i2c_ss_war_ns) {
		i2c_stop_t = st->i2c_ss_war_ns - i2c_stop_t;
		do_div(i2c_stop_t, 1000000); /* ns => ms */
		ms = i2c_stop_t;
		if (!ms)
			ms = 1;
	}
	return ms;
}

static void iqs_enable_irq(struct iqs_state *st)
{
	unsigned int ms;
	unsigned int i;

	if (st->irq_dis && st->i2c->irq) {
		/* ensure IRQ is high after I2C STOP when enabling */
		ms = iqs_i2c_stop_ms(st);
		if (ms) {
			ms <<= 1; /* *2 */
			for (i = 0; i < ms; i++) {
				if (gpio_get_value_cansleep(st->gpio_rdy))
					break;

				usleep_range(500, 1000);
			}
		}

		enable_irq(st->i2c->irq);
		st->irq_dis = false;
		if (st->sts & NVS_STS_SPEW_IRQ)
			dev_info(&st->i2c->dev, "%s delay=%ums\n",
				 __func__, ms >> 1);
	}
}

static void iqs_irq_restore(struct iqs_state *st, bool disable)
{
	if (!disable)
		iqs_enable_irq(st);
}

static void iqs_disable_irq(struct iqs_state *st)
{
	if ((!st->irq_dis) && st->i2c->irq) {
		disable_irq_nosync(st->i2c->irq);
		st->irq_dis = true;
		if (st->sts & NVS_STS_SPEW_IRQ)
			dev_info(&st->i2c->dev, "%s\n", __func__);
	}
}

static int iqs_gpio_sar(struct iqs_state *st, int assert)
{
	int gpio_sar_val;
	int ret = 0;

	if (st->gpio_sar >= 0) {
		/* polarity   assertion   GPIO
		 *     0          0        1
		 *     0          1        0
		 *     1          0        0
		 *     1          1        1
		 */
		assert = !!assert;
		gpio_sar_val = !(st->gpio_sar_asrt_pol ^ assert);
		if (st->gpio_sar_val != gpio_sar_val) {
			ret = gpio_direction_output(st->gpio_sar,
						    gpio_sar_val);
			if (ret) {
				dev_err(&st->i2c->dev,
					"%s assert=%d gpio_sar %d=%d err=%d\n",
					__func__, assert, st->gpio_sar,
					gpio_sar_val, ret);
			} else {
				st->gpio_sar_val = gpio_sar_val;
				if (st->sts & (NVS_STS_SPEW_MSG |
					       IQS_STS_GPIO_SAR))
					dev_info(&st->i2c->dev,
					       "%s assert=%d gpio_sar %d=%d\n",
						 __func__, assert,
						 st->gpio_sar, gpio_sar_val);
			}
		}
	} else {
		ret = -EINVAL;
	}
	return ret;
}

static int iqs_gpio_rdy_poll(struct iqs_state *st)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < st->gpio_rdy_dly_n; i++) {
		ret = gpio_get_value_cansleep(st->gpio_rdy);
		if (st->susrsm || !ret)
			break;

		usleep_range((unsigned long)st->gpio_rdy_dly_min,
			     (unsigned long)st->gpio_rdy_dly_max);
	}
	return ret;
}

static int iqs_gpio_rdy(struct iqs_state *st, bool poll)
{
	bool force = false;
	unsigned int i = 0;
	int ret;

	if (poll)
		ret = iqs_gpio_rdy_poll(st);
	else
		ret = gpio_get_value_cansleep(st->gpio_rdy);
	if (ret && !st->susrsm) {
		force = true;
		iqs_disable_irq(st);
		for (; i < st->gpio_rdy_retry; i++) {
			gpio_direction_output(st->gpio_rdy, 0);
			usleep_range(10000, 12000);
			/* put to tristate */
			gpio_direction_input(st->gpio_rdy);
			ret = iqs_gpio_rdy_poll(st);
			if (st->susrsm || !ret)
				break;
		}
	}
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&st->i2c->dev,
			 "%s gpio_rdy=%d poll=%x force=%x retry=%u exit=%x\n",
			 __func__, ret, poll, force, i, st->susrsm);
	return ret;
}

static unsigned int iqs_rc_i(struct iqs_state *st,
			     const struct iqs_hal_iom *iom)
{
	unsigned int rc_i;

	rc_i = st->hal_tbl[iom->hal_i].ndx + 1;
	rc_i += iom->offset;
	return rc_i;
}

static unsigned int iqs_bits_rd(struct iqs_state *st,
				const struct iqs_hal_iom *iom,
				unsigned int additional_offset)
{
	return st->rc[iqs_rc_i(st, iom) + additional_offset] & iom->mask;
}

static int iqs_i2c(struct iqs_state *st, bool poll)
{
	ssize_t t;
	char spew[128];
	unsigned int i;
	unsigned int ms;
	unsigned int n = 0;
	int ret = -ENODEV;

	if (st->i2c_addr && st->msg_n) {
#ifdef IQS_I2C_M_NO_RD_ACK
		/* This IQS device mangles the I2C protocol for read restarts.
		 * In order to do I2C message stacking to avoid an I2C STOP
		 * that would end the communication window, the last read
		 * byte must not be acknowledged, and instead the restart done.
		 * Probably no normal I2C HW supports this thus requiring the
		 * I2C bus to be bit-banged.
		 * I2C_M_NO_RD_ACK probably wouldn't work (if supported)
		 * because it's just the last read byte that requires this,
		 * not all of them.
		 */
		for (i = 0; i < st->msg_n - 1; i++) {
			if (st->msg[i].flags & I2C_M_RD)
				st->msg[i].flags |= I2C_M_NO_RD_ACK;
		}
#endif /* IQS_I2C_M_NO_RD_ACK */
		for (i = 0; i < st->i2c_retry; i++) {
			/* I2C transactions must be separated by a delay after
			 * the STOP is issued.  Here we just ensure that the
			 * delay time has passed before issueing another
			 * transaction.
			 */
			ms = iqs_i2c_stop_ms(st);
			if (ms)
				msleep(ms);
			if (iqs_gpio_rdy(st, poll)) {
				ret = -EIO;
				break;
			}

			/* Since this device has so many I2C issues, a debug
			 * feature is to use the SAR GPIO as a signal for when
			 * I2C transactions are actually done.
			 */
			if (st->gpio_sar_dev_asrt == IQS_GPIO_SAR_DBG_I2C)
				iqs_gpio_sar(st, 1);
			ret = i2c_transfer(st->i2c->adapter,
					   &st->msg[n], st->msg_n);
			if (st->gpio_sar_dev_asrt == IQS_GPIO_SAR_DBG_I2C)
				iqs_gpio_sar(st, 0);
			st->i2c_stop_ts = nvs_timestamp();
			if (ret == st->msg_n) {
				ret = 0;
				break;
			} else {
				if (ret < 0)
					continue;

				/* skip the successful messages */
				n = ret;
				while (n && st->msg[n].flags & I2C_M_RD)
					n--;
				st->msg_n -= n;
				ret = -EIO;
			}
		}
		if (ret && !st->susrsm)
			iqs_err(st);
		if (st->sts & NVS_STS_SPEW_MSG) {
			st->msg_n += n;
			if (i || ret)
				dev_info(&st->i2c->dev,
					"%s retry=%u poll=%x err=%d exit=%x\n",
					 __func__, i, poll, ret, st->susrsm);
			for (i = 0; i < st->msg_n; i++) {
				n = 0;
				if (st->msg[i].flags & I2C_M_RD) {
					t = sprintf(spew, "read=");
				} else {
					if (st->msg[i].len == 1) {
						/* part of read transaction */
						t = sprintf(spew, "read %#2x=",
							    st->msg[i].buf[0]);
						i++;
					} else {
						t = sprintf(spew,
							    "write %#2x=",
							    st->msg[i].buf[0]);
						n = 1;
					}
				}
				for (; n < st->msg[i].len; n++)
					t += sprintf(spew + t, "%#2x ",
						     st->msg[i].buf[n]);
				dev_info(&st->i2c->dev, "%s %s\n",
					 __func__, spew);
			}
		}
	} else {
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev,
				 "%s NO OP: i2c_addr=%hx  msg_n=%u\n",
				 __func__, st->i2c_addr, st->msg_n);
	}
	st->msg_n = 0;
	return ret;
}

static int iqs_i2c_rd(struct iqs_state *st, int hal_i, u16 len)
{
	if (st->msg_n + 1 < ARRAY_SIZE(st->msg)) {
		st->msg[st->msg_n].flags = 0;
		st->msg[st->msg_n].len = 1;
		st->msg[st->msg_n].buf = &st->rc[st->hal_tbl[hal_i].ndx];
		st->msg_n++;
		st->msg[st->msg_n].flags = I2C_M_RD;
		if (len)
			st->msg[st->msg_n].len = len;
		else
			st->msg[st->msg_n].len = st->hal_tbl[hal_i].len;
		st->msg[st->msg_n].buf = &st->rc[st->hal_tbl[hal_i].ndx + 1];
		st->msg_n++;
		return 0;
	}

	return -EINVAL;
};

static int iqs_i2c_read(struct iqs_state *st, int hal_i, u16 len, bool poll)
{
	iqs_i2c_rd(st, hal_i, len);
	return iqs_i2c(st, poll);
};

static int iqs_i2c_wr(struct iqs_state *st, int hal_i, u16 len)
{
	if (st->msg_n < ARRAY_SIZE(st->msg)) {
		st->msg[st->msg_n].flags = 0;
		if (len)
			st->msg[st->msg_n].len = len + 1;
		else
			st->msg[st->msg_n].len = st->hal_tbl[hal_i].len + 1;
		st->msg[st->msg_n].buf = &st->rc[st->hal_tbl[hal_i].ndx];
		st->msg_n++;
		return 0;
	}

	return -EINVAL;
};

static int iqs_i2c_write(struct iqs_state *st, int hal_i, u16 len,
			 bool poll, bool irq_restore)
{
	bool irq_dis = st->irq_dis;
	int ret;

	iqs_i2c_wr(st, hal_i, len);
	ret = iqs_i2c(st, poll);
	if (irq_restore)
		iqs_irq_restore(st, irq_dis);
	return ret;
};

static int iqs_wr(struct iqs_state *st, unsigned char *wr)
{
	u8 ndx = 0;
	unsigned int msg_n = st->msg_n;
	bool irq_dis;
	unsigned int i;
	unsigned int j;
	unsigned int k;
	unsigned char len;
	unsigned char reg;
	int hal_i;
	int ret = 0;

	if (!st->hal_tbl_n)
		/* exit if HAL not initialized */
		return -EINVAL;

	i = 0;
	while (wr[i] != 0) { /* while a length */
		len = wr[i];
		i++;
		if (len == 0xFF) {
			/* if length == FF then do an I2C write now */
			if (st->msg_n) {
				irq_dis = st->irq_dis;
				ret |= iqs_i2c(st, false);
				iqs_irq_restore(st, irq_dis);
			}
			if (wr[i])
				msleep(wr[i]);
			i++;
			continue;
		}

		/* get the register */
		reg = wr[i];
		i++;
		/* find the register and reg cache index in the hal table */
		for (hal_i = st->hal_tbl_n - 1; hal_i > 0; hal_i--) {
			if (st->hal_tbl[hal_i].reg == reg) {
				if (st->hal_tbl[hal_i].len >= len) {
					ndx = st->hal_tbl[hal_i].ndx + 1;
					break;
				} else {
					dev_err(&st->i2c->dev,
						"%s reg=%hhx ERR: len=%hhu\n",
						__func__, reg, len);
					/* length too long and need to exit */
					return -EFAULT;
				}
			}
		}
		if (hal_i) {
			/* if register and index found, mask data to cache */
			for (j = 0; j < len; j++) {
				k = ndx + j;
				st->rc[k] &= ~wr[i + len];
				st->rc[k] |= wr[i];
				i++;
			}
			ret |= iqs_i2c_wr(st, hal_i, len);
			i += len;
		} else {
			/* if register not found we're lost and need to exit */
			dev_err(&st->i2c->dev,
				"%s ERR: reg=%hhx not found.  FYI: len=%hhu\n",
				__func__, reg, len);
			st->msg_n = msg_n;
			return -EFAULT;
		}
	}

	return ret;
};

static int iqs_write(struct iqs_state *st, unsigned char *wr,
		     bool poll, bool irq_restore)
{
	bool irq_dis = st->irq_dis;
	int ret;

	ret = iqs_wr(st, wr);
	if (st->msg_n) {
		ret |= iqs_i2c(st, poll);
		if (irq_restore)
			iqs_irq_restore(st, irq_dis);
	}
	return ret;
}

static void iqs_op_rd(struct iqs_state *st)
{
	bool prox_binary = false;
	bool prox_full = false;
	unsigned int i;

	st->op_read_n = 0;
	/* add multi_comp if ATI partial is enabled */
	if (iqs_bits_rd(st, &st->hal_bit->ati_partial, 0)) {
		st->op_read_reg[st->op_read_n] = st->hal_bit->multi_comp.hal_i;
		st->op_read_n++;
	}
	/* always test for device reset */
	st->op_read_reg[st->op_read_n] = st->hal_bit->sysflag_reset.hal_i;
	st->op_read_n++;
	/* read either binary data or full counts */
	for (i = 0; i < IQS_DEV_HW_N; i++) {
		if (st->enabled & (1 << i)) {
			if (st->prox[i].proximity_binary_hw) {
				if (!prox_binary) {
					st->op_read_reg[st->op_read_n] =
						 st->hal_bit->touch_prx.hal_i;
					prox_binary = true;
					st->op_read_n++;
				}
			} else if (!prox_full) {
				st->op_read_reg[st->op_read_n] =
						 st->hal_bit->count_prx.hal_i;
				prox_full = true;
				st->op_read_n++;
			}
		}
	}
	if (st->enabled & (st->delta_ch_msk << IQS_DEV_DELTA0)) {
		st->op_read_reg[st->op_read_n] = st->hal_bit->lta.hal_i;
		st->op_read_n++;
		st->op_read_reg[st->op_read_n] = st->hal_bit->delta.hal_i;
		st->op_read_n++;
	}

	st->op_i = st->op_read_n; /* force new read cycle */
}

static int iqs_stream(struct iqs_state *st, bool stream)
{
	unsigned int event_mode = iqs_bits_rd(st, &st->hal_bit->event_mode, 0);
	int ret = 0;

	if (stream && event_mode)
		ret = iqs_wr(st, st->wr_stream);
	else if ((!stream) && (!event_mode))
		ret = iqs_wr(st, st->wr_events);
	else
		ret = 1; /* no op */
	return ret;
}

static int iqs_init(struct iqs_state *st)
{
	bool stream = (st->stream == IQS_STREAM_ALWAYS) ? true : false;
	int ret = 0;

	if (st->hal_tbl_n) {
		/* only if HAL initialized */
		ret = iqs_wr(st, st->dt_init[st->part_i]);
		ret |= iqs_stream(st, stream);
		if (ret < 0) {
			/* if an error then write separately */
			ret = iqs_write(st, st->dt_init[st->part_i],
					false, false);
			if (!ret) {
				ret = iqs_stream(st, stream);
				if (!ret)
					ret = iqs_i2c(st, false);
			}
		} else {
			ret = iqs_i2c(st, false);
		}
	}
	return ret;
}

static int iqs_en(struct iqs_state *st, int snsr_id)
{
	int ret = 0;

	if (snsr_id >= IQS_DEV_N)
		return -EINVAL;

	if (snsr_id < IQS_DEV_HW_N) {
		st->dbnc_lo_n[snsr_id] = st->dbnc_lo[snsr_id];
		st->dbnc_hi_n[snsr_id] = 0;
		ret = iqs_write(st, st->dt_en[st->part_i][snsr_id],
				false, false);
	}
	if (!ret)
		ret = nvs_proximity_enable(&st->prox[snsr_id]);
	return ret;
}

static int iqs_dis(struct iqs_state *st, int snsr_id)
{
	unsigned char *wr = NULL;
	int ret = 0;

	if (snsr_id >= IQS_DEV_N)
		return -EINVAL;

	if (snsr_id < 0)
		wr = st->wr_disable;
	else if (snsr_id < IQS_DEV_HW_N)
		wr = st->dt_dis[st->part_i][snsr_id];
	if (st->hal_tbl_n && wr)
		/* only if HAL initialized */
		ret = iqs_write(st, wr, false, true);
	return ret;
}

static int iqs_reenable(struct iqs_state *st)
{
	unsigned int i;
	int ret;

	ret = iqs_init(st);
	for (i = 0; i < IQS_DEV_N; i++) {
		if (st->enabled & (1 << i))
			ret |= iqs_en(st, i);
	}
	iqs_op_rd(st);
	return ret;
}

static int iqs_reenable_err(struct iqs_state *st)
{
	iqs_err(st);
	iqs_reenable(st);
	return RET_POLL_NEXT;
}

static int iqs_pm(struct iqs_state *st, bool enable)
{
	int ret = 0;

	if (enable) {
		ret = nvs_vregs_enable(&st->i2c->dev, st->vreg,
				       ARRAY_SIZE(iqs_vregs));
		if (ret > 0)
			mdelay(IQS_HW_DELAY_MS);
	} else {
		ret = nvs_vregs_sts(st->vreg, ARRAY_SIZE(iqs_vregs));
		if ((ret < 0) || (ret == ARRAY_SIZE(iqs_vregs))) {
			ret = iqs_dis(st, -1);
		} else if (ret > 0) {
			nvs_vregs_enable(&st->i2c->dev, st->vreg,
					 ARRAY_SIZE(iqs_vregs));
			mdelay(IQS_HW_DELAY_MS);
			ret = iqs_dis(st, -1);
		}
		ret |= nvs_vregs_disable(&st->i2c->dev, st->vreg,
					 ARRAY_SIZE(iqs_vregs));
	}
	if (ret > 0)
		ret = 0;
	if (ret) {
		dev_err(&st->i2c->dev, "%s pwr=%x ERR=%d\n",
			__func__, enable, ret);
	} else {
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s pwr=%x\n",
				 __func__, enable);
	}
	return ret;
}

static void iqs_pm_exit(struct iqs_state *st)
{
	iqs_pm(st, false);
	nvs_vregs_exit(&st->i2c->dev, st->vreg, ARRAY_SIZE(iqs_vregs));
}

static int iqs_pm_init(struct iqs_state *st)
{
	int ret;

	st->enabled = 0;
	nvs_vregs_init(&st->i2c->dev,
		       st->vreg, ARRAY_SIZE(iqs_vregs), iqs_vregs);
	ret = iqs_pm(st, true);
	return ret;
}

static int iqs_delta_tst(struct iqs_state *st, int ch, int lt, int gt)
{
	if (lt < 0 && gt < 0)
		return -1;

	ch += IQS_DEV_DELTA0;
	if (lt >= 0 && gt >= 0) {
		if (lt > gt) {
			if (st->prox[ch].proximity < lt &&
						   st->prox[ch].proximity > gt)
				return 1;
			else
				return 0;
		} else {
			if (st->prox[ch].proximity < lt ||
						   st->prox[ch].proximity > gt)
				return 1;
			else
				return 0;
		}
	}

	if (lt >= 0) {
		if (st->prox[ch].proximity < lt)
			return 1;
	}

	if (gt >= 0) {
		if (st->prox[ch].proximity > gt)
			return 1;
	}

	return 0;
}

static int iqs_rd_delta(struct iqs_state *st, s64 ts)
{
	u16 hw;
	u64 calc_i;
	u64 calc_f;
	s64 calc;
	unsigned int ch;
	unsigned int i;
	unsigned int j;
	int ret;

	for (ch = 0; ch < IQS_CH_N; ch++) {
		if (st->enabled & ((1 << ch) << IQS_DEV_DELTA0)) {
			i = iqs_rc_i(st, &st->hal_bit->delta) + (ch << 1);
			hw = (u16)st->rc[i];
			st->prox[i].hw = hw;
			if (st->delta_avg_n) {
				memcpy(&st->delta_avg[ch][0],
				       &st->delta_avg[ch][1],
				       st->delta_avg_n - 1);
				st->delta_avg[ch][st->delta_avg_n - 1] = hw;
				calc_i = 0;
				for (i = 0; i < st->delta_avg_n; i++)
					calc_i += st->delta_avg[ch][i];
				do_div(calc_i, st->delta_avg_n);
				hw = (u16)calc_i;
			}

			calc_i = hw;
			calc_f = 0;
			if (st->cfg[i].scale.fval) {
				i = IQS_DEV_DELTA0 + ch;
				if (st->cfg[i].resolution.fval) {
					calc_f = (u64)(hw *
						   st->cfg[i].resolution.fval);
					do_div(calc_f, st->cfg[i].scale.fval);
				}
				if (st->cfg[i].resolution.ival) {
					if (st->cfg[i].float_significance)
						calc_i = IQS_FS_NANO;
					else
						calc_i = IQS_FS_MICRO;
					do_div(calc_i, st->cfg[i].scale.fval);
					calc_i *= (u64)(hw *
						   st->cfg[i].resolution.ival);
				}
			}
			calc = (s64)(calc_i + calc_f);
			if (st->sts & NVS_STS_SPEW_DATA)
				dev_info(&st->i2c->dev,
					 "%s ch%u=%lld avg=%u hw=%hu  %lld\n",
					 __func__, ch, calc, hw,
					 st->prox[i].hw, ts);

			st->prox[i].timestamp = ts;
			st->prox[i].proximity = (u32)calc;
			st->nvs->handler(st->prox[i].nvs_st,
					 &st->prox[i].proximity,
					 st->prox[i].timestamp);
		}
	}

	for (i = 0; i < IQS_DELTA_TEST0_N; i++) {
		hw = 0;
		for (j = 0; j < IQS_DELTA_TEST1_N; j++) {
			if (st->delta_tst[i][j].ch < 0) {
				break;
			} else if (st->delta_tst[i][j].ch >= IQS_CH_N) {
				break;
			} else {
				ret = iqs_delta_tst(st, st->delta_tst[i][j].ch,
						    st->delta_tst[i][j].lt,
						    st->delta_tst[i][j].gt);
				if (ret < 0)
					break;

				hw = ret;
				if (!ret)
					break;
			}
		}
		if (!hw)
			i++;
	}

	st->prox[IQS_DEV_DELTA].timestamp = ts;
	st->prox[IQS_DEV_DELTA].hw = hw;
	st->prox[IQS_DEV_DELTA].proximity = (u32)hw;
	st->nvs->handler(st->prox[IQS_DEV_DELTA].nvs_st,
			 &st->prox[IQS_DEV_DELTA].proximity,
			 st->prox[IQS_DEV_DELTA].timestamp);
	return 0;
}

static int iqs_rd_snsr(struct iqs_state *st, s64 ts, int dev,
		       const struct iqs_hal_iom *tch,
		       const struct iqs_hal_iom *cnt)
{
	u16 hw;
	u16 rp;				/* reported proximity */
	int ret;

	if (st->prox[dev].proximity_binary_hw) {
		hw = (iqs_bits_rd(st, tch, 0));
		rp = hw;
		if (rp) {
			st->dbnc_lo_n[dev] = 0;
			if (st->dbnc_hi[dev]) {
				if (st->dbnc_hi_n[dev] < st->dbnc_hi[dev]) {
					st->dbnc_hi_n[dev]++;
					rp = 0;
				}
			}
		} else {
			st->dbnc_hi_n[dev] = 0;
			if (st->dbnc_lo[dev]) {
				if (st->dbnc_lo_n[dev] < st->dbnc_lo[dev]) {
					st->dbnc_lo_n[dev]++;
					rp = 1;
				}
			}
		}
		/* reverse polarity for Android (0=close 1=far) */
		rp = !rp;
	} else {
		hw = (u16)st->rc[iqs_rc_i(st, cnt)];
		rp = hw;
	}
	st->prox[dev].hw = rp;
	st->prox[dev].timestamp = ts;
	ret = nvs_proximity_read(&st->prox[dev]);
	if (st->sts & NVS_STS_SPEW_DATA)
		dev_info(&st->i2c->dev,
			 "%s: hw=%hu rp=%hu %lld  lo_n=%u hi_n=%u  %lldns\n",
			 iqs_snsr_names[dev], hw, rp, ts,
			 st->dbnc_lo_n[dev], st->dbnc_hi_n[dev],
			 ts - st->prox[IQS_DEV_PROX].timestamp);
	return ret;
}

static int iqs_ati_redo(struct iqs_state *st)
{
	iqs_wr(st, st->wr_ati_redo);
	st->ati_redo_n++;
	if (!st->ati_redo_n)
		st->ati_redo_n--;
	/* restart read cycle to get status */
	st->op_i = st->op_read_n;
	return RET_NO_CHANGE;
}

static int iqs_rd(struct iqs_state *st, bool poll)
{
	s64 ts;
	unsigned int i;
	unsigned int k;
	unsigned int ch;
	int mc;
	int ret = 0;

#ifdef IQS_I2C_M_NO_RD_ACK
	/* I2C message stacking */
	for (i = 0; i < st->op_read_n; i++)
		iqs_i2c_rd(st, st->op_read_reg[i], 0);
	ret = iqs_i2c(st, true);
#else /* IQS_I2C_M_NO_RD_ACK */
	if (atomic_xchg(&iqs_ext_sts_chg, 0)) {
		/* DT WAR action based on external state */
		if (iqs_ext_sts)
			iqs_wr(st, st->dt_ext_hi[st->part_i]);
		else
			iqs_wr(st, st->dt_ext_lo[st->part_i]);
		st->op_i = st->op_read_n; /* restart cycle */
	}
	st->op_i++;
	if (st->op_i >= st->op_read_n) {
		st->op_i = 0; /* restart read cycle */
		/* when slider is enabled then device automatically goes into
		 * and stays in stream mode during the slider event.  The DT
		 * stream_mode should be set to (IQS_STREAM_AUTO) for this.
		 */
		if ((st->stream == IQS_STREAM_OFF) || !poll) {
			/* enter stream mode on first I2C transaction if DT
			 * stream_mode IQS_STREAM_OFF OR not polling
			 */
			ret = iqs_stream(st, true);
			if (!ret)
				/* was in event mode */
				poll = false; /* need to force mode switch */
		}
	} else if (st->op_i == 1) {
		iqs_wr(st, st->dt_evnt[st->part_i]); /* DT additional action */
	}
	if (st->op_i == st->op_read_n - 1) {
		/* at the end of read cycle */
		if (st->stream != IQS_STREAM_ALWAYS)
			iqs_stream(st, false); /* event mode at end of reads */
	}
	iqs_i2c_rd(st, st->op_read_reg[st->op_i], 0);
	ret = iqs_i2c(st, poll);
#endif /* IQS_I2C_M_NO_RD_ACK */
	if (ret) {
#if 0
		/* device needs to be power cycled */
		iqs_pm(st, false);
		iqs_pm(st, true);
		return iqs_reenable_err(st);
#endif /* 0 */
	} else {
		/* test for device reset */
		if (st->op_read_reg[st->op_i] ==
					    st->hal_bit->sysflag_reset.hal_i) {
			if (iqs_bits_rd(st, &st->hal_bit->sysflag_reset, 0))
				/* an error occurred so reinitialization */
				return iqs_reenable_err(st);

			if (iqs_bits_rd(st, &st->hal_bit->ati_busy, 0)) {
				/* restart read cycle to get status */
				st->op_i = st->op_read_n;
				return RET_NO_CHANGE;
			}

			if (iqs_bits_rd(st, &st->hal_bit->ati_err, 0))
				/* ATI redo on next I2C access */
				return iqs_ati_redo(st);
		}

		/* test for partial ATI */
		if (st->op_read_reg[st->op_i] ==
					       st->hal_bit->multi_comp.hal_i) {
			/* check if reseed needed */
			ch = iqs_bits_rd(st, &st->hal_bit->active_ch, 0);
			for (i = 0; i < IQS_CH_N; i++) {
				if (ch & (1 << i)) {
					mc = iqs_bits_rd(st,
						  &st->hal_bit->multi_comp, i);
					if (i)
						k = IQS_DEV_TOUCH;
					else
						k = IQS_DEV_PROX;
					if (mc > st->cfg[k].thresh_hi) {
						/* reseed on next I2C access */
						iqs_wr(st, st->wr_reseed);
						/* restart cycle for status */
						st->op_i = st->op_read_n;
						break;
					}
				}
			}
		}
		/* read data */
		ts = nvs_timestamp();
		if (st->enabled & (1 << IQS_DEV_PROX))
			ret |= iqs_rd_snsr(st, ts, IQS_DEV_PROX,
					   &st->hal_bit->touch_prx,
					   &st->hal_bit->count_prx);
		if (st->enabled & (1 << IQS_DEV_TOUCH))
			ret |= iqs_rd_snsr(st, ts, IQS_DEV_TOUCH,
					   &st->hal_bit->touch_tch,
					   &st->hal_bit->count_tch);
		if (st->enabled & (st->delta_ch_msk << IQS_DEV_DELTA0))
			ret |= iqs_rd_delta(st, ts);
		/* TODO: Expect the PO pin used for proximity_binary_hw.
		 *       Use a proximity threshold for SAR GPIO so that
		 *       proximity doesn't have to be in HW binary mode.
		 */
		if (st->gpio_sar_dev_asrt < IQS_DEV_N) {
			/* SAR GPIO assert and deassert can be controlled by
			 * separate sources.
			 * GPIO  polarity | XOR  asserted
			 *  0        0    |  0      1
			 *  0        1    |  1      0
			 *  1        0    |  1      0
			 *  1        1    |  0      1
			 */
			if (st->gpio_sar_val ^ st->gpio_sar_asrt_pol)
				/* currently deasserted */
				iqs_gpio_sar(st, !st->prox[st->
					     gpio_sar_dev_asrt].proximity);
			else
				/* currently asserted */
				iqs_gpio_sar(st, !st->prox[st->
					     gpio_sar_dev_dasrt].proximity);
		}
	}
	if (st->stream == IQS_STREAM_ALWAYS) {
		/* with stream always on we want to control the IRQ rate */
		if (ret != RET_NO_CHANGE) {
			/* keep IRQ enabled if anything but no change */
			ret = RET_HW_UPDATE;
		} else if (st->op_i == st->op_read_n - 1) {
			/* throttle IRQ at end of read cycle */
			iqs_disable_irq(st);
			ret = RET_POLL_NEXT;
		}
	}
	return ret;
}

static int iqs_disable(struct iqs_state *st, int snsr_id)
{
	bool disable = true;
	unsigned int i;
	int ret = 0;

	if (snsr_id >= 0) {
		ret = iqs_dis(st, snsr_id);
		if (!ret)
			st->enabled &= ~(1 << snsr_id);
		if (st->enabled)
			disable = false;
	} else {
		for (i = 0; i < IQS_DEV_N; i++) {
			if (st->enabled & (1 << i))
				iqs_dis(st, i);
		}
	}
	if (disable) {
		iqs_disable_irq(st);
		if (st->dw.work.func)
			cancel_delayed_work(&st->dw);
		ret = iqs_pm(st, false);
		if (!ret)
			st->enabled = 0;
	}
	return ret;
}

static int iqs_enable(struct iqs_state *st, int snsr_id, int enable)
{
	int ret;

	if (enable) {
		enable = st->enabled | (1 << snsr_id);
		ret = iqs_pm(st, true);
		if (!ret) {
			if (!st->enabled)
				ret = iqs_init(st);
			ret |= iqs_en(st, snsr_id);
			if (ret < 0) {
				iqs_disable(st, snsr_id);
			} else {
				st->enabled = enable;
				iqs_op_rd(st);
				mod_delayed_work(system_freezable_wq, &st->dw,
					 msecs_to_jiffies(IQS_START_DELAY_MS));
			}
		}
	} else {
		ret = iqs_disable(st, snsr_id);
	}
	return ret;
}

static int iqs_enables(struct iqs_state *st, unsigned int en_mask)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < IQS_DEV_N; i++) {
		if (en_mask & (1 << i))
			ret |= iqs_enable(st, i, 1);
	}
	return ret;
}

static unsigned int iqs_polldelay(struct iqs_state *st)
{
	unsigned int poll_delay_ms = IQS_POLL_DLY_MS_MAX;
	unsigned int i;

	for (i = 0; i < IQS_DEV_HW_N; i++) {
		if (st->enabled & (1 << i)) {
			if (poll_delay_ms > st->prox[i].poll_delay_ms)
				poll_delay_ms = st->prox[i].poll_delay_ms;
		}
	}
	return poll_delay_ms;
}

static void iqs_read(struct iqs_state *st, bool poll)
{
	unsigned int ms = st->wd_to_ms;
	int ret;

	iqs_mutex_lock(st);
	if (st->resume) {
		if (st->suspend_dis) {
			ret = iqs_reenable(st);
		} else {
			ret = iqs_enables(st, st->susrsm_en);
			st->susrsm_en &= ~st->enabled;
		}
		if (ret) {
			if (st->sts & NVS_STS_SPEW_MSG)
				dev_err(&st->i2c->dev,
					"%s resume ERR=%d Try again in %ums\n",
					__func__, ret, ms);
		} else {
			st->resume = false;
			st->susrsm_en = 0;
			iqs_enable_irq(st);
			if (st->sts & NVS_STS_SPEW_MSG)
				dev_info(&st->i2c->dev, "%s resume complete\n",
					 __func__);
		}
		mod_delayed_work(system_freezable_wq, &st->dw,
				 msecs_to_jiffies(ms));
	} else if (st->enabled) {
#ifdef IQS_I2C_M_NO_RD_ACK
		ret = iqs_rd(st, poll);
		if (ret > RET_POLL_NEXT)
			iqs_enable_irq(st);
		else
			ms = iqs_polldelay(st);
#else /* IQS_I2C_M_NO_RD_ACK */
		if (st->irq_dis) {
			/* if IRQ disabled then in irq throttle mode */
			iqs_enable_irq(st); /* IRQ driven mode */
		} else {
			ret = iqs_rd(st, poll);
			if (ret > RET_POLL_NEXT)
				iqs_enable_irq(st);
			else
				ms = iqs_polldelay(st);
		}
#endif /* IQS_I2C_M_NO_RD_ACK */
		/* always start a delayed work thread as a watchdog */
		mod_delayed_work(system_freezable_wq, &st->dw,
				 msecs_to_jiffies(ms));
		if (st->sts & NVS_STS_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s work delay=%ums\n",
				 __func__, ms);
	}
	iqs_mutex_unlock(st);
}

static void iqs_work(struct work_struct *ws)
{
	struct iqs_state *st = container_of((struct delayed_work *)ws,
					    struct iqs_state, dw);

	iqs_read(st, false);
}

static irqreturn_t iqs_irq_thread(int irq, void *dev_id)
{
	struct iqs_state *st = (struct iqs_state *)dev_id;

	iqs_read(st, true);
	return IRQ_HANDLED;
}

static irqreturn_t iqs_irq_handler(int irq, void *dev_id)
{
	struct iqs_state *st = (struct iqs_state *)dev_id;

	if (st->sts & NVS_STS_SPEW_IRQ)
		dev_info(&st->i2c->dev, "%s\n", __func__);
	return IRQ_WAKE_THREAD;
}

static int iqs_enable_os(void *client, int snsr_id, int enable)
{
	struct iqs_state *st = (struct iqs_state *)client;

	if (enable < 0)
		return st->enabled & (1 << snsr_id);

	if (st->os == IQS_OS_CONTROL || st->sts & (NVS_STS_SHUTDOWN |
						   NVS_STS_SUSPEND))
		return iqs_enable(st, snsr_id, enable);

	if (enable)
		st->prox[snsr_id].report = st->cfg[snsr_id].report_n;
	return 0;
}

static int iqs_batch(void *client, int snsr_id, int flags,
		    unsigned int period_us, unsigned int timeout_us)
{
	struct iqs_state *st = (struct iqs_state *)client;

	if (timeout_us)
		/* timeout not supported (no HW FIFO) */
		return -EINVAL;

	st->prox[snsr_id].delay_us = period_us;
	return 0;
}

static int iqs_thresh_lo(void *client, int snsr_id, int thresh)
{
	struct iqs_state *st = (struct iqs_state *)client;
	unsigned int hal_i;
	unsigned int i;
	unsigned int n;
	int ret;

	if (snsr_id >= IQS_DEV_DELTA && snsr_id <= IQS_DEV_DELTA3) {
		st->cfg[snsr_id].thresh_lo = thresh;
		return 0;
	}

	if (snsr_id == IQS_DEV_TOUCH) {
		hal_i = st->hal_bit->thresh_tch.hal_i;
		i = st->hal_bit->thresh_tch.offset + 1;
		n = i + st->hal_bit->thresh_tch.mask;
	} else {
		hal_i = st->hal_bit->thresh_prx.hal_i;
		i = st->hal_bit->thresh_prx.offset + 1;
		n = i + st->hal_bit->thresh_prx.mask;
	}
	for (; i < n; i++) {
		st->rc[st->hal_tbl[hal_i].ndx + i] = thresh & 0xFF;
		thresh >>= 8;
	}
	ret = iqs_i2c_write(st, hal_i, 0, false, true);
	if (!ret)
		st->cfg[snsr_id].thresh_lo = thresh;
	return ret;
}

static int iqs_regs(void *client, int snsr_id, char *buf)
{
	struct iqs_state *st = (struct iqs_state *)client;
	ssize_t t;
	bool irq_dis;
	unsigned int i;
	unsigned int j;
	unsigned int n;
	int ret = 0;

	iqs_mutex_lock(st);
	irq_dis = st->irq_dis;
#ifdef IQS_I2C_M_NO_RD_ACK
	/* I2C message stacking */
	for (i = 0; i < st->hal_tbl_n; i++)
		iqs_i2c_rd(st, i, 0);
	ret = iqs_i2c(st, false);
	t = sprintf(buf, "registers: (ERR=%d)\n", ret);
	for (i = 0; i < st->hal_tbl_n; i++) {
		n = st->hal_tbl[i].ndx;
		t += sprintf(buf + t, "0x%hhx=", st->rc[n]);
		n = st->hal_tbl[i].ndx + st->hal_tbl[i].len;
		for (j = st->hal_tbl[i].ndx + 1; j <= n; j++)
			t += sprintf(buf + t, "0x%hhx ", st->rc[j]);
		t += sprintf(buf + t, "\n");
	}
#else /* IQS_I2C_M_NO_RD_ACK */
	t = sprintf(buf, "registers:\n");
	for (i = 0; i < st->hal_tbl_n; i++) {
		ret = iqs_i2c_read(st, i, 0, false);
		n = st->hal_tbl[i].ndx;
		if (ret) {
			t += sprintf(buf + t, "0x%hhx=ERR %d",
				     st->rc[n], ret);
		} else {
			t += sprintf(buf + t, "0x%hhx=", st->rc[n]);
			n = st->hal_tbl[i].ndx + st->hal_tbl[i].len;
			for (j = st->hal_tbl[i].ndx + 1; j <= n; j++)
				t += sprintf(buf + t, "0x%hhx ", st->rc[j]);
			t += sprintf(buf + t, "\n");
		}
	}
#endif /* IQS_I2C_M_NO_RD_ACK */
	iqs_irq_restore(st, irq_dis);
	iqs_mutex_unlock(st);
	return t;
}

static int iqs_nvs_write(void *client, int snsr_id, unsigned int nvs)
{
	struct iqs_state *st = (struct iqs_state *)client;
	u8 val = (nvs >> 8) & 0xFF;
	unsigned int offset;
	unsigned int reg;
	unsigned int i;
	int ret = -EINVAL;

	switch (nvs & 0xFF) {
	case IQS_INFO_STS:
	case IQS_INFO_DBG:
		st->dbg = nvs;
		return 0;

	case IQS_INFO_EXT_STATE_SPEW:
		st->sts ^= IQS_STS_EXT_STATE;
		dev_info(&st->i2c->dev, "%s EXT_STATE_SPEW=%x\n",
			 __func__, !!(st->sts & IQS_STS_EXT_STATE));
		return 0;

	case IQS_INFO_GPIO_SAR_SPEW:
		st->sts ^= IQS_STS_GPIO_SAR;
		dev_info(&st->i2c->dev, "%s GPIO_SAR_SPEW=%x\n",
			 __func__, !!(st->sts & IQS_STS_GPIO_SAR));
		return 0;

	case IQS_INFO_GPIO_RDY_INPUT:
		ret = gpio_direction_input(st->gpio_rdy);
		dev_info(&st->i2c->dev,
			 "%s gpio_direction_input(gpio_rdy(%d))=%d\n",
			 __func__, st->gpio_rdy, ret);
		return ret;

	case IQS_INFO_GPIO_RDY_OUTPUT:
		val = !!val;
		ret = gpio_direction_output(st->gpio_rdy, val);
		dev_info(&st->i2c->dev,
			 "%s gpio_direction_output(gpio_rdy(%d), %hhx)=%d\n",
			 __func__, st->gpio_rdy, val, ret);
		return ret;

	case IQS_INFO_GPIO_SAR_OUTPUT:
		val = !!val;
		if (st->gpio_sar >= 0) {
			ret = gpio_direction_output(st->gpio_sar, val);
			if (!ret)
				st->gpio_sar_val = val;
		}
		dev_info(&st->i2c->dev,
			 "%s gpio_direction_output(gpio_sar(%d), %hhx)=%d\n",
			 __func__, st->gpio_sar, val, ret);
		return ret;

	case IQS_INFO_REG_WR:
		offset = ((nvs >> 16) & 0xFF) + 1;
		reg = (nvs >> 24) & 0xFF;
		for (i = 0; i < st->hal_tbl_n; i++) {
			if (st->hal_tbl[i].reg == reg) {
				if (offset > st->hal_tbl[i].len)
					break;

				iqs_mutex_lock(st);
				st->rc[st->hal_tbl[i].ndx + offset] = val;
				ret = iqs_i2c_write(st, i, 0, false, true);
				iqs_mutex_unlock(st);
				dev_info(&st->i2c->dev,
					 "%s %x => %x + %u err=%d\n",
					 __func__, val, reg, offset - 1, ret);
				return ret;
			}
		}
		return ret;

	default:
		ret = 0;
		break;
	}

	return ret;
}

static ssize_t iqs_nvs_dbg_db(struct iqs_state *st, char *buf, ssize_t t,
			      unsigned char *db)
{
	unsigned int i;
	unsigned int j;
	unsigned int n;

	i = 0;
	while (db[i]) {
		n = db[i];
		i++;
		if (n == 0xFF) {
			t += sprintf(buf + t, "flush write and mdelay=%hhu\n",
				     db[i]);
			i++;
			continue;
		}

		t += sprintf(buf + t, "len=%x reg=%x data/mask=", n, db[i]);
		i++;
		for (j = 0; j < n; j++)
			t += sprintf(buf + t, "%x/%x ",
				     db[i + j], db[i + j + n]);
		t += sprintf(buf + t, "\n");
		i += (n << 1);
	}
	if (i == 0)
		t += sprintf(buf + t, "<empty>\n");
	return t;
}

static ssize_t iqs_nvs_dbg_tst(struct iqs_state *st, char *buf, ssize_t t,
			       struct iqs_delta_tst *tst)
{
	if (tst->lt >= 0 && tst->gt >= 0) {
		if (tst->lt > tst->gt)
			t += sprintf(buf + t, "if (ch%d > %d && ch%d < %d)\n",
				     tst->ch, tst->gt, tst->ch, tst->lt);
		else
			t += sprintf(buf + t, "if (ch%d < %d || ch%d > %d)\n",
				     tst->ch, tst->lt, tst->ch, tst->gt);
		return t;
	}

	if (tst->lt >= 0) {
		t += sprintf(buf + t, "if (ch%d < %d)\n", tst->ch, tst->lt);
		return t;
	}

	if (tst->gt >= 0) {
		t += sprintf(buf + t, "if (ch%d > %d)\n", tst->ch, tst->gt);
		return t;
	}

	t += sprintf(buf + t, "exit conditions\n");
	return t;
}

static int iqs_nvs_read(void *client, int snsr_id, char *buf)
{
	struct iqs_state *st = (struct iqs_state *)client;
	u8 prt = (st->dbg >> 24) & 0xFF;
	u8 cmd = (st->dbg >> 16) & 0xFF;
	u8 dev = (st->dbg >> 8) & 0xFF;
	ssize_t t = 0;
	int i;
	int j;
	int n;

	switch (st->dbg & 0xFF) {
	case IQS_INFO_DBG:
		if (cmd >= IQS_DB_CMD_N) {
			t = sprintf(buf, "ERR: UNKNOWN COMMAND\n");
			break;
		}

		if (cmd == IQS_DB_CMD_DELTA) {
			t = sprintf(buf, "DELTA conditions:\n");
			for (i = 0; i < IQS_DELTA_TEST0_N; i++) {
				if (i)
					t += sprintf(buf + t, "if %s:\n",
						     iqs_delta_tst_dt[i]);
				else
					t += sprintf(buf + t, "%s:\n",
						     iqs_delta_tst_dt[i]);
				for (j = 0; j < IQS_DELTA_TEST1_N; j++) {
					if (st->delta_tst[i][j].ch < 0)
						break;
					else if (st->delta_tst[i][j].ch >=
								      IQS_CH_N)
						break;
					else
						t += iqs_nvs_dbg_tst(st,
								     buf, t,
							 &st->delta_tst[i][j]);
				}
			}
			return t;
		}

		if (!prt) {
			i = st->part_i;
		} else if (prt == IQS_DEVID_IQS253) {
			i = IQS_PART_253;
		} else if (prt == IQS_DEVID_IQS263) {
			i = IQS_PART_263;
		} else {
			t = sprintf(buf, "ERR: UNKNOWN PART\n");
			break;
		}

		if (dev > IQS_DEV_HW_N) {
			t = sprintf(buf, "ERR: UNKNOWN DEVICE\n");
			break;
		} else if (dev) {
			n = dev;
			dev--;
		} else {
			n = IQS_DEV_HW_N;
		}

		if (cmd == IQS_DB_CMD_INIT || !cmd) {
			t += sprintf(buf + t, "%s initialization:\n",
				     iqs_i2c_device_id[i].name);
			t += iqs_nvs_dbg_db(st, buf, t, st->dt_init[i]);
		}
		if (cmd == IQS_DB_CMD_EN || !cmd) {
			for (j = dev; j < n; j++) {
				t += sprintf(buf + t, "%s %s enable:\n",
					     iqs_i2c_device_id[i].name,
					     iqs_snsr_names[j]);
				t += iqs_nvs_dbg_db(st, buf, t,
						    st->dt_en[i][j]);
			}
		}
		if (cmd == IQS_DB_CMD_DIS || !cmd) {
			for (j = dev; j < n; j++) {
				t += sprintf(buf + t, "%s %s disable:\n",
					     iqs_i2c_device_id[i].name,
					     iqs_snsr_names[j]);
				t += iqs_nvs_dbg_db(st, buf, t,
						    st->dt_dis[i][j]);
			}
		}
		if (cmd == IQS_DB_CMD_EVNT || !cmd) {
			t += sprintf(buf + t, "%s event:\n",
				     iqs_i2c_device_id[i].name);
			t += iqs_nvs_dbg_db(st, buf, t, st->dt_evnt[i]);
		}
		if (cmd == IQS_DB_CMD_SUSPND || !cmd) {
			t += sprintf(buf + t, "%s suspend:\n",
				     iqs_i2c_device_id[i].name);
			t += iqs_nvs_dbg_db(st, buf, t, st->dt_suspnd[i]);
		}
		if (cmd == IQS_DB_CMD_EXT_LO || !cmd) {
			t += sprintf(buf + t, "%s external_lo:\n",
				     iqs_i2c_device_id[i].name);
			t += iqs_nvs_dbg_db(st, buf, t, st->dt_ext_lo[i]);
		}
		if (cmd == IQS_DB_CMD_EXT_HI || !cmd) {
			t += sprintf(buf + t, "%s external_hi:\n",
				     iqs_i2c_device_id[i].name);
			t += iqs_nvs_dbg_db(st, buf, t, st->dt_ext_hi[i]);
		}
		break;

	default:
		t = sprintf(buf, "IQS driver v. %u\n", IQS_DRIVER_VERSION);
		t += sprintf(buf + t, "ATI redo count=%u\n", st->ati_redo_n);
		st->ati_redo_n = 0;
		t += sprintf(buf + t, "os_options=%x\n", st->os);
		t += sprintf(buf + t, "stream_mode=%x\n", st->stream);
		t += sprintf(buf + t, "watchdog_timeout_ms=%u\n",
			     st->wd_to_ms);
		t += sprintf(buf + t, "i2c_ss_delay_ns=%lld\n",
			     st->i2c_ss_war_ns);
		t += sprintf(buf + t, "i2c_retry=%u\n", st->i2c_retry);
		t += sprintf(buf + t, "gpio_rdy_retry=%u\n",
			     st->gpio_rdy_retry);
		t += sprintf(buf + t, "gpio_rdy_delay_count=%u\n",
			     st->gpio_rdy_dly_n);
		t += sprintf(buf + t, "gpio_rdy_delay_us_min=%u\n",
			     st->gpio_rdy_dly_min);
		t += sprintf(buf + t, "gpio_rdy_delay_us_max=%u\n",
			     st->gpio_rdy_dly_max);
		if (st->gpio_rdy < 0)
			t += sprintf(buf + t, "NO gpio_rdy\n");
		else
			t += sprintf(buf + t, "gpio_rdy %d=%d\n", st->gpio_rdy,
				     gpio_get_value_cansleep(st->gpio_rdy));
		if (st->gpio_sar < 0)
			t += sprintf(buf + t, "NO gpio_sar\n");
		else
			t += sprintf(buf + t, "gpio_sar %d=%d\n", st->gpio_sar,
				     gpio_get_value_cansleep(st->gpio_sar));
		t += sprintf(buf + t, "gpio_sar_assert_polarity=%d\n",
			     st->gpio_sar_asrt_pol);
		t += sprintf(buf + t, "gpio_sar_dev_assert=%s\n",
			     iqs_snsr_names[st->gpio_sar_dev_asrt]);
		t += sprintf(buf + t, "gpio_sar_dev_deassert=%s\n",
			     iqs_snsr_names[st->gpio_sar_dev_dasrt]);
		t += sprintf(buf + t, "gpio_sar_suspend_assert=%d\n",
			     st->gpio_sar_sus_asrt);
		t += sprintf(buf + t, "deferred_resume_ms=%u\n",
			     st->dfr_rsm_ms);
		t += sprintf(buf + t, "resume=%x\n", st->resume);
		for (i = 0; i < IQS_DEV_HW_N; i++) {
			if (st->dbnc_lo[i])
				t += sprintf(buf + t, "%s_debounce_lo=%u\n",
					     iqs_snsr_names[i],
					     st->dbnc_lo[i]);
			if (st->dbnc_hi[i])
				t += sprintf(buf + t, "%s_debounce_hi=%u\n",
					     iqs_snsr_names[i],
					     st->dbnc_hi[i]);
		}
		t += sprintf(buf + t, "SAR_delta_channel_mask=%u\n",
			     st->delta_ch_msk);
		if (st->delta_ch_msk)
			t += sprintf(buf + t, "SAR_delta_average_count=%u\n",
				     st->delta_avg_n);
		t += sprintf(buf + t, "irq=%d\n", st->i2c->irq);
		t += sprintf(buf + t, "irq_disable=%x\n", st->irq_dis);
		t += sprintf(buf + t, "irq_trigger_edge=%x\n",
			     st->irq_trigger_edge);
		for (i = 0; i < IQS_DEV_HW_N; i++)
			t += sprintf(buf + t, "%s_binary_hw=%x\n",
				     iqs_snsr_names[i],
				     st->prox[i].proximity_binary_hw);
	}

	st->dbg = IQS_INFO_STS;
	return t;
}

static struct nvs_fn_dev iqs_fn_dev = {
	.enable				= iqs_enable_os,
	.batch				= iqs_batch,
	.thresh_lo			= iqs_thresh_lo,
	.regs				= iqs_regs,
	.nvs_write			= iqs_nvs_write,
	.nvs_read			= iqs_nvs_read,
};


int sar_external_status(int status)
{
	iqs_ext_sts = status;
	atomic_set(&iqs_ext_sts_chg, 1);
	if (iqs_fn_dev.sts) {
		if (*iqs_fn_dev.sts & (NVS_STS_SPEW_MSG | IQS_STS_EXT_STATE))
			pr_info("%s status=%d\n", __func__, status);
	}
	return 0;
}
EXPORT_SYMBOL(sar_external_status);

#ifdef CONFIG_SUSPEND
static int iqs_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct iqs_state *st = i2c_get_clientdata(client);
	unsigned int i;
	int ret = 0;
	s64 ts = 0; /* = 0 to fix compile */

	if (st->sts & NVS_STS_SPEW_MSG)
		ts = nvs_timestamp();
	/* Due to the device's horrendous communication protocol that causes
	 * unacceptable delays, suspend flag is used to exit pending actions.
	 */
	st->susrsm = true;
	iqs_mutex_lock(st);
	st->susrsm = false;
	st->sts |= NVS_STS_SUSPEND;
	st->susrsm_en |= st->enabled;
	/* determine if we'll be operational during suspend */
	for (i = 0; i < IQS_DEV_HW_N; i++) {
		if ((st->enabled & (1 << i)) && (st->cfg[i].flags &
						 SENSOR_FLAG_WAKE_UP))
			break;
	}
	if (i < IQS_DEV_HW_N) {
		st->suspend_dis = true; /* stay active during suspend */
		/* DT additional action for suspend */
		ret = iqs_write(st, st->dt_suspnd[st->part_i], false, false);
		iqs_enable_irq(st);
		irq_set_irq_wake(st->i2c->irq, 1);
		st->irq_set_irq_wake = true;
	} else {
		st->suspend_dis = false;
		iqs_disable(st, -1);
		if (st->gpio_sar_sus_asrt >= 0)
			iqs_gpio_sar(st, st->gpio_sar_sus_asrt);
	}
	if (st->dw.work.func)
		/* turn off watchdog during suspend */
		cancel_delayed_work(&st->dw);
	iqs_mutex_unlock(st);
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s elapsed t=%lldns  err=%d\n",
			 __func__, nvs_timestamp() - ts, ret);
	return 0;
}

static int iqs_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct iqs_state *st = i2c_get_clientdata(client);
	s64 ts = 0; /* = 0 to fix compile */
	int ret = 0;

	if (st->sts & NVS_STS_SPEW_MSG)
		ts = nvs_timestamp();
	st->susrsm = true;
	iqs_mutex_lock(st);
	st->susrsm = false;
	if (st->irq_set_irq_wake) {
		irq_set_irq_wake(st->i2c->irq, 0);
		st->irq_set_irq_wake = false;
	}
	st->sts &= ~NVS_STS_SUSPEND;
	if (st->susrsm_en) {
		/* Due to the device's horrendous communication protocol that
		 * causes unacceptable delays, resume is deferred.
		 */
		st->resume = true;
		mod_delayed_work(system_freezable_wq, &st->dw,
				 msecs_to_jiffies(st->dfr_rsm_ms));
	}
	iqs_mutex_unlock(st);
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s elapsed t=%lldns  err=%d\n",
			 __func__, nvs_timestamp() - ts, ret);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(iqs_pm_ops, iqs_suspend, iqs_resume);

static void iqs_shutdown(struct i2c_client *client)
{
	struct iqs_state *st = i2c_get_clientdata(client);

	iqs_mutex_lock(st);
	st->sts |= NVS_STS_SHUTDOWN;
	iqs_disable(st, -1);
	iqs_mutex_unlock(st);
	if (st->sts & NVS_STS_SPEW_MSG)
		dev_info(&client->dev, "%s\n", __func__);
}

static int iqs_remove(struct i2c_client *client)
{
	struct iqs_state *st = i2c_get_clientdata(client);
	unsigned int i;

	if (st != NULL) {
		iqs_shutdown(client);
		if (st->nvs) {
			for (i = 0; i < IQS_DEV_N; i++) {
				if (st->prox[i].nvs_st)
					st->nvs->remove(st->prox[i].nvs_st);
			}
		}
		iqs_pm_exit(st);
	}
	for (i = 0; i < IQS_CH_N; i++)
		kfree(st->delta_avg[i]);
	dev_info(&client->dev, "%s\n", __func__);
	return 0;
}

static void iqs_id_part(struct iqs_state *st, const char *part)
{
	unsigned int i;

	for (i = 0; i < IQS_DEV_N; i++)
		st->cfg[i].part = part;
}

static int iqs_id_dev(struct iqs_state *st, const char *name)
{
	unsigned int hal_i;
	unsigned int i;
	int ret = 1;

	/* assume for initial dev id read */
	st->hal_tbl = iqs263_hal_tbl;
	st->hal_tbl_n = ARRAY_SIZE(iqs263_hal_tbl);
	st->hal_bit = &iqs263_hal_bit;
	for (i = 0; i < ARRAY_SIZE(st->msg); i++)
		st->msg[i].addr = st->i2c_addr;
	if (!strcmp(name, IQS_NAME_IQS263))
		st->dev_id = IQS_DEVID_IQS263;
	else if (!strcmp(name, IQS_NAME_IQS253))
		st->dev_id = IQS_DEVID_IQS253;
	if (!st->dev_id) {
		hal_i = st->hal_bit->devinf_id.hal_i;
		i = st->hal_bit->devinf_id.offset + 1;
		ret = iqs_i2c_read(st, hal_i, i, false);
		if (ret) {
			st->hal_tbl_n = 0; /* disable PM I2C */
			return ret;
		} else {
			i += st->hal_tbl[hal_i].ndx;
			st->dev_id = st->rc[i];
		}
	}

	switch (st->dev_id) {
	case IQS_DEVID_IQS263:
		BUG_ON(IQS263_MSG_N != ARRAY_SIZE(iqs263_hal_tbl));
		iqs_id_part(st, IQS_NAME_IQS263);
		st->part_i = IQS_PART_263;
		st->wr_disable = iqs263_wr_disable;
		st->wr_stream = iqs263_wr_stream;
		st->wr_events = iqs263_wr_events;
		st->wr_ati_redo = iqs263_wr_ati_redo;
		st->wr_reseed = iqs263_wr_reseed;
		break;

	case IQS_DEVID_IQS253:
		BUG_ON(IQS253_MSG_N != ARRAY_SIZE(iqs253_hal_tbl));
		iqs_id_part(st, IQS_NAME_IQS253);
		st->part_i = IQS_PART_253;
		st->hal_tbl = iqs253_hal_tbl;
		st->hal_tbl_n = ARRAY_SIZE(iqs253_hal_tbl);
		st->hal_bit = &iqs253_hal_bit;
		st->wr_disable = iqs253_wr_disable;
		st->wr_stream = iqs253_wr_stream;
		st->wr_events = iqs253_wr_events;
		st->wr_ati_redo = iqs253_wr_ati_redo;
		st->wr_reseed = iqs253_wr_reseed;
		break;

	default:
		return -ENODEV;
	}

	for (i = 0; i < st->hal_tbl_n; i++)
		/* fill in register addresses for I2C writes */
		st->rc[st->hal_tbl[i].ndx] = st->hal_tbl[i].reg;
	if (!ret)
		dev_info(&st->i2c->dev, "%s found %s\n",
			 __func__, st->cfg[0].part);
	return 0;
}

static int iqs_id_i2c(struct iqs_state *st, const char *name)
{
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(iqs_i2c_addrs); i++) {
		if (st->i2c->addr == iqs_i2c_addrs[i])
			break;
	}

	if (i < ARRAY_SIZE(iqs_i2c_addrs)) {
		st->i2c_addr = st->i2c->addr;
		ret = iqs_id_dev(st, name);
	} else {
		name = IQS_NAME;
		for (i = 0; i < ARRAY_SIZE(iqs_i2c_addrs); i++) {
			st->i2c_addr = iqs_i2c_addrs[i];
			ret = iqs_id_dev(st, name);
			if (!ret)
				break;
		}
	}
	if (ret)
		st->i2c_addr = 0;
	return ret;
}

static const struct sensor_cfg iqs_cfg_dflt = {
	.snsr_id			= SENSOR_TYPE_PROXIMITY,
	.ch_n				= 1,
	.ch_sz				= 4,
	.part				= IQS_NAME,
	.vendor				= IQS_VENDOR,
	.version			= IQS_PROX_VERSION,
	.max_range			= {
		.ival			= IQS_PROX_MAX_RANGE_IVAL,
		.fval			= IQS_PROX_MAX_RANGE_MICRO,
	},
	.resolution			= {
		.ival			= IQS_PROX_RESOLUTION_IVAL,
		.fval			= IQS_PROX_RESOLUTION_MICRO,
	},
	.milliamp			= {
		.ival			= IQS_PROX_MILLIAMP_IVAL,
		.fval			= IQS_PROX_MILLIAMP_MICRO,
	},
	.delay_us_min			= IQS_POLL_DLY_MS_MIN * 1000,
	.delay_us_max			= IQS_POLL_DLY_MS_MAX * 1000,
	.flags				= SENSOR_FLAG_ON_CHANGE_MODE |
					  SENSOR_FLAG_WAKE_UP,
	.thresh_lo			= IQS_PROX_THRESHOLD,
	.thresh_hi			= IQS_MULTI_THRESHOLD,
};

static int iqs_of_dt_db(struct iqs_state *st, struct device_node *dn,
			char *name, unsigned char *dt_db, int db_n)
{
	char str[16];
	const char *charp;
	unsigned int limit = IQS_MSG_N;
	unsigned int i;
	int n;
	int lenp;
	int ret;

	n = 0;
	for (i = 0; i < limit; i++) {
		ret = sprintf(str, "%s_%u", name, i);
		if (ret <= 0) {
			dev_err(&st->i2c->dev, "%s sprintf(%s_%u)\n",
				__func__, name, i);
			return -ENODEV;
		}

		charp = of_get_property(dn, str, &lenp);
		if (charp) {
			if (lenp < (db_n - n)) {
				memcpy(&dt_db[n], charp, lenp);
				if (dt_db[n] == 0xFF)
					/* flush is done so limit extended */
					limit = i + IQS_MSG_N;
				n += lenp;
			} else {
				dev_err(&st->i2c->dev, "%s ERR: NOMEM @ %s\n",
					__func__, str);
				return -ENOMEM;
			}
		} else {
			dt_db[n] = 0; /* terminate byte stream */
			break;
		}
	}

	return 0;
}

static int iqs_of_dt(struct iqs_state *st, struct device_node *dn)
{
	char str[64];
	char const *pchar;
	const char *dev;
	unsigned int part;
	unsigned int i;
	unsigned int j;
	int ret;

	/* just test if global disable */
	ret = nvs_of_dt(dn, NULL, NULL);
	if (ret == -ENODEV)
		return -ENODEV;

	/* default device specific parameters */
	for (i = 0; i < IQS_DEV_N; i++) {
		memcpy(&st->cfg[i], &iqs_cfg_dflt, sizeof(st->cfg[0]));
		st->cfg[i].name = iqs_snsr_names[i];
		st->prox[i].cfg = &st->cfg[i];
		st->prox[i].hw_mask = 0xFFFF;
		st->prox[i].proximity_binary_hw = true;
		nvs_proximity_of_dt(&st->prox[i], dn, st->cfg[i].name);
	}
	for (i = 0; i <= IQS_CH_N; i++)
		/* delta sensors default disable */
		st->cfg[IQS_DEV_DELTA + i].snsr_id = -1;
	st->wd_to_ms = IQS_POLL_DLY_MS_WATCHDOG;
	st->dfr_rsm_ms = IQS_START_DELAY_MS;
	st->i2c_ss_war_ns = IQS_I2C_STOP_DLY_NS;
	st->i2c_retry = IQS_I2C_RETRY_N;
	st->gpio_rdy_retry = IQS_RDY_RETRY_N;
	st->gpio_rdy_dly_n = IQS_RDY_DELAY_N;
	st->gpio_rdy_dly_min = IQS_RDY_DELAY_US_MIN;
	st->gpio_rdy_dly_max = IQS_RDY_DELAY_US_MAX;
	st->gpio_rdy = -1;
	st->gpio_sar = -1;
	st->gpio_sar_sus_asrt = -1;
	for (i = 0; i < IQS_DELTA_TEST0_N; i++) {
		for (j = 0; j < IQS_DELTA_TEST1_N; j++) {
			st->delta_tst[i][j].ch = -1;
			st->delta_tst[i][j].lt = -1;
			st->delta_tst[i][j].gt = -1;
		}
	}
	/* device tree parameters */
	if (dn) {
		/* device specific parameters */
		for (i = 0; i < IQS_DEV_HW_N; i++) {
			sprintf(str, "%s_debounce_lo", iqs_snsr_names[i]);
			of_property_read_u32(dn, str, &st->dbnc_lo[i]);
			sprintf(str, "%s_debounce_hi", iqs_snsr_names[i]);
			of_property_read_u32(dn, str, &st->dbnc_hi[i]);
		}
		of_property_read_u32(dn, "SAR_delta_average_count",
				     &st->delta_avg_n);
		if (st->delta_avg_n < 2)
			st->delta_avg_n = 0;
		if (!of_property_read_u32(dn, "SAR_delta_channel_mask",
					  &st->delta_ch_msk)) {
			st->delta_ch_msk &= ((1 << IQS_CH_N) - 1);
			if (st->delta_ch_msk) {
				/* enable delta sensor(s) */
				st->cfg[IQS_DEV_DELTA].snsr_id =
							 SENSOR_TYPE_PROXIMITY;
				for (i = 0; i < IQS_CH_N; i++) {
					if (st->delta_ch_msk & (1 << i)) {
						j = IQS_DEV_DELTA0 + i;
						st->cfg[j].snsr_id =
							 SENSOR_TYPE_PROXIMITY;
					}
				}
			}
		}
		of_property_read_u32(dn, "os_options", &st->os);
		of_property_read_u32(dn, "stream_mode", &st->stream);
		if (st->stream >= IQS_STREAM_N) {
			dev_err(&st->i2c->dev,
				"%s ERR: stream_mode %u > %u limit\n",
				__func__, st->stream, IQS_STREAM_N - 1);
			st->stream = IQS_STREAM_OFF;
		}
		of_property_read_u32(dn, "deferred_resume_ms",
				     &st->dfr_rsm_ms);
		i = 0;
		of_property_read_u32(dn, "irq_trigger_edge", &i);
		if (i)
			st->irq_trigger_edge = true;
		of_property_read_u32(dn, "watchdog_timeout_ms", &st->wd_to_ms);
		of_property_read_u32(dn, "i2c_ss_delay_ns",
				     (u32 *)&st->i2c_ss_war_ns);
		of_property_read_u32(dn, "i2c_retry", &st->i2c_retry);
		of_property_read_u32(dn, "gpio_rdy_retry",
				     &st->gpio_rdy_retry);
		of_property_read_u32(dn, "gpio_rdy_delay_count",
				     &st->gpio_rdy_dly_n);
		of_property_read_u32(dn, "gpio_rdy_delay_us_min",
				     &st->gpio_rdy_dly_min);
		of_property_read_u32(dn, "gpio_rdy_delay_us_max",
				     &st->gpio_rdy_dly_max);
		if (!of_property_read_u32(dn, "gpio_sar_assert_polarity",
					  &st->gpio_sar_asrt_pol))
			st->gpio_sar_asrt_pol = !!st->gpio_sar_asrt_pol;
		if (!of_property_read_s32(dn, "gpio_sar_suspend_assert",
					  &st->gpio_sar_sus_asrt))
			st->gpio_sar_sus_asrt = !!st->gpio_sar_sus_asrt;
		st->gpio_rdy = of_get_named_gpio(dn, "gpio_rdy", 0);
		st->gpio_sar = of_get_named_gpio(dn, "gpio_sar", 0);
		if (!of_property_read_string(dn, "gpio_sar_dev_assert",
					     &pchar)) {
			for (i = 0; i < ARRAY_SIZE(iqs_snsr_names); i++) {
				if (!strcasecmp(pchar, iqs_snsr_names[i])) {
					st->gpio_sar_dev_asrt = i;
					break;
				}
			}
		}

		if (!of_property_read_string(dn, "gpio_sar_dev_deassert",
					     &pchar)) {
			for (i = 0; i < ARRAY_SIZE(iqs_snsr_names); i++) {
				if (!strcasecmp(pchar, iqs_snsr_names[i])) {
					st->gpio_sar_dev_dasrt = i;
					break;
				}
			}
		}

		ret = 0;
		for (i = 0; i < IQS_PART_N; i++) {
			if (i == 0)
				part = 253;
			else
				part = 263;
			sprintf(str, "%uinit", part);
			ret |= iqs_of_dt_db(st, dn, str, st->dt_init[i],
					    IQS_DT_INIT_N);
			sprintf(str, "%uevent", part);
			ret |= iqs_of_dt_db(st, dn, str, st->dt_evnt[i],
					    IQS_DT_EVNT_N);
			sprintf(str, "%uexternal_lo", part);
			ret |= iqs_of_dt_db(st, dn, str, st->dt_ext_lo[i],
					    IQS_DT_EXT_N);
			sprintf(str, "%uexternal_hi", part);
			ret |= iqs_of_dt_db(st, dn, str, st->dt_ext_hi[i],
					    IQS_DT_EXT_N);
			sprintf(str, "%ususpend", part);
			ret |= iqs_of_dt_db(st, dn, str, st->dt_suspnd[i],
					    IQS_DT_SUSPND_N);
			for (j = 0; j < IQS_DEV_HW_N; j++) {
				if (j == 0)
					dev = "prox";
				else
					dev = "touch";
				sprintf(str, "%uen_%s", part, dev);
				ret |= iqs_of_dt_db(st, dn, str,
						    st->dt_en[i][j],
						    IQS_DT_ABLE_N);
				sprintf(str, "%udis_%s", part, dev);
				ret |= iqs_of_dt_db(st, dn, str,
						    st->dt_dis[i][j],
						    IQS_DT_ABLE_N);
			}
		}
		if (ret)
			return ret;

		for (i = 0; i < IQS_DELTA_TEST0_N; i++) {
			for (j = 0; j < IQS_DELTA_TEST1_N; j++) {
				sprintf(str, "%s_%u_ch",
					iqs_delta_tst_dt[i], j);
				of_property_read_s32(dn, str,
						     &st->delta_tst[i][j].ch);
				sprintf(str, "%s_%u_lt",
					iqs_delta_tst_dt[i], j);
				of_property_read_s32(dn, str,
						     &st->delta_tst[i][j].lt);
				sprintf(str, "%s_%u_gt",
					iqs_delta_tst_dt[i], j);
				of_property_read_s32(dn, str,
						     &st->delta_tst[i][j].gt);
			}
		}
	}

	for (i = 0; i < IQS_DEV_N; i++) {
		for (j = 0; j < 3; j++) {
			if (st->os)
				/* change 'SAR' to 'sar' for OS visibility */
				iqs_snsr_names[i][j] = tolower((unsigned char)
							 iqs_snsr_names[i][j]);
			else
				/* change 'sar' to 'SAR' */
				iqs_snsr_names[i][j] = toupper((unsigned char)
							 iqs_snsr_names[i][j]);
		}
	}

	if (gpio_is_valid(st->gpio_rdy)) {
		ret = gpio_request(st->gpio_rdy, IQS_NAME);
		if (ret) {
			dev_err(&st->i2c->dev,
				"%s gpio_request(%d %s) ERR:%d\n",
				__func__, st->gpio_rdy, IQS_NAME, ret);
			/* can't communicate with device without this GPIO */
			return -EPROBE_DEFER;
		} else {
			ret = gpio_direction_input(st->gpio_rdy);
			if (ret < 0) {
				dev_err(&st->i2c->dev,
					"%s gpio_direction_input(%d) ERR:%d\n",
					__func__, st->gpio_rdy, ret);
				return -ENODEV;
			}
		}
	} else {
		/* can't communicate with device without this GPIO */
		return -EPROBE_DEFER;
	}

	if (gpio_is_valid(st->gpio_sar)) {
		ret = gpio_request(st->gpio_sar, IQS_NAME);
		if (ret) {
			dev_err(&st->i2c->dev,
				"%s gpio_request(%d %s) ERR:%d\n",
				__func__, st->gpio_sar, IQS_NAME, ret);
		} else {
			/* start with SAR GPIO asserted */
			st->gpio_sar_val = -1;
			iqs_gpio_sar(st, 1);
		}
	}
	return 0;
}

static int iqs_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct iqs_state *st;
	unsigned long irqflags;
	unsigned int en;
	unsigned int i;
	int ret;

	dev_info(&client->dev, "%s\n", __func__);
	st = devm_kzalloc(&client->dev, sizeof(*st), GFP_KERNEL);
	if (st == NULL) {
		dev_err(&client->dev, "%s devm_kzalloc ERR\n", __func__);
		return -ENOMEM;
	}

	i2c_set_clientdata(client, st);
	st->i2c = client;
	ret = iqs_of_dt(st, client->dev.of_node);
	if (ret) {
		if (ret == -ENODEV) {
			dev_info(&client->dev, "%s DT disabled\n", __func__);
		} else {
			dev_err(&client->dev, "%s _of_dt ERR\n", __func__);
			ret = -ENODEV;
		}
		goto iqs_probe_exit;
	}

	iqs_pm_init(st);
	ret = iqs_id_i2c(st, id->name);
	if (ret) {
		dev_err(&client->dev, "%s _id_i2c ERR\n", __func__);
		ret = -ENODEV;
		goto iqs_probe_exit;
	}

	iqs_pm(st, false);
	iqs_fn_dev.sts = &st->sts;
	iqs_fn_dev.errs = &st->errs;
	st->nvs = nvs_iio();
	if (st->nvs == NULL) {
		ret = -ENODEV;
		goto iqs_probe_exit;
	}

	en = 0;
	for (i = 0; i < IQS_DEV_N; i++) {
		nvs_of_dt(client->dev.of_node, &st->cfg[i], NULL);
		ret = st->nvs->probe(&st->prox[i].nvs_st, st, &client->dev,
				     &iqs_fn_dev, &st->cfg[i]);
		st->cfg[i].snsr_id = i;
		if (!ret) {
			st->prox[i].handler = st->nvs->handler;
			en |= 1 << i;
		}
	}
	if (!en) {
		dev_err(&client->dev, "%s nvs_probe ERR\n", __func__);
		ret = -ENODEV;
		goto iqs_probe_exit;
	}

	INIT_DELAYED_WORK(&st->dw, iqs_work);
	if (st->gpio_rdy > 0)
		st->i2c->irq = gpio_to_irq(st->gpio_rdy);
	if (client->irq) {
		irqflags = IRQF_ONESHOT | IRQF_NO_THREAD;
		if (st->irq_trigger_edge)
			irqflags |= IRQF_TRIGGER_FALLING;
		else
			irqflags |= IRQF_TRIGGER_LOW;
		for (i = 0; i < IQS_DEV_HW_N; i++) {
			if (st->cfg[i].snsr_id >= 0) {
				if (st->cfg[i].flags & SENSOR_FLAG_WAKE_UP)
					irqflags |= IRQF_NO_SUSPEND;
			}
		}
		ret = request_threaded_irq(client->irq,
					   iqs_irq_handler, iqs_irq_thread,
					   irqflags, IQS_NAME, st);
		if (ret) {
			dev_err(&client->dev, "%s req_threaded_irq ERR %d\n",
				__func__, ret);
			ret = -ENOMEM;
			goto iqs_probe_exit;
		}
	}

	if (st->delta_ch_msk && st->delta_avg_n) {
		/* allocate data buffers for delta sensors moving average */
		for (i = 0; i < IQS_CH_N; i++) {
			st->delta_avg[i] = kzalloc(sizeof(*st->delta_avg[i] *
							  st->delta_avg_n),
						   GFP_KERNEL);
			if (st->delta_avg[i] == NULL) {
				dev_err(&client->dev,
					"%s kzalloc delta_avg[%u] sz=%u ERR\n",
					__func__, i, st->delta_avg_n << 1);
				ret = -ENOMEM;
				goto iqs_probe_exit;
			}
		}
	}

	iqs_mutex_lock(st);
	if (st->os == IQS_OS_CONTROL) {
		iqs_disable(st, -1);
	} else {
		ret = iqs_enables(st, en);
		if (ret) {
			dev_err(&client->dev, "%s auto enable ERR=%d\n",
				__func__, ret);
			/* use the resume execution to keep trying */
			iqs_pm(st, true);
			st->enabled = en;
			st->susrsm_en = en;
			st->resume = true;
			mod_delayed_work(system_freezable_wq, &st->dw,
					 msecs_to_jiffies(IQS_START_DELAY_MS));
		}
	}
	iqs_mutex_unlock(st);
	dev_info(&client->dev, "%s done\n", __func__);
	return 0;

iqs_probe_exit:
	iqs_remove(client);
	return ret;
}

MODULE_DEVICE_TABLE(i2c, iqs_i2c_device_id);

static const struct of_device_id iqs_of_match[] = {
	{ .compatible = "azoteq,iqs2x3", },
	{ .compatible = "azoteq,iqs253", },
	{ .compatible = "azoteq,iqs263", },
	{},
};

MODULE_DEVICE_TABLE(of, iqs_of_match);

static struct i2c_driver iqs_driver = {
	.class		= I2C_CLASS_HWMON,
	.probe		= iqs_probe,
	.remove		= iqs_remove,
	.shutdown	= iqs_shutdown,
	.driver = {
		.name		= IQS_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(iqs_of_match),
		.pm		= &iqs_pm_ops,
	},
	.id_table	= iqs_i2c_device_id,
};
module_i2c_driver(iqs_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("IQS2X3 driver");
MODULE_AUTHOR("NVIDIA Corporation");

