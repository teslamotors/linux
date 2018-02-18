/*
 * virtual.c - Virtual kernel driver
 *
 * Copyright (c) 2013-2016, NVIDIA CORPORATION. All rights reserved.

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

#define CAMERA_DEVICE_INTERNAL

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/clk.h>

#include <media/nvc.h>
#include <media/camera.h>
#include "camera_common.h"

struct chip_config {
	int clk_num;
	int gpio_num;
};

static int virtual_update(
	struct camera_device *cdev, struct cam_update *upd, int num)
{
	int err = 0;
	int idx;

	dev_dbg(cdev->dev, "%s %d\n", __func__, num);
	mutex_lock(&cdev->mutex);
	for (idx = 0; idx < num; idx++) {
		switch (upd[idx].type) {
		case UPDATE_EDP:
		{
			dev_dbg(cdev->dev, "%s UPDATE_EDP is deprecated\n",
				__func__);
			break;
		}
		case UPDATE_CLOCK:
		{
			struct clk *ck;
			u8 buf[CAMERA_MAX_NAME_LENGTH];

			if (!cdev->num_clk) {
				dev_err(cdev->dev, "NO clock needed.\n");
				err = -ENODEV;
				break;
			}
			if (upd[idx].index >= cdev->num_clk) {
				dev_err(cdev->dev,
					"clock index %d out of range.\n",
					upd[idx].index);
				err = -ENODEV;
				break;
			}

			memset(buf, 0, sizeof(buf));
			if (copy_from_user(buf,
				((const void __user *)
				(unsigned long)upd[idx].arg),
				sizeof(buf) - 1 < upd[idx].size ?
				sizeof(buf) - 1 : upd[idx].size)) {
				dev_err(cdev->dev,
					"%s copy_from_user err line %d\n",
					__func__, __LINE__);
				err = -EFAULT;
				break;
			}

			dev_dbg(cdev->dev, "%s UPDATE_CLOCK %d of %d, %s\n",
				__func__, upd[idx].index, cdev->num_clk, buf);
			ck = devm_clk_get(cdev->dev, buf);
			if (IS_ERR(ck)) {
				dev_err(cdev->dev, "%s: get clock %s FAILED.\n",
					__func__, buf);
				return PTR_ERR(ck);
			}
			cdev->clks[upd[idx].index] = ck;
			dev_dbg(cdev->dev, "UPDATE_CLOCK: %d %s\n",
				upd[idx].index, buf);
			break;
		}
		case UPDATE_PINMUX:
		{
			break;
		}
		case UPDATE_GPIO:
		{
			dev_dbg(cdev->dev, "%s UPDATE_GPIO is deprecated\n",
				__func__);
			break;
		}
		default:
			dev_err(cdev->dev,
				"unsupported upd type %d\n", upd[idx].type);
			break;
		}

		if (err)
			break;
	}
	mutex_unlock(&cdev->mutex);
	return err;
}

static int virtual_power_on(struct camera_device *cdev)
{
	struct camera_reg *pwr_seq = cdev->seq_power_on;
	int err = 0;

	dev_dbg(cdev->dev, "%s %x %p\n",
		__func__, cdev->is_power_on, pwr_seq);
	if (cdev->is_power_on || !pwr_seq)
		return 0;

	mutex_lock(&cdev->mutex);
	err = camera_dev_wr_table(cdev, pwr_seq, NULL);
	if (!err)
		cdev->is_power_on = 1;
	mutex_unlock(&cdev->mutex);
	return err;
}

static int virtual_power_off(struct camera_device *cdev)
{
	struct camera_reg *pwr_seq = cdev->seq_power_off;
	int err = 0;

	dev_dbg(cdev->dev, "%s %x %p\n",
		__func__, cdev->is_power_on, pwr_seq);
	if (!cdev->is_power_on || !pwr_seq)
		return 0;

	mutex_lock(&cdev->mutex);
	err = camera_dev_wr_table(cdev, pwr_seq, NULL);
	if (!err)
		cdev->is_power_on = 0;
	mutex_unlock(&cdev->mutex);

	return err;
}

static int virtual_shutdown(struct camera_device *cdev)
{
	int err = 0;

	dev_dbg(cdev->dev, "%s %x\n", __func__, cdev->is_power_on);
	if (!cdev->is_power_on)
		return 0;

	if (!err)
		err = virtual_power_off(cdev);

	return err;
}

static int virtual_instance_destroy(struct camera_device *cdev)
{
	void *buf;
	u32 idx;

	dev_dbg(cdev->dev, "%s\n", __func__);

	if (!cdev->dev)
		return 0;

	buf = dev_get_drvdata(cdev->dev);
	dev_set_drvdata(cdev->dev, NULL);

	for (idx = 0; idx < cdev->num_reg; idx++)
		if (likely(cdev->regs[idx].vreg))
			regulator_put(cdev->regs[idx].vreg);

	cdev->num_gpio = 0;
	cdev->num_reg = 0;
	kfree(buf);
	return 0;
}

static int virtual_instance_create(struct camera_device *cdev, void *pdata)
{
	struct chip_config *c_info = cdev->chip->private;
	struct camera_reg *pwr_seq = NULL;
	const char *reg_names[8], *clks;
	void *buf;
	u32 idx, pwron_size, pwroff_size, reg_num, clk_num;

	dev_dbg(cdev->dev, "%s\n", __func__);
	memset(reg_names, 0 , sizeof(reg_names));
	if (cdev->of_node) {
		pwr_seq = of_camera_get_pwrseq(
			cdev->dev, cdev->of_node, &pwron_size, &pwroff_size);
		reg_num = of_camera_get_regulators(cdev->dev, cdev->of_node,
			reg_names, sizeof(reg_names) / sizeof(reg_names[0]));
		clks = of_camera_get_clocks(cdev->dev, cdev->of_node, &clk_num);
	} else {
		pwron_size = 0;
		pwroff_size = 0;
		reg_num = 0;
		clks = NULL;
		clk_num = 0;
	}

	buf = kzalloc(c_info->gpio_num * sizeof(*cdev->gpios) +
		reg_num * sizeof(*cdev->regs) +
		c_info->clk_num * sizeof(*cdev->clks) +
		(pwron_size + pwroff_size) * sizeof(*pwr_seq),
		GFP_KERNEL);
	if (buf == NULL) {
		dev_err(cdev->dev, "%s memory low!\n", __func__);
		of_camera_put_pwrseq(cdev->dev, pwr_seq);
		return -ENOMEM;
	}

	cdev->gpios = buf;
	cdev->regs = (void *)(cdev->gpios + c_info->gpio_num);
	cdev->clks = (void *)(cdev->regs + reg_num);
	cdev->num_gpio = c_info->gpio_num;
	cdev->num_reg = reg_num;
	cdev->num_clk = c_info->clk_num;
	cdev->mclk_enable_idx = CAMDEV_INVALID;
	cdev->mclk_disable_idx = CAMDEV_INVALID;
	if (pdata) {
		cdev->pinmux_num =
			((struct camera_platform_data *)pdata)->pinmux_num;
		cdev->pinmux_tbl =
			((struct camera_platform_data *)pdata)->pinmux;
	}

	for (idx = 0; idx < cdev->num_gpio; idx++)
		cdev->gpios[idx].valid = false;

	for (idx = 0; idx < cdev->num_reg; idx++) {
		cdev->regs[idx].vreg_name = (void *)reg_names[idx];
		camera_regulator_get(cdev->dev, &cdev->regs[idx],
			(char *)cdev->regs[idx].vreg_name);
	}

	for (idx = 0; idx < cdev->num_clk && idx < clk_num; idx++) {
		if (clks == NULL)
			break;
		cdev->clks[idx] = devm_clk_get(cdev->dev, clks);
		if (IS_ERR(cdev->clks[idx])) {
			dev_err(cdev->dev, "%s: get clock %s FAILED.\n",
					__func__, clks);
			of_camera_put_pwrseq(cdev->dev, pwr_seq);
			kfree(buf);
			cdev->gpios = buf = NULL;
			return PTR_ERR(cdev->clks[idx]);
		}
		dev_dbg(cdev->dev, "%s - clock: %s\n", __func__, clks);
		clks += strlen(clks) + 1;
	}

	dev_set_drvdata(cdev->dev, cdev->gpios);

	if (pwr_seq) {
		cdev->seq_power_on = (void *)(cdev->clks + c_info->clk_num);
		cdev->seq_power_off = cdev->seq_power_on + pwron_size;
		memcpy(cdev->seq_power_on, pwr_seq,
			(pwron_size + pwroff_size) * sizeof(*pwr_seq));

		dev_dbg(cdev->dev, "%s power_on: %p %u\n",
			__func__, cdev->seq_power_on, pwron_size);
		for (idx = 0; idx < pwron_size; idx++) {
			dev_dbg(cdev->dev, "    %08x - %08x\n",
				cdev->seq_power_on[idx].addr,
				cdev->seq_power_on[idx].val);
		}
		dev_dbg(cdev->dev, "%s power_off: %p %u\n",
			__func__, cdev->seq_power_off, pwroff_size);
		for (idx = 0; idx < pwroff_size; idx++) {
			dev_dbg(cdev->dev, "    %08x - %08x\n",
				cdev->seq_power_off[idx].addr,
				cdev->seq_power_off[idx].val);
		}
	}
	of_camera_put_pwrseq(cdev->dev, pwr_seq);
	return 0;
}

static struct regmap_config regmap_cfg_default = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

static int virtual_device_sanity_check(
	struct device *dev, struct virtual_device *pvd, int *len)
{
	u32 num;
	u8 *nptr;
	int n;

	dev_dbg(dev, "%s: %s, bus type %d, addr bits %d, val bits %d\n",
		__func__, pvd->name, pvd->bus_type,
		pvd->regmap_cfg.addr_bits, pvd->regmap_cfg.val_bits);
	dev_dbg(dev, "gpios %d, regs %d, clks %d\n",
		pvd->gpio_num, pvd->reg_num, pvd->clk_num);
	if (pvd->name[0] == '\0') {
		dev_err(dev, "%s need a device name!\n", __func__);
		return -ENODEV;
	}

	if (pvd->bus_type != CAMERA_DEVICE_TYPE_I2C) {
		dev_err(dev, "%s unsupported device type %d!\n",
		__func__, pvd->bus_type);
		return -ENODEV;
	}

	if (pvd->regmap_cfg.addr_bits != 0 &&
		pvd->regmap_cfg.addr_bits != 8 &&
		pvd->regmap_cfg.addr_bits != 16) {
		dev_err(dev, "%s unsupported address bits %d!\n",
		__func__, pvd->regmap_cfg.addr_bits);
		return -ENODEV;
	}

	if (pvd->regmap_cfg.val_bits != 8 &&
		pvd->regmap_cfg.val_bits != 16) {
		dev_err(dev, "%s unsupported data bits %d!\n",
		__func__, pvd->regmap_cfg.val_bits);
		return -ENODEV;
	}

	if (pvd->regmap_cfg.cache_type != REGCACHE_NONE &&
		pvd->regmap_cfg.cache_type != REGCACHE_RBTREE &&
		pvd->regmap_cfg.cache_type != REGCACHE_COMPRESSED) {
		dev_err(dev, "%s unsupported cache type %d!\n",
		__func__, pvd->regmap_cfg.cache_type);
		return -ENODEV;
	}

	if (pvd->gpio_num >= ARCH_NR_GPIOS) {
		dev_err(dev, "%s too many gpios %d!\n",
		__func__, pvd->gpio_num);
		return -ENODEV;
	}

	if (pvd->gpio_num >= 6) {
		dev_notice(dev, "%s WHAT?! Are you sure you need %d gpios?\n",
			__func__, pvd->gpio_num);
	}

	if (pvd->clk_num >= 5) {
		dev_notice(dev, "%s WHAT?! Are you sure you need %d clocks?\n",
			__func__, pvd->clk_num);
	}

	*len = 0;
	num = pvd->reg_num;
	nptr = &pvd->reg_names[0];
	while (num) {
		n = strlen(nptr);
		if (!n) {
			dev_err(dev, "%s NULL reg name @ %d\n",
				__func__, pvd->reg_num - num);
			return -ENODEV;
		}
		*len += n + 1;
		nptr += CAMERA_MAX_NAME_LENGTH;
		num--;
	}
	dev_dbg(dev, "regulator name size: %d\n", *len);

	if (pvd->pwr_on_size > VIRTUAL_DEV_MAX_POWER_SIZE) {
		dev_err(dev, "%s power on function size too big %d!\n",
		__func__, pvd->pwr_on_size);
		return -ENODEV;
	}

	if (pvd->pwr_off_size > VIRTUAL_DEV_MAX_POWER_SIZE) {
		dev_err(dev, "%s power off function size too big %d!\n",
		__func__, pvd->pwr_off_size);
		return -ENODEV;
	}

	return 0;
}

static int virtual_chip_config(
	struct device *dev,
	struct virtual_device *pvd,
	struct chip_config *c_info)
{
	dev_dbg(dev, "%s regulators:\n", __func__);
	c_info->clk_num = pvd->clk_num;
	c_info->gpio_num = pvd->gpio_num;

	return 0;
}

void *virtual_chip_add(struct device *dev, struct virtual_device *pvd)
{
	struct camera_chip *v_chip;
	struct chip_config *c_info;
	struct regmap_config *p_regmap;
	int buf_len;
	int err = 0;

	dev_info(dev, "%s\n", __func__);

	if (pvd == NULL) {
		dev_err(dev, "%s EMPTY virtual device.\n", __func__);
		return ERR_PTR(-EFAULT);
	}

	err = virtual_device_sanity_check(dev, pvd, &buf_len);
	if (err)
		return ERR_PTR(err);

	buf_len += sizeof(char *) * pvd->reg_num +
		sizeof(struct camera_reg) * pvd->pwr_on_size +
		sizeof(struct camera_reg) * pvd->pwr_off_size;
	v_chip = kzalloc(
		sizeof(*v_chip) + sizeof(*c_info) + buf_len, GFP_KERNEL);
	if (!v_chip) {
		dev_err(dev, "%s unable to allocate memory!\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	c_info = (void *)v_chip + sizeof(*v_chip);
	err = virtual_chip_config(dev, pvd, c_info);
	if (err) {
		kfree(v_chip);
		return ERR_PTR(err);
	}

	strncpy((u8 *)v_chip->name, (u8 const *)pvd->name,
		sizeof(v_chip->name));

	p_regmap = (struct regmap_config *)&v_chip->regmap_cfg;
	memcpy(p_regmap, &regmap_cfg_default, sizeof(*p_regmap));
	v_chip->type = pvd->bus_type;
	if (pvd->regmap_cfg.addr_bits)
		p_regmap->reg_bits = pvd->regmap_cfg.addr_bits;
	if (pvd->regmap_cfg.val_bits)
		p_regmap->val_bits = pvd->regmap_cfg.val_bits;
	p_regmap->cache_type = pvd->regmap_cfg.cache_type;

	INIT_LIST_HEAD(&v_chip->list);
	v_chip->private = c_info;
	v_chip->init = virtual_instance_create;
	v_chip->release = virtual_instance_destroy,
	v_chip->power_on = virtual_power_on,
	v_chip->power_off = virtual_power_off,
	v_chip->shutdown = virtual_shutdown,
	v_chip->update = virtual_update,

	err = camera_chip_add(v_chip);
	if (err) {
		kfree(v_chip);
		if (err != -EEXIST)
			return ERR_PTR(err);
		return NULL;
	}

	return v_chip;
}

static int __init virtual_init(void)
{
	pr_info("%s\n", __func__);
	return 0;
}
device_initcall(virtual_init);

static void __exit virtual_exit(void)
{
}
module_exit(virtual_exit);

MODULE_DESCRIPTION("virtual sensor device");
MODULE_AUTHOR("Charlie Huang <chahuang@nvidia.com>");
MODULE_LICENSE("GPL v2");
