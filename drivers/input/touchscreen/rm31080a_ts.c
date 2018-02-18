/*
 * Raydium RM31080 touchscreen driver
 *
 * Copyright (C) 2012-2016, Raydium Semiconductor Corporation.
 * All Rights Reserved.
 * Copyright (C) 2012-2016, NVIDIA Corporation.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 */
/*=============================================================================
	INCLUDED FILES
=============================================================================*/
#include <linux/module.h>
#include <linux/input.h>	/* BUS_SPI */
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/sched.h>	/* wake_up_process() */
#include <linux/uaccess.h>	/* copy_to_user() */
#include <linux/miscdevice.h>
#include <asm/siginfo.h>	/* siginfo */
#include <linux/rcupdate.h>	/* rcu_read_lock */
#include <linux/sched.h>	/* find_task_by_pid_type */
#include <linux/syscalls.h>	/* sys_clock_gettime() */
#include <linux/random.h>	/* random32() */
#include <linux/suspend.h>	/* pm_notifier */
#include <linux/workqueue.h>
#include <linux/wakelock.h> /* wakelock */
#include <linux/regulator/consumer.h> /* regulator & voltage */
#include <linux/clk.h> /* clock */
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <linux/timer.h>

#include <linux/spi/rm31080a_ts.h>
#include <linux/spi/rm31080a_ctrl.h>

#define CREATE_TRACE_POINTS
#include <trace/events/touchscreen_raydium.h>

#if (INPUT_PROTOCOL_CURRENT_SUPPORT == INPUT_PROTOCOL_TYPE_B)
#include <linux/input/mt.h>
#endif

#if ((ISR_POST_HANDLER == KTHREAD) || ENABLE_EVENT_QUEUE)
#include <linux/kthread.h>	/* kthread_create(),kthread_run() */
#include <linux/sched/rt.h>
#endif

#if ENABLE_EVENT_QUEUE
#include <linux/list.h>
#endif

/*=============================================================================
	DEFINITIONS
=============================================================================*/
#define MAX_SPI_FREQ_HZ			50000000
#define TS_PEN_UP_TIMEOUT		msecs_to_jiffies(50)

#define QUEUE_COUNT					128
#define RM_RAW_DATA_LENGTH			6144
#if ENABLE_QUEUE_GUARD
#define Q_PRO_TH	(QUEUE_COUNT / 10)
#endif

#define RM_SCAN_ACTIVE_MODE			0x00
#define RM_SCAN_PRE_IDLE_MODE		0x01
#define RM_SCAN_IDLE_MODE			0x02

#define RM_NEED_NONE				0x00
#define RM_NEED_TO_SEND_SCAN		0x01
#define RM_NEED_TO_READ_RAW_DATA	0x02
#define RM_NEED_TO_SEND_SIGNAL		0x04

#define TCH_WAKE_LOCK_TIMEOUT		(HZ/2)

#if ENABLE_FREQ_HOPPING  /*ENABLE_SCAN_DATA_HEADER*/
#define QUEUE_HEADER_NUM			(8)
#define SCAN_TYPE_MT				(1)
#else
#define QUEUE_HEADER_NUM			0
#endif

#ifdef ENABLE_SLOW_SCAN
#define RM_SLOW_SCAN_INTERVAL		20
#define RM_SLOW_SCAN_CMD_COUNT		0x10
enum RM_SLOW_SCAN_LEVELS {
	RM_SLOW_SCAN_LEVEL_NORMAL,
	RM_SLOW_SCAN_LEVEL_20,
	RM_SLOW_SCAN_LEVEL_40,
	RM_SLOW_SCAN_LEVEL_60,
	RM_SLOW_SCAN_LEVEL_80,
	RM_SLOW_SCAN_LEVEL_100,
	RM_SLOW_SCAN_LEVEL_MAX,
	RM_SLOW_SCAN_LEVEL_COUNT
};
#endif

enum RM_TEST_MODE {
	RM_TEST_MODE_NULL,
	RM_TEST_MODE_IDLE_SHOW,
	RM_TEST_MODE_IDLE_LEVEL,
	RM_TEST_MODE_CALC_TIME_SHOW,
	RM_TEST_MODE_MAX
};

#ifdef ENABLE_SMOOTH_LEVEL
#define RM_SMOOTH_LEVEL_NORMAL		0
#define RM_SMOOTH_LEVEL_MAX			4
#endif

#define TS_TIMER_PERIOD		HZ

#if (INPUT_PROTOCOL_CURRENT_SUPPORT == INPUT_PROTOCOL_TYPE_B)
#define MAX_SLOT_AMOUNT MAX_REPORT_TOUCHED_POINTS
#endif

#define MASK_USER_SPACE_POINTER 0x00000000FFFFFFFF	/* 64-bit support */

/* do not use printk in kernel files */
#define rm_printk(msg...)	dev_info(&g_spi->dev, msg)

/*=============================================================================
	STRUCTURE DECLARATION
=============================================================================*/
/* TouchScreen Parameters */
struct rm31080a_ts_para {
	u32 u32_hal_pid;
	bool b_init_finish;
	bool b_calc_finish;
	bool b_enable_scriber;
	bool b_is_suspended;
	bool b_is_disabled;
	bool b_init_service;

	u32 u32_watch_dog_cnt;
	u8 u8_watch_dog_flg;
	u8 u8_watch_dog_enable;
	bool b_watch_dog_check;
	u32 u32_watch_dog_time;

	u8 u8_scan_mode_state;
	u8 u8_resume_cnt;

#ifdef ENABLE_SLOW_SCAN
	bool b_enable_slow_scan;
	bool b_slow_scan_flg;
	u32 u32_slow_scan_level;
#endif

	u8 u8_touchfile_check;
	u8 u8_touch_event;

#ifdef ENABLE_SMOOTH_LEVEL
	u32 u32_smooth_level;
#endif
	bool b_selftest_enable;
	u8 u8_selftest_status;
	u8 u8_selftest_result;
	u8 u8_version;
	u8 u8_test_version;
	u8 u8_repeat;
	u16 u16_read_para;
	u8 u8_spi_locked;
	u8 u8_test_mode;
	u8 u8_test_mode_type;
#if ENABLE_FREQ_HOPPING
	u8 u8_ns_para[9];
	u8 u8_ns_mode;
	u8 u8_ns_sel;
	u8 u8_ns_rpt;
#endif
	struct wake_lock wakelock_initialization;

	struct mutex mutex_scan_mode;
	struct mutex mutex_ns_mode;

#if (ISR_POST_HANDLER == WORK_QUEUE)
	struct workqueue_struct *rm_workqueue;
	struct work_struct rm_work;
#elif (ISR_POST_HANDLER == KTHREAD)
	struct mutex mutex_irq_wait;
	bool b_irq_is_waited;
	struct task_struct *rm_irq_post_thread;
	struct sched_param irq_thread_sched;
	bool b_irq_thread_alive;
	bool b_irq_thread_active;
	wait_queue_head_t rm_irq_thread_wait_q;
#endif

#if ENABLE_EVENT_QUEUE
	struct mutex mutex_event_waited;
	bool b_event_is_waited;
	struct task_struct *rm_event_post_thread;
	struct sched_param event_thread_sched;
	bool b_event_thread_alive;
	bool b_event_thread_active;
	struct mutex mutex_event_fetch;
	wait_queue_head_t rm_event_thread_wait_q;
	struct list_head touch_event_list;
#endif

	struct workqueue_struct *rm_timer_workqueue;
	struct work_struct rm_timer_work;

	u8 u8_last_touch_count;
};

struct rm_tch_ts {
	const struct rm_tch_bus_ops *bops;
	struct device *dev;
	struct input_dev *input;
	unsigned int irq;
	char phys[32];
	struct mutex access_mutex;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct regulator *regulator_3v3;
	struct regulator *regulator_1v8;
	struct notifier_block nb_3v3;
	struct notifier_block nb_1v8;
	struct clk *clk;
	unsigned char u8_repeat_counter;
};

struct rm_tch_bus_ops {
	u16 bustype;
	int (*read) (struct device *dev, u8 reg);
	int (*write) (struct device *dev, u8 reg, u16 val);
};

struct rm_tch_queue_info {
	u8(*p_u8_queue)[RM_RAW_DATA_LENGTH];
	u16 u16_front;
	u16 u16_rear;
};

/*=============================================================================
	GLOBAL VARIABLES DECLARATION
=============================================================================*/
static struct miscdevice raydium_ts_miscdev;
struct input_dev *g_input_dev;
struct spi_device *g_spi;
struct rm31080a_ts_para g_st_ts;
struct rm_tch_queue_info g_st_q;

bool g_timer_queue_is_flush;
#if (ISR_POST_HANDLER == WORK_QUEUE)
bool g_worker_queue_is_flush;
#endif

unsigned char *g_pu8_burstread_buf;

unsigned char g_st_cmd_set_idle[KRL_SIZE_SET_IDLE];
unsigned char g_st_cmd_pause_auto[KRL_SIZE_PAUSE_AUTO];
unsigned char g_st_rm_resume_cmd[KRL_SIZE_RM_RESUME];
unsigned char g_st_rm_suspend_cmd[KRL_SIZE_RM_SUSPEND];
unsigned char g_st_rm_readimg_cmd[KRL_SIZE_RM_READ_IMG];
unsigned char g_st_rm_watchdog_cmd[KRL_SIZE_RM_WATCHDOG];
unsigned char g_st_rm_testmode_cmd[KRL_SIZE_RM_TESTMODE];
unsigned char g_st_rm_slow_scan_cmd[KRL_SIZE_RM_SLOWSCAN];
unsigned char g_st_rm_clear_int_cmd[KRL_SIZE_RM_CLEARINT];
unsigned char g_st_rm_scan_start_cmd[KRL_SIZE_RM_SCANSTART];
unsigned char g_st_rm_wait_scan_ok_cmd[KRL_SIZE_RM_WAITSCANOK];
unsigned char g_st_rm_set_rep_time_cmd[KRL_SIZE_RM_SETREPTIME];
unsigned char g_st_rm_ns_para_cmd[KRL_SIZE_RM_NS_PARA];
unsigned char g_st_rm_writeimg_cmd[KRL_SIZE_RM_WRITE_IMAGE];
unsigned char g_st_rm_tlk_cmd[KRL_SIZE_RM_TLK];
unsigned char g_st_rm_kl_testmode_cmd[KRL_SIZE_RM_KL_TESTMODE];

int g_service_busy_report_count;
struct timer_list ts_timer_triggle;

static unsigned char g_spi_buf[8192];
static unsigned int g_spi_bufsize; /*= 0; remove by checkpatch*/
static unsigned char g_spi_addr;
bool b_bl_updated;
u8 g_u8_update_baseline[RM_RAW_DATA_LENGTH];

size_t g_u8_test_mode_count;
char *g_u8_test_mode_buf;

/*=============================================================================
	FUNCTION DECLARATION
=============================================================================*/
static int rm_tch_cmd_process(u8 u8_sel_case, u8 *p_cmd_tbl,
		struct rm_tch_ts *ts);
#if (ISR_POST_HANDLER == KTHREAD)
static int rm_work_thread_function(void *data);
#endif
#if ENABLE_EVENT_QUEUE
static int rm_event_thread_function(void *data);
#endif
static int rm_tch_read_image_data(unsigned char *p);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void rm_tch_early_suspend(struct early_suspend *es);
static void rm_tch_early_resume(struct early_suspend *es);
#endif
static int rm_tch_ts_send_signal(int pid, int i_info);
static void rm_tch_enter_test_mode(u8 flag);
static void rm_ctrl_suspend(struct rm_tch_ts *ts);
static void rm_ctrl_resume(struct rm_tch_ts *ts);
static void rm_ctrl_watchdog_func(unsigned int u32_enable);
static void init_ts_timer(void);
static void ts_timer_triggle_function(unsigned long option);
#if ENABLE_QUEUE_GUARD
static void rm_tch_ctrl_slowscan(u32 level);
#endif

/*=============================================================================
	 Description:
			RM31080 spi interface.
	 Input:

	 Output:
			1:succeed
			0:failed
=============================================================================*/
static int rm_tch_spi_read(u8 u8addr, u8 *rxbuf, size_t len)
{
	int status;
	struct spi_message message;
	struct spi_transfer x[2];

	if (g_st_ts.u8_spi_locked) {
		memset(rxbuf, 0, len);
		if (g_st_ctrl.u8_kernel_msg & DEBUG_REGISTER)
			rm_printk("Raydium - SPI Read Locked!! 0x%x:%zu\n",
				u8addr, len);
		/*return RETURN_FAIL;*/
		return RETURN_OK;
	}

	spi_message_init(&message);
	memset(x, 0, sizeof(x));

	u8addr |= 0x80;
	x[0].len = 1;
	x[0].tx_buf = &u8addr;
	spi_message_add_tail(&x[0], &message);

	x[1].len = len;
	x[1].rx_buf = rxbuf;
	spi_message_add_tail(&x[1], &message);

	/*It returns zero on succcess,else a negative error code.*/
	status = spi_sync(g_spi, &message);

	if (g_st_ctrl.u8_kernel_msg & DEBUG_REGISTER)
		if (g_st_ts.b_init_finish == 0)
			rm_printk("Raydium - READ: addr=0x%2x, value=0x%2x",
				(u8addr&0x7F), rxbuf[0]);

	if (status) {
		dev_err(&g_spi->dev, "Raydium - %s : spi_async failed - error %d\n",
			__func__, status);
		return RETURN_FAIL;
	}

	return RETURN_OK;
}

/*=============================================================================
	 Description:
			RM31080 spi interface.
	 Input:

	 Output:
			1:succeed
			0:failed
=============================================================================*/
static int rm_tch_spi_write(u8 *txbuf, size_t len)
{
	int status;
	/*It returns zero on succcess,else a negative error code.*/

	if (g_st_ts.u8_spi_locked) {
		if (g_st_ctrl.u8_kernel_msg & DEBUG_REGISTER)
			rm_printk("Raydium - SPI write Locked!! 0x%x:0x%x\n",
				txbuf[0], txbuf[1]);
		/*return RETURN_FAIL;*/
		return RETURN_OK;
	}

	status = spi_write(g_spi, txbuf, len);

	if (g_st_ctrl.u8_kernel_msg & DEBUG_REGISTER)
		if (g_st_ts.b_init_finish == 0)
			rm_printk("Raydium - WRITE: addr=0x%2x, value=0x%2x",
				txbuf[0], txbuf[1]);

	if (status) {
		dev_err(&g_spi->dev, "Raydium - %s : spi_write failed - error %d\n",
			__func__, status);
		return RETURN_FAIL;
	}

	return RETURN_OK;
}

/*=============================================================================
	 Description:
			RM31080 spi interface.
	 Input:

	 Output:
			1:succeed
			0:failed
=============================================================================*/
static int rm_tch_spi_burst_write(u8 reg, u8 *txbuf, size_t len)
{
	int i;
	u8 *p_u8_Tmp;
	/*to do: check result*/
	p_u8_Tmp = kmalloc(len + 1, GFP_KERNEL);
	if (p_u8_Tmp == NULL)
		return -ENOMEM;
	p_u8_Tmp[0] = reg;
	for (i = 0; i < len; i++)
		p_u8_Tmp[i + 1] = txbuf[i];
	rm_tch_spi_write(p_u8_Tmp, len + 1);
	kfree(p_u8_Tmp);
	return RETURN_OK;
}

/*=============================================================================
	 Description:
			RM31080 spi interface.
	 Input:

	 Output:
			1:succeed
			0:failed
=============================================================================*/
int rm_tch_spi_byte_read(unsigned char u8_addr, unsigned char *p_u8_value)
{
	return rm_tch_spi_read(u8_addr, p_u8_value, 1);
}

/*=============================================================================
	 Description:
			RM31080 spi interface.
	 Input:

	 Output:
			1:succeed
			0:failed
=============================================================================*/
int rm_tch_spi_byte_write(unsigned char u8_addr, unsigned char u8_value)
{
	u8 buf[2];
	buf[0] = u8_addr;
	buf[1] = u8_value;
	return rm_tch_spi_write(buf, 2);
}

/*===========================================================================*/
static void rm_tch_generate_event(struct rm_tch_ts *dev_touch,
					enum tch_update_reason reason)
{
	char *envp[2];

	switch (reason) {
	case STYLUS_DISABLE_BY_WATER:
		envp[0] = "STYLUS_DISABLE=Water";
		break;
	case STYLUS_DISABLE_BY_NOISE:
		envp[0] = "STYLUS_DISABLE=Noise";
		break;
	case STYLUS_IS_ENABLED:
		envp[0] = "STYLUS_DISABLE=None";
		break;
	default:
		envp[0] = "STYLUS_DISABLE=Others";
		break;
	}
	envp[1] = NULL;
	kobject_uevent_env(&raydium_ts_miscdev.this_device->kobj,
		KOBJ_CHANGE, envp);
}

/*===========================================================================*/
void raydium_change_scan_mode(u8 u8_touch_count)
{
	static u32 u32_no_touch_count; /*= 0; remove by checkpatch*/
	u16 u16_nt_count_thd;

	u16_nt_count_thd = (u16)g_st_ctrl.u8_time2idle * 100;

	if (u8_touch_count) {
		u32_no_touch_count = 0;
		return;
	}

	if (u32_no_touch_count < u16_nt_count_thd)
		u32_no_touch_count++;
	else {
		mutex_lock(&g_st_ts.mutex_scan_mode);
		if (g_st_ts.u8_scan_mode_state == RM_SCAN_ACTIVE_MODE) {
			g_st_ts.u8_scan_mode_state = RM_SCAN_PRE_IDLE_MODE;
			u32_no_touch_count = 0;
		}
		mutex_unlock(&g_st_ts.mutex_scan_mode);
	}
}

#if (INPUT_PROTOCOL_CURRENT_SUPPORT == INPUT_PROTOCOL_TYPE_A)
void raydium_report_pointer(void *p)
{
	int i;
	int i_count;
	int i_max_x, i_max_y;
	struct rm_touch_event *sp_tp;
	static bool b_eraser_used;
	ssize_t missing;

	sp_tp = kmalloc(sizeof(struct rm_touch_event), GFP_KERNEL);
	if (sp_tp == NULL)
		return;

	missing = copy_from_user(sp_tp, p, sizeof(struct rm_touch_event));
	if (missing) {
		dev_err(&g_spi->dev, "Raydium - %s : copy failed - len:%d, miss:%d\n",
			__func__, sizeof(struct rm_touch_event), missing);
		kfree(sp_tp);
		return;
	}

	if ((g_st_ctrl.u16_resolution_x != 0) &&
		(g_st_ctrl.u16_resolution_y != 0)) {
			i_max_x = g_st_ctrl.u16_resolution_x;
			i_max_y = g_st_ctrl.u16_resolution_y;
	} else {
			i_max_x = RM_INPUT_RESOLUTION_X;
			i_max_y = RM_INPUT_RESOLUTION_Y;
	}

	i_count = max(g_st_ts.u8_last_touch_count, sp_tp->uc_touch_count);

	if (i_count && !sp_tp->uc_touch_count) {
		g_st_ts.u8_last_touch_count = 0;
		if (b_eraser_used) {
			input_report_key(g_input_dev,
				BTN_TOOL_RUBBER, 0);
			b_eraser_used = false;
		}
		input_mt_sync(g_input_dev);
		input_sync(g_input_dev);
	} else if (i_count) {
		for (i = 0; i < i_count; i++) {
			if (i == MAX_REPORT_TOUCHED_POINTS)
				break;

			if ((sp_tp->uc_tool_type[i] == POINT_TYPE_ERASER) &&
				((sp_tp->uc_slot[i] & INPUT_SLOT_RESET) ||
				(sp_tp->uc_id[i] == INPUT_ID_RESET))) {
				input_report_key(g_input_dev,
					BTN_TOOL_RUBBER, 0);
				input_mt_sync(g_input_dev);
				b_eraser_used = false;
				continue;
			}

			if ((i < sp_tp->uc_touch_count) &&
				(sp_tp->uc_id[i] != INPUT_ID_RESET)) {
				switch (sp_tp->uc_tool_type[i]) {
				case POINT_TYPE_FINGER:
					input_report_abs(g_input_dev,
						ABS_MT_TOOL_TYPE,
						MT_TOOL_FINGER);
					break;
				case POINT_TYPE_STYLUS:
					input_report_abs(g_input_dev,
							ABS_MT_TOOL_TYPE,
							MT_TOOL_PEN);
					break;
				case POINT_TYPE_ERASER:
					input_report_key(g_input_dev,
						BTN_TOOL_RUBBER, 1);
					b_eraser_used = true;
					break;
				default:
					dev_err(&g_spi->dev,
						"Raydium - point %d is invalid input tool type: %d\n",
						i, sp_tp->uc_tool_type[i]);
					break;
				}

				input_report_abs(g_input_dev,
							ABS_MT_TRACKING_ID,
							sp_tp->uc_id[i]);

				if (sp_tp->us_x[i] >= (i_max_x - 1))
					input_report_abs(g_input_dev,
							ABS_MT_POSITION_X,
							(i_max_x - 1));
				else
					input_report_abs(g_input_dev,
							ABS_MT_POSITION_X,
							sp_tp->us_x[i]);

				if (sp_tp->us_y[i] >= (i_max_y - 1))
					input_report_abs(g_input_dev,
							ABS_MT_POSITION_Y,
							(i_max_y - 1));
				else
					input_report_abs(g_input_dev,
							ABS_MT_POSITION_Y,
							sp_tp->us_y[i]);

				input_report_abs(g_input_dev,
							ABS_MT_PRESSURE,
							sp_tp->us_z[i]);

			}
			input_mt_sync(g_input_dev);
		}
		g_st_ts.u8_last_touch_count = sp_tp->uc_touch_count;
		input_sync(g_input_dev);
	}

	/*if (g_st_ctrl.u8_power_mode)
		raydium_change_scan_mode(sp_tp->uc_touch_count); */

	kfree(sp_tp);
}
#else /*(INPUT_PROTOCOL_CURRENT_SUPPORT == INPUT_PROTOCOL_TYPE_B)*/
void raydium_report_pointer(void *p)
{
	unsigned int target_abs_mt_tool = MT_TOOL_FINGER;
	unsigned int target_key_evt_btn_tool = BTN_TOOL_FINGER;
	int i;
	int i_count;
	int i_max_x, i_max_y;
	struct rm_touch_event *sp_tp;
#if !ENABLE_EVENT_QUEUE
	ssize_t missing;
#endif

	sp_tp = kmalloc(sizeof(struct rm_touch_event), GFP_KERNEL);
	if (sp_tp == NULL)
		return;

#if ENABLE_EVENT_QUEUE
	memcpy(sp_tp, p, sizeof(struct rm_touch_event));
#else
	missing = copy_from_user(sp_tp, p, sizeof(struct rm_touch_event));
	if (missing) {
		dev_err(&g_spi->dev, "Raydium - %s : copy failed - len:%zd, miss:%zu\n",
			__func__, sizeof(struct rm_touch_event), missing);
		kfree(sp_tp);
		return;
	}
#endif

	if ((g_st_ctrl.u16_resolution_x != 0) &&
		(g_st_ctrl.u16_resolution_y != 0)) {
		i_max_x = g_st_ctrl.u16_resolution_x;
		i_max_y = g_st_ctrl.u16_resolution_y;
	} else {
		i_max_x = RM_INPUT_RESOLUTION_X;
		i_max_y = RM_INPUT_RESOLUTION_Y;
	}

	i_count = max(g_st_ts.u8_last_touch_count, sp_tp->uc_touch_count);

	if (i_count && (!sp_tp->uc_touch_count)) {
		g_st_ts.u8_last_touch_count = 0;
		for (i = 0; i < MAX_SLOT_AMOUNT; i++) {
			input_mt_slot(g_input_dev, i);

			input_mt_report_slot_state(
				g_input_dev,
				MT_TOOL_FINGER, false);

			input_report_key(
				g_input_dev,
				BTN_TOOL_RUBBER, false);
		}
		input_sync(g_input_dev);
	} else if (i_count) {
		for (i = 0; i < i_count; i++) {
			if (i == MAX_SLOT_AMOUNT)
				break;

			if (i < sp_tp->uc_touch_count) {
				input_mt_slot(g_input_dev,
					sp_tp->uc_slot[i] & ~INPUT_SLOT_RESET);

				if ((sp_tp->uc_slot[i] & INPUT_SLOT_RESET) ||
					(sp_tp->uc_id[i] == INPUT_ID_RESET)) {
					switch (sp_tp->uc_pre_tool_type[i]) {
					case POINT_TYPE_FINGER:
						target_abs_mt_tool =
							MT_TOOL_FINGER;
						break;
					case POINT_TYPE_STYLUS:
						target_abs_mt_tool =
							MT_TOOL_PEN;
						break;
					case POINT_TYPE_ERASER:
						target_abs_mt_tool =
							MT_TOOL_PEN;
						target_key_evt_btn_tool =
								BTN_TOOL_RUBBER;
						break;
					default:
						dev_err(&g_spi->dev,
						"Raydium - point %d release invalid input tool type: %d, id=%d\n",
						i,
						sp_tp->uc_pre_tool_type[i],
						sp_tp->uc_id[i]);
						break;
					}

					input_mt_report_slot_state(
						g_input_dev,
						target_abs_mt_tool, false);

					if (sp_tp->uc_pre_tool_type[i] ==
						POINT_TYPE_ERASER)
						input_report_key(
							g_input_dev,
							target_key_evt_btn_tool,
							false);
				}

				if (sp_tp->uc_id[i] != INPUT_ID_RESET) {
					switch (sp_tp->uc_tool_type[i]) {
					case POINT_TYPE_FINGER:
						target_abs_mt_tool =
							MT_TOOL_FINGER;
						break;
					case POINT_TYPE_STYLUS:
						target_abs_mt_tool =
							MT_TOOL_PEN;
						break;
					case POINT_TYPE_ERASER:
						target_abs_mt_tool =
							MT_TOOL_PEN;
						target_key_evt_btn_tool =
								BTN_TOOL_RUBBER;
						break;
					default:
						dev_err(&g_spi->dev,
							"Raydium - point %d has invalid input tool type: %d, id=%d\n",
							i,
							sp_tp->uc_tool_type[i],
							sp_tp->uc_id[i]);
						break;
					}

					input_mt_report_slot_state(
						g_input_dev,
						target_abs_mt_tool, true);

					if (sp_tp->us_x[i] >= (i_max_x - 1))
						input_report_abs(
							g_input_dev,
							ABS_MT_POSITION_X,
							(i_max_x - 1));
					else
						input_report_abs(
							g_input_dev,
							ABS_MT_POSITION_X,
							sp_tp->us_x[i]);

					if (sp_tp->us_y[i] >= (i_max_y - 1))
						input_report_abs(
							g_input_dev,
							ABS_MT_POSITION_Y,
							(i_max_y - 1));
					else
						input_report_abs(
							g_input_dev,
							ABS_MT_POSITION_Y,
							sp_tp->us_y[i]);

					input_report_abs(
						g_input_dev,
						ABS_MT_PRESSURE,
						sp_tp->us_z[i]);

					if (sp_tp->uc_tool_type[i]
						== POINT_TYPE_ERASER)
						input_report_key(
							g_input_dev,
							target_key_evt_btn_tool,
							true);
				}
			}
		}
		g_st_ts.u8_last_touch_count = sp_tp->uc_touch_count;
		input_sync(g_input_dev);
	}
	/*if (g_st_ctrl.u8_power_mode)
		raydium_change_scan_mode(sp_tp->uc_touch_count);*/

	kfree(sp_tp);
}
#endif	/* End of INPUT_PROTOCOL_CURRENT_SUPPORT */

#if ENABLE_EVENT_QUEUE
int rm_tch_insert_event_queue(struct rm_touch_event *sp_tmp_tp)
{
	struct rm_touch_event_list *sp_tmp_tel;

	sp_tmp_tel = kmalloc(sizeof(struct rm_touch_event_list), GFP_KERNEL);
	if (sp_tmp_tel == NULL)
		return RETURN_FAIL;

	sp_tmp_tel->event_record =
			kmalloc(sizeof(struct rm_touch_event), GFP_KERNEL);
	if (sp_tmp_tel == NULL)
		return RETURN_FAIL;

	memcpy(sp_tmp_tel->event_record,
		sp_tmp_tp, sizeof(struct rm_touch_event));
	mutex_lock(&g_st_ts.mutex_event_fetch);
	list_add_tail(&sp_tmp_tel->next_event, &g_st_ts.touch_event_list);
	mutex_unlock(&g_st_ts.mutex_event_fetch);

	return RETURN_OK;
}

void rm_tch_enqueue_report_pointer(void *p)
{
	struct rm_touch_event *sp_tp;
	ssize_t missing;
	static u32 pointer_count;

	sp_tp = kmalloc(sizeof(struct rm_touch_event), GFP_KERNEL);
	if (sp_tp == NULL)
		return;

	missing = copy_from_user(sp_tp, p, sizeof(struct rm_touch_event));
	if (missing) {
		dev_err(&g_spi->dev, "Raydium - %s : copy failed - len:%d, miss:%d\n",
			__func__, sizeof(struct rm_touch_event), missing);
		kfree(sp_tp);
		return;
	}

	if (rm_tch_insert_event_queue(sp_tp))
		dev_err(&g_spi->dev, "Raydium - %s : Insert point queue fail!!\n",
			__func__);
	else {
		if (g_st_ctrl.u8_kernel_msg & DEBUG_KTHREAD)
			dev_err(&g_spi->dev, "Raydium - %s : Insert point queue okay ... %d!!\n",
				__func__, pointer_count++);
	}

	kfree(sp_tp);
}

static int rm_event_thread_function(void *data)
{
	struct list_head *ptr, *next;
	struct rm_touch_event_list *entry;

	sched_setscheduler(current_thread_info()->task,
		SCHED_FIFO, &g_st_ts.event_thread_sched);

	do {
		wait_event_interruptible(g_st_ts.rm_event_thread_wait_q,
			g_st_ts.b_event_thread_active);

		if (!g_st_ts.b_init_finish || g_st_ts.b_is_suspended)
			continue;

		while (!list_empty(&g_st_ts.touch_event_list)) {
			list_for_each_safe(ptr, next,
				&g_st_ts.touch_event_list) {
				entry = list_entry(ptr,
						struct rm_touch_event_list,
						next_event);

				raydium_report_pointer(
					(void *)entry->event_record);

				mutex_lock(&g_st_ts.mutex_event_fetch);
				list_del(&entry->next_event);
				mutex_unlock(&g_st_ts.mutex_event_fetch);
				kfree(entry->event_record);
				kfree(entry);
			}
		}
		mutex_lock(&g_st_ts.mutex_event_waited);
		if (!g_st_ts.b_event_is_waited)
			g_st_ts.b_event_thread_active = false;
		else
			g_st_ts.b_event_is_waited = false;
		mutex_unlock(&g_st_ts.mutex_event_waited);
	} while (!kthread_should_stop());

	return RETURN_OK;
}
#endif
/*=============================================================================
	 Description: Read Sensor Raw Data

	 Input:
			*p : Raw Data Buffer Address
	 Output:
			none
=============================================================================*/
static int rm_tch_read_image_data(unsigned char *p)
{
	int ret;

	g_pu8_burstread_buf = p;

#if ENABLE_FREQ_HOPPING  /*ENABLE_SCAN_DATA_HEADER*/
	g_pu8_burstread_buf[0] = SCAN_TYPE_MT;
	g_pu8_burstread_buf[1] = (u8)(g_st_ctrl.u16_data_length >> 8);
	g_pu8_burstread_buf[2] = (u8)(g_st_ctrl.u16_data_length);
	if (g_st_ctrl.u8_ns_func_enable&0x01)
		g_pu8_burstread_buf[3] = g_st_ts.u8_ns_sel;
	else
		g_pu8_burstread_buf[3] = 0;
	g_pu8_burstread_buf[4] = g_st_ts.u8_test_mode_type;
	g_pu8_burstread_buf[5] = 0x00;
	g_pu8_burstread_buf[6] = 0x00;
	g_pu8_burstread_buf[7] = 0x00;
#endif

	ret = rm_tch_cmd_process(0, g_st_rm_readimg_cmd, NULL);
	return ret;
}

void rm_tch_ctrl_set_baseline(u8 *arg, u16 u16Len)
{
	ssize_t missing;

	b_bl_updated = FALSE;
	missing = copy_from_user(g_u8_update_baseline, arg, u16Len);
	if (missing)
		return;
	b_bl_updated = TRUE;
}

static int rm_tch_write_image_data(void)
{
	int ret = RETURN_OK;

	ret = rm_tch_cmd_process(0, g_st_rm_writeimg_cmd, NULL);
	return ret;
}

#if ENABLE_FREQ_HOPPING
void rm_set_ns_para(u8 u8Idx, u8 *u8Para)
{
	int ii;
	struct rm_tch_ts *ts = input_get_drvdata(g_input_dev);

	for (ii = 0; ii < g_st_rm_ns_para_cmd[2]; ii++) {
		ts->u8_repeat_counter = u8Para[ii*3+u8Idx];
		rm_tch_cmd_process(ii, g_st_rm_ns_para_cmd, ts);
	}
}
#endif

void rm_tch_ctrl_enter_auto_mode(void)
{
	g_st_ctrl.u8_idle_mode_check &= ~0x01;

	if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
		rm_printk("Raydium - Enter Auto Scan Mode, bl_update=%d\n",
			b_bl_updated);

	if (b_bl_updated) {
		rm_tch_write_image_data();
		b_bl_updated = FALSE;
	}

	rm_set_repeat_times(g_st_ctrl.u8_idle_digital_repeat_times);
	rm_tch_cmd_process(1, g_st_cmd_set_idle, NULL);
}

void rm_tch_ctrl_leave_auto_mode(void)
{
	struct rm_tch_ts *ts = input_get_drvdata(g_input_dev);

	g_st_ctrl.u8_idle_mode_check |= 0x01;

#if ENABLE_FREQ_HOPPING
	if (g_st_ctrl.u8_ns_func_enable&0x01) {
		mutex_lock(&g_st_ts.mutex_ns_mode);
		rm_set_ns_para(0, (u8 *)&g_st_ts.u8_ns_para[0]);
		mutex_unlock(&g_st_ts.mutex_ns_mode);
	}
#endif

	rm_tch_cmd_process(0, g_st_cmd_set_idle, ts);
	rm_set_repeat_times(g_st_ctrl.u8_active_digital_repeat_times);

	if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
		rm_printk("Raydium - Leave Auto Scan Mode\n");
}

void rm_ctrl_pause_auto_mode(void)
{
	rm_tch_cmd_process(0, g_st_cmd_pause_auto, NULL);
}

int rm_tch_ctrl_clear_int(void)
{
	return rm_tch_cmd_process(0, g_st_rm_clear_int_cmd, NULL);
}


int rm_tch_ctrl_wait_for_scan_finish(u8 u8Idx)
{
	int i;

	for (i = 0; i < 50; i++) { /*50ms = 20Hz*/
		rm_tch_cmd_process(0, g_st_rm_wait_scan_ok_cmd, NULL);

		if (g_st_ts.u16_read_para & 0x01) {
			if (u8Idx)
				return 0;
			else
				usleep_range(1000, 2000);	/* msleep(1); */
		} else
			break;
	}
	return 0;
}

int rm_tch_ctrl_scan_start(void)
{
#if ENABLE_FREQ_HOPPING
	static u8 u8NsSel = 0, u8Rpt = 1;
	if (g_st_ctrl.u8_ns_func_enable&0x01) {
		if (rm_tch_ctrl_wait_for_scan_finish(1))
			return rm_tch_cmd_process(0, g_st_rm_scan_start_cmd,
				NULL);

		mutex_lock(&g_st_ts.mutex_ns_mode);
		g_st_ts.u8_ns_sel = g_st_ts.u8_ns_para[u8NsSel];

		if (u8NsSel < g_st_ts.u8_ns_mode)
			u8NsSel++;
		else
			u8NsSel = 0;

		if (u8Rpt != g_st_ts.u8_ns_rpt) {
			u8Rpt = g_st_ts.u8_ns_rpt;
			rm_set_repeat_times(u8Rpt);
		}

		rm_set_ns_para(u8NsSel, (u8 *)&g_st_ts.u8_ns_para[0]);

		mutex_unlock(&g_st_ts.mutex_ns_mode);
	} else {
		u8NsSel = 0;
		g_st_ts.u8_ns_sel = 0;
	}
#endif
	return rm_tch_cmd_process(0, g_st_rm_scan_start_cmd, NULL);
}

void rm_set_repeat_times(u8 u8Times)
{
	struct rm_tch_ts *ts = input_get_drvdata(g_input_dev);

	if (u8Times <= 1)
		u8Times = 0;
	else
		u8Times = u8Times - 1;

	if (u8Times > 127)
		u8Times = 127;

	/*ts->u8_repeat_counter = (u8Times & 0x1F);*/
	ts->u8_repeat_counter = (u8Times & 0x7F);
	if (u8Times == 0)
		rm_tch_cmd_process(0, g_st_rm_set_rep_time_cmd, ts);
	else
		rm_tch_cmd_process(1, g_st_rm_set_rep_time_cmd, ts);
}

static u32 rm_tch_ctrl_configure(void)
{
	u32 u32_flag;

	mutex_lock(&g_st_ts.mutex_scan_mode);
	switch (g_st_ts.u8_scan_mode_state) {
	case RM_SCAN_ACTIVE_MODE:
		u32_flag =
			RM_NEED_TO_SEND_SCAN |
			RM_NEED_TO_READ_RAW_DATA |
			RM_NEED_TO_SEND_SIGNAL;
		break;

	case RM_SCAN_PRE_IDLE_MODE:
		rm_tch_ctrl_enter_auto_mode();
		g_st_ts.u8_scan_mode_state = RM_SCAN_IDLE_MODE;
		u32_flag = RM_NEED_NONE;
		break;

	case RM_SCAN_IDLE_MODE:
		rm_tch_ctrl_leave_auto_mode();
		g_st_ts.u8_scan_mode_state = RM_SCAN_ACTIVE_MODE;
		u32_flag = RM_NEED_TO_SEND_SCAN;
		break;

	default:
		u32_flag = RM_NEED_NONE;
		break;
	}
	mutex_unlock(&g_st_ts.mutex_scan_mode);

	return u32_flag;
}

int KRL_CMD_CONFIG_1V8_Handler(u8 u8_cmd, u8 u8_on_off, struct rm_tch_ts *ts)
{
	int ret = RETURN_FAIL;
	struct rm_spi_ts_platform_data *pdata;

	pdata = g_input_dev->dev.parent->platform_data;

	if (u8_cmd == KRL_SUB_CMD_SET_1V8_REGULATOR) {
		if (ts) {
			if (ts->regulator_1v8) {
				if (u8_on_off) {
					ret = regulator_enable(
						ts->regulator_1v8);
					if (ret)
						dev_err(&g_spi->dev,
							"Raydium - regulator 1.8V enable failed: %d\n",
							ret);
				} else {
					ret = regulator_disable(
						ts->regulator_1v8);
					if (ret)
						dev_err(&g_spi->dev,
							"Raydium - regulator 1.8V disable failed: %d\n",
							ret);
				}
			} else
				dev_err(&g_spi->dev,
					"Raydium - regulator 1.8V handler fail: %d\n",
					ret);
		} else
			dev_err(&g_spi->dev,
				"Raydium - regulator 1.8V ts fail: %d\n",
				ret);
	} else if (u8_cmd == KRL_SUB_CMD_SET_1V8_GPIO)
		ret = gpio_direction_output(pdata->gpio_1v8, u8_on_off);

	return ret;
}

int KRL_CMD_CONFIG_3V3_Handler(u8 u8_cmd, u8 u8_on_off, struct rm_tch_ts *ts)
{
	int ret = RETURN_FAIL;
	struct rm_spi_ts_platform_data *pdata;

	pdata = g_input_dev->dev.parent->platform_data;

	if (u8_cmd == KRL_SUB_CMD_SET_3V3_REGULATOR) {
		if (ts) {
			if (ts->regulator_3v3) {
				if (u8_on_off) {
					ret = regulator_enable(
						ts->regulator_3v3);
					if (ret)
						dev_err(&g_spi->dev,
							"Raydium - regulator 3.3V enable failed: %d\n",
							ret);
				} else {
					ret = regulator_disable(
						ts->regulator_3v3);
					if (ret)
						dev_err(&g_spi->dev,
							"Raydium - regulator 3.3V disable failed: %d\n",
							ret);
				}
			} else
				dev_err(&g_spi->dev,
					"Raydium - regulator 3.3V handler fail: %d\n",
					ret);
		} else
			dev_err(&g_spi->dev,
				"Raydium - regulator 3.3V ts fail: %d\n",
				ret);
	} else if (u8_cmd == KRL_SUB_CMD_SET_3V3_GPIO)
		ret = gpio_direction_output(pdata->gpio_3v3, u8_on_off);

	return ret;
}

void rm_show_kernel_tbl_name(u8 *p_cmd_tbl)
{
	char target_table_name[32];

	memset(target_table_name, 0,
		sizeof(target_table_name));

	if (p_cmd_tbl == g_st_cmd_set_idle)
		snprintf(target_table_name,
			sizeof(target_table_name), "Set Idle");
	else if (p_cmd_tbl == g_st_cmd_pause_auto)
		snprintf(target_table_name,
			sizeof(target_table_name), "Pause Auto");
	else if (p_cmd_tbl == g_st_rm_resume_cmd)
		snprintf(target_table_name,
			sizeof(target_table_name), "Resume");
	else if (p_cmd_tbl == g_st_rm_suspend_cmd)
		snprintf(target_table_name,
			sizeof(target_table_name), "Suspend");
	else if (p_cmd_tbl == g_st_rm_readimg_cmd)
		snprintf(target_table_name,
			sizeof(target_table_name), "Read Image");
	else if (p_cmd_tbl == g_st_rm_watchdog_cmd)
		snprintf(target_table_name,
			sizeof(target_table_name), "WatchDog");
	else if (p_cmd_tbl == g_st_rm_testmode_cmd)
		snprintf(target_table_name,
			sizeof(target_table_name), "Test Mode");
	else if (p_cmd_tbl == g_st_rm_slow_scan_cmd)
		snprintf(target_table_name,
			sizeof(target_table_name), "Slowscan");
	else if (p_cmd_tbl == g_st_rm_clear_int_cmd)
		snprintf(target_table_name,
			sizeof(target_table_name), "Clear Intterupt");
	else if (p_cmd_tbl == g_st_rm_scan_start_cmd)
		snprintf(target_table_name,
			sizeof(target_table_name), "Scan Start");
	else if (p_cmd_tbl == g_st_rm_wait_scan_ok_cmd)
		snprintf(target_table_name,
			sizeof(target_table_name), "Wait Scan Okay");
	else if (p_cmd_tbl == g_st_rm_set_rep_time_cmd)
		snprintf(target_table_name,
			sizeof(target_table_name), "Set Repeat Time");
	else if (p_cmd_tbl == g_st_rm_ns_para_cmd)
		snprintf(target_table_name,
			sizeof(target_table_name), "Ns Parameter");
	else if (p_cmd_tbl == g_st_rm_writeimg_cmd)
		snprintf(target_table_name,
			sizeof(target_table_name), "Write Image");
	else if (p_cmd_tbl == g_st_rm_tlk_cmd)
		snprintf(target_table_name,
			sizeof(target_table_name), "TLK");
	else if (p_cmd_tbl == g_st_rm_kl_testmode_cmd)
		snprintf(target_table_name,
			sizeof(target_table_name), "Kernel_Test");
	else {
		dev_err(&g_spi->dev, "Raydium - %s : no such kernel table - err:%p\n",
			__func__, p_cmd_tbl);
	}
	dev_err(&g_spi->dev, "Raydium - Table %s cmd failed\n",
		target_table_name);
}

static int rm_tch_cmd_process(u8 u8_sel_case,
	u8 *p_cmd_tbl, struct rm_tch_ts *ts)
{
#define _CMD u16j
#define _ADDR (u16j+1)
#define _SUB_CMD (u16j+1)
#define _DATA (u16j+2)

	static DEFINE_MUTEX(lock);
	u16 u16j = 0, u16strIdx, u16TblLenth, u16Tmp;
	u32 u32Tmp;
	u8 u8i, u8reg = 0;
	int ret = RETURN_FAIL;
	struct rm_spi_ts_platform_data *pdata;

	mutex_lock(&lock);

	pdata = g_input_dev->dev.parent->platform_data;

	if (p_cmd_tbl[KRL_TBL_FIELD_POS_LEN_H]) {
		u16TblLenth = p_cmd_tbl[KRL_TBL_FIELD_POS_LEN_H];
		u16TblLenth <<= 8;
		u16TblLenth |= p_cmd_tbl[KRL_TBL_FIELD_POS_LEN_L];
	} else
		u16TblLenth = p_cmd_tbl[KRL_TBL_FIELD_POS_LEN_L];

	if (u16TblLenth < 3) {
		if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
			dev_info(&g_spi->dev, "Raydium - Null u8_cmd %s : [%p]\n",
				__func__, p_cmd_tbl);
		mutex_unlock(&lock);
		return ret;
	}

	u16strIdx = KRL_TBL_FIELD_POS_CASE_NUM
		+ p_cmd_tbl[KRL_TBL_FIELD_POS_CASE_NUM] + 1;

	if (u8_sel_case) {
		for (u8i = 0; u8i < u8_sel_case; u8i++)
			u16strIdx += (p_cmd_tbl[u8i + KRL_TBL_FIELD_POS_CMD_NUM]
				* KRL_TBL_CMD_LEN);
	}

	for (u8i = 0; u8i < p_cmd_tbl[u8_sel_case + KRL_TBL_FIELD_POS_CMD_NUM];
		u8i++) {
		u16j = u16strIdx + (KRL_TBL_CMD_LEN * u8i);
		ret = RETURN_FAIL;
		switch (p_cmd_tbl[_CMD]) {
		case KRL_CMD_READ:
			ret = rm_tch_spi_read(p_cmd_tbl[_ADDR], &u8reg, 1);
			g_st_ts.u16_read_para = u8reg;
			/*rm_printk("Raydium - KRL_CMD_READ "
			"- 0x%x:0x%x\n", p_cmd_tbl[_ADDR], u8reg);*/
			break;
		case KRL_CMD_WRITE_WO_DATA:
			/*rm_printk("Raydium - KRL_CMD_WRITE_WO_DATA "
			"- 0x%x:0x%x\n", p_cmd_tbl[_ADDR], u8reg);*/
			ret = rm_tch_spi_byte_write(p_cmd_tbl[_ADDR], u8reg);
			break;
		case KRL_CMD_IF_AND_OR:
			if (u8reg & p_cmd_tbl[_ADDR])
				u8reg |= p_cmd_tbl[_DATA];
			ret = RETURN_OK;
			break;
		case KRL_CMD_AND:
			u8reg &= p_cmd_tbl[_DATA];
			ret = RETURN_OK;
			break;
		case KRL_CMD_OR:
			u8reg |= p_cmd_tbl[_DATA];
			ret = RETURN_OK;
			break;
		case KRL_CMD_DRAM_INIT:
			ret = rm_tch_spi_byte_write(0x01, 0x00);
			ret = rm_tch_spi_byte_write(0x02, 0x00);
			break;
		case KRL_CMD_READ_IMG:
			/*rm_printk("Raydium - KRL_CMD_READ_IMG "
			"- 0x%x:%p:%d\n",
			p_cmd_tbl[_ADDR],
			g_pu8_burstread_buf,
			g_st_ctrl.u16_data_length);*/
			if (g_pu8_burstread_buf != NULL) {
				ret = rm_tch_spi_read(p_cmd_tbl[_ADDR],
				g_pu8_burstread_buf + QUEUE_HEADER_NUM,
				g_st_ctrl.u16_data_length
				/* - QUEUE_HEADER_NUM*/);
			}
			g_pu8_burstread_buf = NULL;
			break;
		case KRL_CMD_WRITE_W_DATA:
			/*rm_printk("Raydium - KRL_CMD_WRITE_W_DATA "
			"- 0x%x:0x%x\n", p_cmd_tbl[_ADDR], p_cmd_tbl[_DATA]);*/
			ret = rm_tch_spi_byte_write(p_cmd_tbl[_ADDR],
				p_cmd_tbl[_DATA]);
			break;
		case KRL_CMD_NOT:
			u8reg = ~u8reg;
			/*g_st_ts.u16_read_para = u8reg;*/
			ret = RETURN_OK;
			break;
		case KRL_CMD_XOR:
			u8reg ^= p_cmd_tbl[_DATA];
			ret = RETURN_OK;
			break;
		case KRL_CMD_SEND_SIGNAL:
			u16Tmp = p_cmd_tbl[_DATA];
			/*rm_printk("Raydium - KRL_CMD_SEND_SIGNAL "
			"- %d\n", u16Tmp);*/
			ret = rm_tch_ts_send_signal(g_st_ts.u32_hal_pid,
				(int)u16Tmp);
			if (u16Tmp == RM_SIGNAL_RESUME)
				g_st_ts.u8_resume_cnt++;
			break;
		case KRL_CMD_CONFIG_RST:
			/*rm_printk("Raydium - KRL_CMD_CONFIG_RST "
			"- %d - %d\n", p_cmd_tbl[_SUB_CMD], p_cmd_tbl[_DATA]);*/
			switch (p_cmd_tbl[_SUB_CMD]) {
			case KRL_SUB_CMD_SET_RST_GPIO:
				ret = gpio_direction_output(pdata->gpio_reset,
					p_cmd_tbl[_DATA]);
				break;
			case KRL_SUB_CMD_SET_RST_VALUE:
				gpio_set_value(pdata->gpio_reset,
					p_cmd_tbl[_DATA]);
				ret = RETURN_OK;
				break;
			}
			break;
		case KRL_CMD_CONFIG_3V3:/*Need to check qpio setting*/
			/*rm_printk("Raydium - KRL_CMD_CONFIG_3V3 "
			"- %d - %d\n", p_cmd_tbl[_SUB_CMD], p_cmd_tbl[_DATA]);
			rm_printk("Raydium - 3.3V regulator is %d\n",
			regulator_is_enabled(ts->regulator_3v3));*/
			ret = KRL_CMD_CONFIG_3V3_Handler(p_cmd_tbl[_SUB_CMD],
				p_cmd_tbl[_DATA], ts);
			/*rm_printk("Raydium - 3.3V regulator is %d\n",
			regulator_is_enabled(ts->regulator_3v3));*/
			break;
		case KRL_CMD_CONFIG_1V8:/*Need to check qpio setting*/
			/*rm_printk("Raydium - KRL_CMD_CONFIG_1V8 "
			"- %d - %d\n", p_cmd_tbl[_SUB_CMD], p_cmd_tbl[_DATA]);
			rm_printk("Raydium - 1.8V regulator is %d\n",
			regulator_is_enabled(ts->regulator_1v8));*/
			ret = KRL_CMD_CONFIG_1V8_Handler(p_cmd_tbl[_SUB_CMD],
				p_cmd_tbl[_DATA], ts);
			/*rm_printk("Raydium - 1.8V regulator is %d\n",
			regulator_is_enabled(ts->regulator_1v8));*/
			break;
		case KRL_CMD_CONFIG_CLK:
			/*rm_printk("Raydium - KRL_CMD_CONFIG_CLK"
			" - %d - %d\n", p_cmd_tbl[_SUB_CMD],
			p_cmd_tbl[_DATA]);*/
			if (p_cmd_tbl[_SUB_CMD] == KRL_SUB_CMD_SET_CLK) {
				if (ts && ts->clk) {
					if (p_cmd_tbl[_DATA])
						ret = clk_enable(ts->clk);
					else {
						clk_disable(ts->clk);
						ret = RETURN_OK;
					}
				} else {
					dev_err(&g_spi->dev, "Raydium - %s : No clk handler!\n",
						__func__);
					ret = RETURN_OK;
				}
			} else
				ret = RETURN_FAIL;
			break;
		case KRL_CMD_CONFIG_CS:
			/*rm_printk("Raydium - KRL_CMD_CONFIG_CS "
			"- %d - %d\n", p_cmd_tbl[_SUB_CMD], p_cmd_tbl[_DATA]);*/
			switch (p_cmd_tbl[_SUB_CMD]) {
			case KRL_SUB_CMD_SET_CS_LOW:
#ifdef CS_SUPPORT
			/*
			* spi_cs_low - set chip select pin state
			* @spi: device for which chip select pin state to be set
			* state: if true chip select pin will be kept low else
			* high
			* The return value is zero for success, else
			* errno status code.
			*
			* int spi_cs_low(struct spi_device *spi, bool state)
			*/
				ret = spi_cs_low(g_spi,
					(bool)!!p_cmd_tbl[_DATA]);
#else
				ret = RETURN_OK;
#endif
				break;
			}
			break;
		case KRL_CMD_SET_TIMER:
			/*rm_printk("Raydium - KRL_CMD_SET_TIMER "
			"- %d\n", p_cmd_tbl[_SUB_CMD]);*/
			ret = RETURN_OK;
			if (p_cmd_tbl[_SUB_CMD] == KRL_SUB_CMD_INIT_TIMER)
				init_ts_timer();
			else if (p_cmd_tbl[_SUB_CMD] == KRL_SUB_CMD_ADD_TIMER) {
				if (!timer_pending(&ts_timer_triggle))
					add_timer(&ts_timer_triggle);
			} else if (p_cmd_tbl[_SUB_CMD] == KRL_SUB_CMD_DEL_TIMER)
				/*del_timer of an inactive timer returns 0,
				del_timer of an active timer returns 1*/
				del_timer(&ts_timer_triggle);
			else
				ret = RETURN_FAIL;
			break;
		case KRL_CMD_MSLEEP:
			/*rm_printk("Raydium - KRL_CMD_MSLEEP "
			"- %d ms\n", p_cmd_tbl[_DATA] |
			(p_cmd_tbl[_SUB_CMD] << 8));*/
			u32Tmp = (u16)(p_cmd_tbl[_DATA] |
					(p_cmd_tbl[_SUB_CMD] << 8));

			u32Tmp *= 1000;
			usleep_range(u32Tmp, u32Tmp + 200);
			ret = RETURN_OK;
			break;
		case KRL_CMD_FLUSH_QU:
			/*rm_printk("Raydium - KRL_CMD_FLUSH_QU "
			"- %d\n", p_cmd_tbl[_SUB_CMD]);*/
			ret = RETURN_OK;
			if (p_cmd_tbl[_SUB_CMD] == KRL_SUB_CMD_SENSOR_QU) {
#if (ISR_POST_HANDLER == WORK_QUEUE)
				mutex_unlock(&lock);
				flush_workqueue(g_st_ts.rm_workqueue);
				g_worker_queue_is_flush = true;
				mutex_lock(&lock);
#endif
			} else if (p_cmd_tbl[_SUB_CMD] ==
						KRL_SUB_CMD_TIMER_QU) {
				mutex_unlock(&lock);
				flush_workqueue(g_st_ts.rm_timer_workqueue);
				g_timer_queue_is_flush = true;
				mutex_lock(&lock);
			} else
				ret = RETURN_FAIL;
			break;
		case KRL_CMD_WRITE_W_COUNT:
			/*rm_printk("Raydium - KRL_CMD_WRITE_W_COUNT "
			"- 0x%x: 0x%x..0x%x\n", p_cmd_tbl[_ADDR], u8reg,
			ts->u8_repeat_counter);*/
			if (ts)
				ret = rm_tch_spi_byte_write(p_cmd_tbl[_ADDR],
					u8reg | (ts->u8_repeat_counter));
			else
				ret = RETURN_FAIL;
			break;
		case KRL_CMD_RETURN_RESULT:
			g_st_ts.u16_read_para = u8reg;
			ret = RETURN_OK;
			break;
		case KRL_CMD_RETURN_VALUE:
			g_st_ts.u16_read_para = (p_cmd_tbl[_ADDR] << 8)
				+ (p_cmd_tbl[_DATA]);
			ret = RETURN_OK;
			/*rm_printk("Raydium - KRL_CMD_RETURN_VALUE,
			value=%d", ret);*/
			break;
		case KRL_CMD_WRITE_IMG:
			/*rm_printk("Raydium - KRL_CMD_WRITE_IMG -
			0x%x:0x%x:%d\n", p_cmd_tbl[_ADDR], g_pu8_burstread_buf,
			g_st_ctrl.u16_data_length);*/
			/*ret = rm_tch_spi_byte_write(p_cmd_tbl[_ADDR],
				g_u8_update_baseline[0]);
			ret = rm_tch_spi_write(&g_u8_update_baseline[1],
				g_st_ctrl.u16_data_length - 1);*/
			ret = rm_tch_spi_burst_write(p_cmd_tbl[_ADDR],
				&g_u8_update_baseline[0],
				g_st_ctrl.u16_data_length);
			break;
		case KRL_CMD_CONFIG_IRQ:
			if (ts && (p_cmd_tbl[_SUB_CMD]
					== KRL_SUB_CMD_SET_IRQ)) {
				if (ts->irq) {
					if (p_cmd_tbl[_DATA])
						enable_irq(ts->irq);
					else
						disable_irq(ts->irq);
					} else {
						dev_err(&g_spi->dev,
							"Raydium - %s : No irq handler!\n",
							__func__);
					}
				ret = RETURN_OK;
			} else
				ret = RETURN_FAIL;
			break;
		case KRL_CMD_DUMMY:
			ret = RETURN_OK;
			break;
		default:
			break;
		}

		if (ret) {
			rm_show_kernel_tbl_name(p_cmd_tbl);

			dev_err(&g_spi->dev, "Raydium - u8_cmd:0x%x, addr:0x%x, data:0x%x\n",
				p_cmd_tbl[_CMD],
				p_cmd_tbl[_ADDR],
				p_cmd_tbl[_DATA]);
			break;
		}
	}

	mutex_unlock(&lock);

	return ret;
}

int rm_set_kernel_tbl(int i_func_idx, u8 *p_u8_src)
{
	ssize_t missing;
	u16 u16_len = 0;
	u8 *p_u8_len;
	u8 *p_u8_dst;

	switch (i_func_idx) {
	case KRL_INDEX_FUNC_SET_IDLE:
		p_u8_dst = g_st_cmd_set_idle;
		break;
	case KRL_INDEX_FUNC_PAUSE_AUTO:
		p_u8_dst = g_st_cmd_pause_auto;
		break;
	case KRL_INDEX_RM_RESUME:
		p_u8_dst = g_st_rm_resume_cmd;
		break;
	case KRL_INDEX_RM_SUSPEND:
		p_u8_dst = g_st_rm_suspend_cmd;
		break;
	case KRL_INDEX_RM_READ_IMG:
		p_u8_dst = g_st_rm_readimg_cmd;
		break;
	case KRL_INDEX_RM_WATCHDOG:
		p_u8_dst = g_st_rm_watchdog_cmd;
		break;
	case KRL_INDEX_RM_TESTMODE:
		p_u8_dst = g_st_rm_testmode_cmd;
		break;
	case KRL_INDEX_RM_SLOWSCAN:
		p_u8_dst = g_st_rm_slow_scan_cmd;
		break;
	case KRL_INDEX_RM_CLEARINT:
		p_u8_dst = g_st_rm_clear_int_cmd;
		break;
	case KRL_INDEX_RM_SCANSTART:
		p_u8_dst = g_st_rm_scan_start_cmd;
		break;
	case KRL_INDEX_RM_WAITSCANOK:
		p_u8_dst = g_st_rm_wait_scan_ok_cmd;
		break;
	case KRL_INDEX_RM_SETREPTIME:
		p_u8_dst = g_st_rm_set_rep_time_cmd;
		break;
	case KRL_INDEX_RM_NSPARA:
		p_u8_dst = g_st_rm_ns_para_cmd;
		break;
	case KRL_INDEX_RM_WRITE_IMG:
		p_u8_dst = g_st_rm_writeimg_cmd;
		break;
	case KRL_INDEX_RM_TLK:
		p_u8_dst = g_st_rm_tlk_cmd;
		break;
	case KRL_INDEX_RM_KL_TESTMODE:
		p_u8_dst = g_st_rm_kl_testmode_cmd;
		break;
	default:
		dev_err(&g_spi->dev, "Raydium - %s : no such kernel table - err:%d\n",
			__func__, i_func_idx);
		return RETURN_FAIL;
	}

	p_u8_len = kmalloc(KRL_TBL_FIELD_POS_CASE_NUM, GFP_KERNEL);
	if (p_u8_len == NULL)
		return RETURN_FAIL;

	missing = copy_from_user(p_u8_len, p_u8_src,
		KRL_TBL_FIELD_POS_CASE_NUM);
	if (missing) {
		dev_err(&g_spi->dev, "Raydium - %s : copy failed - len:%d, miss:%zu\n",
			__func__, KRL_TBL_FIELD_POS_CASE_NUM, missing);
		kfree(p_u8_len);
		return missing;
	}

	u16_len = p_u8_len[KRL_TBL_FIELD_POS_LEN_H];
	u16_len <<= 8;
	u16_len |= p_u8_len[KRL_TBL_FIELD_POS_LEN_L];

	missing = copy_from_user(p_u8_dst, p_u8_src, u16_len);
	if (missing) {
		dev_err(&g_spi->dev, "Raydium - %s : copy failed - len:%d, miss:%zu\n",
			__func__, u16_len, missing);
		kfree(p_u8_len);
		return missing;
	}

	kfree(p_u8_len);
	return RETURN_OK;
}
/*=============================================================================

=============================================================================*/
static void rm_tch_enter_manual_mode(void)
{
#if (ISR_POST_HANDLER == WORK_QUEUE)
	flush_workqueue(g_st_ts.rm_workqueue);
#endif
	mutex_lock(&g_st_ts.mutex_scan_mode);
	if (g_st_ts.u8_scan_mode_state == RM_SCAN_ACTIVE_MODE) {
		mutex_unlock(&g_st_ts.mutex_scan_mode);
		return;
	}

	if (g_st_ts.u8_scan_mode_state == RM_SCAN_PRE_IDLE_MODE) {
		g_st_ts.u8_scan_mode_state = RM_SCAN_ACTIVE_MODE;
		mutex_unlock(&g_st_ts.mutex_scan_mode);
		return;
	}

	if (g_st_ts.u8_scan_mode_state == RM_SCAN_IDLE_MODE) {
		rm_tch_ctrl_leave_auto_mode();
		g_st_ts.u8_scan_mode_state = RM_SCAN_ACTIVE_MODE;
		mutex_unlock(&g_st_ts.mutex_scan_mode);
		usleep_range(10000, 10050);/*msleep(10);*/
		return;
	}

	mutex_unlock(&g_st_ts.mutex_scan_mode);
}

static u32 rm_tch_get_platform_id(u8 *p)
{
	u32 u32_ret;
	u8 u8_temp = 0;

	struct rm_spi_ts_platform_data *pdata;
	pdata = g_input_dev->dev.parent->platform_data;

	u8_temp = (u8)pdata->platform_id;
	u32_ret = copy_to_user(p, &u8_temp, sizeof(u8));

	return u32_ret;
}

static u32 rm_tch_get_gpio_sensor_select(u8 *p)
{
	u32 u32_ret = 0;
	u8 u8_temp = 0;
	struct rm_spi_ts_platform_data *pdata;
	pdata = g_input_dev->dev.parent->platform_data;

/* Read from GPIO
	u32_ret = gpio_set_value(pdata->gpio_sensor_select0)
		| (1 << gpio_set_value(pdata->gpio_sensor_select1));
*/

	/* Read from data struct */
	if (pdata->gpio_sensor_select0)
		u8_temp |= 0x01;
	if (pdata->gpio_sensor_select1)
		u8_temp |= 0x02;

	if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
		rm_printk("Raydium - %s : %d\n",
				__func__, u8_temp);

	u32_ret = copy_to_user(p, &u8_temp, sizeof(u8));

	return u32_ret;
}

static u32 rm_tch_get_spi_lock_status(u8 *p)
{
	u32 u32_ret;
	u8 u8_temp;

	if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
		rm_printk("Raydium - SPI_LOCK = %d\n", g_st_ts.u8_spi_locked);

	if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
		rm_printk("Raydium - IS_SUSPENDED = %d\n",
			g_st_ts.b_is_suspended);

	u8_temp = g_st_ts.u8_spi_locked | g_st_ts.b_is_suspended;

	u32_ret = copy_to_user(p, &u8_temp, sizeof(u8));

	if (g_st_ts.u8_spi_locked && g_st_ts.u8_resume_cnt)
		g_st_ts.u8_resume_cnt--;

	return u32_ret;
}

/*===========================================================================*/
static int rm_tch_ts_send_signal(int pid, int i_info)
{
	struct siginfo info;
	struct task_struct *t;
	int ret = RETURN_OK;

	if (!pid) {
		dev_err(&g_spi->dev, "Raydium - %s : pid failed\n", __func__);
		return RETURN_FAIL;
	}

	/* send the signal */
	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = RM_TS_SIGNAL;
	info.si_code = SI_QUEUE;
	/*
		this is bit of a trickery: SI_QUEUE is normally
		used by sigqueue from user space, and kernel
		space should use SI_KERNEL. But if SI_KERNEL
		is used the real_time data is not delivered to
		the user space signal handler function.
	*/
	info.si_int = i_info;	/*real time signals may have 32 bits of data.*/

	rcu_read_lock();
	t = find_task_by_vpid(pid);
	rcu_read_unlock();
	if (t == NULL) {
		dev_err(&g_spi->dev, "Raydium - %s : no such pid\n", __func__);
		return RETURN_FAIL;
	} else /*send the signal*/
		ret = send_sig_info(RM_TS_SIGNAL, &info, t);

	if (ret)
		dev_err(&g_spi->dev, "Raydium - %s : send sig failed err:%d\n",
			__func__, ret);

	return ret;
}

/*=============================================================================
	Description:
		Queuing functions.
	Input:
		N/A
	Output:
		0:succeed
		others:error code
=============================================================================*/
static void rm_tch_queue_reset(void)
{
	g_st_q.u16_rear = 0;
	g_st_q.u16_front = 0;
}

static int rm_tch_queue_init(void)
{
	rm_tch_queue_reset();
	g_st_q.p_u8_queue = kmalloc(QUEUE_COUNT * RM_RAW_DATA_LENGTH,
								GFP_KERNEL);
	if (g_st_q.p_u8_queue == NULL)
		return -ENOMEM;

	return RETURN_OK;
}

static void rm_tch_queue_free(void)
{
	if (!g_st_q.p_u8_queue)
		return;
	kfree(g_st_q.p_u8_queue);
	g_st_q.p_u8_queue = NULL;
}

#ifdef ENABLE_CALC_QUEUE_COUNT
static int rm_tch_queue_get_current_count(void)
{
	if (g_st_q.u16_rear >= g_st_q.u16_front)
		return g_st_q.u16_rear - g_st_q.u16_front;

	return (QUEUE_COUNT - g_st_q.u16_front) + g_st_q.u16_rear;
}
#endif

/*=============================================================================
	Description:
	About full/empty buffer distinction,
	There are a number of solutions like:
	1.Always keep one slot open.
	2.Use a fill count to distinguish the two cases.
	3.Use read and write counts to get the fill count from.
	4.Use absolute indices.
	we chose "keep one slot open" to make it simple and robust
	and also avoid race condition.
	Input:
		N/A
	Output:
		1:empty
		0:not empty
=============================================================================*/
static int rm_tch_queue_is_empty(void)
{
	if (g_st_q.u16_rear == g_st_q.u16_front)
		return TRUE;
	return FALSE;
}

/*=============================================================================
	Description:
	check queue full.
	Input:
		N/A
	Output:
		1:full
		0:not full
=============================================================================*/
static int rm_tch_queue_is_full(void)
{
	u16 u16_front = g_st_q.u16_front;

	if (g_st_q.u16_rear + 1 == u16_front)
		return TRUE;

	if ((g_st_q.u16_rear == (QUEUE_COUNT - 1)) && (u16_front == 0))
		return TRUE;

	return FALSE;
}

static void *rm_tch_enqueue_start(void)
{
	if (!g_st_q.p_u8_queue)	/*error handling for no memory*/
		return NULL;

	if (!rm_tch_queue_is_full()) {
		g_service_busy_report_count = 100;
		return &g_st_q.p_u8_queue[g_st_q.u16_rear];
	}

	if (g_service_busy_report_count < 0) {
		g_service_busy_report_count = 100;
		rm_printk("Raydium - touch service is busy,try again.\n");
	} else {
		g_service_busy_report_count--;
	}
	return NULL;
}

static void rm_tch_enqueue_finish(void)
{
	if (g_st_q.u16_rear == (QUEUE_COUNT - 1))
		g_st_q.u16_rear = 0;
	else
		g_st_q.u16_rear++;
}

static void *rm_tch_dequeue_start(void)
{
	if (!rm_tch_queue_is_empty())
		return &g_st_q.p_u8_queue[g_st_q.u16_front];

	return NULL;
}

static void rm_tch_dequeue_finish(void)
{
	if (g_st_q.u16_front == (QUEUE_COUNT - 1))
		g_st_q.u16_front = 0;
	else
		g_st_q.u16_front++;
}

static long rm_tch_queue_read_raw_data(u8 *p, u32 u32Len)
{
	u8 *p_u8_queue;
	u32 u32Ret;
	p_u8_queue = rm_tch_dequeue_start();
	if (!p_u8_queue)
		return RETURN_FAIL;

	u32Ret = copy_to_user(p, p_u8_queue, u32Len);
	if (u32Ret)
		return RETURN_FAIL;

	rm_tch_dequeue_finish();
	return RETURN_OK;
}

#if ENABLE_QUEUE_GUARD
static void rm_tch_queue_protector(void)
{
	u16 u16_q_front = g_st_q.u16_front;
	u16 u16_q_rear = g_st_q.u16_rear;
	u8 u8_queue_count = 0;
	u8 u8_down_level = 1;
	static int slowscan_level = RM_SLOW_SCAN_LEVEL_MAX;

	if (u16_q_front > u16_q_rear)
		u8_queue_count = QUEUE_COUNT - u16_q_front + u16_q_rear;
	else
		u8_queue_count = u16_q_rear - u16_q_front;

	for (; u8_down_level < RM_SLOW_SCAN_LEVEL_COUNT; u8_down_level++) {
		if (u8_queue_count < (Q_PRO_TH * u8_down_level))
			break;
	}

	u8_down_level = RM_SLOW_SCAN_LEVEL_COUNT - u8_down_level;
	if (slowscan_level != u8_down_level) {
		slowscan_level = u8_down_level;
		rm_tch_ctrl_slowscan(slowscan_level);
	}

	if (slowscan_level > RM_SLOW_SCAN_LEVEL_100)
		g_st_ts.b_enable_slow_scan = false;
	else
		g_st_ts.b_enable_slow_scan = true;
}
#endif
/*===========================================================================*/
#if (ISR_POST_HANDLER == WORK_QUEUE)
static void rm_work_handler(struct work_struct *work)
{
	void *p_kernel_buffer;
	u32 u32_flag;
	int i_ret;

	if (!g_st_ts.b_init_finish
		|| g_st_ts.b_is_suspended
		|| g_worker_queue_is_flush)
		return;

	i_ret = rm_tch_ctrl_clear_int();

#ifdef ENABLE_SLOW_SCAN
	if (g_st_ts.b_slow_scan_flg == true) {
		rm_tch_cmd_process((u8)(g_st_ts.u32_slow_scan_level - 1),
		g_st_rm_slow_scan_cmd, NULL);
		g_st_ts.b_slow_scan_flg = false;
	}
#endif

	u32_flag = rm_tch_ctrl_configure();

	if (u32_flag & RM_NEED_TO_SEND_SCAN)
		rm_tch_ctrl_scan_start();

	if (u32_flag & RM_NEED_TO_READ_RAW_DATA) {
#if ENABLE_QUEUE_GUARD
		rm_tch_queue_protector();
#endif
		p_kernel_buffer = rm_tch_enqueue_start();
		if (p_kernel_buffer) {
			i_ret = rm_tch_read_image_data((u8 *) p_kernel_buffer);
			if (!i_ret)
				rm_tch_enqueue_finish();
		}
	}

	if (u32_flag & RM_NEED_TO_SEND_SIGNAL) {
		if (g_st_ts.b_calc_finish) {
			g_st_ts.b_calc_finish = 0;
			rm_tch_ts_send_signal(g_st_ts.u32_hal_pid,
				RM_SIGNAL_INTR);
		}
	}
}
#elif (ISR_POST_HANDLER == KTHREAD)
static int rm_work_thread_function(void *data)
{
	void *p_kernel_buffer;
	u32 u32_flag;
	int i_ret;

	sched_setscheduler(current_thread_info()->task,
		SCHED_FIFO, &g_st_ts.irq_thread_sched);

	do {
		wait_event_interruptible(g_st_ts.rm_irq_thread_wait_q,
			g_st_ts.b_irq_thread_active);

		if (!g_st_ts.b_init_finish || g_st_ts.b_is_suspended)
			continue;

		i_ret = rm_tch_ctrl_clear_int();

#ifdef ENABLE_SLOW_SCAN
		if (g_st_ts.b_slow_scan_flg == true) {
			rm_tch_cmd_process(
				(u8)(g_st_ts.u32_slow_scan_level - 1),
				g_st_rm_slow_scan_cmd, NULL);
			g_st_ts.b_slow_scan_flg = false;
		}
#endif

		u32_flag = rm_tch_ctrl_configure();

		if (u32_flag & RM_NEED_TO_SEND_SCAN)
			rm_tch_ctrl_scan_start();

		if (u32_flag & RM_NEED_TO_READ_RAW_DATA) {
#if ENABLE_QUEUE_GUARD
			rm_tch_queue_protector();
#endif
			p_kernel_buffer = rm_tch_enqueue_start();
			if (p_kernel_buffer) {
				i_ret = rm_tch_read_image_data(
					(u8 *) p_kernel_buffer);
				if (!i_ret)
					rm_tch_enqueue_finish();
			}
		}

		if (u32_flag & RM_NEED_TO_SEND_SIGNAL) {
			if (g_st_ts.b_calc_finish) {
				g_st_ts.b_calc_finish = 0;
				rm_tch_ts_send_signal(g_st_ts.u32_hal_pid,
					RM_SIGNAL_INTR);
			}
		}
		mutex_lock(&g_st_ts.mutex_irq_wait);
		if (!g_st_ts.b_irq_is_waited)
			g_st_ts.b_irq_thread_active = false;
		else
			g_st_ts.b_irq_is_waited = false;
		mutex_unlock(&g_st_ts.mutex_irq_wait);
	} while (!kthread_should_stop());

	return 0;
}
#endif

static void rm_tch_init_ts_structure_part(void)
{
	g_st_ts.b_init_finish = 0;
	g_st_ts.b_calc_finish = 0;
	g_st_ts.b_enable_scriber = 0;

#ifdef ENABLE_SLOW_SCAN
	g_st_ts.b_enable_slow_scan = false;
	g_st_ts.b_slow_scan_flg = false;
#endif
	mutex_lock(&g_st_ts.mutex_scan_mode);
	g_st_ts.u8_scan_mode_state = RM_SCAN_ACTIVE_MODE;
	mutex_unlock(&g_st_ts.mutex_scan_mode);

	g_pu8_burstread_buf = NULL;
#if (ISR_POST_HANDLER == WORK_QUEUE)
	g_worker_queue_is_flush = false;
#elif (ISR_POST_HANDLER == KTHREAD)
	mutex_lock(&g_st_ts.mutex_irq_wait);
	g_st_ts.b_irq_is_waited = false;
	mutex_unlock(&g_st_ts.mutex_irq_wait);
#endif

#if ENABLE_EVENT_QUEUE
	mutex_lock(&g_st_ts.mutex_event_waited);
	g_st_ts.b_event_is_waited = false;
	mutex_unlock(&g_st_ts.mutex_event_waited);
#endif
	g_timer_queue_is_flush = false;
	g_st_ts.u16_read_para = 0;

	rm_ctrl_watchdog_func(0);

	g_st_ts.b_is_suspended = false;
	g_st_ts.b_is_disabled = false;
	/*g_st_ts.u8_test_mode = false;*/
	g_st_ts.u8_test_mode_type = RM_TEST_MODE_NULL;

	b_bl_updated = false;
}

/*===========================================================================*/
static void rm_ctrl_watchdog_func(unsigned int u32_enable)
{
	g_st_ts.u8_watch_dog_flg = 0;
	g_st_ts.u32_watch_dog_cnt = 0;
	g_st_ts.b_watch_dog_check = 0;

	g_st_ts.u8_watch_dog_enable = u32_enable & 0x01;
	g_st_ts.u32_watch_dog_time = u32_enable >> 16;

	if (u32_enable&0x01) {
		g_st_ts.u8_watch_dog_enable = 1;
		g_st_ts.u32_watch_dog_time = u32_enable >> 16;
	} else {
		g_st_ts.u8_watch_dog_enable = 0;
		g_st_ts.u32_watch_dog_time = 0xFFFFFFFF;
	}

	if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
		rm_printk("Raydium - WatchDogEnable=%d, WatchDogTime=%d\n",
			g_st_ts.u8_watch_dog_enable,
			g_st_ts.u32_watch_dog_time);
}

static void rm_watchdog_work_function(unsigned char scan_mode)
{
	if ((g_st_ts.u8_watch_dog_enable == 0) ||
		(g_st_ts.b_init_finish == 0))
		return;

	if (g_st_ts.u32_watch_dog_cnt++ >= g_st_ts.u32_watch_dog_time) {
		if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
			rm_printk("Raydium - watchdog work: Time:%dsec Cnt:%d,Flg:%d(%x)\n",
				g_st_ts.u32_watch_dog_time,
				g_st_ts.u32_watch_dog_cnt,
				g_st_ts.u8_watch_dog_flg,
				g_st_ts.u8_scan_mode_state);

		switch (scan_mode) {
		case RM_SCAN_ACTIVE_MODE:
			g_st_ts.u32_watch_dog_cnt = 0;
			g_st_ts.u8_watch_dog_flg = 1;
			break;
		case RM_SCAN_IDLE_MODE:
			g_st_ts.u32_watch_dog_cnt = 0;
			g_st_ts.b_watch_dog_check = 1;
			break;
		}
	}

	if (g_st_ts.u8_watch_dog_flg) {
		/*WATCH DOG RESET*/
		/*rm_printk("Raydium - WatchDog Resume\n");*/
		mutex_unlock(&g_st_ts.mutex_scan_mode);
		rm_tch_init_ts_structure_part();
		mutex_lock(&g_st_ts.mutex_scan_mode);
		g_st_ts.b_is_suspended = true;
		rm_tch_cmd_process(0, g_st_rm_watchdog_cmd, NULL);
		g_st_ts.b_is_suspended = false;
		return;
	}

}

static u8 rm_timer_trigger_function(void)
{
	static u32 u32TimerCnt; /*= 0; remove by checkpatch*/

	if (u32TimerCnt++ < g_st_ctrl.u8_timer_trigger_scale) {
		return FALSE;
	} else {
		/*rm_printk("Raydium - rm_timer_work_handler:%x,%x\n",
		g_st_ctrl.u8_timer_trigger_scale, u32TimerCnt);*/
		u32TimerCnt = 0;
		return TRUE;
	}
}

static void rm_timer_work_handler(struct work_struct *work)
{
	static u16 u32TimerCnt; /*= 0; remove by checkpatch*/
	if (g_st_ts.b_is_suspended)
		return;

	if (g_timer_queue_is_flush == true)
		return;

	if (rm_timer_trigger_function()) {
		mutex_lock(&g_st_ts.mutex_scan_mode);
		if (g_st_ts.u8_scan_mode_state != RM_SCAN_ACTIVE_MODE)
			rm_watchdog_work_function(RM_SCAN_IDLE_MODE);
		else
			rm_watchdog_work_function(RM_SCAN_ACTIVE_MODE);
		mutex_unlock(&g_st_ts.mutex_scan_mode);
	}

	if (g_st_ts.b_watch_dog_check == 1) {
		rm_tch_ts_send_signal(g_st_ts.u32_hal_pid,
			RM_SIGNAL_WATCH_DOG_CHECK);
		g_st_ts.b_watch_dog_check = 0;
	}

	if (g_st_ts.u8_watch_dog_enable) {
		u32TimerCnt++;
		if (u32TimerCnt > g_st_ts.u32_watch_dog_time &&
			g_st_ts.u32_watch_dog_time !=
				g_st_ctrl.u8_watch_dog_normal_cnt) {
			g_st_ts.u32_watch_dog_time =
				g_st_ctrl.u8_watch_dog_normal_cnt;
			if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
				rm_printk("Raydium - WDT:Normal mode\n");
			u32TimerCnt = 0;
		}
	} else {
		u32TimerCnt = 0;
	}
}

/*========================================================================= */
static void rm_tch_enable_irq(struct rm_tch_ts *ts)
{
	enable_irq(ts->irq);
}

static void rm_tch_disable_irq(struct rm_tch_ts *ts)
{
	disable_irq(ts->irq);
}

#ifdef ENABLE_SLOW_SCAN
/*=============================================================================
 * Description:
 *		Context dependent touch system.
 *		Change scan speed for slowscan function.
 * Input:
 *		slowscan level
 * Output:
 *		N/A
 *===========================================================================*/
static void rm_tch_ctrl_slowscan(u32 level)
{
	if (level == RM_SLOW_SCAN_LEVEL_NORMAL)
		level = RM_SLOW_SCAN_LEVEL_20;
	if (level > RM_SLOW_SCAN_LEVEL_100)
		level = RM_SLOW_SCAN_LEVEL_MAX;

	g_st_ts.u32_slow_scan_level = level;
	g_st_ts.b_slow_scan_flg = true;
}

static u32 rm_tch_slowscan_round(u32 val)
{
	u32 i;

	for (i = 0; i < RM_SLOW_SCAN_LEVEL_COUNT; i++) {
		if ((i * RM_SLOW_SCAN_INTERVAL) >= val)
			break;
	}

	if (i > RM_SLOW_SCAN_LEVEL_MAX)
		i = RM_SLOW_SCAN_LEVEL_MAX;

	return i;
}

static ssize_t rm_tch_slowscan_handler(const char *buf, size_t count)
{
	unsigned long val;
	ssize_t error;
	ssize_t ret;

	if (count < 2)
		return -EINVAL;

	ret = (ssize_t) count;

	if (count == 2) {
		if (buf[0] == '0') {
			g_st_ts.b_enable_slow_scan = false;
			rm_tch_ctrl_slowscan(RM_SLOW_SCAN_LEVEL_MAX);
		} else if (buf[0] == '1') {
			g_st_ts.b_enable_slow_scan = true;
			rm_tch_ctrl_slowscan(RM_SLOW_SCAN_LEVEL_60);
		}
	} else if ((buf[0] == '2') && (buf[1] == ' ')) {
		error = kstrtoul(&buf[2], 10, &val);

		if (error) {
			ret = error;
		} else {
			g_st_ts.b_enable_slow_scan = true;
			rm_tch_ctrl_slowscan(rm_tch_slowscan_round((u32)val));
		}
	}
	return ret;
}
#endif

static ssize_t rm_tch_slowscan_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#ifdef ENABLE_SLOW_SCAN
	return sprintf(buf, "Slow Scan:%s\nScan Rate:%dHz\n",
			g_st_ts.b_enable_slow_scan ?
			"Enabled" : "Disabled",
			g_st_ts.u32_slow_scan_level * RM_SLOW_SCAN_INTERVAL);
#else
	return sprintf(buf, "Not implemented yet\n");
#endif
}

static ssize_t rm_tch_slowscan_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
#ifdef ENABLE_SLOW_SCAN
	return rm_tch_slowscan_handler(buf, count);
#else
	return count;
#endif
}

static ssize_t rm_tch_touchfile_check_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return sprintf(buf, "0x%x\n", g_st_ts.u8_touchfile_check);
}

static ssize_t rm_tch_touchfile_check_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

static ssize_t rm_tch_touch_event_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return sprintf(buf, "0x%x\n",
		g_st_ts.u8_touch_event);
}

static ssize_t rm_tch_touch_event_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

static void rm_tch_smooth_level_change(unsigned long val)
{
	int i_info;

	if (val > RM_SMOOTH_LEVEL_MAX)
		return;

	g_st_ts.u32_smooth_level = val;

	i_info = (RM_SIGNAL_PARA_SMOOTH << 24) |
			(val << 16) |
			RM_SIGNAL_CHANGE_PARA;

	rm_tch_ts_send_signal(g_st_ts.u32_hal_pid, i_info);
}

static ssize_t rm_tch_smooth_level_handler(const char *buf, size_t count)
{
	unsigned long val;
	ssize_t error;
	ssize_t ret;

	if (count != 2)
		return -EINVAL;

	ret = (ssize_t) count;
	error = kstrtoul(buf, 10, &val);

	if (error)
		ret = error;
	else
		rm_tch_smooth_level_change(val);

	return ret;
}

static ssize_t rm_tch_smooth_level_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return sprintf(buf, "Smooth level:%d\n", g_st_ts.u32_smooth_level);
}

static ssize_t rm_tch_smooth_level_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	rm_tch_smooth_level_handler(buf, count);
	return count;
}

void rm_set_kernel_test_para(u8 u8Idx, u8 u8Para)
{
	struct rm_tch_ts *ts = input_get_drvdata(g_input_dev);

	ts->u8_repeat_counter = u8Para;
	rm_tch_cmd_process(u8Idx, g_st_rm_kl_testmode_cmd, ts);
}

static ssize_t rm_tch_testmode_handler(const char *buf, size_t count)
{
	unsigned long val = 0;
	ssize_t error;
	ssize_t ret;

	if (count < 2)
		return -EINVAL;

	ret = (ssize_t) count;

	if (count == 2) {
		if (buf[0] == '0') {
			g_st_ts.u8_test_mode = false;
			g_st_ts.u8_test_mode_type = RM_TEST_MODE_NULL;
			rm_set_kernel_test_para(0, g_st_ctrl.u8_idle_mode_thd);
		} else if (buf[0] == '1') {
			g_st_ts.u8_test_mode = true;
			g_st_ts.u8_test_mode_type = RM_TEST_MODE_IDLE_SHOW;
		}
	} else if ((buf[0] == '2') && (buf[1] == ' ')) {
		error = kstrtoul(&buf[2], 10, &val);

		if (error) {
			if ((buf[2] == '2') && (buf[3] == ' ')) {
				g_st_ts.u8_test_mode = true;
				g_st_ts.u8_test_mode_type =
				RM_TEST_MODE_IDLE_LEVEL;
				error = kstrtoul(&buf[4], 10, &val);
				if (error) {
					g_st_ts.u8_test_mode = false;
					ret = error;
				} else {
					rm_set_kernel_test_para(0, val);
				}
			} else {
				g_st_ts.u8_test_mode = false;
				ret = error;
			}
		} else {
		g_st_ts.u8_test_mode = true;
		g_st_ts.u8_test_mode_type = 1 << ((u8)val - 1);
		switch (val) {
		case RM_TEST_MODE_IDLE_SHOW:
		if (g_st_ts.u8_scan_mode_state == RM_SCAN_IDLE_MODE)
#if (ISR_POST_HANDLER == WORK_QUEUE)
			queue_work(g_st_ts.rm_workqueue,
				&g_st_ts.rm_work);
#elif (ISR_POST_HANDLER == KTHREAD)
		{
			if (waitqueue_active(&g_st_ts.rm_irq_thread_wait_q)) {
				g_st_ts.b_irq_thread_active = true;
				wake_up_interruptible(
					&g_st_ts.rm_irq_thread_wait_q);
			} else {
				mutex_lock(&g_st_ts.mutex_irq_wait);
				g_st_ts.b_irq_is_waited = true;
				mutex_unlock(&g_st_ts.mutex_irq_wait);
			}
		}
#endif
		break;
		case RM_TEST_MODE_IDLE_LEVEL:
			if ((buf[2] == '2') && (buf[3] == ' ')) {
				error = kstrtoul(&buf[4], 0, &val);
				if (error) {
					g_st_ts.u8_test_mode = false;
					ret = error;
				} else {
					rm_set_kernel_test_para(0, val);
				}
			}
			break;
		case RM_TEST_MODE_CALC_TIME_SHOW:
			break;
		default:
			g_st_ts.u8_test_mode = false;
			g_st_ts.u8_test_mode_type = 0;
		break;
		}
		}
	}
	rm_printk("Raydium - rm_kernel_test_mode:%s,Type:%d,Para:%d",
		g_st_ts.u8_test_mode ?
		"Enabled" : "Disabled",
		g_st_ts.u8_test_mode_type, (u8)val);
			return ret;
}

static ssize_t rm_tch_test_mode_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
		return sprintf(buf, "Test Mode:%s\nType:%d\n",
			g_st_ts.u8_test_mode ?
			"Enabled" : "Disabled",
			g_st_ts.u8_test_mode_type);
}

static ssize_t rm_tch_test_mode_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	g_u8_test_mode_count = count;
	memcpy(&g_u8_test_mode_buf, &buf, sizeof(buf));

	return rm_tch_testmode_handler(buf, count);
}

static ssize_t rm_tch_self_test_handler(struct rm_tch_ts *ts,
	const char *buf, size_t count)
{
	unsigned long val;
	ssize_t error;
	ssize_t ret;
	int i_info;

	ret = (ssize_t) count;

	if (count != 2)
		return -EINVAL;

	if (g_st_ts.u8_selftest_status == RM_SELF_TEST_STATUS_TESTING)
		return ret;

	rm_tch_enter_test_mode(1);

	g_st_ts.u8_selftest_result = RM_SELF_TEST_RESULT_PASS;

	error = kstrtoul(buf, 10, &val);

	if (error) {
		ret = error;
		rm_tch_enter_test_mode(0);
	} else if (val == 0) {
		rm_tch_enter_test_mode(0);
	} else if ((val >= 0x01) && (val <= 0xFF)) {
		i_info = (RM_SIGNAL_PARA_SELF_TEST << 24) |
				(val << 16) |
				RM_SIGNAL_CHANGE_PARA;
		rm_tch_ts_send_signal(g_st_ts.u32_hal_pid, i_info);
	}

	return ret;
}
static ssize_t selftest_platform_id_gpio_set(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

static ssize_t selftest_platform_id_gpio_get(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct rm_spi_ts_platform_data *pdata;

	pdata = g_input_dev->dev.parent->platform_data;

	buf[0] = (char)pdata->platform_id;

	/* Read from data struct */
	if (pdata->gpio_sensor_select0)
		buf[1] |= 0x01;
	if (pdata->gpio_sensor_select1)
		buf[1] |= 0x02;
	return 2;
}

static ssize_t selftest_enable_set(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	rm_printk("enter test mode buf[0] = %d\n", buf[0]);
	rm_tch_enter_test_mode(buf[0]);
	return count;
}

static ssize_t selftest_enable_get(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return 1;
}

static ssize_t rm_tch_self_test_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return sprintf(buf, "Self_Test:Status:%d ,Result:%d\n",
					g_st_ts.u8_selftest_status,
					g_st_ts.u8_selftest_result);
}

static ssize_t rm_tch_self_test_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct rm_tch_ts *ts = dev_get_drvdata(dev);
	rm_tch_self_test_handler(ts, buf, count);
	return count;
}

static ssize_t rm_tch_version_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return sprintf(buf, "Release V 0x%02X, Test V 0x%02X\n",
		g_st_ts.u8_version,
		g_st_ts.u8_test_version);
}

static ssize_t rm_tch_version_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

static ssize_t rm_tch_module_detect_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return sprintf(buf, "%s\n", "Raydium Touch Module");
}

static ssize_t rm_tch_module_detect_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

static void rm_tch_report_mode_change(unsigned long val)
{
	int i_info;

	g_st_ctrl.u8_event_report_mode = (u8)val;

	i_info = (RM_SIGNAL_PARA_REPORT_MODE_CHANGE << 24) |
			(g_st_ctrl.u8_event_report_mode << 16) |
			RM_SIGNAL_REPORT_MODE_CHANGE;

	rm_tch_ts_send_signal(g_st_ts.u32_hal_pid, i_info);
}

static ssize_t rm_tch_report_mode_handler(const char *buf, size_t count)
{
	unsigned long val;
	ssize_t error;
	ssize_t ret;

	if (count != 2)
		return -EINVAL;

	ret = (ssize_t) count;
	error = kstrtoul(buf, 10, &val);

	if (error)
		ret = error;
	else {
		if ((val >= EVENT_REPORT_MODE_STYLUS_ERASER_FINGER) &&
			(val < EVENT_REPORT_MODE_TYPE_NUM))
			rm_tch_report_mode_change(val);
		else
			ret = RETURN_FAIL;
	}

	return ret;
}

static ssize_t rm_tch_report_mode_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return sprintf(buf, "Raydium Touch Report Mode : %d\n",
		g_st_ctrl.u8_event_report_mode);
}

static ssize_t rm_tch_report_mode_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	rm_tch_report_mode_handler(buf, count);
	return count;
}

static ssize_t selftest_spi_byte_read_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	buf[0] = g_spi_buf[0];
	return 2;
}

static ssize_t selftest_spi_byte_read_set(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return rm_tch_spi_byte_read(buf[0], &g_spi_buf[0]);
}

static ssize_t selftest_spi_byte_write_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 1;
}

static ssize_t selftest_spi_byte_write_set(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return rm_tch_spi_byte_write(buf[0], buf[1]);
}

static ssize_t selftest_spi_burst_read_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;

	/*rm_printk("selftest_spi_burst_read_get %d", g_spi_bufsize);*/
	ret = rm_tch_spi_read(g_spi_addr, buf, g_spi_bufsize);
	if (ret == RETURN_OK)
		return g_spi_bufsize;

	return 0;
}


static ssize_t selftest_spi_burst_read_set(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	/*rm_printk("selftest_spi_burst_read_set %d",count);*/
	g_spi_addr = 0;
	g_spi_bufsize = 0;
	g_spi_addr = buf[0];
	g_spi_bufsize = buf[1];
	g_spi_bufsize <<= 8;
	g_spi_bufsize |= buf[2];

	return 0;
}

static ssize_t selftest_spi_burst_write_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 1;
}

static ssize_t selftest_spi_burst_write_set(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return rm_tch_spi_write((u8 *)buf, count);
}

static DEVICE_ATTR(get_platform_id_gpio, 0640,
					selftest_platform_id_gpio_get,
					selftest_platform_id_gpio_set);

static DEVICE_ATTR(selftest_enable, 0640,
					selftest_enable_get,
					selftest_enable_set);

static DEVICE_ATTR(selftest_spi_byte_read, 0640,
					selftest_spi_byte_read_get,
					selftest_spi_byte_read_set);

static DEVICE_ATTR(selftest_spi_byte_write, 0640,
					selftest_spi_byte_write_get,
					selftest_spi_byte_write_set);

static DEVICE_ATTR(selftest_spi_burst_read, 0640,
					selftest_spi_burst_read_get,
					selftest_spi_burst_read_set);

static DEVICE_ATTR(selftest_spi_burst_write, 0640,
					selftest_spi_burst_write_get,
					selftest_spi_burst_write_set);

static DEVICE_ATTR(slowscan_enable, 0640,
					rm_tch_slowscan_show,
					rm_tch_slowscan_store);

static DEVICE_ATTR(touchfile_check, 0640,
					rm_tch_touchfile_check_show,
					rm_tch_touchfile_check_store);

static DEVICE_ATTR(touch_event, 0640,
					rm_tch_touch_event_show,
					rm_tch_touch_event_store);

static DEVICE_ATTR(smooth_level, 0640,
					rm_tch_smooth_level_show,
					rm_tch_smooth_level_store);

static DEVICE_ATTR(self_test, 0640,
					rm_tch_self_test_show,
					rm_tch_self_test_store);

static DEVICE_ATTR(version, 0640,
					rm_tch_version_show,
					rm_tch_version_store);

static DEVICE_ATTR(module_detect, 0640,
					rm_tch_module_detect_show,
					rm_tch_module_detect_store);

static DEVICE_ATTR(report_mode, 0640,
					rm_tch_report_mode_show,
					rm_tch_report_mode_store);

static DEVICE_ATTR(test_mode, 0640,
					rm_tch_test_mode_show,
					rm_tch_test_mode_store);

static struct attribute *rm_ts_attributes[] = {
	&dev_attr_get_platform_id_gpio.attr,
	&dev_attr_slowscan_enable.attr,
	&dev_attr_touchfile_check.attr,
	&dev_attr_touch_event.attr,
	&dev_attr_selftest_enable.attr,
	&dev_attr_selftest_spi_byte_read.attr,
	&dev_attr_selftest_spi_byte_write.attr,
	&dev_attr_selftest_spi_burst_read.attr,
	&dev_attr_selftest_spi_burst_write.attr,
	&dev_attr_smooth_level.attr,
	&dev_attr_self_test.attr,
	&dev_attr_version.attr,
	&dev_attr_module_detect.attr,
	&dev_attr_report_mode.attr,
	&dev_attr_test_mode.attr,
	NULL
};

static const struct attribute_group rm_ts_attr_group = {
	.attrs = rm_ts_attributes,
};

static int rm_tch_input_open(struct input_dev *input)
{
	struct rm_tch_ts *ts = input_get_drvdata(input);

	rm_tch_enable_irq(ts);

	return RETURN_OK;
}

static void rm_tch_input_close(struct input_dev *input)
{
	struct rm_tch_ts *ts = input_get_drvdata(input);

	rm_tch_disable_irq(ts);
}

static irqreturn_t rm_tch_irq(int irq, void *handle)
{
	g_st_ts.u32_watch_dog_cnt = 0;

	trace_touchscreen_raydium_irq("Raydium_interrupt");

	if (/*g_st_ctrl.u8_power_mode &&*/
		(g_st_ts.u8_scan_mode_state == RM_SCAN_IDLE_MODE)) {
		input_event(g_input_dev, EV_MSC, MSC_ACTIVITY, 1);
#if (INPUT_PROTOCOL_CURRENT_SUPPORT == INPUT_PROTOCOL_TYPE_B)
		input_sync(g_input_dev);
#endif
	}

	if (g_st_ts.b_init_service && g_st_ts.b_init_finish
		&& g_st_ts.b_is_suspended == false) {
#if (ISR_POST_HANDLER == WORK_QUEUE)
		queue_work(g_st_ts.rm_workqueue, &g_st_ts.rm_work);
#elif (ISR_POST_HANDLER == KTHREAD)
		if (waitqueue_active(&g_st_ts.rm_irq_thread_wait_q)) {
			g_st_ts.b_irq_thread_active = true;
			wake_up_interruptible(&g_st_ts.rm_irq_thread_wait_q);
		} else {
			mutex_lock(&g_st_ts.mutex_irq_wait);
			g_st_ts.b_irq_is_waited = true;
			mutex_unlock(&g_st_ts.mutex_irq_wait);
		}
#endif
	}

	return IRQ_HANDLED;
}

static void rm_tch_enter_test_mode(u8 flag)
{
	if (flag) { /*enter test mode*/
		g_st_ts.b_selftest_enable = 1;
		g_st_ts.u8_selftest_status = RM_SELF_TEST_STATUS_TESTING;
		g_st_ts.b_is_suspended = true;
#if (ISR_POST_HANDLER == WORK_QUEUE)
		flush_workqueue(g_st_ts.rm_workqueue);
#endif
		flush_workqueue(g_st_ts.rm_timer_workqueue);
	} else { /*leave test mode*/
		g_st_ts.b_selftest_enable = 0;

		rm_tch_init_ts_structure_part();
		g_st_ts.b_is_suspended = false;
	}

	rm_tch_cmd_process(flag, g_st_rm_testmode_cmd, NULL);

	if (!flag)
		g_st_ts.u8_selftest_status = RM_SELF_TEST_STATUS_FINISH;
}

void rm_tch_set_variable(unsigned int index, unsigned int arg)
{
#if ENABLE_FREQ_HOPPING
	ssize_t missing;
#endif
	struct rm_tch_ts *ts = input_get_drvdata(g_input_dev);

	switch (index) {
	case RM_VARIABLE_SELF_TEST_RESULT:
		g_st_ts.u8_selftest_result = (u8) arg;
		rm_tch_enter_test_mode(0);
		break;
	case RM_VARIABLE_SCRIBER_FLAG:
		g_st_ts.b_enable_scriber = (bool) arg;
		break;
	case RM_VARIABLE_AUTOSCAN_FLAG:
		/*g_st_ctrl.u8_power_mode = (bool) arg;*/
		mutex_lock(&g_st_ts.mutex_scan_mode);
		if (g_st_ts.u8_scan_mode_state == RM_SCAN_ACTIVE_MODE)
			g_st_ts.u8_scan_mode_state = RM_SCAN_PRE_IDLE_MODE;
		mutex_unlock(&g_st_ts.mutex_scan_mode);
		break;
	case RM_VARIABLE_TEST_VERSION:
		g_st_ts.u8_test_version = (u8) arg;
		break;
	case RM_VARIABLE_VERSION:
		g_st_ts.u8_version = (u8) arg;
		dev_info(&g_spi->dev, "Raydium - Firmware v%d.%d\n",
				g_st_ts.u8_version, g_st_ts.u8_test_version);
		break;
	case RM_VARIABLE_IDLEMODECHECK:
		g_st_ctrl.u8_idle_mode_check = (u8) arg;
		if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
			rm_printk("Raydium - u8_idle_mode_check %2x\n",
				arg);
		break;
	case RM_VARIABLE_REPEAT:
		/*rm_printk("Raydium - Repeat %d\n", arg);*/
		g_st_ts.u8_repeat = (u8) arg;
#if ENABLE_FREQ_HOPPING
		g_st_ts.u8_ns_rpt = (u8) arg;
		if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
			rm_printk("g_st_ctrl.u8_ns_rpt %d\n",
				g_st_ts.u8_ns_rpt);
#endif
		break;
	case RM_VARIABLE_WATCHDOG_FLAG:
		rm_ctrl_watchdog_func(arg);
		g_st_ctrl.u8_ns_func_enable = ((u8)arg >> 6);
		if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
			rm_printk("g_st_ctrl.u8_ns_func_enable %d\n",
				g_st_ctrl.u8_ns_func_enable);
		break;
	case RM_VARIABLE_SET_SPI_UNLOCK:
		if (g_st_ts.u8_resume_cnt > 1)
			break;
		g_st_ts.u8_spi_locked = 0;
		if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
			rm_printk("Raydium - SET_SPI_UNLOCK\n");
		break;
	case RM_VARIABLE_SET_WAKE_UNLOCK:
		if (wake_lock_active(&g_st_ts.wakelock_initialization))
			wake_unlock(&g_st_ts.wakelock_initialization);
		break;
#if ENABLE_FREQ_HOPPING
	case RM_VARIABLE_DPW:
		mutex_lock(&g_st_ts.mutex_ns_mode);
		missing = copy_from_user(&g_st_ts.u8_ns_para[0],
			((u8 *)(uintptr_t)arg), 9);
		/*missing = copy_from_user(&g_st_ts.bDP[0],
			((u8 *)arg), 3);*/
		/*missing += copy_from_user(&g_st_ts.bSWCPW[0],
			(((u8 *)arg) + 3), 3);*/
		/*missing += copy_from_user(&g_st_ts.bSWC[0],
			(((u8 *)arg) + 6), 3);*/
		mutex_unlock(&g_st_ts.mutex_ns_mode);
		if (missing)
			dev_err(&g_spi->dev, "Raydium - %s RM_VARIABLE_DPW : copy failed - miss:%zu\n",
				__func__, missing);
		/*memcpy(&g_st_ts.bDP[0], ((u8 *)arg), 3);*/
		/*memcpy(&g_st_ts.bSWCPW[0], (((u8 *)arg) + 3), 3);*/
		/*memcpy(&g_st_ts.bSWC[0], (((u8 *)arg) + 6), 3);*/
		if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
			rm_printk("g_st_ctrl.DPW 0x%x:0x%x:0x%x # 0x%x:0x%x:0x%x,# 0x%x:0x%x:0x%x\n",
			/*g_st_ts.bDP[0], g_st_ts.bDP[1],
			g_st_ts.bDP[2], g_st_ts.bSWCPW[0],
			g_st_ts.bSWCPW[1], g_st_ts.bSWCPW[2],
			g_st_ts.bSWC[0], g_st_ts.bSWC[1],
			g_st_ts.bSWC[2]);*/
			g_st_ts.u8_ns_para[0], g_st_ts.u8_ns_para[1],
			g_st_ts.u8_ns_para[2], g_st_ts.u8_ns_para[3],
			g_st_ts.u8_ns_para[4], g_st_ts.u8_ns_para[5],
			g_st_ts.u8_ns_para[6], g_st_ts.u8_ns_para[7],
			g_st_ts.u8_ns_para[8]);
		break;
	case RM_VARIABLE_NS_MODE:
		mutex_lock(&g_st_ts.mutex_ns_mode);
		g_st_ts.u8_ns_mode = (u8)arg;
		mutex_unlock(&g_st_ts.mutex_ns_mode);
		if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
			rm_printk("g_st_ctrl.u8_ns_mode=%d\n",
				g_st_ts.u8_ns_mode);
		break;
#endif
	case RM_VARIABLE_TOUCHFILE_STATUS:
		g_st_ts.u8_touchfile_check = (u8)(arg);
		break;

	case RM_VARIABLE_TOUCH_EVENT:
		g_st_ts.u8_touch_event = (u8)(arg);
		rm_tch_generate_event(ts, g_st_ts.u8_touch_event);
		break;

	default:
		break;
	}

}

static u32 rm_tch_get_variable(unsigned int index, u8 *arg)
{
	u32 ret = RETURN_OK;
	switch (index) {
	case RM_VARIABLE_PLATFORM_ID:
		ret = rm_tch_get_platform_id((u8 *) arg);
		break;
	case RM_VARIABLE_GPIO_SELECT:
		ret = rm_tch_get_gpio_sensor_select((u8 *) arg);
		break;
	case RM_VARIABLE_CHECK_SPI_LOCK:
		ret = rm_tch_get_spi_lock_status((u8 *) arg);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static void rm_tch_init_ts_structure(void)
{
	g_st_ts.u32_hal_pid = 0;
	memset(&g_st_ts, 0, sizeof(struct rm31080a_ts_para));

#ifdef ENABLE_SLOW_SCAN
	g_st_ts.u32_slow_scan_level = RM_SLOW_SCAN_LEVEL_MAX;
#endif

	g_st_ts.u8_last_touch_count = 0;

#if (ISR_POST_HANDLER == WORK_QUEUE)
	g_st_ts.rm_workqueue = create_singlethread_workqueue("rm_work");
	INIT_WORK(&g_st_ts.rm_work, rm_work_handler);
#elif (ISR_POST_HANDLER == KTHREAD)
	init_waitqueue_head(&g_st_ts.rm_irq_thread_wait_q);
	g_st_ts.b_irq_thread_active = false;
	g_st_ts.b_irq_thread_alive = false;
	mutex_init(&g_st_ts.mutex_irq_wait);
#endif

#if ENABLE_EVENT_QUEUE
	init_waitqueue_head(&g_st_ts.rm_event_thread_wait_q);
	g_st_ts.b_event_thread_active = false;
	g_st_ts.b_event_thread_alive = false;
	mutex_init(&g_st_ts.mutex_event_waited);
	mutex_init(&g_st_ts.mutex_event_fetch);
	INIT_LIST_HEAD(&g_st_ts.touch_event_list);
#endif

	g_st_ts.rm_timer_workqueue =
		create_singlethread_workqueue("rm_idle_work");
	INIT_WORK(&g_st_ts.rm_timer_work, rm_timer_work_handler);

	wake_lock_init(&g_st_ts.wakelock_initialization,
		WAKE_LOCK_SUSPEND, "TouchInitialLock");

	mutex_init(&g_st_ts.mutex_scan_mode);
	mutex_init(&g_st_ts.mutex_ns_mode);

	g_st_ts.u8_resume_cnt = 0;
	g_st_ts.u8_touchfile_check = 0xFF;
	g_st_ts.u8_touch_event = 0xFF;
	g_st_ts.b_init_service = false;
	g_st_ts.u8_test_mode = false;
	rm_tch_ctrl_init();
}

static int rm31080_voltage_notifier_1v8(struct notifier_block *nb,
	unsigned long event, void *ignored)
{
	if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
		rm_printk("Raydium - REGULATOR EVENT 1.8V:0x%x\n",
			(unsigned int)event);

	return NOTIFY_OK;
}

static int rm31080_voltage_notifier_3v3(struct notifier_block *nb,
	unsigned long event, void *ignored)
{
	if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
		rm_printk("Raydium - REGULATOR EVENT 3.3V:0x%x\n",
			(unsigned int)event);

	return NOTIFY_OK;
}

/*===========================================================================*/
static void rm_ctrl_resume(struct rm_tch_ts *ts)
{
	g_st_ts.u8_spi_locked = 1;
	if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
		rm_printk("Raydium - SPI_LOCKED by resume!!\n");

	rm_tch_init_ts_structure_part();
	rm_tch_cmd_process(0, g_st_rm_resume_cmd, ts);

	if (g_st_ts.u8_test_mode)
		rm_tch_testmode_handler(g_u8_test_mode_buf,
			g_u8_test_mode_count);
}

static void rm_ctrl_suspend(struct rm_tch_ts *ts)
{
#if (INPUT_PROTOCOL_CURRENT_SUPPORT == INPUT_PROTOCOL_TYPE_B)
	int i;
#endif
	if (g_st_ts.b_is_suspended == true)
		return;

	g_st_ts.b_is_suspended = true;
	g_st_ts.b_init_finish = 0;

	if (g_st_ts.u8_scan_mode_state == RM_SCAN_IDLE_MODE)
		rm_ctrl_pause_auto_mode();

	rm_tch_ctrl_wait_for_scan_finish(0);

#if (ISR_POST_HANDLER == KTHREAD)
	if (!IS_ERR(g_st_ts.rm_irq_post_thread)) {
		g_st_ts.b_irq_thread_active = true;
		wake_up_interruptible(&g_st_ts.rm_irq_thread_wait_q);
		if (kthread_stop(g_st_ts.rm_irq_post_thread)) {
			if (g_st_ctrl.u8_kernel_msg & DEBUG_KTHREAD)
				rm_printk("Raydium - Kill IRQ poster failed!\n");
		} else {
			g_st_ts.b_irq_thread_alive = false;
			if (g_st_ctrl.u8_kernel_msg & DEBUG_KTHREAD)
				rm_printk("Raydium - Kill IRQ poster successfully!\n");
		}
	} else {
		if (g_st_ctrl.u8_kernel_msg & DEBUG_KTHREAD)
			rm_printk("Raydium - No IRQ poster exist!\n");
	}
#endif

#if ENABLE_EVENT_QUEUE
	if (!IS_ERR(g_st_ts.rm_event_post_thread)) {
		g_st_ts.b_event_thread_active = true;
		wake_up_interruptible(&g_st_ts.rm_event_thread_wait_q);
		if (kthread_stop(g_st_ts.rm_event_post_thread)) {
			if (g_st_ctrl.u8_kernel_msg & DEBUG_KTHREAD)
				rm_printk("Raydium - Kill Event poster failed!\n");
		} else {
			g_st_ts.b_event_thread_alive = false;
			if (g_st_ctrl.u8_kernel_msg & DEBUG_KTHREAD)
				rm_printk("Raydium - Kill Event poster successfully!\n");
		}
	} else {
		if (g_st_ctrl.u8_kernel_msg & DEBUG_KTHREAD)
			rm_printk("Raydium - No Event poster exist!\n");
	}
#endif

	rm_tch_cmd_process(0, g_st_rm_suspend_cmd, ts);
	rm_tch_ctrl_wait_for_scan_finish(0);
	rm_tch_cmd_process(1, g_st_rm_suspend_cmd, ts);

#if (INPUT_PROTOCOL_CURRENT_SUPPORT == INPUT_PROTOCOL_TYPE_B)
	for (i = 0; i < MAX_SLOT_AMOUNT; i++) {
		input_mt_slot(g_input_dev, i);

		input_mt_report_slot_state(
			g_input_dev,
			MT_TOOL_FINGER, false);

		input_report_key(
			g_input_dev,
			BTN_TOOL_RUBBER, false);
	}
	input_sync(g_input_dev);
	g_st_ts.u8_last_touch_count = 0;
#endif

	g_st_ts.u8_spi_locked = 1;
	if (g_st_ctrl.u8_kernel_msg & DEBUG_DRIVER)
		rm_printk("Raydium - SPI_LOCKED by suspend!!\n");

}

static int rm_tch_suspend(struct rm_tch_ts *ts)
{
	if (g_st_ts.b_init_service) {
		dev_info(ts->dev, "Raydium - Disable input device\n");
		rm_ctrl_suspend(ts);
		dev_info(ts->dev, "Raydium - Disable input device done\n");
	}
	return RETURN_OK;
}

static int rm_tch_resume(struct rm_tch_ts *ts)
{
	if (g_st_ts.b_init_service) {
		dev_info(ts->dev, "Raydium - Enable input device\n");
		if (wake_lock_active(&g_st_ts.wakelock_initialization))
			wake_unlock(&g_st_ts.wakelock_initialization);
		wake_lock_timeout(&g_st_ts.wakelock_initialization,
			TCH_WAKE_LOCK_TIMEOUT);
		rm_ctrl_resume(ts);
	}
	return RETURN_OK;
}

#ifdef CONFIG_PM
static int rm_dev_pm_suspend(struct device *dev)
{
	struct rm_tch_ts *ts = dev_get_drvdata(dev);
	if (!g_st_ts.b_is_suspended && !g_st_ts.b_is_disabled) {
		rm_tch_suspend(ts);
#if defined(CONFIG_ANDROID)
		dev_info(ts->dev, "disabled without input powerhal support.\n");
#endif
	}
	return RETURN_OK;
}

static int rm_dev_pm_resume(struct device *dev)
{
	struct rm_tch_ts *ts = dev_get_drvdata(dev);
	if (!g_st_ts.b_is_disabled) {
		rm_tch_resume(ts);
#if defined(CONFIG_ANDROID)
		dev_info(ts->dev, "enabled without input powerhal support.\n");
#endif
	}
	return RETURN_OK;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void rm_tch_early_suspend(struct early_suspend *es)
{
	struct rm_tch_ts *ts;
	struct device *dev;

	ts = container_of(es, struct rm_tch_ts, early_suspend);
	dev = ts->dev;

	if (rm_tch_suspend(dev))
		dev_err(dev, "Raydium - %s : failed\n", __func__);
}

static void rm_tch_early_resume(struct early_suspend *es)
{
	struct rm_tch_ts *ts;
	struct device *dev;

	ts = container_of(es, struct rm_tch_ts, early_suspend);
	dev = ts->dev;

	if (rm_tch_resume(dev))
		dev_err(dev, "Raydium - %s : failed\n", __func__);
}
#endif			/*CONFIG_HAS_EARLYSUSPEND*/
#endif			/*CONFIG_PM*/

/* NVIDIA 20121026 */
/* support to disable power and clock when display is off */
static int rm_tch_input_enable(struct input_dev *in_dev)
{
	int error = RETURN_OK;

	struct rm_tch_ts *ts = input_get_drvdata(in_dev);

	g_st_ts.b_is_disabled = false;
	error = rm_tch_resume(ts);
	if (error)
		dev_err(ts->dev, "Raydium - %s : failed\n", __func__);

	return error;
}

static int rm_tch_input_disable(struct input_dev *in_dev)
{
	int error = RETURN_OK;

	struct rm_tch_ts *ts = input_get_drvdata(in_dev);

	error = rm_tch_suspend(ts);
	g_st_ts.b_is_disabled = true;
	if (error)
		dev_err(ts->dev, "Raydium - %s : failed\n", __func__);

	return error;
}

#if defined(CONFIG_TRUSTED_LITTLE_KERNEL)
/*===========================================================================*/
void raydium_tlk_ns_touch_suspend(void)
{
	struct rm_tch_ts *ts = input_get_drvdata(g_input_dev);

	rm_printk("tlk_ns_touch_suspend\n");

	rm_tch_enter_manual_mode();
	mutex_lock(&g_st_ts.mutex_scan_mode);
	mutex_lock(&g_st_ts.mutex_ns_mode);
	rm_tch_cmd_process(0, g_st_rm_tlk_cmd, ts);
}
EXPORT_SYMBOL(raydium_tlk_ns_touch_suspend);

/*===========================================================================*/
void raydium_tlk_ns_touch_resume(void)
{
	struct rm_tch_ts *ts = input_get_drvdata(g_input_dev);

	rm_printk("tlk_ns_touch_resume\n");

	rm_tch_ts_send_signal(g_st_ts.u32_hal_pid, 0x03);
	rm_tch_cmd_process(1, g_st_rm_tlk_cmd, ts);

	mutex_unlock(&g_st_ts.mutex_scan_mode);
	mutex_unlock(&g_st_ts.mutex_ns_mode);
}
EXPORT_SYMBOL(raydium_tlk_ns_touch_resume);
/*===========================================================================*/
#endif  /*CONFIG_TRUSTED_LITTLE_KERNEL*/

static void rm_tch_set_input_resolution(unsigned int x, unsigned int y)
{
	input_set_abs_params(g_input_dev, ABS_MT_POSITION_X, 0, x - 1, 0, 0);
	input_set_abs_params(g_input_dev, ABS_MT_POSITION_Y, 0, y - 1, 0, 0);
}

static struct rm_spi_ts_platform_data *rm_ts_parse_dt(struct device *dev,
					int irq)
{
	struct rm_spi_ts_platform_data *pdata;
	struct device_node *np = dev->of_node;
	const char *str;
	int ret, val, irq_gpio;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->gpio_reset = of_get_named_gpio_flags(np, "reset-gpio", 0, NULL);
	if (!gpio_is_valid(pdata->gpio_reset)) {
		dev_err(dev, "Invalid reset-gpio\n");
		return ERR_PTR(-EINVAL);
	}
	ret = gpio_request(pdata->gpio_reset, "reset-gpio");
	if (ret < 0) {
		dev_err(dev, "gpio_request fail\n");
		return ERR_PTR(-EINVAL);
	}
	gpio_direction_output(pdata->gpio_reset, 0);

	ret = of_property_read_u32(np, "interrupts", &irq_gpio);
	if (!gpio_is_valid(irq_gpio)) {
		dev_err(dev, "Invalid irq-gpio\n");
		ret = -EINVAL;
		goto exit_release_reset_gpio;
	}

	ret = gpio_request(irq_gpio, "irq-gpio");
	if (ret < 0) {
		dev_err(dev, "irq_request fail\n");
		ret = -EINVAL;
		goto exit_release_reset_gpio;
	}
	gpio_direction_input(irq_gpio);

	ret = of_property_read_u32(np, "config", &val);
	if (ret < 0)
		goto exit_release_all_gpio;
	pdata->config = (unsigned char *)(uintptr_t)val;

	ret = of_property_read_u32(np, "platform-id", &val);
	if (ret < 0)
		goto exit_release_all_gpio;
	pdata->platform_id = val;

	ret = of_property_read_string(np, "name-of-clock", &str);
	if (ret < 0)
		goto exit_release_all_gpio;
	pdata->name_of_clock = (char *)str;

	ret = of_property_read_string(np, "name-of-clock-con", &str);
	if (ret < 0)
		goto exit_release_all_gpio;
	pdata->name_of_clock_con = (char *)str;

	pdata->gpio_sensor_select0 = of_property_read_bool(np, "gpio-sensor-select0");
	pdata->gpio_sensor_select1 = of_property_read_bool(np, "gpio-sensor-select1");

	return pdata;

exit_release_all_gpio:
	gpio_free(irq_gpio);

exit_release_reset_gpio:
	gpio_free(pdata->gpio_reset);
	return ERR_PTR(ret);
}

struct rm_tch_ts *rm_tch_input_init(struct device *dev, unsigned int irq,
	const struct rm_tch_bus_ops *bops)
{
	struct rm_tch_ts *ts;
	struct input_dev *input_dev;
	struct rm_spi_ts_platform_data *pdata;
	int err = -EINVAL;

	if (!irq) {
		dev_err(dev, "Raydium - no IRQ?\n");
		err = -EINVAL;
		goto err_out;
	}

	ts = kzalloc(sizeof(struct rm_tch_ts), GFP_KERNEL);

	input_dev = input_allocate_device();

	if (!ts || !input_dev) {
		dev_err(dev, "Raydium - Failed to allocate memory\n");
		err = -ENOMEM;
		goto err_free_mem;
	}

	g_input_dev = input_dev;

	ts->bops = bops;
	ts->dev = dev;
	ts->input = input_dev;
	ts->irq = irq;


	if (dev->of_node) {
		pr_info("Load platform data from DT.\n");
		pdata = rm_ts_parse_dt(dev, irq);
		if (IS_ERR(pdata)) {
			dev_err(&g_spi->dev, "Raydium - failed to parse dt\n");
			err = -EINVAL;
			goto err_free_mem;
		}
		dev->platform_data = pdata;
	}

	pdata = dev->platform_data;

	if (pdata->name_of_clock || pdata->name_of_clock_con) {
		ts->clk = clk_get_sys(pdata->name_of_clock,
			pdata->name_of_clock_con);
		if (IS_ERR(ts->clk)) {
			dev_err(&g_spi->dev, "Raydium - failed to get touch_clk: (%s, %s)\n",
				pdata->name_of_clock, pdata->name_of_clock_con);
			err = -EINVAL;
			goto err_free_mem;
		}
	}

	snprintf(ts->phys, sizeof(ts->phys), "%s/input0", dev_name(dev));

	input_dev->name = "touch";
	input_dev->phys = ts->phys;
	input_dev->dev.parent = dev;
	input_dev->id.bustype = bops->bustype;

	input_dev->enable = rm_tch_input_enable;
	input_dev->disable = rm_tch_input_disable;
	input_dev->enabled = true;
	input_dev->open = rm_tch_input_open;
	input_dev->close = rm_tch_input_close;
	input_dev->hint_events_per_packet = 256U;

	input_set_drvdata(input_dev, ts);
	input_set_capability(input_dev, EV_MSC, MSC_ACTIVITY);

#if (INPUT_PROTOCOL_CURRENT_SUPPORT == INPUT_PROTOCOL_TYPE_A)
	__set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 32, 0, 0);

	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOOL_RUBBER, input_dev->keybit);
#else
	__set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE,
		0, 0xFF, 0, 0);

	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOOL_RUBBER, input_dev->keybit);
#ifdef INPUT_MT_DIRECT
	input_mt_init_slots(input_dev,
		MAX_SLOT_AMOUNT,
		0);
#else
	input_mt_init_slots(input_dev,
		MAX_SLOT_AMOUNT);
#endif
#endif
	input_set_abs_params(input_dev,
		ABS_MT_TOOL_TYPE, 0,
		MT_TOOL_MAX, 0, 0);

	rm_tch_set_input_resolution(RM_INPUT_RESOLUTION_X,
				RM_INPUT_RESOLUTION_Y);

	err = request_threaded_irq(ts->irq, NULL, rm_tch_irq,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, dev_name(dev), ts);
	if (err) {
		dev_err(dev, "Raydium - irq %d busy?\n", ts->irq);
		goto err_free_mem;
	}
	mutex_init(&ts->access_mutex);
#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = rm_tch_early_suspend;
	ts->early_suspend.resume = rm_tch_early_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	rm_tch_disable_irq(ts);

	err = sysfs_create_group(&dev->kobj, &rm_ts_attr_group);

	if (err)
		goto err_free_irq;

	err = input_register_device(input_dev);
	if (err)
		goto err_remove_attr;

	return ts;

err_remove_attr:
	sysfs_remove_group(&dev->kobj, &rm_ts_attr_group);
err_free_irq:
	free_irq(ts->irq, ts);
err_free_mem:
	input_free_device(input_dev);
	kfree(ts);
err_out:
	return ERR_PTR(err);
}

static int dev_open(struct inode *inode, struct file *filp)
{
	return RETURN_OK;
}

static int dev_release(struct inode *inode, struct file *filp)
{
	g_st_ts.b_init_finish = 0;

	rm_tch_enter_manual_mode();
	return RETURN_OK;
}

static ssize_t
dev_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
	ssize_t missing, status;
	int ret;
	u8 *p_u8_my_buf;

	p_u8_my_buf = kmalloc(count, GFP_KERNEL);
	if (p_u8_my_buf == NULL)
		return -ENOMEM;

	p_u8_my_buf[0] = buf[0];
	ret = rm_tch_spi_read(p_u8_my_buf[0], p_u8_my_buf, count);

	if (ret) {
		status = -EFAULT;
		rm_printk("Raydium - rm_tch_spi_read() fail\n");
	} else {
		status = count;
		missing = copy_to_user(buf, p_u8_my_buf, count);

		if (missing) {
			if (missing == status)
				status = -EFAULT;
			else
				status = status - missing;
		}
	}

	kfree(p_u8_my_buf);
	return status;
}

static ssize_t dev_write(struct file *filp,
		const char __user *buf,	size_t count, loff_t *pos)
{
	u8 *p_u8_my_buf;
	int ret;
	unsigned long missing;
	ssize_t status = 0;

	p_u8_my_buf = kmalloc(count, GFP_KERNEL);
	if (p_u8_my_buf == NULL)
		return -ENOMEM;

	missing = copy_from_user(p_u8_my_buf, buf, count);
	if (missing)
		status = -EFAULT;
	else {
		ret = rm_tch_spi_write(p_u8_my_buf, count);
		if (ret)
			status = -EFAULT;
		else
			status = count;
	}

	kfree(p_u8_my_buf);
	return status;
}

/*=============================================================================
	Description:
		I/O Control routin.
	Input:
		file:
		u8_cmd :
		arg :
	Output:
		1: succeed
		0: failed
	Note: To avoid context switch,please don't add debug message
		  in this function.
=============================================================================*/
static long dev_ioctl(struct file *file,
		unsigned int u8_cmd, unsigned long arg)
{
	long ret = RETURN_OK;
	unsigned int index;
	struct rm_tch_ts *ts = input_get_drvdata(g_input_dev);

	index = (u8_cmd >> 16) & 0xFFFF;

	switch (u8_cmd & 0xFFFF) {
	case RM_IOCTL_REPORT_POINT:
#if ENABLE_EVENT_QUEUE
		rm_tch_enqueue_report_pointer(
			(void *)(arg & MASK_USER_SPACE_POINTER));

		if (waitqueue_active(&g_st_ts.rm_event_thread_wait_q)) {
			g_st_ts.b_event_thread_active = true;
			wake_up_interruptible(&g_st_ts.rm_event_thread_wait_q);
		} else {
			mutex_lock(&g_st_ts.mutex_event_waited);
			g_st_ts.b_event_is_waited = true;
			mutex_unlock(&g_st_ts.mutex_event_waited);
		}
#else
		raydium_report_pointer((void *)(arg & MASK_USER_SPACE_POINTER));
#endif
		break;
	case RM_IOCTL_FINISH_CALC:
		g_st_ts.b_calc_finish = 1;
		break;
	case RM_IOCTL_READ_RAW_DATA:
		ret = rm_tch_queue_read_raw_data(
			(u8 *)(arg & MASK_USER_SPACE_POINTER),
			index);
		break;
	case RM_IOCTL_GET_SACN_MODE:
		ret = rm_tch_ctrl_get_idle_mode(
			(u8 *)(arg & MASK_USER_SPACE_POINTER));
		break;
	case RM_IOCTL_SET_HAL_PID:
		g_st_ts.u32_hal_pid = (u32)arg;
		break;
	case RM_IOCTL_WATCH_DOG:
		g_st_ts.u8_watch_dog_flg = 1;
		g_st_ts.b_watch_dog_check = 0;
		break;
	case RM_IOCTL_GET_VARIABLE:
		ret = rm_tch_get_variable(index,
			((u8 *)(arg & MASK_USER_SPACE_POINTER)));
		break;
	case RM_IOCTL_INIT_START:
		g_st_ts.b_init_finish = 0;
		rm_tch_enter_manual_mode();
		break;
	case RM_IOCTL_INIT_END:
		g_st_ts.b_init_finish = 1;
		g_st_ts.b_calc_finish = 1;
		if (g_st_ts.u8_resume_cnt)	/* In case issued by boot-up*/
			g_st_ts.u8_resume_cnt--;
		if (wake_lock_active(&g_st_ts.wakelock_initialization))
			wake_unlock(&g_st_ts.wakelock_initialization);
#if (ISR_POST_HANDLER == KTHREAD)
		if (!g_st_ts.b_irq_thread_alive) {
			g_st_ts.irq_thread_sched.sched_priority =
				MAX_USER_RT_PRIO / 2;
			g_st_ts.rm_irq_post_thread =
				kthread_run(rm_work_thread_function,
					NULL, "RaydiumIrq_Poster");
			if (IS_ERR(g_st_ts.rm_irq_post_thread)) {
				if (g_st_ctrl.u8_kernel_msg & DEBUG_KTHREAD)
					rm_printk("Raydium - Create IRQ poster failed!\n");
			} else {
				g_st_ts.b_irq_thread_active = false;
				g_st_ts.b_irq_thread_alive = true;
				mutex_lock(&g_st_ts.mutex_irq_wait);
				g_st_ts.b_irq_is_waited = false;
				mutex_unlock(&g_st_ts.mutex_irq_wait);
				if (g_st_ctrl.u8_kernel_msg & DEBUG_KTHREAD)
					rm_printk("Raydium - Create IRQ poster successfully!\n");
			}
		} else {
			if (g_st_ctrl.u8_kernel_msg & DEBUG_KTHREAD)
				rm_printk("Raydium - IRQ poster is alive!\n");
		}
#endif
#if ENABLE_EVENT_QUEUE
		if (!g_st_ts.b_event_thread_alive) {
			g_st_ts.event_thread_sched.sched_priority =
				MAX_USER_RT_PRIO / 2;
			g_st_ts.rm_event_post_thread =
				kthread_run(rm_event_thread_function,
					NULL, "RaydiumEvent_Poster");
			if (IS_ERR(g_st_ts.rm_event_post_thread)) {
				if (g_st_ctrl.u8_kernel_msg & DEBUG_KTHREAD)
					rm_printk("Raydium - Create Event poster failed!\n");
			} else {
				g_st_ts.b_event_thread_active = false;
				g_st_ts.b_event_thread_alive = true;
				mutex_lock(&g_st_ts.mutex_event_waited);
				g_st_ts.b_event_is_waited = false;
				mutex_unlock(&g_st_ts.mutex_event_waited);
				if (g_st_ctrl.u8_kernel_msg & DEBUG_KTHREAD)
					rm_printk("Raydium - Create Event poster successfully!\n");
			}
		} else {
			if (g_st_ctrl.u8_kernel_msg & DEBUG_KTHREAD)
				rm_printk("Raydium - Event poster is alive!\n");
		}
#endif
		rm_printk("Raydium - Enable input device done\n");
		break;
	case RM_IOCTL_SCRIBER_CTRL:
		g_st_ts.b_enable_scriber = (bool) arg;
		break;
	case RM_IOCTL_SET_PARAMETER:
		rm_tch_ctrl_set_parameter(
			(void *)(arg & MASK_USER_SPACE_POINTER));
		rm_tch_set_input_resolution(g_st_ctrl.u16_resolution_x,
			g_st_ctrl.u16_resolution_y);
		break;
	case RM_IOCTL_SET_BASELINE:
		rm_tch_ctrl_set_baseline(
			(u8 *)(arg & MASK_USER_SPACE_POINTER),
			g_st_ctrl.u16_data_length);
		break;
	case RM_IOCTL_SET_VARIABLE:
		rm_tch_set_variable(index, arg);
		break;
	case RM_IOCTL_SET_KRL_TBL:
		ret = rm_set_kernel_tbl(index,
			((u8 *)(arg & MASK_USER_SPACE_POINTER)));
		break;
	case RM_IOCTL_INIT_SERVICE:
		g_st_ts.b_init_service = true;
		break;
	case RM_IOCTL_SET_CLK:
		rm_printk("Raydium - Clock set to %d\n", (u32)arg);
		if (ts && ts->clk) {
			if ((u32)arg)
				ret = clk_enable(ts->clk);
			else
				clk_disable(ts->clk);
		} else
			dev_err(&g_spi->dev, "Raydium - %s : No clk handler!\n",
				__func__);
		break;
	default:
		return -EINVAL;
		break;
	}

	return !ret;
}

static const struct file_operations dev_fops = {
	.owner = THIS_MODULE,
	.open = dev_open,
	.release = dev_release,
	.read = dev_read,
	.write = dev_write,
	.unlocked_ioctl = dev_ioctl,
	.compat_ioctl = dev_ioctl,
};

static struct miscdevice raydium_ts_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "touch",
	.fops = &dev_fops,
};

static const struct rm_tch_bus_ops rm_tch_spi_bus_ops = {
	.bustype = BUS_SPI,
};

static void init_ts_timer(void)
{
	init_timer(&ts_timer_triggle);
	ts_timer_triggle.function = ts_timer_triggle_function;
	ts_timer_triggle.data = ((unsigned long) 0);
	ts_timer_triggle.expires = jiffies + TS_TIMER_PERIOD;
}

static void ts_timer_triggle_function(unsigned long option)
{
	queue_work(g_st_ts.rm_timer_workqueue, &g_st_ts.rm_timer_work);
	ts_timer_triggle.expires = jiffies + TS_TIMER_PERIOD;
	add_timer(&ts_timer_triggle);
}

/*===========================================================================*/
#if ENABLE_SPI_SETTING
static int rm_tch_spi_setting(u32 speed)
{
	int err;
	if ((speed == 0) || (speed > 18))
		return RETURN_FAIL;

	g_spi->max_speed_hz = speed * 1000 * 1000;
	err = spi_setup(g_spi);
	if (err) {
		dev_dbg(&g_spi->dev, "Raydium - Change SPI setting failed\n");
		return err;
	}
	return RETURN_OK;
}
#endif

static void rm_tch_spi_shutdown(struct spi_device *spi)
{
	struct rm_tch_ts *ts = spi_get_drvdata(spi);

	free_irq(ts->irq, ts);

	if (ts->regulator_3v3 &&
			regulator_is_enabled(ts->regulator_3v3))
		regulator_disable(ts->regulator_3v3);
	if (ts->regulator_1v8 &&
		regulator_is_enabled(ts->regulator_1v8))
		regulator_disable(ts->regulator_1v8);
}

static int rm_tch_spi_remove(struct spi_device *spi)
{
	struct rm_tch_ts *ts = spi_get_drvdata(spi);

	del_timer(&ts_timer_triggle);
	if (g_st_ts.rm_timer_workqueue)
		destroy_workqueue(g_st_ts.rm_timer_workqueue);

	rm_tch_queue_free();

#if (ISR_POST_HANDLER == WORK_QUEUE)
	if (g_st_ts.rm_workqueue)
		destroy_workqueue(g_st_ts.rm_workqueue);
#endif

	if (&g_st_ts.wakelock_initialization)
		wake_lock_destroy(&g_st_ts.wakelock_initialization);

	mutex_destroy(&g_st_ts.mutex_scan_mode);
	mutex_destroy(&g_st_ts.mutex_ns_mode);

	sysfs_remove_group(&raydium_ts_miscdev.this_device->kobj,
						&rm_ts_attr_group);
	misc_deregister(&raydium_ts_miscdev);
	sysfs_remove_group(&ts->dev->kobj, &rm_ts_attr_group);
	free_irq(ts->irq, ts);
	input_unregister_device(ts->input);

	if (ts->regulator_3v3 && ts->regulator_1v8) {
		regulator_unregister_notifier(ts->regulator_3v3, &ts->nb_3v3);
		regulator_unregister_notifier(ts->regulator_1v8, &ts->nb_1v8);
		regulator_disable(ts->regulator_3v3);
		regulator_disable(ts->regulator_1v8);
	}
	kfree(ts);
	spi_set_drvdata(spi, NULL);

	return RETURN_OK;
}

static int rm_tch_regulator_init(struct rm_tch_ts *ts)
{
	int error;

	ts->regulator_3v3 = devm_regulator_get(&g_spi->dev, "avdd");
	if (IS_ERR(ts->regulator_3v3)) {
		dev_err(&g_spi->dev, "Raydium - regulator_get failed: %ld\n",
			PTR_ERR(ts->regulator_3v3));
		goto err_null_regulator;
	}

	ts->regulator_1v8 = devm_regulator_get(&g_spi->dev, "dvdd");
	if (IS_ERR(ts->regulator_1v8)) {
		dev_err(&g_spi->dev, "Raydium - regulator_get failed: %ld\n",
			PTR_ERR(ts->regulator_1v8));
		goto err_null_regulator;
	}

	/* Enable 1v8 first*/
	if (regulator_is_enabled(ts->regulator_1v8))
		rm_printk("Raydium - %s : 1.8V regulator is already enabled!\n",
				__func__);

	error = regulator_enable(ts->regulator_1v8);
	if (error) {
		dev_err(&g_spi->dev,
			"Raydium - 1.8V regulator enable failed: %d\n", error);
		goto err_null_regulator;
	}

	usleep_range(5000, 6000);
	/* Enable 3v3 then*/
	if (regulator_is_enabled(ts->regulator_3v3))
		rm_printk("Raydium - %s : 3.3V regulator is already enabled!\n",
				__func__);
	error = regulator_enable(ts->regulator_3v3);
	if (error) {
		dev_err(&g_spi->dev,
			"Raydium - regulator enable failed: %d\n", error);
		goto err_disable_1v8_regulator;
	}

	ts->nb_1v8.notifier_call = &rm31080_voltage_notifier_1v8;
	error = regulator_register_notifier(ts->regulator_1v8, &ts->nb_1v8);
	if (error) {
		dev_err(&g_spi->dev,
			"Raydium - regulator notifier request failed: %d\n",
			error);
		goto err_disable_regulator;
	}

	ts->nb_3v3.notifier_call = &rm31080_voltage_notifier_3v3;
	error = regulator_register_notifier(ts->regulator_3v3, &ts->nb_3v3);
	if (error) {
		dev_err(&g_spi->dev,
			"Raydium - regulator notifier request failed: %d\n",
			error);
		goto err_unregister_notifier;
	}

	return RETURN_OK;

err_unregister_notifier:
	regulator_unregister_notifier(ts->regulator_1v8, &ts->nb_1v8);
err_disable_regulator:
	regulator_disable(ts->regulator_3v3);
err_disable_1v8_regulator:
	regulator_disable(ts->regulator_1v8);
err_null_regulator:
	ts->regulator_3v3 = NULL;
	ts->regulator_1v8 = NULL;
	return RETURN_FAIL;
}

static int rm_tch_spi_probe(struct spi_device *spi)
{
	struct rm_tch_ts *ts;
	struct rm_spi_ts_platform_data *pdata;
	int ret;

	g_spi = spi;

	rm_tch_init_ts_structure();
	rm_tch_init_ts_structure_part();

	if (spi->max_speed_hz > MAX_SPI_FREQ_HZ) {
		dev_err(&spi->dev, "Raydium - SPI CLK %d Hz?\n",
			spi->max_speed_hz);
		ret = -EINVAL;
		goto err_spi_speed;
	}
	ts = rm_tch_input_init(&spi->dev, spi->irq, &rm_tch_spi_bus_ops);
	if (IS_ERR(ts)) {
		dev_err(&spi->dev, "Raydium - Input Device Initialization Fail!\n");
		ret = PTR_ERR(ts);
		goto err_spi_speed;
	}

	spi_set_drvdata(spi, ts);

	if (rm_tch_regulator_init(ts)) {
		dev_err(&spi->dev, "Raydium - regulator Initialization Fail!\n");
		ret = -EINVAL;
		goto err_regulator_init;
	}
	pdata = g_input_dev->dev.parent->platform_data;
	usleep_range(5000, 6000);

	gpio_set_value(pdata->gpio_reset, 0);
	msleep(120);
	gpio_set_value(pdata->gpio_reset, 1);
	msleep(20);

	ret = misc_register(&raydium_ts_miscdev);
	if (ret) {
		dev_err(&spi->dev, "Raydium - cannot register miscdev: %d\n",
			ret);
		goto err_misc_reg;
	}
	ret = sysfs_create_group(&raydium_ts_miscdev.this_device->kobj,
						&rm_ts_attr_group);
	if (ret) {
		dev_err(&spi->dev, "Raydium - cannot create group: %d\n",
			ret);
		goto err_create_sysfs;
	}

	ret = rm_tch_queue_init();
	if (ret) {
		dev_err(&spi->dev, "Raydium - could not init queue: %d\n",
			ret);
		goto err_queue_init;
	}

	init_ts_timer();
	add_timer(&ts_timer_triggle);

	rm_printk("Raydium - Spi Probe Done!!\n");
	return RETURN_OK;

err_queue_init:
	sysfs_remove_group(&raydium_ts_miscdev.this_device->kobj,
						&rm_ts_attr_group);
err_create_sysfs:
	misc_deregister(&raydium_ts_miscdev);
err_misc_reg:
	if (ts->regulator_3v3 && ts->regulator_1v8) {
		regulator_unregister_notifier(ts->regulator_3v3, &ts->nb_3v3);
		regulator_unregister_notifier(ts->regulator_1v8, &ts->nb_1v8);
		regulator_disable(ts->regulator_3v3);
		regulator_disable(ts->regulator_1v8);
	}
err_regulator_init:
	spi_set_drvdata(spi, NULL);
	input_unregister_device(ts->input);
	sysfs_remove_group(&ts->dev->kobj, &rm_ts_attr_group);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif
	mutex_destroy(&ts->access_mutex);
	free_irq(ts->irq, ts);
	input_free_device(g_input_dev);
	kfree(ts);
err_spi_speed:
	if (g_st_ts.rm_timer_workqueue)
		destroy_workqueue(g_st_ts.rm_timer_workqueue);
#if (ISR_POST_HANDLER == WORK_QUEUE)
	if (g_st_ts.rm_workqueue)
		destroy_workqueue(g_st_ts.rm_workqueue);
#endif
	mutex_destroy(&g_st_ts.mutex_scan_mode);
	mutex_destroy(&g_st_ts.mutex_ns_mode);
	return ret;
}

#if defined(CONFIG_PM)
static const struct dev_pm_ops rm_pm_ops = {
	.suspend = rm_dev_pm_suspend,
	.resume = rm_dev_pm_resume,
};
#endif

static const struct of_device_id rm_ts_dt_match[] = {
	{ .compatible = "raydium, rm_ts_spidev" },
	{ },
};
MODULE_DEVICE_TABLE(of, rm_ts_dt_match);

static struct spi_driver rm_tch_spi_driver = {
	.driver = {
		.name = "rm_ts_spidev",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
#if defined(CONFIG_PM)
		.pm     = &rm_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table = rm_ts_dt_match,
#endif
	},
	.probe = rm_tch_spi_probe,
	.remove = rm_tch_spi_remove,
	.shutdown = rm_tch_spi_shutdown,
};

static int __init rm_tch_spi_init(void)
{
	return spi_register_driver(&rm_tch_spi_driver);
}

static void __exit rm_tch_spi_exit(void)
{
	spi_unregister_driver(&rm_tch_spi_driver);
}

module_init(rm_tch_spi_init);
module_exit(rm_tch_spi_exit);

MODULE_AUTHOR("Valentine Hsu <valentine.hsu@rad-ic.com>");
MODULE_DESCRIPTION("Raydium touchscreen SPI bus driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:raydium-t007");
