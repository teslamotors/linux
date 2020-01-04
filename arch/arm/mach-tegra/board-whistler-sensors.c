/*
 * arch/arm/mach-tegra/board-whistler-sensors.c
 *
 * Copyright (c) 2010-2011, NVIDIA CORPORATION, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of NVIDIA CORPORATION nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/i2c.h>
#include <mach/gpio.h>
#include <media/ov5650.h>

#include "gpio-names.h"

#define CAMERA_RESET2_SHUTTER_GPIO	TEGRA_GPIO_PBB1
#define CAMERA_PWNDN1_GPIO			TEGRA_GPIO_PBB4
#define CAMERA_PWNDN2_STROBE_GPIO	TEGRA_GPIO_PBB5
#define CAMERA_RESET1_GPIO			TEGRA_GPIO_PD2
#define CAMERA_FLASH_GPIO			TEGRA_GPIO_PA0

static int whistler_camera_init(void)
{
	tegra_gpio_enable(CAMERA_PWNDN1_GPIO);
	gpio_request(CAMERA_PWNDN1_GPIO, "camera_powerdown");
	gpio_direction_output(CAMERA_PWNDN1_GPIO, 0);
	gpio_export(CAMERA_PWNDN1_GPIO, false);

	tegra_gpio_enable(CAMERA_RESET1_GPIO);
	gpio_request(CAMERA_RESET1_GPIO, "camera_reset1");
	gpio_direction_output(CAMERA_RESET1_GPIO, 1);
	gpio_export(CAMERA_RESET1_GPIO, false);

	tegra_gpio_enable(CAMERA_RESET2_SHUTTER_GPIO);
	gpio_request(CAMERA_RESET2_SHUTTER_GPIO, "camera_reset2_shutter");
	gpio_direction_output(CAMERA_RESET2_SHUTTER_GPIO, 1);
	gpio_export(CAMERA_RESET2_SHUTTER_GPIO, false);

	tegra_gpio_enable(CAMERA_PWNDN2_STROBE_GPIO);
	gpio_request(CAMERA_PWNDN2_STROBE_GPIO, "camera_pwrdwn2_strobe");
	gpio_direction_output(CAMERA_PWNDN2_STROBE_GPIO, 0);
	gpio_export(CAMERA_PWNDN2_STROBE_GPIO, false);

	tegra_gpio_enable(CAMERA_FLASH_GPIO);
	gpio_request(CAMERA_FLASH_GPIO, "camera_flash");
	gpio_direction_output(CAMERA_FLASH_GPIO, 0);
	gpio_export(CAMERA_FLASH_GPIO, false);

	return 0;
}

static int whistler_ov5650_power_on(void)
{
	return 0;
}

static int whistler_ov5650_power_off(void)
{
	return 0;
}

struct ov5650_platform_data whistler_ov5650_data = {
	.power_on = whistler_ov5650_power_on,
	.power_off = whistler_ov5650_power_off,
};

static struct i2c_board_info whistler_i2c3_board_info[] = {
	{
		I2C_BOARD_INFO("ov5650", 0x36),
		.platform_data = &whistler_ov5650_data,
	},
};

int __init whistler_sensors_init(void)
{
	whistler_camera_init();

	i2c_register_board_info(3, whistler_i2c3_board_info,
		ARRAY_SIZE(whistler_i2c3_board_info));

	return 0;
}
