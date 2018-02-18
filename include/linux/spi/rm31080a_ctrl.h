/*
 * Raydium RM31080 touchscreen header
 *
 * Copyright (C) 2012-2014, Raydium Semiconductor Corporation.
 * Copyright (C) 2012-2014, NVIDIA Corporation.  All Rights Reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#ifndef _RM31080A_CTRL_H_
#define _RM31080A_CTRL_H_

struct rm_tch_ctrl_para {

	unsigned short u16_data_length;
	unsigned short u16_ic_version;
	unsigned short u16_resolution_x;
	unsigned short u16_resolution_y;
	unsigned char u8_active_digital_repeat_times;
	unsigned char u8_analog_repeat_times;
	unsigned char u8_idle_digital_repeat_times;
	unsigned char u8_time2idle;
	/*unsigned char u8_power_mode;*/
	unsigned char u8_kernel_msg;
	unsigned char u8_timer_trigger_scale;
	unsigned char u8_idle_mode_check;
	unsigned char u8_watch_dog_normal_cnt;
	unsigned char u8_ns_func_enable;
	unsigned char u8_event_report_mode;
	unsigned char u8_idle_mode_thd;
};

extern struct rm_tch_ctrl_para g_st_ctrl;

int rm_tch_ctrl_clear_int(void);
int rm_tch_ctrl_scan_start(void);

void rm_tch_ctrl_init(void);
unsigned char rm_tch_ctrl_get_idle_mode(unsigned char *p);
void rm_tch_ctrl_set_parameter(void *arg);
void rm_set_repeat_times(u8 u8Times);
#endif				/*_RM31080A_CTRL_H_*/
