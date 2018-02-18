/*
 * esxxx.h - header for esxxx I2C interface
 *
 * Copyright (C) 2011-2012 Audience, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef __ESXXX_H__
#define __ESXXX_H__

#include <linux/types.h>

/* External clock rate to earSmart chips */

#define ES_EXT_CLK_6000KHZ  0x00 /* 6.0 MHz    */
#define ES_EXT_CLK_6144KHZ  0x01 /* 6.144 MHz  */
#define ES_EXT_CLK_9600KHZ  0x02 /* 9.6 MHz    */
#define ES_EXT_CLK_12000KHZ 0x03 /* 12.0 MHz   */
#define ES_EXT_CLK_12288KHZ 0x04 /* 12.288 MHz */
#define ES_EXT_CLK_13000KHZ 0x05 /* 13.0 MHz   */
#define ES_EXT_CLK_15360KHZ 0x06 /* 15.36 MHz  */
#define ES_EXT_CLK_18432KHZ 0x07 /* 18.432 MHz */
#define ES_EXT_CLK_19200KHZ 0x08 /* 19.2 MHz   */
#define ES_EXT_CLK_24000KHZ 0x09 /* 24 MHz     */
#define ES_EXT_CLK_24576KHZ 0x0a /* 24.576 MHz */
#define ES_EXT_CLK_26000KHZ 0x0b /* 26 MHz     */

/*
 * IRQ type
 */

enum {
	ES_DISABLED,
	ES_ACTIVE_LOW,
	ES_ACTIVE_HIGH,
	ES_FALLING_EDGE,
	ES_RISING_EDGE,
};

/*
 * Firmware API completion mode: Polling or Interrupt
 */
enum {
	ES_CMD_COMP_POLL,
	ES_CMD_COMP_INTR,
};

/* es755 specific */
/* Button Controls */
union es755_btn_ctl1 {
	u8 value;
	struct {
		u8 btn_press_settling_time:3;
		u8 btn_press_polling_rate:2;
		u8 res1:2;
		u8 btn_press_det:1;
	} fields;
};

union es755_btn_ctl2 {
	u8 value;
	struct {
		u8 double_btn_timer:4;
		u8 res1:2;
		u8 mic_det_settling_timer:2;
	} fields;
};

union es755_btn_ctl3 {
	u8 value;
	struct {
		u8 long_btn_timer:4;
		u8 res1:3;
		u8 adc_btn_mute:1;
	} fields;
};

union es755_btn_ctl4 {
	u8 value;
	struct {
		u8 valid_levels:6;
		u8 impd_det_timer:2;
	} fields;
};

struct es755_btn_cfg {
	u8 btn_press_settling_time;
	u8 btn_press_polling_rate;
	u8 btn_press_det_act;

	u8 double_btn_timer;
	u8 mic_det_settling_timer;

	u8 long_btn_timer;
	u8 adc_btn_mute;

	u8 valid_levels;
	u8 impd_det_timer;
};

struct esxxx_accdet_config {
	int	btn_serial_cfg;
	int	btn_parallel_cfg;
	int	btn_detection_rate;
	int	btn_press_settling_time;
	int	btn_bounce_time;
	int	btn_long_press_time;
	int	debounce_timer;
	int	plug_det_enabled;
	int	mic_det_enabled;
};

struct esxxx_platform_data {
	unsigned int	irq_base, irq_end;
	unsigned int	gpio_a_irq_type;
	unsigned int	gpio_b_irq_type;
	int	reset_gpio;
	int	wakeup_gpio;
	int	gpioa_gpio;
	int	gpiob_gpio;
	int	accdet_gpio;
	int	int_gpio;
	int	uart_gpio;
	int	extclk_gpio;
	bool		enable_hs_uart_intf;
	u8		ext_clk_rate;
	u8		cmd_comp_mode;
	struct esxxx_accdet_config accdet_cfg;
	void		*priv;
	int (*esxxx_clk_cb) (int);
};

#endif /* __ESXXX_H__ */
