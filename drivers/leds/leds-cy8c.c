/*
 * CY8C4014 LED chip driver
 *
 * Copyright (C) 2014-2015 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/bug.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/leds.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>

/* register definitions */
#define P1961_REG_CMD			0x00
#define P1961_REG_CMD_DAT		0x01
#define P1961_REG_CMD_STATUS		0x02
#define P1961_REG_APP_MINOR_REV		0x03
#define P1961_REG_APP_MAJOR_REV		0x04
#define P1961_REG_LED_STATE		0x05
#define P1961_REG_LED_ON_TIME		0x06
#define P1961_REG_LED_OFF_TIME		0x07
#define P1961_REG_MAX_BRIGHT		0x08
#define P1961_REG_NOM_BRIGHT		0x09
#define P1961_REG_LED_RAMP_UP		0x0A
#define P1961_REG_LED_RAMP_DOWN		0x0B
#define P1961_REG_HAPTIC_EN		0x0C
#define P1961_REG_HAPTIC_DRIVE_TIME	0x0D
#define P1961_REG_HAPTIC_BRAKE_DLY	0x0E
#define P1961_REG_HAPTIC_BRAKE_T	0x0F
#define P1961_REG_SOUND_PULSE_LEN	0x10
#define P1961_REG_MAX			0x11
#define P1961_CMD_ENTER_BL		0x01
#define P1961_CMD_WRITE_EEPROM		0x05

#define P1961_LED_STATE_BLINK		0x01
#define P1961_LED_STATE_BREATH		0x02
#define P1961_LED_STATE_SOLID		0x03
#define P1961_LED_STATE_OFF		0x04

#define P1961_CMD_GOTO_BOOT			0x01
#define P1961_CMD_GOTO_APP			0x3B

/* boot device mode address */
#define P1961_BOOT_DEV_ADDR			0x08

#define MAX_COMMAND_SIZE			512
#define BASE_CMD_SIZE				0x07
#define CMD_START					0x01
#define CMD_ENTER_BOOTLOADER		0x38
#define CMD_EXIT_BOOTLOADER			0x3B
#define CMD_STOP					0x17
#define COMMAND_DATA_SIZE			0x01
#define RESET						0x00
#define COMMAND_SIZE (BASE_CMD_SIZE + COMMAND_DATA_SIZE)

enum modes {
	MODE_BLINK = 0,
	MODE_BREATH,
	MODE_NORMAL,
};

struct mode_control {
	enum modes mode;
	char *mode_name;
	unsigned char reg_mode;
	unsigned char reg_mode_val;
	unsigned char reg_on_reg;
	unsigned char reg_off_reg;
};

static struct mode_control mode_controls[] = {
	{MODE_BLINK, "blink",
		P1961_REG_LED_STATE, P1961_LED_STATE_BLINK,
		P1961_REG_LED_ON_TIME, P1961_REG_LED_OFF_TIME},
	{MODE_BREATH, "breath",
		P1961_REG_LED_STATE, P1961_LED_STATE_BREATH,
		P1961_REG_LED_RAMP_UP, P1961_REG_LED_RAMP_DOWN},
	{MODE_NORMAL, "normal",
		P1961_REG_LED_STATE, P1961_LED_STATE_SOLID,
		P1961_REG_NOM_BRIGHT, P1961_REG_NOM_BRIGHT},
};
static int mode_controls_size =
	sizeof(mode_controls) / sizeof(mode_controls[0]);

#define DEVICE_MODE_INVALID		0
#define DEVICE_MODE_APP			1
#define DEVICE_MODE_BOOT		2

struct cy8c_data {
	/* app device */
	struct i2c_client *client;
	struct regmap *regmap;
	struct led_classdev led;
	int mode_index; /* mode index to mode_controls */
	bool led_on_plugin;
	u8 default_brightness;
	struct mutex lock;
	struct miscdevice miscdev;

	/* boot device */
	struct i2c_client *client_boot;
	struct i2c_adapter *adap;
	struct i2c_board_info brd_boot;
	atomic_t in_use;

	atomic_t device_mode;
};

typedef unsigned short (*cy8c_crc_algo_t)(
	unsigned char *buf, unsigned long size);

static int cy8c_get_mode_index(
	struct cy8c_data *data)
{
	int mode_index = 0;
	mutex_lock(&data->lock);
	mode_index = data->mode_index;
	mutex_unlock(&data->lock);
	return mode_index;
}

static void cy8c_set_mode_index(
	struct cy8c_data *data,
	int mode_index)
{
	mutex_lock(&data->lock);
	data->mode_index = mode_index;
	mutex_unlock(&data->lock);
}

static void set_led_brightness(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	struct cy8c_data *data = container_of(led_cdev, struct cy8c_data, led);
	int ret;

	if (atomic_read(&data->device_mode) != DEVICE_MODE_APP) {
		dev_err(&data->client->dev, "device not in app mode %s\n",
			__func__);
		return;
	}

	ret = regmap_write(data->regmap,
		P1961_REG_NOM_BRIGHT, value & 0xff);
/* not to write data to eeprom */
#if 0
	ret |= regmap_write(data->regmap,
		P1961_REG_CMD_DAT, P1961_REG_NOM_BRIGHT);
	ret |= regmap_write(data->regmap,
		P1961_REG_CMD, P1961_CMD_WRITE_EEPROM);
#endif
	if (unlikely(ret))
		dev_err(&data->client->dev, "cannot write %d\n", value);
}

static enum led_brightness get_led_brightness(struct led_classdev *led_cdev)
{
	unsigned int val;
	int ret;
	struct cy8c_data *data = container_of(led_cdev, struct cy8c_data, led);

	ret = regmap_read(data->regmap,	P1961_REG_NOM_BRIGHT, &val);
	if (ret)
		dev_err(&data->client->dev, "cannot read %d\n", val);
	val = val & 0xff;
	return val;
}

static int of_led_parse_pdata(struct i2c_client *client,
	struct cy8c_data *data)
{
	struct device_node *np = client->dev.of_node;
	u32 value;
	data->led.name = of_get_property(np, "label", NULL) ? : np->name;

	data->led_on_plugin = of_property_read_bool(np, "led-on-plugin");

	if (of_property_read_u32(np, "default-brightness", &value))
		data->default_brightness = 0xff;
	else
		data->default_brightness = value & 0xff;

	return 0;
}

static int cy8c_debug_set(void *data, u64 val)
{
	struct cy8c_data *cy_data = data;

	val = val & 0xff;
	if (val > P1961_CMD_ENTER_BL)
		return -EINVAL;

	pr_info("%s: send reset cmd %lld\n", __func__, val);
	regmap_write(cy_data->regmap, P1961_REG_CMD, val & 0xff);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cy8c_debug_fops, NULL, cy8c_debug_set, "%lld\n");

static ssize_t cy8c_effects_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cy8c_data *data = NULL;
	ssize_t ret = -EINVAL;
	struct regmap *regmap = NULL;
	int i = 0;
	int len = 0;

	data = container_of(led_cdev, struct cy8c_data, led);
	regmap = data->regmap;

	if (atomic_read(&data->device_mode) != DEVICE_MODE_APP) {
		dev_err(&data->client->dev, "device not in app mode %s\n",
			__func__);
		return -EAGAIN;
	}

	if (buf == NULL || buf[0] == 0) {
		dev_err(&data->client->dev, "input buf invalid.\n");
		return -EINVAL;
	}

	for (i = 0; i < mode_controls_size; i++) {
		/* use strncmp instead of strcmp
		   strcmp always returns incorrect value
		*/
		len = min(strlen(buf),
			strlen(mode_controls[i].mode_name));
		if (!strncmp(buf, mode_controls[i].mode_name, len))
			break;
	}

	if (i == mode_controls_size) {
		dev_err(&data->client->dev, "cannot find %s\n", buf);
		return -EINVAL;
	}

	ret = regmap_write(regmap, mode_controls[i].reg_mode,
		mode_controls[i].reg_mode_val);

	if (ret == 0)
		cy8c_set_mode_index(data, i);

	return ret == 0 ? size : -ENODEV;
}

static ssize_t cy8c_effects_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cy8c_data *data = NULL;
	int mode_index = 0;
	data = container_of(led_cdev, struct cy8c_data, led);

	if (atomic_read(&data->device_mode) != DEVICE_MODE_APP) {
		dev_err(&data->client->dev, "device not in app mode %s\n",
			__func__);
		return -EAGAIN;
	}

	mode_index = cy8c_get_mode_index(data);
	if (mode_index >= mode_controls_size) {
		dev_err(&data->client->dev, "mode error\n");
		return -EINVAL;
	} else
		return sprintf(buf, "%s\n",
			mode_controls[mode_index].mode_name);
}

static ssize_t cy8c_params_set(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cy8c_data *data = NULL;
	ssize_t ret = -EINVAL;
	struct regmap *regmap = NULL;
	int on_time, off_time;
	enum modes mode = 0;

	on_time = 0;
	off_time = 0;
	data = container_of(led_cdev, struct cy8c_data, led);
	regmap = data->regmap;

	if (atomic_read(&data->device_mode) != DEVICE_MODE_APP) {
		dev_err(&data->client->dev, "device not in app mode %s\n",
			__func__);
		return -EAGAIN;
	}

	if (buf == NULL || buf[0] == 0) {
		dev_err(&data->client->dev, "input buf invalid\n");
		return -EINVAL;
	}

	if (sscanf(buf, "%d %d", &on_time, &off_time) != 2) {
		dev_err(&data->client->dev, "input data format invalid\n");
		return -EINVAL;
	}

	if (on_time < 0 || on_time > 255) {
		dev_err(&data->client->dev, "input on time out of range\n");
		return -EINVAL;
	}

	if (off_time < 0 || off_time > 255) {
		dev_err(&data->client->dev, "input off time out of range\n");
		return -EINVAL;
	}

	mutex_lock(&data->lock);
	mode = mode_controls[data->mode_index].mode;
	if (mode != MODE_NORMAL) {
		ret = regmap_write(regmap,
			mode_controls[data->mode_index].reg_on_reg,
			on_time);
		ret |= regmap_write(regmap,
			mode_controls[data->mode_index].reg_off_reg,
			off_time);
	} else {
		dev_err(&data->client->dev, "mode in normal\n");
	}
	mutex_unlock(&data->lock);

	return ret == 0 ? size : ret;
}

static ssize_t cy8c_params_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cy8c_data *data = NULL;
	int on_time, off_time;
	enum modes mode = 0;
	ssize_t ret = -EINVAL;
	data = container_of(led_cdev, struct cy8c_data, led);
	on_time = off_time = 0;

	if (atomic_read(&data->device_mode) != DEVICE_MODE_APP) {
		dev_err(&data->client->dev, "device not in app mode %s\n",
			__func__);
		return -EAGAIN;
	}

	mutex_lock(&data->lock);
	if (data->mode_index >= mode_controls_size) {
		mutex_unlock(&data->lock);
		return -EINVAL;
	}

	mode = mode_controls[data->mode_index].mode;

	if (mode != MODE_NORMAL) {
		ret = regmap_read(data->regmap,
			mode_controls[data->mode_index].reg_on_reg,
			&on_time);
		ret |= regmap_read(data->regmap,
			mode_controls[data->mode_index].reg_off_reg,
			&off_time);
	} else {
		dev_err(&data->client->dev, "mode in normal\n");
	}
	mutex_unlock(&data->lock);

	if (ret)
		return ret;
	else
		return sprintf(buf, "%d %d\n", on_time, off_time);
}

static unsigned short cy8c_crc_algo_sum(
	unsigned char *buf, unsigned long size)
{
	unsigned short sum = 0;
	while (size-- > 0)
		sum += *buf++;

	return 1 + ~sum;
}

static unsigned short cy8c_crc_algo_crc(
	unsigned char *buf, unsigned long size)
{
	unsigned short crc = 0xffff;
	unsigned short tmp;
	int i;

	if (size == 0)
		return ~crc;

	do {
		for (i = 0, tmp = 0x00ff & *buf++;
			i < 8;
			i++, tmp >>= 1) {
			if ((crc & 0x0001) ^ (tmp & 0x0001))
				crc = (crc >> 1) ^ 0x8408;
			else
				crc >>= 1;
		}
	} while (--size);

	crc = ~crc;
	tmp = crc;
	crc = (crc << 8) | (tmp >> 8 & 0xFF);

	return crc;
}

static cy8c_crc_algo_t crc_algos[] = {
	&cy8c_crc_algo_sum,
	&cy8c_crc_algo_crc,
};

static int cy8c_send_app_mode(struct cy8c_data *data,
		cy8c_crc_algo_t crc_func)
{
	unsigned char cmd_buf[COMMAND_SIZE] = {0, };
	unsigned short checksum;
	unsigned long res_size;
	unsigned long cmd_size;
	int ret;

	if (COMMAND_SIZE < 3)
		return -EINVAL;

	res_size = BASE_CMD_SIZE;
	cmd_size = COMMAND_SIZE;
	cmd_buf[0] = CMD_START;
	cmd_buf[1] = CMD_EXIT_BOOTLOADER;
	cmd_buf[2] = (unsigned char)COMMAND_DATA_SIZE;
	cmd_buf[3] = (unsigned char)(COMMAND_DATA_SIZE >> 8);
	cmd_buf[4] = RESET;
	checksum = (*crc_func)(cmd_buf, COMMAND_SIZE - 3);
	cmd_buf[5] = (unsigned char)checksum;
	cmd_buf[6] = (unsigned char)(checksum >> 8);
	cmd_buf[7] = CMD_STOP;

	ret = i2c_master_send(data->client_boot, cmd_buf, cmd_size);

	return ret;
}

static int cy8c_boot_mode_enter(struct cy8c_data *data,
		cy8c_crc_algo_t crc_func)
{
	const unsigned long RESULT_DATA_SIZE = 8;
	unsigned short checksum;
	unsigned char cmd_buf[BASE_CMD_SIZE];
	unsigned char res_buf[BASE_CMD_SIZE + RESULT_DATA_SIZE];
	int res_size, cmd_size;
	int ret;

	if (COMMAND_SIZE < 3)
		return -EINVAL;

	res_size = BASE_CMD_SIZE + RESULT_DATA_SIZE;
	cmd_size = BASE_CMD_SIZE;
	cmd_buf[0] = CMD_START;
	cmd_buf[1] = CMD_ENTER_BOOTLOADER;
	cmd_buf[2] = 0;
	cmd_buf[3] = 0;
	checksum = (*crc_func)(cmd_buf, BASE_CMD_SIZE - 3);
	cmd_buf[4] = (unsigned char)checksum;
	cmd_buf[5] = (unsigned char)(checksum >> 8);
	cmd_buf[6] = CMD_STOP;

	ret = i2c_master_send(data->client_boot, cmd_buf, cmd_size);
	if (ret < 0)
		return ret;

	ret = i2c_master_recv(data->client_boot, res_buf, res_size);

	return ret;
}

static int cy8c_boot_mode_test(struct cy8c_data *data)
{
	int i, ret;
	for (i = 0; i < sizeof(crc_algos)/sizeof(crc_algos[0]); i++) {
		ret = cy8c_boot_mode_enter(data, crc_algos[i]);
		if (ret > 0)
			ret = 0;
		else
			dev_err(&data->client->dev,
				"device not in boot mode\n");

		if (ret == 0)
			break;
	}

	return ret;
}

static int cy8c_app_mode_test(struct cy8c_data *data)
{
	int ret, reg;
	ret = regmap_read(data->regmap, P1961_REG_APP_MINOR_REV, &reg);
	return ret;
}

/*
	app mode
	called from user space

	The packet sent to device in boot mode needs a crc value
	as parameter. There are 2 algorithms to compute the crc
	according to the content of the fw file in user space code
	implementation at vendor/bin/vendor/nvidia/loki/utils/cyload.

	In the current design the 'sum' algo is used to compute
	crc, but we will support both of the ALGOes just in case.
*/
static int cy8c_boot_to_app(struct cy8c_data *data)
{
	int i, ret;

	for (i = 0; i < sizeof(crc_algos)/sizeof(crc_algos[0]); i++) {
		ret = cy8c_send_app_mode(data, crc_algos[i]);
		if (ret > 0)
			ret = 0;
		else
			dev_err(&data->client->dev,
				"cannot put device to app mode\n");

		if (ret == 0) {

			/* 100 ms is enough in test */
			msleep(100);

			ret = cy8c_app_mode_test(data);
			if (unlikely(ret))
				dev_err(&data->client->dev,
					"device not in app mode %d\n", i);
			else
				atomic_set(&data->device_mode, DEVICE_MODE_APP);
		}

		if (ret == 0)
			break;
	}

	return ret;
}

static ssize_t cy8c_boot_mode_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	int device_mode = -1;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cy8c_data *data = NULL;
	char *name = "unknown";

	data = container_of(led_cdev, struct cy8c_data, led);

	device_mode = atomic_read(&data->device_mode);

	if (device_mode == DEVICE_MODE_APP)
		name = "app";
	else if (device_mode == DEVICE_MODE_BOOT)
		name = "boot";

	return sprintf(buf, "%s\n", name);
}

/*
	0 -> APP mode
	1 -> Boot mode
*/
static ssize_t cy8c_boot_mode_set(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cy8c_data *data = NULL;
	ssize_t ret = -EINVAL;
	struct regmap *regmap = NULL;
	int action = -1;

	data = container_of(led_cdev, struct cy8c_data, led);
	regmap = data->regmap;

	if (buf == NULL || buf[0] == 0) {
		dev_err(&data->client->dev, "input buf invalid\n");
		return -EINVAL;
	}

	if (sscanf(buf, "%d", &action) != 1) {
		dev_err(&data->client->dev, "input data format invalid\n");
		return -EINVAL;
	}

	if (action != 0 && action != 1) {
		dev_err(&data->client->dev, "input data format value\n");
		return -EINVAL;
	}

	/* boot mode */
	if (action) {
		ret = regmap_write(regmap,
			P1961_REG_CMD,
			P1961_CMD_GOTO_BOOT);

		if (unlikely(ret))
			dev_err(&data->client->dev, "cannot put dev to boot mode\n");
		else
			atomic_set(&data->device_mode, DEVICE_MODE_BOOT);
	} else if (!action && attr == NULL) {
		/*
			app mode
			called from boot device close
		*/
		ret = cy8c_app_mode_test(data);

		if (unlikely(ret)) {
			dev_err(&data->client->dev, "device not in app mode %s\n",
				__func__);
			dev_err(&data->client->dev, "try to jump to app %s\n",
				__func__);
			ret = cy8c_boot_to_app(data);
			if (ret == 0)
				atomic_set(&data->device_mode, DEVICE_MODE_APP);
		} else
			atomic_set(&data->device_mode, DEVICE_MODE_APP);
	} else if (!action && attr != NULL)
		ret = cy8c_boot_to_app(data);

	return ret == 0 ? size : ret;
}

static ssize_t cy8c_version_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cy8c_data *data = NULL;
	ssize_t ret = 0;
	ssize_t count = 0;
	int version = -1;
	data = container_of(led_cdev, struct cy8c_data, led);

	if (atomic_read(&data->device_mode) != DEVICE_MODE_APP) {
		dev_err(&data->client->dev, "device not in app mode %s\n",
			__func__);
		return -EAGAIN;
	}

	ret = regmap_read(data->regmap,
		P1961_REG_APP_MINOR_REV,
		&version);
	if (unlikely(ret)) {
		dev_err(&data->client->dev, "device in boot mode?\n");
		return ret;
	}
	count += sprintf(buf, "%02x: %02x\n", P1961_REG_APP_MINOR_REV, version);

	ret = regmap_read(data->regmap,
		P1961_REG_APP_MAJOR_REV,
		&version);
	if (unlikely(ret)) {
		dev_err(&data->client->dev, "device in boot mode?\n");
		return ret;
	}
	count += sprintf(buf + count, "%02x: %02x\n",
		P1961_REG_APP_MAJOR_REV, version);

	return count;
}

static ssize_t cy8c_version_set(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct cy8c_data *data = NULL;
	data = container_of(led_cdev, struct cy8c_data, led);
	dev_err(&data->client->dev, "not implemented\n");
	return -1;
}

static DEVICE_ATTR(effects, S_IRUGO|S_IWUSR,
		cy8c_effects_show, cy8c_effects_set);
static DEVICE_ATTR(params, S_IRUGO|S_IWUSR,
		cy8c_params_show, cy8c_params_set);
static DEVICE_ATTR(boot_mode, S_IRUGO|S_IWUSR,
		cy8c_boot_mode_show, cy8c_boot_mode_set);
static DEVICE_ATTR(version, S_IRUGO|S_IWUSR,
		cy8c_version_show, cy8c_version_set);

static int led_boot_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = (struct miscdevice *)file->private_data;
	struct cy8c_data *data = NULL;
	data = (struct cy8c_data *)container_of(miscdev,
		struct cy8c_data, miscdev);

	if (atomic_read(&data->device_mode) != DEVICE_MODE_BOOT) {
		dev_err(&data->client->dev,
			"wait for device in boot mode\n");
		return -EIO;
	}

	if (atomic_xchg(&data->in_use, 1)) {
		dev_err(&data->client->dev, "already opened\n");
		return -EBUSY;
	}
	file->private_data = data;
	dev_err(&data->client->dev, "opened\n");
	return 0;
}

static int led_boot_release(struct inode *inode, struct file *file)
{
	struct cy8c_data *data = NULL;
	int ret;
	data = file->private_data;
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&data->in_use, 0));

	ret = cy8c_boot_mode_set(data->led.dev, NULL, "0", 1);
	if (ret > 0)
		ret = 0;

	dev_err(&data->client->dev, "released\n");

	return ret;
}

static ssize_t led_boot_read(struct file *filp, char __user *buf,
				  size_t count, loff_t *offset)
{
	struct cy8c_data *data = filp->private_data;
	unsigned char *p = NULL;
	int ret;

	if (atomic_read(&data->device_mode) != DEVICE_MODE_BOOT) {
		dev_err(&data->client->dev, "device not in boot mode\n");
		return -EINVAL;
	}

	if (count == 0) {
		dev_err(&data->client->dev, "no data\n");
		return -EINVAL;
	}

	p = kzalloc(count, GFP_KERNEL);

	if (p == NULL) {
		dev_err(&data->client->dev, "no memory\n");
		return -ENOMEM;
	}

	/* Read data */
	ret = i2c_master_recv(data->client_boot, p, count);
	if (ret != count) {
		dev_err(&data->client->dev,
			"failed to read %d\n", ret);
		ret = -EIO;
		goto end;
	}

	ret = copy_to_user(buf, p, count);
	if (ret) {
		dev_err(&data->client->dev,
			"failed to copy from user space\n");
	}

end:
	if (p)
		kfree(p);

	return ret == 0 ? count : ret;
}

static ssize_t led_boot_write(struct file *filp, const char __user *buf,
				   size_t count, loff_t *offset)
{
	struct cy8c_data *data = filp->private_data;
	unsigned char *p = NULL;
	int ret;

	if (atomic_read(&data->device_mode) != DEVICE_MODE_BOOT) {
		dev_err(&data->client->dev, "device not in boot mode\n");
		return -EINVAL;
	}

	if (count == 0) {
		dev_err(&data->client->dev, "no data\n");
		return -EINVAL;
	}

	p = kzalloc(count, GFP_KERNEL);

	if (p == NULL) {
		dev_err(&data->client->dev, "no memory\n");
		return -ENOMEM;
	}

	if (copy_from_user(p, buf, count)) {
		dev_err(&data->client->dev,
			"failed to copy from user space\n");
		ret = -EFAULT;
		goto end;
	}

	/* Write data */
	ret = i2c_master_send(data->client_boot, p, count);
	if (ret != count) {
		dev_err(&data->client->dev,
			"failed to write %d\n", ret);
		ret = -EIO;
	}

end:
	if (p)
		kfree(p);

	return ret;
}

static const struct file_operations led_boot_fileops = {
	.owner = THIS_MODULE,
	.open = led_boot_open,
	.release = led_boot_release,
	.read = led_boot_read,
	.write = led_boot_write,
};

static int cy8c_apply_default_settings(struct cy8c_data *data)
{
	int ret;
	u8 state;

	if (!data)
		return -EINVAL;

	pr_debug("%s: led-on-plugin %d, default-brightness %d\n", __func__,
				data->led_on_plugin, data->default_brightness);
	ret = regmap_write(data->regmap, P1961_REG_NOM_BRIGHT,
						data->default_brightness);
	ret |= regmap_write(data->regmap, P1961_REG_CMD_DAT,
						P1961_REG_NOM_BRIGHT);
	ret |= regmap_write(data->regmap,
				P1961_REG_CMD, P1961_CMD_WRITE_EEPROM);
	if (ret)
		dev_err(&data->client->dev, "cannot write brightness\n");

	if (data->led_on_plugin)
		state = P1961_LED_STATE_OFF;
	else
		state = P1961_LED_STATE_SOLID;

	ret = regmap_write(data->regmap, P1961_REG_LED_STATE, state);
	ret |= regmap_write(data->regmap, P1961_REG_CMD_DAT,
						P1961_REG_LED_STATE);
	ret |= regmap_write(data->regmap,
			P1961_REG_CMD, P1961_CMD_WRITE_EEPROM);
	if (ret)
		dev_err(&data->client->dev, "cannot write led state\n");
	return ret;
}

static int cy8c_led_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = client->adapter;
	struct cy8c_data *data;
	struct regmap_config rconfig;
	int ret, reg;
	struct dentry *d;
	char dname[16] = "cy8c-led-boot";

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "i2c functionality check fail.\n");
		return -ENODEV;
	}

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/* enable print in show/get */
	data->client = client;

	of_led_parse_pdata(client, data);

	data->led.brightness_set = set_led_brightness;
	data->led.brightness_get = get_led_brightness;

	memset(&rconfig, 0, sizeof(rconfig));
	rconfig.reg_bits = 8;
	rconfig.val_bits = 8;
	rconfig.cache_type = REGCACHE_NONE;
	rconfig.max_register = P1961_REG_MAX-1;

	/*This should happen before set clientdata*/
	data->regmap = regmap_init_i2c(client, &rconfig);
	if (!data->regmap) {
		devm_kfree(&client->dev, data);
		dev_err(&client->dev, "Failed to allocate register map\n");
		return -ENOMEM;
	}

	/* enable print in show/get */
	data->client = client;

	/* boot device client */
	data->adap = i2c_get_adapter(client->adapter->nr);
	memset(&data->brd_boot, 0, sizeof(data->brd_boot));
	strncpy(data->brd_boot.type, dname, sizeof(data->brd_boot.type));
	data->brd_boot.addr = P1961_BOOT_DEV_ADDR;
	data->client_boot = i2c_new_device(data->adap, &data->brd_boot);

	i2c_set_clientdata(client, data);

	/*
		Sometimes the app fw is broken or out of integration,
		so we will detect app mode first then boot mode
	*/
	ret = regmap_read(data->regmap, P1961_REG_APP_MAJOR_REV, &reg);
	if (ret == 0) {
		dev_dbg(&client->dev, "[nv-foster] rev: 0x%02x ", reg);

		ret = regmap_read(data->regmap, P1961_REG_APP_MINOR_REV, &reg);
		if (ret) {
			dev_err(&client->dev, "Failed to read revision-minor\n");
			goto err1;
		}

		dev_dbg(&client->dev, "0x%02x\n", reg);

		ret = cy8c_apply_default_settings(data);
		if (ret) {
			dev_err(&client->dev, "Failed to read revision-minor\n");
			goto err1;
		}
		atomic_set(&data->device_mode, DEVICE_MODE_APP);
	} else {

		dev_dbg(&client->dev,
			"[nv-foster] not detect app device\n");
		dev_dbg(&client->dev,
			"[nv-foster] continue to detect boot device\n");

		/* Detect whether boot device is available */
		ret = cy8c_boot_mode_test(data);
		if (ret == 0) {
			dev_dbg(&client->dev,
				"[nv-foster] boot device detected\n");
			atomic_set(&data->device_mode, DEVICE_MODE_BOOT);
		} else {
			dev_dbg(&client->dev,
				"[nv-foster] boot device not detected\n");
			goto err1;
		}
	}

	/* initially mode index */
	data->mode_index = 2;

	mutex_init(&data->lock);

	cy8c_apply_default_settings(data);

	ret = led_classdev_register(&client->dev, &data->led);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to register foster led\n");
		goto err1;
	}
	else
		dev_info(&client->dev, "LED registered (%s)\n", data->led.name);

	ret = device_create_file(data->led.dev, &dev_attr_effects);
	if (ret) {
		dev_err(&client->dev, "Failed to register effects sys node\n");
		goto err2;
	}
	ret = device_create_file(data->led.dev, &dev_attr_params);
	if (ret) {
		dev_err(&client->dev, "Failed to register params sys node\n");
		goto err3;
	}
	ret = device_create_file(data->led.dev, &dev_attr_boot_mode);
	if (ret) {
		dev_err(&client->dev, "Failed to register boot sys node\n");
		goto err4;
	}
	ret = device_create_file(data->led.dev, &dev_attr_version);
	if (ret) {
		dev_err(&client->dev, "Failed to register version sys node\n");
		goto err5;
	}

	data->miscdev.name = dname;
	data->miscdev.fops = &led_boot_fileops;
	data->miscdev.minor = MISC_DYNAMIC_MINOR;
	ret = misc_register(&data->miscdev);
	if (ret) {
		dev_err(&client->dev, "%s unable to register misc device %s\n",
			__func__, dname);
		goto err6;
	}

	/* create debugfs for f/w loading purpose */
	d = debugfs_create_file("cy8c_led", S_IRUGO, NULL, data,
							&cy8c_debug_fops);
	if (!d)
		pr_err("Failed to create suspend_mode debug file\n");

	return ret;

err6:
	device_remove_file(data->led.dev, &dev_attr_version);

err5:
	device_remove_file(data->led.dev, &dev_attr_boot_mode);

err4:
	device_remove_file(data->led.dev, &dev_attr_params);

err3:
	device_remove_file(data->led.dev, &dev_attr_effects);

err2:
	led_classdev_unregister(&data->led);

err1:
	if (data->client_boot)
		i2c_unregister_device(data->client_boot);
	if (data->adap)
		i2c_put_adapter(data->adap);

	devm_kfree(&client->dev, data);

	return ret;
}

static int cy8c_led_remove(struct i2c_client *client)
{
	struct cy8c_data *data = i2c_get_clientdata(client);

	if (data->client_boot)
		i2c_unregister_device(data->client_boot);
	if (data->adap)
		i2c_put_adapter(data->adap);
	if (data->miscdev.this_device)
		misc_deregister(&data->miscdev);
	device_remove_file(data->led.dev, &dev_attr_effects);
	device_remove_file(data->led.dev, &dev_attr_params);
	device_remove_file(data->led.dev, &dev_attr_boot_mode);
	device_remove_file(data->led.dev, &dev_attr_version);
	led_classdev_unregister(&data->led);
	mutex_destroy(&data->lock);

	return 0;
}

static const struct i2c_device_id cy8c_led_id[] = {
	{"cy8c_led", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, cy8c_led_id);

#ifdef CONFIG_OF
static struct of_device_id cy8c_of_match[] = {
	{.compatible = "nvidia,cy8c_led", },
	{ },
};
#endif

static struct i2c_driver cy8c_led_driver = {
	.driver = {
		.name   = "cy8c_led",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table =
			of_match_ptr(cy8c_of_match),
#endif
	},
	.probe	  = cy8c_led_probe,
	.remove	 = cy8c_led_remove,
	.id_table   = cy8c_led_id,
};

module_i2c_driver(cy8c_led_driver);

MODULE_AUTHOR("Vinayak Pane <vpane@nvidia.com>");
MODULE_DESCRIPTION("CY8C I2C based LED controller driver");
MODULE_LICENSE("GPL");
