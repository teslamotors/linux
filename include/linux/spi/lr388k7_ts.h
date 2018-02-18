/*
 * This file is part of LR388K7 touchscreen driver
 *
 * Copyright (C) 2013, Sharp Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef _LINUX_SPI_LR388K7_H
#define _LINUX_SPI_LR388K7_H

#include <linux/types.h>

enum {
	LR388K7_MODEL_DEFAULT = 0,
};

struct lr388k7_platform_data {
	u16 model;
	int gpio_reset;
	int gpio_irq;
	int ts_touch_num_max;
	int ts_x_max;
	int ts_y_max;
	int ts_pressure_max;
	int ts_x_fudge;
	int ts_y_fudge;
	int ts_pressure_fudge;
	int ts_x_plate_ohm;
	int ts_flip_x;
	int ts_flip_y;
	int ts_swap_xy;
	int platform_id;
	unsigned char *name_of_clock;
	unsigned char *name_of_clock_con;
	int gpio_clk_sel;
};

struct st_touch_point_t {
	s16 x;
	s16 y;
	u16 z;
	u8 status;
	u8 width;
	u8 height;
	u8 id;
};

struct lr388k7_slowscan {
	u16 enable;
	u16 scan_rate;
};

struct lr388k7_wakeup {
	u16 enable;
	u16 num_tap;
};

struct lr388k7_ts_dbg {
	struct lr388k7_slowscan slowscan;
	struct lr388k7_wakeup wakeup;
	u8 u8ForceCap;
	u8 u8Dump;
	u8 u8Test;
};

enum {
	LR388K7_IOCTL_TOUCH_REPORT = 0x1001,
	LR388K7_IOCTL_READ_REGISTER,
	LR388K7_IOCTL_SET_HAL_PID,
	LR388K7_IOCTL_READ_RAW_DATA,
	LR388K7_IOCTL_FINISH_CALC,
	LR388K7_IOCTL_SET_RAW_DATA_SIZE,
	LR388K7_IOCTL_SET_BANK_ADDR_IND,
	LR388K7_IOCTL_SET_REVISION,
	LR388K7_IOCTL_GET_DEBUG_STATUS,
	LR388K7_IOCTL_SET_MODE,
	LR388K7_IOCTL_GET_MODE,
	LR388K7_IOCTL_RESET,
	LR388K7_IOCTL_READ_CMD_DATA,
	LR388K7_IOCTL_CLEAR_BUFFER,
	LR388K7_IOCTL_READ_WO_LIMIT,
	LR388K7_IOCTL_ACTIVE_REPORT,
	LR388K7_IOCTL_GET_VALUE,
	LR388K7_IOCTL_SET_CLK_SEL,
	LR388K7_IOCTL_SET_STATE,
	LR388K7_IOCTL_SEND_UDEV,
};

enum {
	LR388K7_SIGNAL_INTR = 0x00000001, /* Raw Data Ready */
	LR388K7_SIGNAL_CMDR = 0x00000002, /* Command Done */
	LR388K7_SIGNAL_CRCE = 0x00000003, /* CRC Error */
	LR388K7_SIGNAL_BOOT = 0x00000004, /* Boot */
	LR388K7_SIGNAL_CTRL = 0x00000005, /* Control */
	LR388K7_SIGNAL_WAKE = 0x00000006, /* Wake up */
	LR388K7_SIGNAL_SLEP = 0x00000007, /* Sleep */
	LR388K7_SIGNAL_MODE = 0x00000008, /* Mode */
};

enum {
	LR388K7_GET_PLATFORM_ID = 1,
};

#define LR388K7_SIGNAL (44)

#endif
