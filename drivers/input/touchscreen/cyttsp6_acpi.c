/* SPDX-License-Identifier: GPL-2.0
 *
 * Support for loading platform data from ACPI in the cyttsp6 driver.
 * For use with Cypress touchscreen controllers.
 * Supported parts include:
 * CY8CTMA46X
 * CY8CTMA1036/768
 * CYTMA445
 *
 * Copyright (C) 2019 Tesla Motors, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "cyttsp6_regs.h"
#include <linux/device.h>
#include <linux/property.h>
#include <linux/cyttsp6_platform.h>
#include <linux/acpi.h>

/* From drivers/input/touchscreen/cyttsp6_devtree.c */
static const char * const touch_setting_names[CY_IC_GRPNUM_NUM] = {
	NULL,			/* CY_IC_GRPNUM_RESERVED */
	"cy,cmd_regs",		/* CY_IC_GRPNUM_CMD_REGS */
	"cy,tch_rep",		/* CY_IC_GRPNUM_TCH_REP */
	"cy,data_rec",		/* CY_IC_GRPNUM_DATA_REC */
	"cy,test_rec",		/* CY_IC_GRPNUM_TEST_REC */
	"cy,pcfg_rec",		/* CY_IC_GRPNUM_PCFG_REC */
	"cy,tch_parm_val",	/* CY_IC_GRPNUM_TCH_PARM_VAL */
	"cy,tch_parm_size",	/* CY_IC_GRPNUM_TCH_PARM_SIZE */
	NULL,			/* CY_IC_GRPNUM_RESERVED1 */
	NULL,			/* CY_IC_GRPNUM_RESERVED2 */
	"cy,opcfg_rec",		/* CY_IC_GRPNUM_OPCFG_REC */
	"cy,ddata_rec",		/* CY_IC_GRPNUM_DDATA_REC */
	"cy,mdata_rec",		/* CY_IC_GRPNUM_MDATA_REC */
	"cy,test_regs",		/* CY_IC_GRPNUM_TEST_REGS */
	"cy,btn_keys",		/* CY_IC_GRPNUM_BTN_KEYS */
	NULL,			/* CY_IC_GRPNUM_TTHE_REGS */
};

static inline int read_property_u32(struct device *dev,
				    struct fwnode_handle *fwnode,
				    const char *name, u32 *target)
{
	int err = fwnode_property_read_u32(fwnode, name, target);
	if (err)
		dev_dbg(dev, "%s can't find \"%s\" property. err=%i",
			  __func__, name, err);
	return err;
}

static inline int read_property_u8(struct device *dev,
				    struct fwnode_handle *fwnode,
				    const char *name, u8 *target)
{
	int err = fwnode_property_read_u8(fwnode, name, target);
	if (err)
		dev_dbg(dev, "%s can't find \"%s\" property. err=%i",
			  __func__, name, err);
	return err;
}

static inline int read_property_u16(struct device *dev,
				    struct fwnode_handle *fwnode,
				    const char *name, u16 *target)
{
	int err = fwnode_property_read_u16(fwnode, name, target);
	if (err)
		dev_dbg(dev, "%s can't find \"%s\" property. err=%i",
			  __func__, name, err);
	return err;
}

/* memory does not need allocating for target. This is done internally and
 * target is set to point to it
 */
static inline int read_property_string(struct device *dev,
				    struct fwnode_handle *fwnode,
				    const char *name, const char **target)
{
	int err = fwnode_property_read_string(fwnode, name, target);
	if (err)
		dev_dbg(dev, "%s can't find \"%s\" property. err=%i",
			  __func__, name, err);
	return err;
}

static inline int read_property_u16_array(struct device *dev,
					  struct fwnode_handle *fwnode,
					  const char *name, u16 *target,
					  size_t nval)
{
	int err = fwnode_property_read_u16_array(fwnode, name, target, nval);
	if (err)
		dev_dbg(dev, "%s can't find \"%s\" property. err=%i",
			  __func__, name, err);
	return err;
}

static inline struct fwnode_handle *find_child(struct device *dev,
					       const char *name)
{
	struct fwnode_handle *child = NULL;

	device_for_each_child_node(dev, child) {
		const char *working_name = NULL;

		if (fwnode_property_read_string(child, "name", &working_name))
			continue; /* no name for this child. Skip. */
		if (strcmp(name, working_name) == 0)
			return child;
	}

	return NULL;
}

static int add_suffix(const char *name, const char *suffix, char **out)
{
	char *res = NULL;
	const size_t size = strlen(name) + strlen(suffix) + 1;

	res = kmalloc(size, GFP_KERNEL);
	if (!res)
		return -ENOMEM;

	strcpy(res, name);
	strcat(res, suffix);

	*out = res;
	return 0;
}

#define NAME_TAG_SUFFIX "-tag"

static int get_touch_setting(struct device *dev, const char *name,
			     struct fwnode_handle *fwnode,
			     struct touch_settings **out_setting)
{
	struct touch_settings *setting = NULL;
	char *tag_name = NULL;
	int err = 0;

	setting = devm_kzalloc(dev, sizeof(*setting), GFP_KERNEL);
	if (!setting)
		return -ENOMEM;

	/* read size */
	err = read_property_u16_array(dev, fwnode, name, NULL, 0);
	if (err <= 0)
		return err;
	setting->size = err;

	/* read data */
	setting->data = devm_kmalloc_array(dev, setting->size, sizeof(u16),
					   GFP_KERNEL);
	if (!setting->data)
		return -ENOMEM;

	err = read_property_u16_array(dev, fwnode, name, (u16 *) setting->data,
				      setting->size);
	if (err < 0)
		return err;

	/* read tag */
	err = add_suffix(name, NAME_TAG_SUFFIX, &tag_name);
	if (err)
		return err;

	/* this is optional in the device-tree code */
	read_property_u8(dev, fwnode, tag_name, &setting->tag);
	kfree(tag_name);
	tag_name = NULL;

	*out_setting = setting;
	return 0;
}

static int get_touch_framework(struct device *dev, struct fwnode_handle *fwnode,
			       struct touch_framework **out_frmwrk)
{
	struct touch_framework *frmwrk = NULL;
	int err = 0;

	frmwrk = devm_kzalloc(dev, sizeof(*frmwrk), GFP_KERNEL);
	if (!frmwrk)
		return -ENOMEM;

	/* read in size */
	err = read_property_u16_array(dev, fwnode, "framework.abs", NULL, 0);
	if (err <= 0)
		return err;
	frmwrk->size = err;

	/* check for valid size */
	if (frmwrk->size % CY_NUM_ABS_SET)
		return -EINVAL;

	frmwrk->abs = devm_kmalloc_array(dev, frmwrk->size, sizeof(u16),
					 GFP_KERNEL);
	if (!frmwrk->abs)
		return -ENOMEM;

	/* required field */
	err = read_property_u16_array(dev, fwnode, "framework.abs",
				      (u16 *) frmwrk->abs, frmwrk->size);
	if (err < 0)
		return err;

	/* optional field */
	read_property_u8(dev, fwnode, "framework.enable_vkeys",
			       &frmwrk->enable_vkeys);

	*out_frmwrk = frmwrk;
	return 0;
}

static int get_core_pdata(struct device *dev,
			  struct cyttsp6_core_platform_data **out_core)
{
	struct cyttsp6_core_platform_data *core = NULL;
	struct fwnode_handle *child = NULL;
	int err = 0;
	int i = 0;

	core = devm_kzalloc(dev, sizeof(*core), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	child = find_child(dev, "cy,core");
	if (!child) {
		dev_err(dev, "%s DSDT has no \"cy,core\" device", __func__);
		return -ENODEV;
	}

	/* irq_gpio is required */
	err = read_property_u32(dev, child, "irq_gpio", &core->irq_gpio);
	if (err)
		return err;

	/* optional fields */
	read_property_u32(dev, child, "rst_gpio", &core->rst_gpio);
	read_property_u32(dev, child, "err_gpio", &core->err_gpio);
	read_property_u32(dev, child, "level_irq_udelay",
			  &core->level_irq_udelay);
	read_property_u32(dev, child, "max_xfer_len", &core->max_xfer_len);
	read_property_u32(dev, child, "flags", &core->flags);
	read_property_u8(dev, child, "easy_wakeup_gesture",
			  &core->easy_wakeup_gesture);

	for (i = 0; i < ARRAY_SIZE(touch_setting_names); ++i) {
		if (touch_setting_names[i] == NULL)
			continue;

		/* optional field */
		get_touch_setting(dev, touch_setting_names[i], child,
					&core->sett[i]);
		/* no need to NULL on error because kzalloc already did that */
	}

	core->xres = cyttsp6_xres;
	core->init = cyttsp6_init;
	core->power = cyttsp6_power;
	core->detect = cyttsp6_detect;
	core->irq_stat = cyttsp6_irq_stat;
	core->error_stat = cyttsp6_error_stat;

	*out_core = core;
	return 0;
}

static int get_mt_pdata(struct device *dev,
			struct cyttsp6_mt_platform_data **out_mt)
{
	struct cyttsp6_mt_platform_data *mt = NULL;
	struct fwnode_handle *child = NULL;
	int err = 0;

	/* allocate memory */
	mt = devm_kzalloc(dev, sizeof(*mt), GFP_KERNEL);
	if (!mt)
		return -ENOMEM;

	/* find child node */
	child = find_child(dev, "cy,mt");
	if (!child) {
		dev_err(dev, "%s DSDT has no \"cy,mt\" device", __func__);
		return -ENODEV;
	}

	/* required fields */
	err = read_property_string(dev, child, "inp_dev_name",
				   &mt->inp_dev_name);
	if (err)
		return err;

	err = get_touch_framework(dev, child, &mt->frmwrk);
	if (err)
		return err;

	/* optional fields */
	read_property_u16(dev, child, "flags", &mt->flags);
	read_property_u32(dev, child, "vkeys_x", &mt->vkeys_x);
	read_property_u32(dev, child, "vkeys_y", &mt->vkeys_y);

	*out_mt = mt;
	return 0;
}

static int get_btn_pdata(struct device *dev,
			 struct cyttsp6_btn_platform_data **out_btn)
{
	struct cyttsp6_btn_platform_data *btn = NULL;
	struct fwnode_handle *child = NULL;
	int err = 0;

	btn = devm_kzalloc(dev, sizeof(*btn), GFP_KERNEL);
	if (!btn)
		return -ENOMEM;

	child = find_child(dev, "cy,btn");
	if (!child) {
		dev_err(dev, "%s DSDT has no \"cy,btn\" device", __func__);
		return -ENODEV;
	}

	/* required field */
	err = read_property_string(dev, child, "inp_dev_name",
				   &btn->inp_dev_name);
	if (err)
		return err;

	*out_btn = btn;
	return 0;
}

static int get_prox_pdata(struct device *dev,
			  struct cyttsp6_proximity_platform_data **out_prox)
{
	struct cyttsp6_proximity_platform_data *prox = NULL;
	struct fwnode_handle *child = NULL;
	int err = 0;

	prox = devm_kzalloc(dev, sizeof(*prox), GFP_KERNEL);
	if (!prox)
		return -ENOMEM;

	child = find_child(dev, "cy,prox");
	if (!child) {
		dev_err(dev, "%s DSDT has no \"cy,prox\" device", __func__);
		return -ENODEV;
	}

	/* required fields */
	err = read_property_string(dev, child, "inp_dev_name",
				   &prox->inp_dev_name);
	if (err)
		return err;

	err = get_touch_framework(dev, child, &prox->frmwrk);
	if (err)
		return err;

	*out_prox = prox;
	return 0;
}

/**
 * cyttsp6_acpi_get_pdata - get platform data from ACPI
 * @dev: The device to search in for ACPI properties
 *
 * Platform data is saved to dev->platform_data.
 *
 * Return: 0 on success,
 *         %-ENOMEM if memory allocation fails.
 *	   %-EINVAL if given arguments are not valid,
 *	   %-ENODATA if a property does not have a value,
 *	   %-EPROTO or %-EILSEQ if a property has the wrong type,
 *	   %-EOVERFLOW if the size of a property is not as expected,
 *	   %-ENXIO if no suitable firmware interface is present,
 *         %-ENODEV if a child device is missing.
 */
int cyttsp6_acpi_get_pdata(struct device *dev)
{
	struct cyttsp6_platform_data *pdata = NULL;
	int err = 0;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	err = get_core_pdata(dev, &pdata->core_pdata);
	if (err)
		return err;

	err = get_mt_pdata(dev, &pdata->mt_pdata);
	if (err)
		return err;

	err = get_btn_pdata(dev, &pdata->btn_pdata);
	if (err)
		return err;

	err = get_prox_pdata(dev, &pdata->prox_pdata);
	if (err)
		return err;

	pdata->loader_pdata = &_cyttsp6_loader_platform_data;

	/* Required for Device Access Group 7 support */
	pdata->core_pdata->sett[CY_IC_GRPNUM_TCH_PARM_SIZE] =
		_cyttsp6_loader_platform_data.ttconfig->param_size;

	dev->platform_data = pdata;
	return 0;
}
