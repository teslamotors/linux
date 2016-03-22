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

#ifndef __AS3638_H__
#define __AS3638_H__

#define AS3638_NAME			"as3638"
#define AS3638_I2C_ADDR			0x30

#define AS3638_FLASH_MAX_BRIGHTNESS_LED1	500  /* mA, IR LED */
#define AS3638_TORCH_MAX_BRIGHTNESS_LED1	99   /* mA, IR LED */
#define AS3638_FLASH_MAX_BRIGHTNESS_LED2	1200 /* mA, Ultra White LED */
#define AS3638_TORCH_MAX_BRIGHTNESS_LED2	88   /* mA, Ultra White LED */
#define AS3638_FLASH_MAX_BRIGHTNESS_LED3	1200 /* mA, Warm White LED */
#define AS3638_TORCH_MAX_BRIGHTNESS_LED3	88   /* mA, Warm White LED */

enum as3638_led_id {
	AS3638_LED1 = 0,
	AS3638_LED2,
	AS3638_LED3,
	AS3638_LED_MAX,
};
#define AS3638_NO_LED -1

struct as3638_platform_data {
	int gpio_torch;
	int gpio_strobe;
	int gpio_reset;
	u32 flash_max_brightness[AS3638_LED_MAX];
	u32 torch_max_brightness[AS3638_LED_MAX];
};

#endif /* __AS3638_H__ */
