/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CAMERA_GPIO_H__
#define __CAMERA_GPIO_H__

int cam_gpio_register(struct i2c_client *client,
			unsigned pin_num);

void cam_gpio_deregister(struct i2c_client *client,
			unsigned pin_num);

int cam_gpio_ctrl(struct i2c_client *client,
			unsigned pin_num, int ref_inc, bool active_high);

#endif
/* __CAMERA_GPIO_H__ */
