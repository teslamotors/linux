/*
 * virtual.c - Camera GPIO driver
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.

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

#include <linux/list.h>
#include <linux/debugfs.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/gpio.h>

#include "camera_gpio.h"

struct camera_gpio {
	struct list_head list;
	unsigned gpio_num;
	struct mutex mutex;
	atomic_t state_cnt;
	atomic_t use_cnt;
};

static DEFINE_MUTEX(g_mutex);
static LIST_HEAD(cam_gpio_list);

int cam_gpio_register(struct i2c_client *client,
			unsigned pin_num) {
	struct camera_gpio *new_gpio;
	struct camera_gpio *next_gpio;

	mutex_lock(&g_mutex);


	list_for_each_entry(next_gpio, &cam_gpio_list, list) {
		if (next_gpio->gpio_num == pin_num) {
			dev_dbg(&client->dev,
				 "%s: gpio pin %u already registered.\n",
				 __func__, pin_num);

			atomic_inc(&next_gpio->use_cnt);

			mutex_unlock(&g_mutex);
			return 0;
		}
	}

	/* gpio is not present in the cam_gpio_list, add it */
	new_gpio = kzalloc(sizeof(*new_gpio), GFP_KERNEL);
	if (!new_gpio) {
		dev_err(&client->dev, "%s: memory low!\n", __func__);
		mutex_unlock(&g_mutex);
		return -EFAULT;
	}

	dev_dbg(&client->dev, "%s: adding cam gpio %u\n",
		 __func__, pin_num);

	new_gpio->gpio_num = pin_num;
	mutex_init(&new_gpio->mutex);
	atomic_inc(&new_gpio->use_cnt);

	list_add(&new_gpio->list, &cam_gpio_list);

	mutex_unlock(&g_mutex);
	return 0;
}
EXPORT_SYMBOL(cam_gpio_register);

void cam_gpio_deregister(struct i2c_client *client,
			unsigned pin_num) {
	struct camera_gpio *next_gpio;

	mutex_lock(&g_mutex);


	list_for_each_entry(next_gpio, &cam_gpio_list, list) {
		if (next_gpio->gpio_num == pin_num) {
			atomic_dec(&next_gpio->use_cnt);

			if (atomic_read(&next_gpio->use_cnt) == 0) {
				list_del(&next_gpio->list);
				kfree(next_gpio);

				dev_dbg(&client->dev,
					 "%s: removing cam gpio %u\n",
					 __func__, pin_num);
			}

			break;
		}
	}

	mutex_unlock(&g_mutex);
	return;
}
EXPORT_SYMBOL(cam_gpio_deregister);

int cam_gpio_ctrl(struct i2c_client *client,
			unsigned pin_num, int val,
			bool active_high) /* val: 0=deassert, 1=assert */
{
	struct camera_gpio *next_gpio;
	int err = -EINVAL;
	int pin_val;
	bool found = false;

	list_for_each_entry(next_gpio, &cam_gpio_list, list) {
		mutex_lock(&next_gpio->mutex);
		if (next_gpio->gpio_num == pin_num) {
			found = true;

			if (!atomic_read(&next_gpio->state_cnt) &&
					!val) {
				dev_err(&client->dev,
					 "%s: state cnt can't be < 0\n",
					 __func__);
				mutex_unlock(&next_gpio->mutex);
				return err;
			}

			if (val)
				atomic_inc(&next_gpio->state_cnt);
			else
				atomic_dec(&next_gpio->state_cnt);

			pin_val = active_high ? val : !val;
			pin_val &= 1;
			err = pin_val;

			/* subtract val allows a 0 check to be
			 * used to indicate that gpio can be written to*/
			if (atomic_read(&next_gpio->state_cnt) - val == 0) {
				gpio_set_value_cansleep(pin_num, pin_val);
				dev_dbg(&client->dev, "%s %u %d\n",
					 __func__, pin_num, pin_val);
			}
		}
		mutex_unlock(&next_gpio->mutex);
	}

	if (!found)
		dev_dbg(&client->dev,
			 "WARNING %s: gpio %u not in list\n",
			 __func__, pin_num);

	return err; /* return value written or error */
}
EXPORT_SYMBOL(cam_gpio_ctrl);
