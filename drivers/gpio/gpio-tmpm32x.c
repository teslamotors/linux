/*
 * drivers/gpio/gpio-tmpm32x.c
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm.h>
#include <linux/mutex.h>
#include <linux/tty.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/gpio-tmpm32x.h>

#ifdef CONFIG_OF_GPIO
#include <linux/of_platform.h>
#endif

#define TMPM_CMD_DEBUG 1
#define TMPM_RSP_DEBUG 0

#define MCU_WRITE_CMD	'W'
#define MCU_READ_CMD	'R'
#define MCU_INPUT_CMD	'I'
#define MCU_OUTPUT_CMD	'O'

#define RECV_BUF_LEN 32
#define MAX_RETRY_COUNT 3000

/* sending command is 'nvXXX\n'
   response string is 'nvXXX+\r\n' or 'nvXXX-\r\n' */
#define MCU_RESP_BYTE(X) ((X) + 2)

struct tmpm32x_chip {
	struct file *fptr;
	struct mutex tty_mutex;
	struct gpio_chip gpio_chip;
	const char *ttyport;
};

static int tmpm32x_get_cmd_string(char *buf, unsigned offset,
		int value, char type)
{
	if (buf && (offset < TMPM32X_MAX_GPIO)) {
		switch (type) {
		case MCU_WRITE_CMD:
			sprintf(buf, "NV%c%02X%d\r",
					type, offset, value ? 1 : 0);
			break;
		case MCU_READ_CMD:
		case MCU_INPUT_CMD:
			sprintf(buf, "NV%c%02X\r", type, offset);
			break;
		case MCU_OUTPUT_CMD:
			sprintf(buf, "NV%c%c%d%d\r",
					type, (offset / 8) + 'A', offset % 8, value ? 1 : 0);
			break;
		default:
			return -EINVAL;
			break;
		}
		return 0;
	}
	return -EINVAL;
}

static int tmpm32x_file_write(struct file *fptr, unsigned char *buf, int len)
{
	int ret;
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	fptr->f_pos = 0;
	ret = fptr->f_op->write(fptr, buf, len, &fptr->f_pos);
	set_fs(oldfs);
	return ret;
}

static int tmpm32x_file_read(struct file *fptr, unsigned char *buf,
		int buf_size, int req_size, int retry_cnt)
{
	int n_read = 0;
	int retry = 0;
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	fptr->f_pos = 0;
	do {
		ret = fptr->f_op->read(fptr, &(buf[n_read]),
				buf_size - n_read, &fptr->f_pos);
		if (ret >= 0)
			n_read += ret;
		else {
			n_read = -1;
			break;
		}

		if (n_read >= req_size)
			break;

		if ((++retry) > retry_cnt) {
			n_read = -1;
			break;
		}
		usleep_range(1000, 2000);
	} while (buf_size > n_read);
	set_fs(oldfs);

	return n_read;
}

static void tmpm32x_gpio_write(struct gpio_chip *gc, char *cmd, char *rsp)
{
	int n_write = 0, n_read = 0;
	int ret = 0;
	char buf[RECV_BUF_LEN];
	struct tmpm32x_chip *chip;

	chip = container_of(gc, struct tmpm32x_chip, gpio_chip);

#if TMPM_CMD_DEBUG
	pr_info("%s: CMD: %s\n", __func__, cmd);
#endif

	mutex_lock(&chip->tty_mutex);

	n_write = strlen(cmd);
	ret = tmpm32x_file_write(chip->fptr, cmd, n_write);
	if (ret == -1)
		pr_err("%s: write error\n", __func__);
	else if (ret == n_write)
		n_read = tmpm32x_file_read(chip->fptr, buf, RECV_BUF_LEN,
				MCU_RESP_BYTE(n_write), MAX_RETRY_COUNT);
	else
		pr_err("%s: cannot write the full command, w-byte[%d]\n",
				__func__, ret);

	if (n_read > 0)
		buf[n_read] = 0;

	if (rsp)
		strcpy(rsp, buf);

	mutex_unlock(&chip->tty_mutex);

#if TMPM_RSP_DEBUG
	pr_info("%s: fptr-read byte [%d]\n", __func__, n_read);
	if (n_read > 0)
		pr_info("%s: RSP: %s", __func__, buf);
#endif
}

static u32 tmpm32x_gpio_read(struct gpio_chip *gc, char *cmd, char *rsp)
{
	int n_write = 0, n_read = 0;
	int val = -1;
	int ret;
	char buf[RECV_BUF_LEN];
	struct tmpm32x_chip *chip;

	chip = container_of(gc, struct tmpm32x_chip, gpio_chip);
#if TMPM_CMD_DEBUG
	pr_info("%s: CMD: %s\n", __func__, cmd);
#endif

	mutex_lock(&chip->tty_mutex);

	n_write = strlen(cmd);
	ret = tmpm32x_file_write(chip->fptr, cmd, n_write);
	if (ret == -1)
		pr_err("%s: write error\n", __func__);
	else if (ret == n_write)
		n_read = tmpm32x_file_read(chip->fptr, buf, RECV_BUF_LEN,
				MCU_RESP_BYTE(n_write), MAX_RETRY_COUNT);
	else
		pr_err("%s: cannot write the full command, w-byte[%d]\n",
				__func__, ret);

	if (n_read > 0)
		buf[n_read] = 0;

	if (rsp)
		strcpy(rsp, buf);

	mutex_unlock(&chip->tty_mutex);

#if TMPM_RSP_DEBUG
	pr_info("%s: fptr-read byte [%d]\n", __func__, n_read);
	if (n_read > 0)
		pr_info("%s: RSP: %s", __func__, buf);
#endif

	if (n_read > 0) {
		if (buf[n_write - 1] == '0')
			val = 0;
		else if (buf[n_write - 1] == '1')
			val = 1;
		else {
			val = -1;
			pr_err("%s: gpio read error\n", __func__);
		}
	}
	return val;
}

static int tmpm32x_gpio_request(struct gpio_chip *gc, unsigned offset)
{
	pr_info("%s: (%u)\n", __func__, offset);
	return 0;
}

static void tmpm32x_gpio_free(struct gpio_chip *gc, unsigned offset)
{
	pr_info("%s: (%u)\n", __func__, offset);
	return;
}

static void tmpm32x_gpio_set(struct gpio_chip *gc, unsigned offset, int value)
{
	char cmd[RECV_BUF_LEN];
	char rsp[RECV_BUF_LEN];

	if (tmpm32x_get_cmd_string(cmd, offset, value, MCU_WRITE_CMD)) {
		pr_err("%s: cannot get cmd string. offset[0x%02X], value[%d]\n",
				__func__, offset, value);
		return;
	}
	tmpm32x_gpio_write(gc, cmd, rsp);
}

static int tmpm32x_gpio_get(struct gpio_chip *gc, unsigned offset)
{
	char cmd[RECV_BUF_LEN];
	char rsp[RECV_BUF_LEN];

	if (tmpm32x_get_cmd_string(cmd, offset, 0, MCU_READ_CMD)) {
		pr_err("%s: cannot get cmd string. offset[0x%02X]\n",
				__func__, offset);
		return -1;
	}

	return tmpm32x_gpio_read(gc, cmd, rsp);
}

static int tmpm32x_gpio_direction_input(struct gpio_chip *gc, unsigned offset)
{
	char cmd[RECV_BUF_LEN];
	char rsp[RECV_BUF_LEN];

	if (tmpm32x_get_cmd_string(cmd, offset, 0, MCU_INPUT_CMD)) {
		pr_err("%s: cannot get cmd string. offset[0x%02X]\n",
				__func__, offset);
		return -1;
	}
	tmpm32x_gpio_write(gc, cmd, rsp);

	return 0;
}

static int tmpm32x_gpio_direction_output(struct gpio_chip *gc,
		unsigned offset, int value)
{
	char cmd[RECV_BUF_LEN];
	char rsp[RECV_BUF_LEN];

	if (tmpm32x_get_cmd_string(cmd, offset, value, MCU_OUTPUT_CMD)) {
		pr_err("%s: cannot get cmd string. offset[0x%02X]\n",
				__func__, offset);
		return -1;
	}
	tmpm32x_gpio_write(gc, cmd, rsp);

	return 0;
}

static struct tmpm32x_chip mcu_chip = {
	.gpio_chip = {
		.label              = "tmpm32x-gpio",
		.request            = tmpm32x_gpio_request,
		.free               = tmpm32x_gpio_free,
		.direction_input    = tmpm32x_gpio_direction_input,
		.get                = tmpm32x_gpio_get,
		.direction_output   = tmpm32x_gpio_direction_output,
		.set                = tmpm32x_gpio_set,
		.can_sleep          = 1,
		.owner              = THIS_MODULE,
		.base               = TMPM32X_GPIO_BASE,
	},
};

static struct of_device_id tmpm32x_gpio_of_match[] = {
	{ .compatible = "toshiba,tmpm32x-gpio" },
	{ .compatible = "nvidia,tmpm32x-gpio" },
	{ },
};

MODULE_DEVICE_TABLE(of, tmpm32x_gpio_of_match);

static void tmpm32x_read_fw_ver(char *ver)
{
	char buf[RECV_BUF_LEN];

	/* clear MCU side buffer */
	tmpm32x_gpio_write(&mcu_chip.gpio_chip, "NVV\r", buf);

	memset(buf, 0, sizeof(buf));
	tmpm32x_gpio_write(&mcu_chip.gpio_chip, "NVV\r", buf);
	if (buf[3] == '-')
		sprintf(buf, "0.3");
	else {
		ver[0] = buf[3];
		ver[1] = '.';
		ver[2] = buf[4];
		ver[3] = 0;
	}
}

static int tmpm32x_tty_init(struct gpio_chip *gc)
{
	mm_segment_t oldfs;
	struct ktermios settings;
	struct tmpm32x_chip *chip;
	int ret = 0;

	chip = container_of(gc, struct tmpm32x_chip, gpio_chip);

	mutex_init(&chip->tty_mutex);

	mutex_lock(&chip->tty_mutex);
	chip->fptr = filp_open(mcu_chip.ttyport, O_RDWR | O_NOCTTY, 0666);
	if (IS_ERR(chip->fptr)) {
		pr_err("%s: cannot open %s\n", __func__, mcu_chip.ttyport);
		ret = -1;
		goto exit;
	}

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	memset(&settings, 0, sizeof(settings));

	settings.c_cflag |= B115200;
	settings.c_cflag |= CLOCAL;
	settings.c_cflag |= CREAD;

	settings.c_cflag &= ~ECHO;
	settings.c_cflag &= ~ECHOE;

	settings.c_cflag &= ~PARENB;
	settings.c_cflag &= ~CSTOPB;
	settings.c_cflag &= ~CSIZE;
	settings.c_cflag |= CS8;

	settings.c_oflag = 0;

	settings.c_ispeed = B115200;
	settings.c_ospeed = B115200;

	settings.c_cc[VTIME] = 0;
	settings.c_cc[VMIN] = 0;

	chip->fptr->f_op->unlocked_ioctl(chip->fptr,
			TCFLSH, TCIFLUSH);
	chip->fptr->f_op->unlocked_ioctl(chip->fptr,
			TCSETS, (unsigned long)&settings);

	set_fs(oldfs);

exit:
	mutex_unlock(&chip->tty_mutex);
	return ret;
}

static int tmpm32x_gpio_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device_node *np = pdev->dev.of_node;
	char buf[10];

	match = of_match_device(tmpm32x_gpio_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		return -ENODEV;
	}
	mcu_chip.gpio_chip.dev = &pdev->dev;
	mcu_chip.gpio_chip.ngpio = TMPM32X_MAX_GPIO;
	mcu_chip.gpio_chip.of_node = pdev->dev.of_node;
	of_property_read_string(np, "nvidia,ttyport", &mcu_chip.ttyport);
	dev_info(&pdev->dev, "ttyport: %s\n", mcu_chip.ttyport);

	if (tmpm32x_tty_init(&mcu_chip.gpio_chip) < 0)
		return -1;

	gpiochip_add(&mcu_chip.gpio_chip);

	/* read MCU F/W version */
	tmpm32x_read_fw_ver(buf);
	dev_info(&pdev->dev, "MCU F/W version: %s\n", buf);

	return 0;
}

static int tmpm32x_gpio_remove(struct platform_device *pdev)
{
	int ret = 0;
	ret = gpiochip_remove(&mcu_chip.gpio_chip);
	if (!IS_ERR(mcu_chip.fptr) && mcu_chip.fptr)
		filp_close(mcu_chip.fptr, NULL);
	return 0;
}

static struct platform_driver tmpm32x_gpio_driver = {
	.driver		= {
		.name	= "tmpm32x-gpio",
		.owner	= THIS_MODULE,
		.of_match_table = tmpm32x_gpio_of_match,
	},
	.probe		= tmpm32x_gpio_probe,
	.remove		= tmpm32x_gpio_remove,
};

static int __init tmpm32x_init(void)
{
	return platform_driver_register(&tmpm32x_gpio_driver);
}
module_init(tmpm32x_init);

static void __exit tmpm32x_exit(void)
{
	platform_driver_unregister(&tmpm32x_gpio_driver);
}
module_exit(tmpm32x_exit);

MODULE_AUTHOR("Jubeom Kim <jubeomk@nvidia.com>");
MODULE_DESCRIPTION("GPIO MCU driver");
MODULE_LICENSE("GPL");
