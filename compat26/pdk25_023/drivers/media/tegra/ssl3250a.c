/*
 * ssl3250a.c - ssl3250a flash/torch kernel driver
 *
 * Copyright (C) 2011 NVIDIA Corp.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */


#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <media/ssl3250a.h>

#define SSL3250A_I2C_REG_AMP		0x00
#define SSL3250A_I2C_REG_TMR		0x01
#define SSL3250A_I2C_REG_STRB		0x02
#define SSL3250A_I2C_REG_STS		0x03


enum {
	SSL3250A_GPIO_ACT,
	SSL3250A_GPIO_EN1,
	SSL3250A_GPIO_EN2,
	SSL3250A_GPIO_STRB,
};

struct ssl3250a_info {
	struct i2c_client *i2c_client;
	struct ssl3250a_platform_data *pdata;
};

static struct ssl3250a_info *info;

static int ssl3250a_gpio(u8 gpio, u8 val)
{
	int prev_val;

	switch (gpio) {
	case SSL3250A_GPIO_ACT:
		if (info->pdata && info->pdata->gpio_act) {
			prev_val = info->pdata->gpio_act(val);
			if (val && (prev_val ^ val))
				mdelay(1); /*delay for device ready*/
			return 0;
		}
		return -1;

	case SSL3250A_GPIO_EN1:
		if (info->pdata && info->pdata->gpio_en1) {
			info->pdata->gpio_en1(val);
			return 0;
		}
		return -1;

	case SSL3250A_GPIO_EN2:
		if (info->pdata && info->pdata->gpio_en2) {
			info->pdata->gpio_en2(val);
			return 0;
		}
		return -1;

	case SSL3250A_GPIO_STRB:
		if (info->pdata && info->pdata->gpio_strb) {
			info->pdata->gpio_strb(val);
			return 0;
		}

	default:
		return -1;
	}
}

static int ssl3250a_get_reg(u8 addr, u8 *val)
{
	struct i2c_client *client = info->i2c_client;
	struct i2c_msg msg[2];
	unsigned char data[2];

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = data;

	data[0] = (u8) (addr);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data + 1;

	*val = 0;

	if (i2c_transfer(client->adapter, msg, 2) == 2) {
		*val = data[1];
		return 0;
	} else {
		return -1;
	}
}

static int ssl3250a_set_reg(u8 addr, u8 val)
{
	struct i2c_client *client = info->i2c_client;
	struct i2c_msg msg;
	unsigned char data[2];

	data[0] = (u8) (addr);
	data[1] = (u8) (val);
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = data;

	if (i2c_transfer(client->adapter, &msg, 1) == 1)
		return 0;
	else
		return -1;
}

static long ssl3250a_ioctl(
	struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	u8 val = (u8)arg;
	u8 reg;

	switch (cmd) {
	case SSL3250A_IOCTL_MODE_SHUTDOWN:
		ssl3250a_gpio(SSL3250A_GPIO_ACT, 0);
		return 0;

	case SSL3250A_IOCTL_MODE_STANDBY:
		ssl3250a_gpio(SSL3250A_GPIO_ACT, 1);
		if (info->pdata->config & 0x01) {	/*0:0 0=I2C, 1=GPIO*/
			ssl3250a_gpio(SSL3250A_GPIO_EN1, 0);
			ssl3250a_gpio(SSL3250A_GPIO_EN2, 0);
			return 0;
		} else {
			return ssl3250a_set_reg(SSL3250A_I2C_REG_AMP, 0x00);
		}

/* Amp limit for torch, flash, and LED is controlled by external circuitry in
 * GPIO mode.  In I2C mode amp limit is controlled by chip registers and the
 * limit values are in the board-sensors file.
 */
	case SSL3250A_IOTCL_MODE_TORCH:
		ssl3250a_gpio(SSL3250A_GPIO_ACT, 1);
		if (info->pdata->config & 0x01) {	/*0:0 0=I2C, 1=GPIO*/
			ssl3250a_gpio(SSL3250A_GPIO_EN1, 0);
			ssl3250a_gpio(SSL3250A_GPIO_EN2, 1);
			return 0;
		} else {
			if (val > info->pdata->max_amp_torch)
				val = info->pdata->max_amp_torch;
			val = ((val << 3) & 0xF8);	/*7:3=torch amps*/
			if (!ssl3250a_get_reg(SSL3250A_I2C_REG_AMP, &reg)) {
				val = val | (reg & 0x07); /*shared w/ LED 2:0*/
				return ssl3250a_set_reg(SSL3250A_I2C_REG_AMP,
							val);
			} else {
				return -1;
			}
		}

	case SSL3250A_IOCTL_MODE_FLASH:
		ssl3250a_gpio(SSL3250A_GPIO_ACT, 1);
		if (info->pdata->config & 0x01) {	/*0:0 0=I2C, 1=GPIO*/
			ssl3250a_gpio(SSL3250A_GPIO_EN1, 1);
			ssl3250a_gpio(SSL3250A_GPIO_EN2, 1);
			return 0;
		} else {
			if (val != 0)			/*if 0 then flash=off*/
				val = val + 11;		/*flash starts at 12*/
			if (val > info->pdata->max_amp_flash)
				val = info->pdata->max_amp_flash;
			val = ((val << 3) & 0xF8);	/*7:3=flash amps*/
			if (!ssl3250a_get_reg(SSL3250A_I2C_REG_AMP, &reg)) {
				val = val | (reg & 0x07); /*shared w/ LED 2:0*/
				return ssl3250a_set_reg(SSL3250A_I2C_REG_AMP,
							val);
			} else {
				return -1;
			}
		}

	case SSL3250A_IOCTL_MODE_LED:
		ssl3250a_gpio(SSL3250A_GPIO_ACT, 1);
		if (info->pdata->config & 0x01) {	/*0:0 0=I2C, 1=GPIO*/
			ssl3250a_gpio(SSL3250A_GPIO_EN1, 1);
			ssl3250a_gpio(SSL3250A_GPIO_EN2, 0);
			return 0;
		} else {
			if (val > info->pdata->max_amp_indic)
				val = info->pdata->max_amp_indic;
			val = (val & 0x07);		/*2:0=LED amps*/
			if (!ssl3250a_get_reg(SSL3250A_I2C_REG_AMP, &reg)) {
				val = val | (reg & 0xF8); /*shared w/ 7:3*/
				return ssl3250a_set_reg(SSL3250A_I2C_REG_AMP,
							val);
			} else {
				return -1;
			}
		}

	case SSL3250A_IOCTL_STRB:
		if (val)
			val = 0x01;			/*bit 0=I2C, >0=GPIO*/
		/* if STRB GPIO use that regardless of operation mode */
		if (!ssl3250a_gpio(SSL3250A_GPIO_STRB, val))
			return 0;
		if (!info->pdata->config & 0x01)	/*0:0 0=I2C, 1=GPIO*/
			return ssl3250a_set_reg(SSL3250A_I2C_REG_STRB, val);
		else
			return -1;

	case SSL3250A_IOCTL_TIMER:
		if (!info->pdata->config & 0x01)	/*if I2C mode*/
			return ssl3250a_set_reg(SSL3250A_I2C_REG_TMR, val);

	default:
		return -1;
	}
}


static int ssl3250a_open(struct inode *inode, struct file *file)
{
	int err;
	u8 reg;
	file->private_data = info;

	pr_info("%s\n", __func__);
	if (info->pdata && info->pdata->init) {
		err = info->pdata->init();
		if (err)
			pr_err("ssl3250a_open: Board init failed\n");
	}
	ssl3250a_gpio(SSL3250A_GPIO_ACT, 1);
	err = ssl3250a_get_reg(SSL3250A_I2C_REG_STS, &reg);
	ssl3250a_gpio(SSL3250A_GPIO_ACT, 0);
	if (err)
		pr_err("ssl3250a_open: Device init failed\n");
	return 0;
}

int ssl3250a_release(struct inode *inode, struct file *file)
{
	if (info->pdata && info->pdata->exit)
		info->pdata->exit();
	file->private_data = NULL;
	return 0;
}


static const struct file_operations ssl3250a_fileops = {
	.owner = THIS_MODULE,
	.open = ssl3250a_open,
	.unlocked_ioctl = ssl3250a_ioctl,
	.release = ssl3250a_release,
};

static struct miscdevice ssl3250a_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ssl3250a",
	.fops = &ssl3250a_fileops,
};

static int ssl3250a_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int err;
	info = kzalloc(sizeof(struct ssl3250a_info), GFP_KERNEL);
	if (!info) {
		pr_err("ssl3250a: Unable to allocate memory!\n");
		return -ENOMEM;
	}
	err = misc_register(&ssl3250a_device);
	if (err) {
		pr_err("ssl3250a: Unable to register misc device!\n");
		kfree(info);
		return err;
	}
	info->pdata = client->dev.platform_data;
	info->i2c_client = client;
	i2c_set_clientdata(client, info);
	return 0;
}

static int ssl3250a_remove(struct i2c_client *client)
{
	info = i2c_get_clientdata(client);
	misc_deregister(&ssl3250a_device);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ssl3250a_id[] = {
	{ "ssl3250a", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, ssl3250a_id);

static struct i2c_driver ssl3250a_i2c_driver = {
	.driver = {
		.name = "ssl3250a",
		.owner = THIS_MODULE,
	},
	.probe = ssl3250a_probe,
	.remove = ssl3250a_remove,
	.id_table = ssl3250a_id,
};

static int __init ssl3250a_init(void)
{
	return i2c_add_driver(&ssl3250a_i2c_driver);
}

static void __exit ssl3250a_exit(void)
{
	i2c_del_driver(&ssl3250a_i2c_driver);
}

module_init(ssl3250a_init);
module_exit(ssl3250a_exit);

