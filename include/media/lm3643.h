/*
 * Copyright (c) 2013--2016 Intel Corporation.
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

#ifndef __LM3643_H__
#define __LM3643_H__


#define LM3643_NAME			"lm3643"
#define LM3643_I2C_ADDR_REVA		(0x67)
#define LM3643_I2C_ADDR			(0x63)

#define LM3643_FLASH_TOUT_DEF		0xa /*150ms*/

#define LM3643_FLASH_BRT_MIN 		10900
#define LM3643_FLASH_BRT_STEP 		11725
#define LM3643_FLASH_BRT_MAX 		1500000
#define LM3643_FLASH_BRT_mA_TO_REG(a)   \
	(((a) * 1000) < LM3643_FLASH_BRT_MIN ? LM3643_FLASH_BRT_MIN  :  \
	((((a) * 1000) - LM3643_FLASH_BRT_MIN) / LM3643_FLASH_BRT_STEP))

#define LM3643_FLASH_BRT_REG_TO_mA(a)           \
	(((a) * LM3643_FLASH_BRT_STEP + LM3643_FLASH_BRT_MIN) / 1000)

#define LM3643_FLASH_DEFAULT_BRIGHTNESS 729000

#define LM3643_TORCH_BRT_MIN		 977
#define LM3643_TORCH_BRT_STEP		 1400
#define LM3643_TORCH_BRT_MAX 		179000
#define LM3643_TORCH_BRT_mA_TO_REG(a)   \
	(((a) * 1000) < LM3643_TORCH_BRT_MIN ? LM3643_TORCH_BRT_MIN : \
	((((a) * 1000) - LM3643_TORCH_BRT_MIN) / LM3643_TORCH_BRT_STEP))

#define LM3643_TORCH_DEFAULT_BRIGHTNESS 89


#define LM3643_FLASH_TOUT_MIN		0
#define LM3643_FLASH_TOUT_STEP		1
#define LM3643_FLASH_TOUT_MAX		15

/* struct lm3643_platform_data
 */
struct lm3643_platform_data {
	int gpio_torch;
	int gpio_strobe;
	int gpio_reset;
	int flash_max_brightness; /*mA*/
	int torch_max_brightness; /*mA*/
};

#endif /* __LM3643_H__ */
