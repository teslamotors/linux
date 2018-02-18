/*
 * HID driver for NVIDIA Shield Joystick
 *
 * Copyright (c) 2013-2014, NVIDIA Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/device.h>
#include <linux/input.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "hid-ids.h"

#define JOYSTICK_FUZZ 64
#define TRIGGER_FUZZ 64
#define JOYSTICK_FLAT 64
#define TRIGGER_FLAT 0

#define JOYSTICK_HOME_CODE 15
#define JOYSTICK_BACK_CODE 12

static int nvidia_input_mapped(struct hid_device *hdev, struct hid_input *hi,
			      struct hid_field *field, struct hid_usage *usage,
			      unsigned long **bit, int *max)
{
	int a = field->logical_minimum;
	int b = field->logical_maximum;
	int fuzz;
	int flat;

	if ((usage->type == EV_ABS) && (field->application == HID_GD_GAMEPAD
			|| field->application == HID_GD_JOYSTICK)) {
		switch (usage->hid) {
		case HID_GD_X:
		case HID_GD_Y:
		case HID_GD_RX:
		case HID_GD_RY:
			fuzz = JOYSTICK_FUZZ;
			flat = JOYSTICK_FLAT;
			break;
		case HID_GD_Z:
		case HID_GD_RZ:
			fuzz = TRIGGER_FUZZ;
			flat = TRIGGER_FLAT;
			break;
		default: return 0;/*Use generic mapping for HatX, HatY*/
		}
		set_bit(usage->type, hi->input->evbit);
		set_bit(usage->code, *bit);
		input_set_abs_params(hi->input, usage->code, a, b, fuzz, flat);
		input_abs_set_res(hi->input, usage->code,
			hidinput_calc_abs_res(field, usage->code));
		return -1;
	}
	return 0;
}

static int nvidia_input_mapping(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max) {
	int code;
	if ((usage->hid & HID_USAGE_PAGE) == HID_UP_BUTTON) {
		code = ((usage->hid - 1) & HID_USAGE);
		if (code == JOYSTICK_HOME_CODE) {
			code = KEY_HOME;
			hid_map_usage(hi, usage, bit, max, EV_KEY, code);
			return 1;
		} else if (code == JOYSTICK_BACK_CODE) {
			code = KEY_BACK;
			hid_map_usage(hi, usage, bit, max, EV_KEY, code);
			return 1;
		}
	}
	return 0;
}


static const struct hid_device_id nvidia_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_NVIDIA, USB_DEVICE_ID_NVIDIA_LOKI) },
	{ }
};
MODULE_DEVICE_TABLE(hid, nvidia_devices);

static struct hid_driver nvidia_driver = {
	.name = "hid-nvidia",
	.id_table = nvidia_devices,
	.input_mapped = nvidia_input_mapped,
	.input_mapping = nvidia_input_mapping,
};
module_hid_driver(nvidia_driver);

MODULE_AUTHOR("Jun Yan <juyan@nvidia.com>");
MODULE_LICENSE("GPL");
