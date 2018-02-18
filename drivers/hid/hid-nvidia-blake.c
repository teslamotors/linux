/*
 * HID driver for NVIDIA Shield Wireless Joystick
 *
 * Copyright (c) 2013-2016, NVIDIA Corporation. All Rights Reserved.
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

#define MAX_CHAR 255

#define TOUCHPAD_DEFAULT_X 128
#define TOUCHPAD_DEFAULT_Y 128
#define TOUCH_HID_REPORT_SIZE 4
#define TOUCH_ACTION_MASK 0x8
#define TOUCH_ACTION_SHFT 3
#define TOUCH_ACTION_UP 0
#define TOUCH_ACTION_DOWN 1
#define TOUCH_REPORT_ID 2

#define MAX_SPEED 10
#define DEFAULT_SPEED 7

#define SCALE_LEN 6

#define MOUSE_MODE_STR "mouse"
#define GESTURE_MODE_STR "gesture"
#define ABSOLUTE_MODE_STR "absolute"
#define DISABLED_MODE_STR "disabled"
#define UNKNOWN_MODE_STR "unknown"

#define MAX_REL 255
#define MAX_ABS 65535

#define MAX_DPAD_MOVE 4

#define XSCALE 1
#define YSCALE 0

struct nvidia_tp_loc {
	u8 x;
	u8 y;
	u8 action;
	u8 speed;		/* Not used for now but keep as an parameter */
	u8 mode;		/* Trackpad mode */
	u8 release;
	u8 tp_size;
	struct hid_report *ff_report;
};

enum {
	MOUSE_MODE = 0,
	GESTURE_MODE,
	ABSOLUTE_MODE,
	DISABLED_MODE,
};

/*
 * Scale mapping is a table based on quadratic function
 */
static const s8 blake_touch_scale_table_x[SCALE_LEN] = {
	0, 1, 4, 8, 13, 15};

static const s8 blake_touch_scale_table_y[SCALE_LEN] = {
	0, 3, 10, 17, 22, 25};

static signed short scale_rel(u8 rel, u8 coeff, u8 xy)
{
	s8 s_rel = (s8)rel;
	s8 sign = (s_rel >= 0) ? 1 : -1;
	s8 abs_rel = s_rel * sign;

	if (abs_rel >= SCALE_LEN)
		abs_rel = SCALE_LEN - 1;

	/* If Direction is X or Y */
	if (xy) {
		return (signed short)((s_rel +
			sign*blake_touch_scale_table_x[abs_rel]));
	} else {
		return (signed short)((s_rel +
			sign*blake_touch_scale_table_y[abs_rel]));
	}
}

static __s32 scale_rel_to_abs(__s32 rel)
{
	__s32 val;

	if (rel < 0)
		val = MAX_REL + rel;
	else
		val = rel;

	val = val * MAX_ABS / MAX_REL;
	return val;
}

static int nvidia_raw_event(struct hid_device *hdev,
		struct hid_report *report, u8 *data, int size) {

	unsigned id;
	struct nvidia_tp_loc *loc =
		(struct nvidia_tp_loc *)hid_get_drvdata(hdev);
	u8 action;
	u16 x, y;
	int press = 0;
	int release = 0;
	signed short relx, rely;
	signed short relx_raw, rely_raw;

	if (!report)
		return -EINVAL;
	id = report->id;

	if (!loc)
		return -EINVAL;

	/* If not valid touch events, let generic driver to handle this */
	if (id != TOUCH_REPORT_ID)
		return 0;

	/* If driver is disabled, don't report anything to generic driver */
	if (loc->mode == DISABLED_MODE)
		return 1;

	if (!data)
		return -EINVAL;
	action = (data[1] & TOUCH_ACTION_MASK) >> TOUCH_ACTION_SHFT;
	x = data[2];
	y = data[4];



	if (!loc->action && action)
		press = 1;
	else if (loc->action && !action)
		release = 1;
	else if (!loc->action && !action)
		return 1; /* Double release, don't do anything */

	relx_raw = x - loc->x;
	rely_raw = y - loc->y;

	relx = scale_rel(relx_raw, loc->speed, XSCALE);
	rely = scale_rel(rely_raw, (loc->speed), YSCALE);

	loc->action = action;

	dbg_hid("%u %u %u rel %d %d\n", action, x, y, (s8)relx, (s8)rely);

	loc->x = x;
	loc->y = y;

	/* If it's a press event, don't report. */
	if (press)
		return 1;

	/* Not a press event, we need to report it to input subsystem,
	 * set values and pass to generic driver */
	if (loc->mode == ABSOLUTE_MODE) {
		/* Report raw absolute data to generic driver */
		if (loc->tp_size == 8) {
			data[2] = x & 0xff;
			data[3] = y & 0xff;
		}
	} else if (loc->mode == GESTURE_MODE) {
		/* Report raw relative data to generic driver */
		if (loc->tp_size == 8) {
			data[2] = relx_raw & 0xff;
			data[3] = rely_raw & 0xff;
		} else {
			data[2] = relx_raw & 0xff;
			data[3] = relx_raw >> 8;
			data[4] = rely_raw & 0xff;
			data[5] = rely_raw >> 8;
		}
		if (release)
			loc->release = 1;
		else
			loc->release = 0;
	} else {
		/* MOUSE_MODE */
		if (loc->tp_size == 8) {
			data[2] = relx & 0xff;
			data[3] = rely & 0xff;
		} else {
			data[2] = relx & 0xff;
			data[3] = relx >> 8;
			data[4] = rely & 0xff;
			data[5] = rely >> 8;
		}
	}
	return 0;
}

static int nvidia_event(struct hid_device *hdev, struct hid_field *field,
		struct hid_usage *usage, __s32 value) {

	struct nvidia_tp_loc *loc =
		(struct nvidia_tp_loc *)hid_get_drvdata(hdev);
	__u16 keycode = ABS_HAT0X;

	if (!loc)
		return 0;

	/* If not in absolute mode or gesture mode, we pass as it is */
	if (loc->mode != ABSOLUTE_MODE && loc->mode != GESTURE_MODE)
		return 0;

	/* If not mouse event, we pass as it is */
	if (field->physical != HID_GD_MOUSE
			&& field->application != HID_GD_MOUSE)
		return 0;

	/* If not relative event, we pass as it is */
	if (usage->type != EV_REL)
		return 0;

	if (loc->mode == ABSOLUTE_MODE) {
		value = scale_rel_to_abs(value);
		input_event(field->hidinput->input, EV_ABS, usage->code,
				value);
	} else {
		/* GESTURE_MODE */
		value = (value > 1) ? 1 : ((value < -1) ? -1 : 0);
		if (usage->code == REL_X)
			keycode = ABS_HAT0X;
		else if (usage->code == REL_Y)
			keycode = ABS_HAT0Y;

		if (!loc->release)
			input_event(field->hidinput->input, EV_ABS, keycode,
				value);
		else
			input_event(field->hidinput->input, EV_ABS, keycode,
				0);
	}
	return 1;
}

static ssize_t blake_show_mode(struct device *dev,
		struct device_attribute *attr,
		char *buf) {

	struct hid_device *hdev =
		container_of(dev, struct hid_device, dev);

	struct nvidia_tp_loc *loc =
		(struct nvidia_tp_loc *)hid_get_drvdata(hdev);

	if (!loc)
		return snprintf(buf, MAX_CHAR, UNKNOWN_MODE_STR);

	switch (loc->mode) {
	case MOUSE_MODE:
		return snprintf(buf, MAX_CHAR, MOUSE_MODE_STR);
	case GESTURE_MODE:
		return snprintf(buf, MAX_CHAR, GESTURE_MODE_STR);
	case ABSOLUTE_MODE:
		return snprintf(buf, MAX_CHAR, ABSOLUTE_MODE_STR);
	case DISABLED_MODE:
		return snprintf(buf, MAX_CHAR, DISABLED_MODE_STR);
	default:
		return snprintf(buf, MAX_CHAR, UNKNOWN_MODE_STR);
	}
}

static ssize_t blake_store_mode(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count) {

	struct hid_device *hdev = container_of(dev, struct hid_device, dev);
	struct nvidia_tp_loc *loc =
		(struct nvidia_tp_loc *)hid_get_drvdata(hdev);
	size_t buflen;
	char blake_mode[MAX_CHAR];

	if (!loc)
		return count;

	blake_mode[sizeof(blake_mode) - 1] = '\0';
	strncpy(blake_mode, buf, sizeof(blake_mode) - 1);
	buflen = strlen(blake_mode);

	if (buflen && blake_mode[buflen - 1] == '\n')
		blake_mode[buflen - 1] = '\0';

	if (!strcmp(blake_mode, MOUSE_MODE_STR))
		loc->mode = MOUSE_MODE;
	else if (!strcmp(blake_mode, GESTURE_MODE_STR))
		loc->mode = GESTURE_MODE;
	else if (!strcmp(blake_mode, ABSOLUTE_MODE_STR))
		loc->mode = ABSOLUTE_MODE;
	else if (!strcmp(blake_mode, DISABLED_MODE_STR))
		loc->mode = DISABLED_MODE;
	return count;
}

static ssize_t blake_show_speed(struct device *dev,
		struct device_attribute *attr,
		char *buf) {

	struct hid_device *hdev =
		container_of(dev, struct hid_device, dev);

	struct nvidia_tp_loc *loc =
		(struct nvidia_tp_loc *)hid_get_drvdata(hdev);

	return snprintf(buf, MAX_CHAR, "%d\n", loc->speed);
}

static ssize_t blake_store_speed(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count) {

	struct hid_device *hdev = container_of(dev, struct hid_device, dev);
	struct nvidia_tp_loc *loc =
		(struct nvidia_tp_loc *)hid_get_drvdata(hdev);
	unsigned long speed_val;

	if (!loc)
		return count;

	if (!buf)
		return count;

	if (!kstrtoul(buf, 10, &speed_val)) {
		if (speed_val > MAX_SPEED)
			return count;
		loc->speed = (u8)speed_val;
	}

	return count;
}


static int nvidia_ff_play(struct input_dev *dev, void *data,
			 struct ff_effect *effect)
{
	struct hid_device *hid = input_get_drvdata(dev);
	struct nvidia_tp_loc *loc = data;
	int left, right, length;

	left = effect->u.rumble.strong_magnitude;
	right = effect->u.rumble.weak_magnitude;
	length = effect->replay.length;

	dbg_hid("called with 0x%04x 0x%04x 0x%04x", left, right, length);

	length = length > 10000 ? 10000 : length;

	loc->ff_report->field[0]->value[0] = left;
	loc->ff_report->field[0]->value[1] = right;
	loc->ff_report->field[0]->value[2] = length;

	hid_hw_request(hid, loc->ff_report, HID_REQ_SET_REPORT);

	return 0;
}


static int nvidia_find_tp_len(struct hid_device *hdev,
	struct nvidia_tp_loc *loc)
{
	struct hid_report *report;
	struct list_head *report_list =
			&hdev->report_enum[HID_INPUT_REPORT].report_list;
	u8 i;

	if (list_empty(report_list)) {
		hid_err(hdev, "no output reports found\n");
		return -ENODEV;
	}

	/* set default */
	loc->tp_size = 8;
	list_for_each_entry(report, report_list, list) {
		if (report->id == TOUCH_REPORT_ID) {
			/* check if we are reporting u8 or u16
			 * for x/y size need to handle FW change
			 */
			for (i = 0; i < report->maxfield; i++) {
				if (report->field[i]->application
						== HID_GD_MOUSE) {
					if (report->field[i]->report_size
						== 16) {
						loc->tp_size = 16;
						break;
					}
				}
			}
		}

	}
	return 0;
}

static int nvidia_init_ff(struct hid_device *hdev, struct nvidia_tp_loc *loc)
{
	struct hid_report *report;
	struct hid_input *hidinput = list_entry(hdev->inputs.next,
						struct hid_input, list);
	struct list_head *report_list =
			&hdev->report_enum[HID_OUTPUT_REPORT].report_list;
	struct input_dev *dev = hidinput->input;
	int error;

	if (list_empty(report_list)) {
		hid_err(hdev, "no output reports found\n");
		return -ENODEV;
	}

	list_for_each_entry(report, report_list, list) {
		if (report->maxfield < 1) {
			hid_warn(hdev, "no fields in the report\n");
			continue;
		}

		if (report->field[0]->application != (HID_UP_CUSTOM | 0x02)) {
			hid_warn(hdev, "application usage doesn't match expected\n");
			continue;
		}

		if (report->field[0]->report_count < 3) {
			hid_warn(hdev, "not enough values in the field\n");
			continue;
		}

		goto found_report;
	}
	hid_warn(hdev, "FF report not found\n");
	loc->ff_report = NULL;
	return -ENODEV;

found_report:
	loc->ff_report = report;

	set_bit(FF_RUMBLE, dev->ffbit);
	error = input_ff_create_memless(dev, loc, nvidia_ff_play);

	if (error) {
		loc->ff_report = NULL;
		clear_bit(FF_RUMBLE, dev->ffbit);
		hid_warn(hdev, "Couldn't create FF device");
		return error;
	}

	hid_info(hdev, "Force feedback enabled\n");

	return 0;
}

static DEVICE_ATTR(speed, S_IRUGO | S_IWUSR,
	blake_show_speed, blake_store_speed);
static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR,
	blake_show_mode, blake_store_mode);

static int nvidia_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;
	struct nvidia_tp_loc *loc;

	loc = devm_kzalloc(&hdev->dev, sizeof(*loc), GFP_KERNEL);
	if (!loc)
		return -ENOMEM;

	loc->x = TOUCHPAD_DEFAULT_X;
	loc->y = TOUCHPAD_DEFAULT_Y;
	loc->action = 0;
	loc->speed = DEFAULT_SPEED;
	loc->mode = DISABLED_MODE;
	hid_set_drvdata(hdev, loc);

	/* Parse the HID report now */
	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err_parse;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT & ~HID_CONNECT_FF);
	if (ret)
		goto err_parse;

	nvidia_init_ff(hdev, loc);
	nvidia_find_tp_len(hdev, loc);

	ret = device_create_file(&hdev->dev, &dev_attr_speed);
	if (ret) {
		hid_warn(hdev,
			"cannot create sysfs speed attribute err: %d\n", ret);
	}

	ret = device_create_file(&hdev->dev, &dev_attr_mode);
	if (ret)
		hid_warn(hdev,
			"cannot create sysfs mode attribute err: %d\n", ret);

	kobject_uevent(&hdev->dev.kobj, KOBJ_CHANGE);

	return 0;

err_parse:
	return ret;
}

static void nvidia_remove(struct hid_device *hdev)
{
	struct nvidia_tp_loc *loc = hid_get_drvdata(hdev);

	if (!loc)
		return;

	device_remove_file(&hdev->dev, &dev_attr_speed);
	device_remove_file(&hdev->dev, &dev_attr_mode);

	hid_hw_stop(hdev);
}

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

static const struct hid_device_id nvidia_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_NVIDIA, USB_DEVICE_ID_NVIDIA_BLAKE) },
	{ }
};
MODULE_DEVICE_TABLE(hid, nvidia_devices);

static struct hid_driver nvidia_driver = {
	.name = "hid-nvidia-blake",
	.id_table = nvidia_devices,
	.input_mapped = nvidia_input_mapped,
	.raw_event = nvidia_raw_event,
	.event = nvidia_event,
	.probe = nvidia_probe,
	.remove = nvidia_remove,
};
module_hid_driver(nvidia_driver);

MODULE_AUTHOR("Jun Yan <juyan@nvidia.com>");
MODULE_LICENSE("GPL");
