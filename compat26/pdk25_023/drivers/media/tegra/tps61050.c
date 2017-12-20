/*
 * tps61050.c - tps61050 flash/torch kernel driver
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
#include <linux/uaccess.h>
#include <linux/list.h>
#include <media/tps61050.h>

#define TPS61050_REG0		0x00
#define TPS61050_REG1		0x01
#define TPS61050_REG2		0x02
#define TPS61050_REG3		0x03


enum {
	TPS61050_GPIO_ENVM,
	TPS61050_GPIO_SYNC,
};

static struct tps61050_flash_capabilities flash_capabilities = {
	NUM_FLASH_LEVELS - 1, /*entry 0 doesn't count*/
	{
		{ 0,    0xFFFFFFFF, 0 },
		{ 150,  558,        2 },
		{ 200,  558,        2 },
		{ 300,  558,        2 },
		{ 400,  558,        2 },
		{ 500,  558,        2 },
		{ 700,  558,        2 },
		{ 900,  558,        2 },
		{ 900,  558,        2 }
	}
};

static struct tps61050_torch_capabilities torch_capabilities = {
	NUM_TORCH_LEVELS - 1, /*entry 0 doesn't count*/
	{
		0,
		50,
		75,
		100,
		150,
		200,
		491,
		491
	}
};

struct tps61050_info {
	int api_pwr;
	char dname[11];
	struct i2c_client *i2c_client;
	struct tps61050_platform_data *pdata;
	struct miscdevice miscdev;
	struct list_head list;
};

static struct tps61050_pin_state dummy_tps61050_pinstate = {
	.mask		= 0x0000,
	.values		= 0x0000,
};

static struct tps61050_platform_data dummy_tps61050_data = {
	.cfg		= 0,
	.num		= 1,
	.max_amp_torch	= 7,
	.max_amp_flash	= 7,
	.pinstate	= &dummy_tps61050_pinstate,
	.init		= NULL,
	.exit		= NULL,
	.pm		= NULL,
	.gpio_envm	= NULL,
	.gpio_sync	= NULL,
};

static LIST_HEAD(info_list);
static DEFINE_SPINLOCK(driver_lock);

static int tps61050_reg_rd(struct tps61050_info *info, u8 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 data[2];

	msg[0].addr = info->i2c_client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = data;
	data[0] = reg;
	msg[1].addr = info->i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data + 1;
	*val = 0;
	if (i2c_transfer(info->i2c_client->adapter, msg, 2) == 2) {
		*val = data[1];
		return 0;
	} else {
		return -1;
	}
}

static int tps61050_reg_wr(struct tps61050_info *info, u8 reg, u8 val)
{
	struct i2c_msg msg;
	u8 data[2];

	data[0] = reg;
	data[1] = val;
	msg.addr = info->i2c_client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = data;
	if (i2c_transfer(info->i2c_client->adapter, &msg, 1) == 1)
		return 0;
	else
		return -1;
}

static int tps61050_gpio(struct tps61050_info *info, int gpio, int val)
{
	int prev_val;

	if (val)
		val = 1;
	switch (gpio) {
	case TPS61050_GPIO_ENVM:
		if (info->pdata->gpio_envm) {
			prev_val = info->pdata->gpio_envm(val);
			if (val && (prev_val ^ val))
				mdelay(1); /* delay for device ready */
			return 0;
		}
		return -1;

	case TPS61050_GPIO_SYNC:
		if (info->pdata->gpio_sync) {
			info->pdata->gpio_sync(val);
			return 0;
		}
		return -1;

	default:
		return -1;
	}
}

static int tps61050_pm_brd_wr(struct tps61050_info *info, int pwr)
{
	int ret = 0;

	if (info->pdata->pm)
		ret = info->pdata->pm(pwr);
	return ret;
}

static int tps61050_pm_wr(struct tps61050_info *info, int pwr)
{
	int ret = -1;
	u8 reg;

	switch (pwr) {
	case TPS61050_PWR_OFF:
		tps61050_gpio(info, TPS61050_GPIO_SYNC, 0);
		ret = tps61050_pm_brd_wr(info, TPS61050_PWR_COMM);
		ret |= tps61050_reg_wr(info, TPS61050_REG0, 0x00);
		ret |= tps61050_reg_wr(info, TPS61050_REG1, 0x00);
		ret |= tps61050_pm_brd_wr(info, TPS61050_PWR_OFF);
		break;

	case TPS61050_PWR_STDBY:
		ret = tps61050_pm_brd_wr(info, TPS61050_PWR_COMM);
		ret |= tps61050_reg_rd(info, TPS61050_REG0, &reg);
		reg = reg & 0x3F; /* 7:6 = mode */
		ret |= tps61050_reg_wr(info, TPS61050_REG0, reg);
		ret |= tps61050_pm_brd_wr(info, TPS61050_PWR_STDBY);
		break;

	case TPS61050_PWR_COMM:
		ret = tps61050_pm_brd_wr(info, TPS61050_PWR_COMM);
		break;

	case TPS61050_PWR_ON:
		ret = tps61050_pm_brd_wr(info, TPS61050_PWR_ON);
		break;

	default:
		break;
	}

	if (ret)
		pr_err("%s: error = %d!\n", __func__, ret);
	return ret;
}

static int tps61050_pm_api_wr(struct tps61050_info *info, int pwr)
{
	int ret;

	ret = tps61050_pm_wr(info, pwr);
	if (!ret)
		info->api_pwr = pwr;
	else
		info->api_pwr = TPS61050_PWR_ERR;
	return ret;
}

static int tps61050_pm_dev_wr(struct tps61050_info *info, int pwr)
{
	int ret = 0;

	if (pwr > info->api_pwr)
		ret = tps61050_pm_wr(info, pwr);
	return ret;
}

static int tps61050_sts_rd(struct tps61050_info *info, u8 *status)
{
	int ret;

	*status = 0;
	tps61050_pm_dev_wr(info, TPS61050_PWR_COMM);
	ret = tps61050_reg_rd(info, TPS61050_REG2, status);
	tps61050_pm_dev_wr(info, TPS61050_PWR_OFF);
	return ret;
}

static int tps61050_param_rd(struct tps61050_info *info, long arg)
{
	struct tps61050_param param_data;
	int ret;
	u8 reg;

	if (copy_from_user(&param_data,
			(const void __user *)arg,
			sizeof(struct tps61050_param))) {
		pr_info("%s %d\n", __func__, __LINE__);
		return -1;
	}

	switch (param_data.param) {
	case TPS61050_FLASH_CAPS:
		if (param_data.sizeofvalue <
				sizeof(struct tps61050_flash_capabilities)) {
			pr_info("%s %d\n", __func__, __LINE__);
			return -1;
		}

		if (copy_to_user((void __user *)param_data.p_value,
				&flash_capabilities,
				sizeof(struct tps61050_flash_capabilities))) {
			pr_info("%s %d\n", __func__, __LINE__);
			return -1;
		}

		return 0;

	case TPS61050_FLASH_LEVEL:
		if (param_data.sizeofvalue <
				sizeof(flash_capabilities.levels[0].guidenum)) {
			pr_info("%s %d\n", __func__, __LINE__);
			return -1;
		}

		if (info->api_pwr != TPS61050_PWR_ON)
			tps61050_pm_dev_wr(info, TPS61050_PWR_COMM);
		ret = tps61050_reg_rd(info, TPS61050_REG1, &reg);
		/* no need to check return vlaue below */
		tps61050_pm_dev_wr(info, TPS61050_PWR_OFF);
		if (reg & 0x80) { /* 7:7 flash on/off */
			reg &= 0x07; /* 2:0 flash setting */
			reg++; /* flash setting +1 if flash on */
		} else {
			reg = 0; /* flash is off */
		}
		if (copy_to_user((void __user *)param_data.p_value,
				&flash_capabilities.levels[reg].guidenum,
				sizeof
				(flash_capabilities.levels[reg].guidenum))) {
			pr_info("%s %d\n", __func__, __LINE__);
			return -1;
		}

		return ret;

	case TPS61050_TORCH_CAPS:
		if (param_data.sizeofvalue <
				sizeof(struct tps61050_torch_capabilities)) {
			pr_info("%s %d\n", __func__, __LINE__);
			return -1;
		}

		if (copy_to_user((void __user *)param_data.p_value,
				&torch_capabilities,
				sizeof
				(struct tps61050_torch_capabilities))) {
			pr_info("%s %d\n", __func__, __LINE__);
			return -1;
		}

		return 0;

	case TPS61050_TORCH_LEVEL:
		if (param_data.sizeofvalue <
				sizeof(torch_capabilities.guidenum[0])) {
			pr_info("%s %d\n", __func__, __LINE__);
			return -1;
		}

		if (info->api_pwr != TPS61050_PWR_ON)
			tps61050_pm_dev_wr(info, TPS61050_PWR_COMM);
		ret = tps61050_reg_rd(info, TPS61050_REG0, &reg);
		/* no need to check return vlaue below */
		tps61050_pm_dev_wr(info, TPS61050_PWR_OFF);
		reg &= 0x07;
		if (copy_to_user((void __user *)param_data.p_value,
				&torch_capabilities.guidenum[reg],
				sizeof(torch_capabilities.guidenum[reg]))) {
			pr_info("%s %d\n", __func__, __LINE__);
			return -1;
		}

		return ret;

	case TPS61050_FLASH_PIN_STATE:
		if (param_data.sizeofvalue !=
				sizeof(struct tps61050_pin_state)) {
			pr_info("%s %d\n", __func__, __LINE__);
			return -1;
		}

		if (copy_to_user((void __user *)param_data.p_value,
				info->pdata->pinstate,
				sizeof(struct tps61050_pin_state))) {
			pr_info("%s %d\n", __func__, __LINE__);
			return -1;
		}

		return 0;

	default:
		pr_err("%s: unsurported parameter read\n", __func__);
		return -1;
	}
}

static int tps61050_param_wr(struct tps61050_info *info, long arg)
{
	struct tps61050_param param_data;
	u8 reg;
	u8 val;
	int ret = 0;

	if (copy_from_user(&param_data,
				(const void __user *)arg,
				sizeof(struct tps61050_param))) {
		pr_info("%s %d\n", __func__, __LINE__);
		return -1;
	}

	/*
	7:6 flash/torch mode
	0 0 = off (power save)
	0 1 = torch only (torch power is 2:0 REG0 where 0 = off)
	1 0 = flash and torch (flash power is 2:0 REG1 (0 is a power level))
	1 1 = N/A
	Note that 7:6 of REG0 and REG1 are shadowed with each other.
	In the code below we want to turn on/off one
	without affecting the other.
	*/
	val = *(u8 *)param_data.p_value;
	switch (param_data.param) {
	case TPS61050_FLASH_LEVEL:
		if (info->api_pwr != TPS61050_PWR_ON)
			tps61050_pm_dev_wr(info, TPS61050_PWR_ON);
		if (val) {
			val--;
			if (val > info->pdata->max_amp_flash)
				val = info->pdata->max_amp_flash;
			val |= 0x80; /* 7:7=flash mode */
		} else {
			ret = tps61050_reg_rd(info, TPS61050_REG0, &reg);
			if (reg & 0x07) /* 2:0=torch setting */
				val = 0x40; /* 6:6 enable just torch */
		}
		ret |= tps61050_reg_wr(info, TPS61050_REG1, val);
		val &= 0xC0; /* 7:6=flash/torch mode */
		if (!val && (info->api_pwr != TPS61050_PWR_ON))
			tps61050_pm_wr(info, TPS61050_PWR_OFF);
		return ret;

	case TPS61050_TORCH_LEVEL:
		if (info->api_pwr != TPS61050_PWR_ON)
			tps61050_pm_dev_wr(info, TPS61050_PWR_ON);
		ret = tps61050_reg_rd(info, TPS61050_REG1, &reg);
		reg &= 0x80; /* 7:7=flash */
		if (val) {
			if (val > info->pdata->max_amp_torch)
				val = info->pdata->max_amp_torch;

			if (!reg) /* test if flash/torch off */
				val |= (0x40); /* 6:6=torch only mode */
		} else {
			val |= reg;
		}
		/* odm will check error and decide what to do */
		ret |= tps61050_reg_wr(info, TPS61050_REG0, val);
		val &= 0xC0; /* 7:6=mode */
		if (!val && (info->api_pwr != TPS61050_PWR_ON))
			tps61050_pm_wr(info, TPS61050_PWR_OFF);
		return ret;

	case TPS61050_FLASH_PIN_STATE:
		if (info->pdata->gpio_sync) {
			return tps61050_gpio(info,
					TPS61050_GPIO_SYNC,
					(int)val);
		} else {
			if (val)
				val = 0x08; /* 3:3=soft trigger */
			ret = tps61050_reg_rd(info, TPS61050_REG1, &reg);
			val |= reg;
			/* odm will check error and decide what to do */
			ret |= tps61050_reg_wr(info, TPS61050_REG1, val);
			return ret;
		}

	default:
		pr_err("%s: unsurported parameter write\n", __func__);
		return -1;
	}
}

static long tps61050_ioctl(struct file *file,
		unsigned int cmd,
		unsigned long arg)
{
	int pwr;
	struct tps61050_info *info = file->private_data;

	switch (cmd) {
	case TPS61050_IOCTL_PWR:
		pwr = (int)arg * 2;
		return tps61050_pm_api_wr(info, pwr);

	case TPS61050_IOCTL_PARAM_RD:
		return tps61050_param_rd(info, arg);

	case TPS61050_IOCTL_PARAM_WR:
		return tps61050_param_wr(info, arg);

	default:
		pr_err("%s: unsupported ioctl\n", __func__);
		return -1;
	}
}


static int tps61050_open(struct inode *inode, struct file *file)
{
	int ret;
	u8 reg;
	struct tps61050_info *info = NULL, *pos = NULL;

	pr_debug("%s\n", __func__);
	rcu_read_lock();
	list_for_each_entry_rcu(pos, &info_list, list) {
		if (pos->miscdev.minor == iminor(inode)) {
			info = pos;
			break;
		}
	}
	rcu_read_unlock();
	if (!info)
		return -ENODEV;

	file->private_data = info;
	if (info->pdata->init) {
		ret = info->pdata->init();
		if (ret) {
			pr_err("%s: board init failed\n", __func__);
			return -1;
		}
	}

	ret = tps61050_sts_rd(info, &reg);
	if ((reg & 0xC0) || ret) /* status error bits */
		pr_err("%s: Device init failed\n", __func__);
	return ret;
}

static int tps61050_release(struct inode *inode, struct file *file)
{
	struct tps61050_info *info = file->private_data;

	pr_info("%s\n", __func__);
	tps61050_pm_api_wr(info, TPS61050_PWR_OFF);
	if (info->pdata->exit)
		info->pdata->exit();
	file->private_data = NULL;
	return 0;
}

static const struct file_operations tps61050_fileops = {
	.owner = THIS_MODULE,
	.open = tps61050_open,
	.unlocked_ioctl = tps61050_ioctl,
	.release = tps61050_release,
};

static int tps61050_remove(struct i2c_client *client)
{
	struct tps61050_info *info = i2c_get_clientdata(client);

	pr_debug("%s\n", __func__);
	tps61050_pm_api_wr(info, TPS61050_PWR_OFF);
	spin_lock(&driver_lock);
	list_del_rcu(&info->list);
	spin_unlock(&driver_lock);
	synchronize_rcu();
	misc_deregister(&info->miscdev);
	kfree(info);
	return 0;
}

static int tps61050_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct tps61050_info *info = NULL;
	u8 reg;

	pr_info("%s\n", __func__);
	info = kzalloc(sizeof(struct tps61050_info), GFP_KERNEL);
	if (info == NULL) {
		pr_err("tps61050: unable to allocate memory!\n");
		return -ENOMEM;
	}

	info->i2c_client = client;
	if (client->dev.platform_data)
		info->pdata = client->dev.platform_data;
	else
		info->pdata = &dummy_tps61050_data;
	i2c_set_clientdata(client, info);
	INIT_LIST_HEAD(&info->list);
	snprintf(info->dname, sizeof(info->dname), "tps61050-%c",
					('0' + info->pdata->num));
	info->miscdev.name = info->dname;
	info->miscdev.fops = &tps61050_fileops;
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	if (misc_register(&info->miscdev)) {
		pr_err("tps61050: unable to register misc device!\n");
		kfree(info);
		return -ENODEV;
	}

	spin_lock(&driver_lock);
	list_add_rcu(&info->list, &info_list);
	spin_unlock(&driver_lock);
	if ((tps61050_sts_rd(info, &reg) < 0) &&
			(info->pdata->cfg & (TPS61050_CFG_NOSENSOR))) {
		tps61050_remove(client);
		return -ENODEV;
	}

	return 0;
}

static const struct i2c_device_id tps61050_id[] = {
	{ "tps61050", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, tps61050_id);

static struct i2c_driver tps61050_i2c_driver = {
	.driver = {
		.name = "tps61050",
		.owner = THIS_MODULE,
	},
	.id_table = tps61050_id,
	.probe = tps61050_probe,
	.remove = tps61050_remove,
};

static int __init tps61050_init(void)
{
	pr_info("%s\n", __func__);
	return i2c_add_driver(&tps61050_i2c_driver);
}

static void __exit tps61050_exit(void)
{
	pr_info("%s\n", __func__);
	i2c_del_driver(&tps61050_i2c_driver);
}

module_init(tps61050_init);
module_exit(tps61050_exit);
