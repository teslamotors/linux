// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2014 - 2018 Intel Corporation
 *
 * Author: Vinod Govindapillai <vinod.govindapillai@intel.com>
 *
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <media/v4l2-async.h>
#include <media/v4l2-device.h>

#include <uapi/linux/media-bus-format.h>

#include "crlmodule.h"
#include "crlmodule-nvm.h"
#include "crlmodule-regs.h"
#include "crlmodule-msrlist.h"

#ifdef CONFIG_INTEL_IPU4_OV13858
bool vcm_in_use;
EXPORT_SYMBOL(vcm_in_use);
void crlmodule_vcm_gpio_set_value(unsigned int gpio, int value)
{
	gpio_set_value(gpio, value);
}
EXPORT_SYMBOL(crlmodule_vcm_gpio_set_value);
#endif

static void crlmodule_update_current_mode(struct crl_sensor *sensor);

static int __crlmodule_get_variable_ref(struct crl_sensor *sensor,
					enum crl_member_data_reference_ids ref,
					u32 *val)
{
	switch (ref) {
	case CRL_VAR_REF_OUTPUT_WIDTH:
		*val = sensor->src->crop[CRL_PAD_SRC].width;
		break;
	case CRL_VAR_REF_OUTPUT_HEIGHT:
		*val = sensor->src->crop[CRL_PAD_SRC].height;
		break;
	case CRL_VAR_REF_BITSPERPIXEL:
		*val = sensor->sensor_ds->csi_fmts[
				   sensor->fmt_index].bits_per_pixel;
		break;
	default:
		return -EINVAL;
	};

	return 0;
}

/*
 * Get the data format index from the configuration definition data
 */
static int __crlmodule_get_data_fmt_index(struct crl_sensor *sensor,
					  u32 code)
{
	unsigned int i;

	for (i = 0; i < sensor->sensor_ds->csi_fmts_items; i++) {
		if (sensor->sensor_ds->csi_fmts[i].code == code)
			return i;
	}

	return -EINVAL;
}

/*
 * Find the index of the v4l2 ctrl pointer from the array of v4l2 ctrls
 * maintained by the CRL module based on the ctrl id.
 */
static int __crlmodule_get_crl_ctrl_index(struct crl_sensor *sensor,
					  u32 id, unsigned int *index)
{
	unsigned int i;

	for (i = 0; i < sensor->sensor_ds->v4l2_ctrls_items; i++)
		if (sensor->v4l2_ctrl_bank[i].ctrl_id == id)
			break;

	if (i >= sensor->sensor_ds->v4l2_ctrls_items)
		return -EINVAL;

	*index = i;
	return 0;
}

/*
 * Finds the value of a specific v4l2 ctrl based on the ctrl-id
 */
static int __crlmodule_get_ctrl_value(struct crl_sensor *sensor,
				      u32 id, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	struct v4l2_ctrl *ctrl;
	unsigned int i;
	int ret;

	ret = __crlmodule_get_crl_ctrl_index(sensor, id, &i);
	if (ret)
		return ret;

	/* If no corresponding v4l2 ctrl created, return */
	if (!sensor->v4l2_ctrl_bank[i].ctrl) {
		dev_dbg(&client->dev,
			"%s ctrl_id: 0x%x desc: %s not ready\n", __func__, id,
			sensor->v4l2_ctrl_bank[i].name);
		return -ENODATA;
	}

	ctrl = sensor->v4l2_ctrl_bank[i].ctrl;
	switch (sensor->v4l2_ctrl_bank[i].type) {
	case CRL_V4L2_CTRL_TYPE_MENU_INT:
		*val = ctrl->qmenu_int[ctrl->val];
		break;
	case CRL_V4L2_CTRL_TYPE_INTEGER:
	default:
		*val = ctrl->val;
	}

	dev_dbg(&client->dev, "%s ctrl_id: 0x%x desc: %s val: %d\n",
			__func__, id,
			sensor->v4l2_ctrl_bank[i].name, *val);
	return 0;
}

/*
 * Finds the v4l2 ctrl based on the control id
 */
static struct v4l2_ctrl *__crlmodule_get_v4l2_ctrl(struct crl_sensor *sensor,
						u32 id)
{
	unsigned int i;

	if (__crlmodule_get_crl_ctrl_index(sensor, id, &i))
		return NULL;

	return sensor->v4l2_ctrl_bank[i].ctrl;
}

/*
 * Grab / Release controls based on the ctrl update context
 */
static void __crlmodule_grab_v4l2_ctrl(struct crl_sensor *sensor,
				enum crl_v4l2ctrl_update_context ctxt,
				bool action)
{
	struct crl_v4l2_ctrl *crl_ctrl;
	unsigned int i;

	for (i = 0; i < sensor->sensor_ds->v4l2_ctrls_items; i++) {
		crl_ctrl = &sensor->v4l2_ctrl_bank[i];

		if (crl_ctrl->context == ctxt)
			v4l2_ctrl_grab(crl_ctrl->ctrl, action);
	}
}

/*
 * Checks if the v4l2 ctrl sepecific data is satisfied in the mode and PLL
 * selection logic.
 */
static bool __crlmodule_compare_ctrl_specific_data(
			struct crl_sensor *sensor,
			unsigned int items,
			struct crl_ctrl_data_pair *ctrl_val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	unsigned int i;
	u32 val;
	int ret;

	/* Go through all the controls associated with this config */
	for (i = 0; i < items; i++) {
		/* Get the value set for the control */
		ret = __crlmodule_get_ctrl_value(sensor, ctrl_val[i].ctrl_id,
						 &val);
		if (ret) {
			dev_err(&client->dev, "%s ctrl_id: 0x%x not found\n",
				__func__, ctrl_val[i].ctrl_id);
			return false;
		}

		/* Compare the value from the sensor definition file config */
		if (val != ctrl_val[i].data) {
			dev_dbg(&client->dev,
				"%s ctrl_id: 0x%x value not match %d != %d\n",
				__func__, ctrl_val[i].ctrl_id, val,
				ctrl_val[i].data);
			return false;
		}
	}

	dev_dbg(&client->dev, "%s success\n",  __func__);
	return true;
}

/*
 * Finds the correct PLL settings index based on the parameters
 */
static int __crlmodule_update_pll_index(struct crl_sensor *sensor)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	const struct crl_pll_configuration *pll_config;
	const struct crl_csi_data_fmt *fmts =
		&sensor->sensor_ds->csi_fmts[sensor->fmt_index];
	u32 link_freq;
	unsigned int i;

	link_freq = sensor->link_freq->qmenu_int[sensor->link_freq->val];

	dev_dbg(&client->dev, "%s PLL Items: %d link_freq: %d\n", __func__,
			sensor->sensor_ds->pll_config_items, link_freq);

	for (i = 0; i < sensor->sensor_ds->pll_config_items; i++) {
		pll_config = &sensor->sensor_ds->pll_configs[i];

		if (pll_config->op_sys_clk != link_freq)
			continue;

		if (pll_config->input_clk != sensor->platform_data->ext_clk)
			continue;

		/* if pll_config->csi_lanes == 0, lanes do not matter */
		if (pll_config->csi_lanes)
			if (sensor->platform_data->lanes !=
					pll_config->csi_lanes)
				continue;

		/* PLL config must match to bpps*/
		if (fmts->bits_per_pixel != pll_config->bitsperpixel)
			continue;

		/* Check if there are any dynamic compare items */
		if (sensor->ext_ctrl_impacts_pll_selection &&
		    !__crlmodule_compare_ctrl_specific_data(sensor,
						pll_config->comp_items,
						pll_config->ctrl_data))
			continue;

		/* Found PLL index */
		dev_dbg(&client->dev, "%s Found PLL index: %d for freq: %d\n",
				__func__, i, link_freq);

		sensor->pll_index = i;

		/* Update the control values for pixelrate_pa and csi */
		__v4l2_ctrl_s_ctrl_int64(sensor->pixel_rate_pa,
				pll_config->pixel_rate_pa);
		__v4l2_ctrl_s_ctrl_int64(sensor->pixel_rate_csi,
				pll_config->pixel_rate_csi);
		return 0;
	}

	dev_err(&client->dev, "%s no configuration found for freq: %d\n",
			__func__, link_freq);
	return -EINVAL;
}

/*
 * Perform the action for the dependency control
 */
static void __crlmodule_dep_ctrl_perform_action(
					struct crl_sensor *sensor,
					struct crl_dep_ctrl_provision *prov,
					u32 *val, u32 *dep_val)
{
	enum crl_dep_ctrl_condition cond;
	unsigned int i;
	u32 temp;

	if (*val > *dep_val)
		cond = CRL_DEP_CTRL_CONDITION_GREATER;
	else if (*val < *dep_val)
		cond = CRL_DEP_CTRL_CONDITION_LESSER;
	else
		cond = CRL_DEP_CTRL_CONDITION_EQUAL;

	for (i = 0; i < prov->action_items; i++) {
		if (prov->action[i].cond == cond)
			break;
	}

	/* No handler found-. Return completed */
	if (i >= prov->action_items)
		return;

	/* if this is dependency control, switch val and dep val */
	if (prov->action_type == CRL_DEP_CTRL_ACTION_TYPE_DEP_CTRL) {
		temp = *val;
		*val = *dep_val;
		*dep_val = temp;
	}

	switch (prov->action[i].action) {
	case CRL_DEP_CTRL_CONDITION_ADD:
		*val = *dep_val + prov->action[i].action_value;
		break;
	case CRL_DEP_CTRL_CONDITION_SUBTRACT:
		*val = *dep_val - prov->action[i].action_value;
		break;
	case CRL_DEP_CTRL_CONDITION_MULTIPLY:
		*val = *dep_val * prov->action[i].action_value;
		break;
	case CRL_DEP_CTRL_CONDITION_DIVIDE:
		*val = *dep_val / prov->action[i].action_value;
		break;
	}

	/* if this is dependency control, switch val and dep val back*/
	if (prov->action_type == CRL_DEP_CTRL_ACTION_TYPE_DEP_CTRL) {
		temp = *val;
		*val = *dep_val;
		*dep_val = temp;
	}

	return;
}

/*
 * Parse the dynamic entity based on the Operand type
 */
static int __crlmodule_parse_dynamic_entity(struct crl_sensor *sensor,
					struct crl_dynamic_entity entity,
					u32 *val)
{
	switch (entity.entity_type) {
	case CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST:
		*val = entity.entity_val;
		return 0;
	case CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF:
		return __crlmodule_get_variable_ref(sensor,
						entity.entity_val, val);
	case CRL_DYNAMIC_VAL_OPERAND_TYPE_CTRL_VAL:
		return __crlmodule_get_ctrl_value(sensor,
						entity.entity_val, val);
	case CRL_DYNAMIC_VAL_OPERAND_TYPE_REG_VAL: {
		struct crl_register_read_rep reg;

		/* Note: Only 8bit registers are supported. */
		reg.address = entity.entity_val;
		reg.len = CRL_REG_LEN_08BIT;
		reg.mask = 0xff;
		reg.dev_i2c_addr = CRL_I2C_ADDRESS_NO_OVERRIDE;
		return crlmodule_read_reg(sensor, reg, val);
	}
	default:
		break;
	};

	return -EINVAL;
}

static int __crlmodule_calc_dynamic_entity_values(
					struct crl_sensor *sensor,
					unsigned int ops_items,
					struct crl_arithmetic_ops *ops_arr,
					unsigned int *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	unsigned int i;

	/* perform the bitwise operation on val one by one */
	for (i = 0; i < ops_items; i++) {
		struct crl_arithmetic_ops *ops = &ops_arr[i];
		u32 operand;
		int ret = __crlmodule_parse_dynamic_entity(sensor, ops->operand,
							&operand);
		if (ret) {
			dev_dbg(&client->dev,
				"%s failed to parse dynamic entity: %d %d\n",
				__func__, ops->operand.entity_type,
				ops->operand.entity_val);
			return ret;
		}

		switch (ops->op) {
		case CRL_BITWISE_AND:
			*val &= operand;
			break;
		case CRL_BITWISE_OR:
			*val |= operand;
			break;
		case CRL_BITWISE_LSHIFT:
			*val <<= operand;
			break;
		case CRL_BITWISE_RSHIFT:
			*val >>= operand;
			break;
		case CRL_BITWISE_XOR:
			*val ^= operand;
			break;
		case CRL_BITWISE_COMPLEMENT:
			*val = ~(*val);
			break;
		case CRL_ADD:
			*val += operand;
			break;
		case CRL_SUBTRACT:
			*val = *val > operand ? *val - operand : operand - *val;
			break;
		case CRL_MULTIPLY:
			*val *= operand;
			break;
		case CRL_DIV:
			*val /= operand;
			break;
		case CRL_ASSIGNMENT:
			*val = operand;
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

/*
 * Dynamic registers' value is not direct but depends on a referrence value.
 * This kind of registers are mainly used in crlmodule's v4l2 ctrl logic.
 *
 * This is to handle cases like the below examples, where mutliple registers
 * need to be modified based on the input value "val"
 * R3000 = val & 0xff and R3001 = val >> 8 & 0xff and R3002 = val >> 16 & 0xff
 * R4001 = val and R4002 = val or
 * R2800 = FLL - val and R2802 = LLP - val
 */
static int __crlmodule_parse_and_write_dynamic_reg(struct crl_sensor *sensor,
					struct crl_dynamic_register_access *reg,
					unsigned int val)
{
	int ret;

	/*
	 * Get the value associated with the dynamic entity. "val" might
	 * change after this call based on the arithmetic operations added for
	 * this group
	 */
	ret = __crlmodule_calc_dynamic_entity_values(sensor, reg->ops_items,
							reg->ops, &val);
	if (ret)
		return ret;

	/* Now ready to write the value */
	return crlmodule_write_reg(sensor, reg->dev_i2c_addr, reg->address,
					reg->len, reg->mask, val);
}

static int __crlmodule_update_dynamic_regs(struct crl_sensor *sensor,
					struct crl_v4l2_ctrl *crl_ctrl,
					unsigned int val)
{
	unsigned int i;
	int ret;

	for (i = 0; i < crl_ctrl->regs_items; i++) {
		/*
		* Each register group must start from the initial value, not
		* as a continuation of the previous calculations. The sensor
		* configurations must take care of this restriction.
		*/
		ret = __crlmodule_parse_and_write_dynamic_reg(sensor,
						&crl_ctrl->regs[i], val);
		if (ret)
			return ret;
	}

	return 0;
}

/*
 * Perform the action for the dependent register lists
 */
static int __crlmodule_handle_dependency_regs(
			struct crl_sensor *sensor,
			struct crl_v4l2_ctrl *crl_ctrl,
			unsigned int val)
{
	unsigned int i;
	int ret;

	for (i = 0; i < crl_ctrl->crl_ctrl_dep_reg_list; i++) {
		struct crl_dep_reg_list *list = &crl_ctrl->dep_regs[i];
		enum crl_dep_ctrl_condition condition;
		unsigned int j;
		u32 dep_val;

		/* Parse the condition value */
		ret = __crlmodule_parse_dynamic_entity(sensor, list->cond_value,
							&dep_val);
		if (ret)
			return ret;

		/* Get the kind of condition for this value */
		if (val > dep_val)
			condition = CRL_DEP_CTRL_CONDITION_GREATER;
		else if (val < dep_val)
			condition = CRL_DEP_CTRL_CONDITION_LESSER;
		else
			condition = CRL_DEP_CTRL_CONDITION_EQUAL;

		/*
		 * Compare the register list specific condition and if matching
		 * write the corresponding register lists to the sensor.
		 */
		if (condition == list->reg_cond) {
			/* Handle the direct registers if any */
			if (list->no_direct_regs && list->direct_regs) {
				ret = crlmodule_write_regs(sensor,
				       list->direct_regs, list->no_direct_regs);
				if (ret)
					return ret;
			}

			/* Handle the dynamic registers if any */
			for (j = 0; j < list->no_dyn_items; j++) {
				ret = __crlmodule_parse_and_write_dynamic_reg(
					sensor, &list->dyn_regs[j], val);
				if (ret)
					return ret;
			}
			break;
		}
	}

	return 0;
}

/*
 * Handles the dependency control actions. Dependency control is a control
 * which' value depends on the current control. This information is encoded in
 * the sensor configuration file.
 */
static int __crlmodule_handle_dependency_ctrl(
					   struct crl_sensor *sensor,
					   struct crl_v4l2_ctrl *crl_ctrl,
					   unsigned int *val,
					   enum crl_dep_ctrl_action_type type)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	struct crl_v4l2_ctrl *dep_crl_ctrl;
	struct crl_dep_ctrl_provision *dep_prov;
	unsigned int i, idx;
	u32 dep_val;
	int ret;

	dev_dbg(&client->dev, "%s ctrl_id: 0x%x dependency controls: %d\n",
			       __func__, crl_ctrl->ctrl_id,
			       crl_ctrl->dep_items);

	for (i = 0; i < crl_ctrl->dep_items; i++) {
		dep_prov = &crl_ctrl->dep_ctrls[i];

		/* If not the type, continue */
		if (dep_prov->action_type != type)
			continue;

		/* Get the value from the dependency ctrl */
		ret = __crlmodule_get_ctrl_value(sensor, dep_prov->ctrl_id,
						 &dep_val);
		if (ret) {
			dev_err(&client->dev, "%s ctrl_id: 0x%x not found\n",
					       __func__, dep_prov->ctrl_id);
			/* TODO! Shoud continue? */
			continue;
		}

		/* Perform the action */
		__crlmodule_dep_ctrl_perform_action(sensor, dep_prov, val,
						    &dep_val);

		/* if this is dependency control, update the register */
		if (dep_prov->action_type ==
					CRL_DEP_CTRL_ACTION_TYPE_DEP_CTRL) {
			ret = __crlmodule_get_crl_ctrl_index(sensor,
						dep_prov->ctrl_id, &idx);
			if (ret)
				continue;

			dep_crl_ctrl = &sensor->v4l2_ctrl_bank[idx];

			/* Update the dynamic registers for the dep control */
			ret = __crlmodule_update_dynamic_regs(sensor,
							dep_crl_ctrl, dep_val);
			if (ret)
				dev_info(&client->dev,
					"%s dynamic reg update failed for %s\n",
					__func__, dep_crl_ctrl->name);

			/* Handle dependened register lists for dep control */
			ret = __crlmodule_handle_dependency_regs(sensor,
							dep_crl_ctrl, dep_val);
			if (ret)
				dev_info(&client->dev,
					"%s handle dep regs failed for %s\n",
					__func__, dep_crl_ctrl->name);
		}
	}

	return 0;
}

static int crlmodule_get_fmt_index(struct crl_sensor *sensor,
				   u8 pixel_order, u8 bpp)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	const struct crl_csi_data_fmt *f;
	int i;

	/*
	 * Go through the fmt list and check if this format with matching bpp
	 * is supported by this module definition file
	 */
	for (i = 0; i < sensor->sensor_ds->csi_fmts_items; i++) {
		f = &sensor->sensor_ds->csi_fmts[i];

		if (f->pixel_order == pixel_order && f->bits_per_pixel == bpp)
			return i;
	}

	dev_err(&client->dev, "%s no supported format for order: %d bpp: %d\n",
			      __func__, pixel_order, bpp);

	return -EINVAL;
}

static int __crlmodule_update_flip_info(struct crl_sensor *sensor,
					struct crl_v4l2_ctrl *crl_ctrl,
					struct v4l2_ctrl *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	const struct crl_csi_data_fmt *fmt =
		&sensor->sensor_ds->csi_fmts[sensor->fmt_index];
	u8 bpp = fmt->bits_per_pixel;
	u8 flip_info = sensor->flip_info;
	u8 new_order;
	int i, ret;

	dev_dbg(&client->dev, "%s current flip_info: %d curr index: %d\n",
			       __func__, flip_info, sensor->fmt_index);

	switch (ctrl->id) {
	case V4L2_CID_HFLIP:
		flip_info &= CRL_FLIP_HFLIP_MASK;
		flip_info |= ctrl->val > 0 ? CRL_FLIP_HFLIP : 0;
		break;
	case V4L2_CID_VFLIP:
		flip_info &= CRL_FLIP_VFLIP_MASK;
		flip_info |= ctrl->val > 0 ? CRL_FLIP_VFLIP : 0;
		break;
	}

	dev_dbg(&client->dev, "%s flip success new flip_info: %d\n",
			       __func__, flip_info);

	/* First check if the module actually supports any pixelorder changes */
	for (i = 0; i < sensor->sensor_ds->flip_items; i++) {
		if (flip_info == sensor->sensor_ds->flip_data[i].flip) {
			new_order = sensor->sensor_ds->flip_data[i].pixel_order;
			break;
		}
	}

	if (i >= sensor->sensor_ds->flip_items) {
		dev_err(&client->dev, "%s flip not supported %d\n",
				      __func__, flip_info);
		return -EINVAL;
	}

	/* Skip format re-selection if pixel order is unrelated to flipping. */
	if (new_order == CRL_PIXEL_ORDER_IGNORE)
		return 0;

	/*
	 * Flip changes only pixel order. So check if the supported format list
	 * has any format with new pixel order and current bits per pixel
	 */
	i = crlmodule_get_fmt_index(sensor, new_order, bpp);
	if (i < 0) {
		dev_err(&client->dev, "%s no format found order: %d bpp: %d\n",
				      __func__, new_order, bpp);
		return -EINVAL;
	}

	ret = __crlmodule_update_dynamic_regs(sensor, crl_ctrl, ctrl->val);
	if (ret) {
		dev_err(&client->dev, "%s register access failed\n", __func__);
		return ret;
	}

	/* New format found. Update info */
	sensor->fmt_index = i;
	sensor->flip_info = flip_info;

	dev_dbg(&client->dev, "%s flip success flip: %d new fmt index: %d\n",
			      __func__, flip_info, i);

	return 0;
}
static int __crlmodule_update_framesize(struct crl_sensor *sensor,
					struct crl_v4l2_ctrl *crl_ctrl,
					struct v4l2_ctrl *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	const struct crl_mode_rep *mode = sensor->current_mode;
	unsigned int val;
	int ret;

	switch (ctrl->id) {
	case V4L2_CID_FRAME_LENGTH_LINES:
		val = max(ctrl->val, mode->min_fll);
		break;
	case V4L2_CID_LINE_LENGTH_PIXELS:
		val = max(ctrl->val, mode->min_llp);
		break;
	default:
		return -EINVAL;
	}

	ret = __crlmodule_update_dynamic_regs(sensor, crl_ctrl, val);
	if (ret)
		return ret;

	ctrl->val = val;
	ctrl->cur.val = val;
	dev_dbg(&client->dev, "%s: set v4l2 id:0x%0x value %d\n",
				__func__, ctrl->id, val);

	return 0;
}
static int __crlmodule_update_blanking(struct crl_sensor *sensor,
					struct crl_v4l2_ctrl *crl_ctrl,
					struct v4l2_ctrl *ctrl)
{
	unsigned int val;

	switch (ctrl->id) {
	case V4L2_CID_HBLANK:
		val = sensor->pixel_array->crop[CRL_PA_PAD_SRC].width +
		      ctrl->val;
		break;
	case V4L2_CID_VBLANK:
		val = sensor->pixel_array->crop[CRL_PA_PAD_SRC].height +
		      ctrl->val;
		break;
	default:
		return -EINVAL;
	}

	return __crlmodule_update_dynamic_regs(sensor, crl_ctrl, val);
}

static void __crlmodule_update_selection_impact_flags(
				struct crl_sensor *sensor,
				struct crl_v4l2_ctrl *crl_ctrl)
{
	if (crl_ctrl->impact & CRL_IMPACTS_PLL_SELECTION)
		sensor->ext_ctrl_impacts_pll_selection = true;

	if (crl_ctrl->impact & CRL_IMPACTS_MODE_SELECTION)
		sensor->ext_ctrl_impacts_mode_selection = true;
}

static struct crl_v4l2_ctrl *__crlmodule_find_crlctrl(
						struct crl_sensor *sensor,
						struct v4l2_ctrl *ctrl)
{
	struct crl_v4l2_ctrl *crl_ctrl;
	unsigned int i;

	for (i = 0; i < sensor->sensor_ds->v4l2_ctrls_items; i++) {
		crl_ctrl = &sensor->v4l2_ctrl_bank[i];
		if (crl_ctrl->ctrl == ctrl)
			return crl_ctrl;
	}

	return NULL;
}

static int crlmodule_reset_crlctrl_value(struct crl_sensor *sensor,
					 unsigned int new_mode)
{
	struct crl_v4l2_ctrl *crl_ctrl;
	const struct crl_mode_rep *this;
	unsigned int i;

	if (!sensor->v4l2_ctrl_bank)
		return -EINVAL;

	this = &sensor->sensor_ds->modes[new_mode];

	for (i = 0; i < sensor->sensor_ds->v4l2_ctrls_items; i++) {
		crl_ctrl = &sensor->v4l2_ctrl_bank[i];

		switch (crl_ctrl->ctrl_id) {
		case V4L2_CID_FRAME_LENGTH_LINES:
			if (crl_ctrl->ctrl) {
				crl_ctrl->ctrl->val = this->min_fll;
				crl_ctrl->ctrl->cur.val = this->min_fll;
			}
			break;
		case V4L2_CID_LINE_LENGTH_PIXELS:
			if (crl_ctrl->ctrl) {
				crl_ctrl->ctrl->val = this->min_llp;
				crl_ctrl->ctrl->cur.val = this->min_llp;
			}
			break;
		}
	}

	return 0;
}

static int crlmodule_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct crl_sensor *sensor = container_of(ctrl->handler,
			   struct crl_subdev, ctrl_handler)->sensor;
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	struct crl_v4l2_ctrl *crl_ctrl = NULL;
	int ret = 0;

	dev_dbg(&client->dev, "%s id:0x%0x val:%d\n", __func__, ctrl->id,
			      ctrl->val);

	/*
	 * Need to find the corresponding crlmodule wrapper for this v4l2_ctrl.
	 * This is needed because all the register information is associated
	 * with the crlmodule's wrapper v4l2ctrl.
	 */
	crl_ctrl = __crlmodule_find_crlctrl(sensor, ctrl);
	if (!crl_ctrl) {
		dev_err(&client->dev, "%s ctrl :0x%x not supported\n",
				      __func__, ctrl->id);
		return -EINVAL;
	}

	dev_dbg(&client->dev, "%s id:0x%x name:%s\n", __func__, ctrl->id,
			      crl_ctrl->name);

	/* Then go through the mandatory controls */
	switch (ctrl->id) {
	case V4L2_CID_LINK_FREQ:
		/* Go through the supported list and compare the values */
		ret = __crlmodule_update_pll_index(sensor);
		goto out;
	};

	/* update the selection impacts flags */
	__crlmodule_update_selection_impact_flags(sensor, crl_ctrl);

	/*
	 * Dependency control is a control whose value is affected by the value
	 * for the current control. For example, vblank can be a dependency
	 * control for exposure. Whenever exposure changes, the sensor can
	 * automatically adjust the vblank or rely on manual adjustment. In
	 * case of manual adjustment the sensor configuration file needs to
	 * specify the dependency control, the condition for an action and
	 * typs of action.
	 *
	 * Now check if there is any dependency controls for this. And if there
	 * are any we need to split the action to two. First if the current
	 * control needs to be changed, then do it before updating the register.
	 * If some other control is affected, then do it after wrriting the
	 * current values
	 *
	 * Now check in the dependency control list, if the action type is
	 * "self" and update the value accordingly now
	 */
	__crlmodule_handle_dependency_ctrl(sensor, crl_ctrl, &ctrl->val,
				     CRL_DEP_CTRL_ACTION_TYPE_SELF);

	/* Handle specific controls */
	switch (ctrl->id) {
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
		ret = __crlmodule_update_flip_info(sensor, crl_ctrl, ctrl);
		goto out;

	case V4L2_CID_VBLANK:
	case V4L2_CID_HBLANK:
		if (sensor->blanking_ctrl_not_use) {
			dev_info(&client->dev, "%s Blanking controls are \
			not used in this configuration, setting them has \
			no effect\n", __func__);
			/* Disable control*/
			v4l2_ctrl_activate(ctrl, false);

		} else {
			ret = __crlmodule_update_blanking(sensor,
					crl_ctrl, ctrl);
		}
		goto out;

	case V4L2_CID_FRAME_LENGTH_LINES:
	case V4L2_CID_LINE_LENGTH_PIXELS:
		ret = __crlmodule_update_framesize(sensor, crl_ctrl, ctrl);
		goto out;

	case CRL_CID_SENSOR_MODE:
		/*
		 * If sensor mode is changed, some v4l2 ctrls value need
		 * to be reset to default value, or else the value set in
		 * previous mode will influence the setting in the current
		 * mode. Especially for llp and fll.
		 */
		if (sensor->sensor_mode != ctrl->val)
			crlmodule_reset_crlctrl_value(sensor, ctrl->val);

		sensor->sensor_mode = ctrl->val;
		crlmodule_update_current_mode(sensor);
		goto out;
	}

	ret = __crlmodule_update_dynamic_regs(sensor, crl_ctrl, ctrl->val);
	if (ret)
		goto out;

	ret = __crlmodule_handle_dependency_regs(sensor, crl_ctrl,
						ctrl->val);

out:
	/*
	 * Now check in the dependency control list, if the action type is
	 * "dependency control" and update the value accordingly now
	 */
	if (!ret && crl_ctrl)
		__crlmodule_handle_dependency_ctrl(sensor, crl_ctrl, &ctrl->val,
					     CRL_DEP_CTRL_ACTION_TYPE_DEP_CTRL);
	return ret;
}

static int crlmodule_get_ctrl(struct v4l2_ctrl *ctrl)
{
	struct crl_sensor *sensor = container_of(ctrl->handler,
			   struct crl_subdev, ctrl_handler)->sensor;
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	struct crl_v4l2_ctrl *crl_ctrl;
	struct crl_dynamic_register_access *reg;

	/*
	 * Need to find the corresponding crlmodule wrapper for this v4l2_ctrl.
	 * This is needed because all the register information is associated
	 * with the crlmodule's wrapper v4l2ctrl.
	 */
	crl_ctrl = __crlmodule_find_crlctrl(sensor, ctrl);
	if (!crl_ctrl) {
		dev_err(&client->dev, "%s ctrl :0x%x not supported\n",
				      __func__, ctrl->id);
		return -EINVAL;
	}

	dev_dbg(&client->dev, "%s id:0x%x name:%s\n", __func__, ctrl->id,
			      crl_ctrl->name);

	/* cannot handle if the V4L2_CTRL_FLAG_READ_ONLY flag is not set */
	if (!(ctrl->flags & V4L2_CTRL_FLAG_READ_ONLY)) {
		dev_err(&client->dev, "%s Control id:0x%x is not read only\n",
				      __func__, ctrl->id);
		return -EINVAL;
	}

	/*
	 * Found the crl control wrapper. Use the dynamic entity information
	 * to calculate the value for this control. For get control, there
	 * could be only one item in the crl_dynamic_register_access. ctrl->
	 * regs_items must be 1. Also the crl_dynamic_register_access.address
	 * and crl_dynamic_register_access.len are not used.
	 * Instead the values to be found or calculated need to be encoded into
	 * crl_dynamic_register_access.crl_arithmetic_ops. It has possibility
	 * to read from registers, existing control values and simple arithmetic
	 * operations etc.
	 */
	if (crl_ctrl->regs_items > 1)
		dev_warn(&client->dev,
			 "%s multiple dynamic entities, will skip the rest\n",
			 __func__);
	reg = &crl_ctrl->regs[0];

	/* Get the value associated with the dynamic entity */
	return  __crlmodule_calc_dynamic_entity_values(sensor, reg->ops_items,
						       reg->ops, &ctrl->val);
}

static const struct v4l2_ctrl_ops crlmodule_ctrl_ops = {
	.s_ctrl = crlmodule_set_ctrl,
	.g_volatile_ctrl = crlmodule_get_ctrl,
};

static struct v4l2_ctrl_handler *__crlmodule_get_sd_ctrl_handler(
					struct crl_sensor *sensor,
					enum crl_subdev_type sd_type)
{
	switch (sd_type) {
	case CRL_SUBDEV_TYPE_SCALER:
	case CRL_SUBDEV_TYPE_BINNER:
		return &sensor->src->ctrl_handler;

	case CRL_SUBDEV_TYPE_PIXEL_ARRAY:
		if (sensor->pixel_array)
			return &sensor->pixel_array->ctrl_handler;
		break;
	};

	return NULL;
}

static int __crlmodule_init_link_freq_ctrl_menu(
					struct crl_sensor *sensor,
					struct crl_v4l2_ctrl *crl_ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	unsigned int items = 0;
	unsigned int i;

	/* Cannot handle if the control type is not integer menu */
	if (crl_ctrl->type != CRL_V4L2_CTRL_TYPE_MENU_INT)
		return 0;

	/* If the menu contents exist, skip filling it dynamically */
	if (crl_ctrl->data.v4l2_int_menu.menu)
		return 0;

	sensor->link_freq_menu = devm_kzalloc(&client->dev, sizeof(s64) *
					 sensor->sensor_ds->pll_config_items,
					 GFP_KERNEL);
	if (!sensor->link_freq_menu)
		return -ENOMEM;

	for (i = 0; i < sensor->sensor_ds->pll_config_items; i++) {
		bool dup = false;
		unsigned int j;

		/*
		 * Skip the duplicate entries. We are using the value to match
		 * not the index
		 */
		for (j = 0; j < items && !dup; j++)
			dup = (sensor->link_freq_menu[j] ==
			       sensor->sensor_ds->pll_configs[i].op_sys_clk);
		if (dup)
			continue;

		sensor->link_freq_menu[items] =
				   sensor->sensor_ds->pll_configs[i].op_sys_clk;
		items++;
	}

	crl_ctrl->data.v4l2_int_menu.menu = sensor->link_freq_menu;

	/* items will not be 0 as there will be atleast one pll_config_item */
	crl_ctrl->data.v4l2_int_menu.max = items - 1;

	return 0;
}

static int crlmodule_init_controls(struct crl_sensor *sensor)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	unsigned int pa_ctrls = 0;
	unsigned int src_ctrls = 0;
	struct crl_v4l2_ctrl *crl_ctrl;
	struct v4l2_ctrl_handler *ctrl_handler;
	struct v4l2_ctrl_config cfg = { 0 };
	unsigned int i;
	int rval;

	sensor->v4l2_ctrl_bank = devm_kzalloc(&client->dev,
		sizeof(struct crl_v4l2_ctrl) *
		 sensor->sensor_ds->v4l2_ctrls_items,
		 GFP_KERNEL);
	if (!sensor->v4l2_ctrl_bank)
		return -ENOMEM;

	/* Prepare to initialise the v4l2_ctrls from the crl wrapper */
	for (i = 0; i < sensor->sensor_ds->v4l2_ctrls_items; i++) {
		/*
		 * First copy the v4l2_ctrls to the sensor as there could be
		 * more than one similar sensors in a product which could share
		 * the same configuration files
		 */
		sensor->v4l2_ctrl_bank[i] =
					sensor->sensor_ds->v4l2_ctrl_bank[i];

		crl_ctrl = &sensor->v4l2_ctrl_bank[i];
		if (crl_ctrl->sd_type == CRL_SUBDEV_TYPE_PIXEL_ARRAY)
			pa_ctrls++;

		if (crl_ctrl->sd_type == CRL_SUBDEV_TYPE_SCALER ||
		    crl_ctrl->sd_type == CRL_SUBDEV_TYPE_BINNER)
			src_ctrls++;

		/* populate the v4l2_ctrl for the Link_freq dynamically */
		if (crl_ctrl->ctrl_id == V4L2_CID_LINK_FREQ &&
			(crl_ctrl->sd_type == CRL_SUBDEV_TYPE_SCALER ||
			 crl_ctrl->sd_type == CRL_SUBDEV_TYPE_BINNER)) {
			rval = __crlmodule_init_link_freq_ctrl_menu(sensor,
								    crl_ctrl);
			if (rval)
				return rval;
		}
	}
	dev_dbg(&client->dev, "%s pa_ctrls: %d src_ctrls: %d\n", __func__,
			       pa_ctrls, src_ctrls);

	if (pa_ctrls) {
		rval = v4l2_ctrl_handler_init(
					&sensor->pixel_array->ctrl_handler,
					pa_ctrls);
		if (rval)
			return rval;
		sensor->pixel_array->ctrl_handler.lock = &sensor->mutex;
	}

	if (src_ctrls) {
		rval = v4l2_ctrl_handler_init(
					&sensor->src->ctrl_handler,
					src_ctrls);
		if (rval)
			return rval;
		sensor->src->ctrl_handler.lock = &sensor->mutex;
	}

	for (i = 0; i < sensor->sensor_ds->v4l2_ctrls_items; i++) {
		crl_ctrl = &sensor->v4l2_ctrl_bank[i];
		ctrl_handler = __crlmodule_get_sd_ctrl_handler(sensor,
					crl_ctrl->sd_type);

		if (!ctrl_handler)
			continue;

		switch (crl_ctrl->type) {
		case CRL_V4L2_CTRL_TYPE_MENU_ITEMS:
			crl_ctrl->ctrl = v4l2_ctrl_new_std_menu_items(
					 ctrl_handler, &crlmodule_ctrl_ops,
					 crl_ctrl->ctrl_id,
					 crl_ctrl->data.v4l2_menu_items.size,
					 0, 0,
					 crl_ctrl->data.v4l2_menu_items.menu);
			break;
		case CRL_V4L2_CTRL_TYPE_MENU_INT:
			crl_ctrl->ctrl = v4l2_ctrl_new_int_menu(ctrl_handler,
					 &crlmodule_ctrl_ops, crl_ctrl->ctrl_id,
					 crl_ctrl->data.v4l2_int_menu.max,
					 crl_ctrl->data.v4l2_int_menu.def,
					 crl_ctrl->data.v4l2_int_menu.menu);
			break;
		case CRL_V4L2_CTRL_TYPE_INTEGER64:
		case CRL_V4L2_CTRL_TYPE_INTEGER:
			crl_ctrl->ctrl = v4l2_ctrl_new_std(ctrl_handler,
					 &crlmodule_ctrl_ops, crl_ctrl->ctrl_id,
					 crl_ctrl->data.std_data.min,
					 crl_ctrl->data.std_data.max,
					 crl_ctrl->data.std_data.step,
					 crl_ctrl->data.std_data.def);
			break;
		case CRL_V4L2_CTRL_TYPE_CUSTOM:
			cfg.ops = &crlmodule_ctrl_ops;
			cfg.id = crl_ctrl->ctrl_id;
			cfg.name = crl_ctrl->name;
			cfg.type = crl_ctrl->v4l2_type;
			if ((crl_ctrl->v4l2_type == V4L2_CTRL_TYPE_INTEGER) ||
				(crl_ctrl->v4l2_type ==
				 V4L2_CTRL_TYPE_INTEGER64)) {
				cfg.max = crl_ctrl->data.std_data.max;
				cfg.min =  crl_ctrl->data.std_data.min;
				cfg.step  = crl_ctrl->data.std_data.step;
				cfg.def = crl_ctrl->data.std_data.def;
				cfg.qmenu = 0;
				cfg.elem_size = 0;
			} else if (crl_ctrl->v4l2_type == V4L2_CTRL_TYPE_MENU) {
				cfg.max = crl_ctrl->data.v4l2_menu_items.size
					- 1;
				cfg.min =  0;
				cfg.step  = 0;
				cfg.def = 0;
				cfg.qmenu = crl_ctrl->data.v4l2_menu_items.menu;
				cfg.elem_size = 0;
			} else {
				dev_dbg(&client->dev,
					"%s Custom Control: type %d\n",
					__func__, crl_ctrl->v4l2_type);
				continue;
			}
			crl_ctrl->ctrl = v4l2_ctrl_new_custom(ctrl_handler,
					&cfg, NULL);
			break;
		case CRL_V4L2_CTRL_TYPE_BOOLEAN:
		case CRL_V4L2_CTRL_TYPE_BUTTON:
		case CRL_V4L2_CTRL_TYPE_CTRL_CLASS:
		default:
			dev_err(&client->dev,
				"%s Invalid control type\n", __func__);
			continue;
			break;
		}

		if (!crl_ctrl->ctrl)
			continue;
		/*
		 * Blanking and framesize controls access to same register,
		 * Blank controls are disabled if framesize controls exists.
		 */
		if (crl_ctrl->ctrl_id == V4L2_CID_FRAME_LENGTH_LINES ||
			crl_ctrl->ctrl_id == V4L2_CID_LINE_LENGTH_PIXELS)
			sensor->blanking_ctrl_not_use = 1;

		if (crl_ctrl->ctrl_id == CRL_CID_SENSOR_MODE)
			sensor->direct_mode_in_use = 1;

		/* Save mandatory control references - link_freq in src sd */
		if (crl_ctrl->ctrl_id == V4L2_CID_LINK_FREQ &&
			(crl_ctrl->sd_type == CRL_SUBDEV_TYPE_SCALER ||
			crl_ctrl->sd_type == CRL_SUBDEV_TYPE_BINNER))
			sensor->link_freq = crl_ctrl->ctrl;

		/* Save mandatory control references - pixel_rate_pa PA sd */
		if (crl_ctrl->ctrl_id == V4L2_CID_PIXEL_RATE &&
		    crl_ctrl->sd_type == CRL_SUBDEV_TYPE_PIXEL_ARRAY)
			sensor->pixel_rate_pa = crl_ctrl->ctrl;

		/* Save mandatory control references - pixel_rate_csi src sd */
		if (crl_ctrl->ctrl_id == V4L2_CID_PIXEL_RATE &&
			(crl_ctrl->sd_type == CRL_SUBDEV_TYPE_SCALER ||
			crl_ctrl->sd_type == CRL_SUBDEV_TYPE_BINNER))
			sensor->pixel_rate_csi = crl_ctrl->ctrl;

		crl_ctrl->ctrl->flags |= crl_ctrl->flags;

		dev_dbg(&client->dev,
			"%s idx: %d ctrl_id: 0x%x ctrl_name: %s\n ctrl: 0x%p",
			__func__, i, crl_ctrl->ctrl_id, crl_ctrl->name,
			crl_ctrl->ctrl);

		if (ctrl_handler->error) {
			dev_err(&client->dev,
				"%s controls initialization failed (%d)\n",
				__func__, ctrl_handler->error);
			rval = ctrl_handler->error;
			goto error;
		}
	}

	sensor->pixel_array->sd.ctrl_handler =
				&sensor->pixel_array->ctrl_handler;

	sensor->src->sd.ctrl_handler = &sensor->src->ctrl_handler;

	return 0;

error:
	v4l2_ctrl_handler_free(&sensor->pixel_array->ctrl_handler);
	v4l2_ctrl_handler_free(&sensor->src->ctrl_handler);

	return rval;
}

static bool __crlmodule_rect_matches(struct i2c_client *client,
				     const struct v4l2_rect *const rect1,
				     const struct v4l2_rect *const rect2)
{
	dev_dbg(&client->dev, "%s rect1 l:%d t:%d w:%d h:%d\n", __func__,
		rect1->left, rect1->top, rect1->width, rect1->height);
	dev_dbg(&client->dev, "%s rect2 l:%d t:%d w:%d h:%d\n", __func__,
		rect2->left, rect2->top, rect2->width, rect2->height);

	return (rect1->left == rect2->left &&
		rect1->top == rect2->top &&
		rect1->width == rect2->width &&
		rect1->height == rect2->height);
}

static unsigned int __crlmodule_get_mode_min_llp(struct crl_sensor *sensor)
{
	const struct crl_mode_rep *mode = sensor->current_mode;
	const struct crl_sensor_limits *limits =
		sensor->sensor_ds->sensor_limits;
	unsigned int width = sensor->pixel_array->crop[CRL_PA_PAD_SRC].width;
	unsigned int min_llp;

	if (mode->min_llp)
		min_llp = mode->min_llp; /* mode specific limit */
	else if (limits->min_line_length_pixels)
		min_llp = limits->min_line_length_pixels; /* sensor limit */
	else /* No restrictions */
		min_llp = width;

	return min_llp;
}

static unsigned  int __crlmodule_get_mode_max_llp(struct crl_sensor *sensor)
{
	const struct crl_mode_rep *mode = sensor->current_mode;
	const struct crl_sensor_limits *limits =
		sensor->sensor_ds->sensor_limits;
	unsigned int max_llp;

	if (mode->max_llp)
		max_llp = mode->max_llp; /* mode specific limit */
	else if (limits->min_line_length_pixels)
		max_llp = limits->max_line_length_pixels; /* sensor limit */
	else /* No restrictions */
		max_llp = USHRT_MAX;

	return max_llp;
}

static unsigned int __crlmodule_get_mode_min_fll(struct crl_sensor *sensor)
{
	const struct crl_mode_rep *mode = sensor->current_mode;
	const struct crl_sensor_limits *limits =
		sensor->sensor_ds->sensor_limits;
	unsigned int height = sensor->pixel_array->crop[CRL_PA_PAD_SRC].height;
	unsigned int min_fll;

	if (mode->min_fll)
		min_fll = mode->min_fll; /* mode specific limit */
	else if (limits->min_frame_length_lines)
		min_fll = limits->min_frame_length_lines; /* sensor limit */
	else /* No restrictions */
		min_fll = height;

	return min_fll;
}

static unsigned int __crlmodule_get_mode_max_fll(struct crl_sensor *sensor)
{
	const struct crl_mode_rep *mode = sensor->current_mode;
	const struct crl_sensor_limits *limits =
		sensor->sensor_ds->sensor_limits;
	unsigned int max_fll;

	if (mode->max_fll)
		max_fll = mode->max_fll; /* mode specific limit */
	else if (limits->min_line_length_pixels)
		max_fll = limits->max_line_length_pixels; /* sensor limit */
	else /* No restrictions */
		max_fll = USHRT_MAX;

	return max_fll;
}

static void crlmodule_update_framesize(struct crl_sensor *sensor)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	unsigned int min_llp, max_llp, min_fll, max_fll;
	struct v4l2_ctrl *llength;
	struct v4l2_ctrl *flength;

	llength = __crlmodule_get_v4l2_ctrl(sensor,
			V4L2_CID_LINE_LENGTH_PIXELS);
	flength = __crlmodule_get_v4l2_ctrl(sensor,
			V4L2_CID_FRAME_LENGTH_LINES);

	if (llength) {
		min_llp = __crlmodule_get_mode_min_llp(sensor);
		max_llp = __crlmodule_get_mode_max_llp(sensor);

		llength->minimum = min_llp;
		llength->maximum = max_llp;
		llength->default_value = llength->minimum;
		dev_dbg(&client->dev, "%s llp:%d\n", __func__, llength->val);
	}

	if (flength) {
		min_fll = __crlmodule_get_mode_min_fll(sensor);
		max_fll = __crlmodule_get_mode_max_fll(sensor);
		flength->minimum = min_fll;
		flength->maximum = max_fll;
		flength->default_value = flength->minimum;
		dev_dbg(&client->dev, "%s fll:%d\n", __func__, flength->val);
	}
}

static int crlmodule_update_frame_blanking(struct crl_sensor *sensor)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	unsigned int width = sensor->pixel_array->crop[CRL_PA_PAD_SRC].width;
	unsigned int height = sensor->pixel_array->crop[CRL_PA_PAD_SRC].height;
	unsigned int min_llp, max_llp, min_fll, max_fll;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hblank;

	vblank = __crlmodule_get_v4l2_ctrl(sensor, V4L2_CID_VBLANK);
	hblank = __crlmodule_get_v4l2_ctrl(sensor, V4L2_CID_HBLANK);

	if (hblank) {
		min_llp = __crlmodule_get_mode_min_llp(sensor);
		max_llp = __crlmodule_get_mode_max_llp(sensor);

		hblank->minimum = min_llp - width;
		hblank->maximum = max_llp - width;
		hblank->default_value = hblank->minimum;
		dev_dbg(&client->dev, "%s hblank:%d\n", __func__, hblank->val);
	}

	if (vblank) {
		min_fll = __crlmodule_get_mode_min_fll(sensor);
		max_fll = __crlmodule_get_mode_max_fll(sensor);

		vblank->minimum = min_fll - height;
		vblank->maximum = max_fll - height;
		vblank->default_value = vblank->minimum;
		dev_dbg(&client->dev, "%s vblank:%d\n", __func__, vblank->val);
	}

	return 0;
}

static int __crlmodule_rect_index(enum crl_subdev_type type,
				  const struct crl_mode_rep *mode)
{
	int i;

	for (i = 0; i < mode->sd_rects_items; i++) {
		if (type == mode->sd_rects[i].subdev_type)
			return i;
	}

	return -1;
}

static void crlmodule_update_mode_bysel(struct crl_sensor *sensor)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	const struct crl_mode_rep *this;
	unsigned int i;
	int rect_index;

	dev_dbg(&client->dev, "%s look for w: %d, h: %d, in [%d] modes\n",
			      __func__, sensor->src->crop[CRL_PAD_SRC].width,
			       sensor->src->crop[CRL_PAD_SRC].height,
			       sensor->sensor_ds->modes_items);

	for (i = 0; i < sensor->sensor_ds->modes_items; i++) {
		this = &sensor->sensor_ds->modes[i];

		dev_dbg(&client->dev, "%s check mode list[%d] w: %d, h: %d\n",
				      __func__, i, this->width, this->height);
		if (this->width != sensor->src->crop[CRL_PAD_SRC].width ||
		    this->height != sensor->src->crop[CRL_PAD_SRC].height)
			continue;

		if (sensor->pixel_array) {
			dev_dbg(&client->dev, "%s Compare PA out rect\n",
					__func__);
			rect_index =
			__crlmodule_rect_index(CRL_SUBDEV_TYPE_PIXEL_ARRAY,
					this);
			if (rect_index < 0)
				continue;
			if (!__crlmodule_rect_matches(client,
				&sensor->pixel_array->crop[CRL_PA_PAD_SRC],
				&this->sd_rects[rect_index].out_rect))
				continue;
		}
		if (sensor->binner) {
			dev_dbg(&client->dev, "%s binning hor: %d vs. %d\n",
					      __func__,
					      sensor->binning_horizontal,
					      this->binn_hor);
			if (sensor->binning_horizontal != this->binn_hor)
				continue;

			dev_dbg(&client->dev, "%s binning vert: %d vs. %d\n",
					      __func__,
					      sensor->binning_vertical,
					      this->binn_vert);
			if (sensor->binning_vertical != this->binn_vert)
				continue;

			dev_dbg(&client->dev, "%s binner in rect\n", __func__);
			rect_index =
				__crlmodule_rect_index(CRL_SUBDEV_TYPE_BINNER,
					this);
			if (rect_index < 0)
				continue;
			if (!__crlmodule_rect_matches(client,
				&sensor->binner->crop[CRL_PAD_SINK],
				&this->sd_rects[rect_index].in_rect))
				continue;

			dev_dbg(&client->dev, "%s binner out rect\n", __func__);
			if (!__crlmodule_rect_matches(client,
				&sensor->binner->crop[CRL_PAD_SRC],
				&this->sd_rects[rect_index].out_rect))
				continue;
		}

		if (sensor->scaler) {
			dev_dbg(&client->dev, "%s scaler scale_m %d vs. %d\n",
					      __func__, sensor->scale_m,
					      this->scale_m);
			if (sensor->scale_m != this->scale_m)
				continue;

			rect_index =
				__crlmodule_rect_index(CRL_SUBDEV_TYPE_SCALER,
					this);
			if (rect_index < 0)
				continue;

			dev_dbg(&client->dev, "%s scaler in rect\n", __func__);
			if (!__crlmodule_rect_matches(client,
				&sensor->scaler->crop[CRL_PAD_SINK],
				&this->sd_rects[rect_index].in_rect))
				continue;

			dev_dbg(&client->dev, "%s scaler out rect\n", __func__);
			if (!__crlmodule_rect_matches(client,
				&sensor->scaler->crop[CRL_PAD_SRC],
				&this->sd_rects[rect_index].out_rect))
				continue;
		}

		/* Check if there are any dynamic compare items */
		if (sensor->ext_ctrl_impacts_mode_selection &&
		    !__crlmodule_compare_ctrl_specific_data(sensor,
							    this->comp_items,
							    this->ctrl_data))
			continue;

		/* Found a perfect match! */
		dev_dbg(&client->dev, "%s found mode. idx: %d\n", __func__, i);
		break;
	}

	/* If no modes found, fall back to the fail safe mode index */
	if (i >= sensor->sensor_ds->modes_items) {
		i = sensor->sensor_ds->fail_safe_mode_index;
		this = &sensor->sensor_ds->modes[i];
		dev_dbg(&client->dev,
			 "%s no matching mode, set to default: %d\n",
			 __func__, i);
	}

	sensor->current_mode = this;
}

static void crlmodule_update_mode_v4l2ctrl(struct crl_sensor *sensor)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	const struct crl_mode_rep *this;
	int i;

	dev_dbg(&client->dev, "%s Sensor Mode :%d\n",
		 __func__, sensor->sensor_mode);
	/* point to selected mode */
	this = &sensor->sensor_ds->modes[sensor->sensor_mode];
	sensor->current_mode = this;

	for (i = 0; i < this->sd_rects_items; i++) {

		if (CRL_SUBDEV_TYPE_PIXEL_ARRAY ==
		 this->sd_rects[i].subdev_type) {
			sensor->pixel_array->crop[CRL_PA_PAD_SRC] =
			 this->sd_rects[i].out_rect;
		}

		if (CRL_SUBDEV_TYPE_BINNER ==
		 this->sd_rects[i].subdev_type) {
			sensor->binner->sink_fmt =
			 this->sd_rects[i].in_rect;
			sensor->binner->crop[CRL_PAD_SINK] =
			 this->sd_rects[i].in_rect;
			sensor->binner->crop[CRL_PAD_SRC] =
			 this->sd_rects[i].out_rect;
			sensor->binning_vertical = this->binn_vert;
			sensor->binning_horizontal = this->binn_hor;
			if (this->binn_vert > 1)
				sensor->binner->compose =
				 this->sd_rects[i].out_rect;
		}

		if (CRL_SUBDEV_TYPE_SCALER ==
		 this->sd_rects[i].subdev_type) {
			sensor->scaler->crop[CRL_PAD_SINK] =
			 this->sd_rects[i].in_rect;
			sensor->scaler->crop[CRL_PAD_SRC] =
			 this->sd_rects[i].out_rect;
			sensor->scaler->sink_fmt =
			 this->sd_rects[i].in_rect;
			sensor->scale_m = this->scale_m;
			if (this->scale_m != 1)
				sensor->scaler->compose =
				 this->sd_rects[i].out_rect;
		}
	}

	/* Set source */
	sensor->src->crop[CRL_PAD_SRC].width = this->width;
	sensor->src->crop[CRL_PAD_SRC].height = this->height;
}

static void crlmodule_update_current_mode(struct crl_sensor *sensor)
{
	const struct crl_mode_rep *this;
	int i;

	if (sensor->direct_mode_in_use)
		crlmodule_update_mode_v4l2ctrl(sensor);
	else
		crlmodule_update_mode_bysel(sensor);

	/*
	 * We have a valid mode now. If there are any mode specific "get"
	 * controls defined in the configuration it could be queried by the
	 * user space for any mode specific information. So go through the
	 * mode specific v4l2_ctrls and update its value from the selected mode.
	 */

	this = sensor->current_mode;

	for (i = 0; i < this->comp_items; i++) {
		struct crl_ctrl_data_pair *ctrl_comp = &this->ctrl_data[i];
		unsigned int idx;

		/* Get the v4l2_ctrl pointer corresponding ctrl id */
		if (__crlmodule_get_crl_ctrl_index(sensor, ctrl_comp->ctrl_id,
						   &idx))
			/* If not found, move to the next ctrl */
			continue;

		/* No need to update this control, if this is a set op ctrl */
		if (sensor->v4l2_ctrl_bank[idx].op_type == CRL_V4L2_CTRL_SET_OP)
			continue;

		/* Update the control value */
		__v4l2_ctrl_s_ctrl(sensor->v4l2_ctrl_bank[idx].ctrl,
				   ctrl_comp->data);
	}

	if (sensor->blanking_ctrl_not_use)
		crlmodule_update_framesize(sensor);
	else
		crlmodule_update_frame_blanking(sensor);
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Slightly modified based on the CRL Module changes
 */
static int __crlmodule_get_format(struct v4l2_subdev *subdev,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_format *fmt)
{
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct v4l2_rect *r;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		fmt->format = *v4l2_subdev_get_try_format(subdev, cfg,
				fmt->pad);
		return 0;
	}

	if (fmt->pad == ssd->source_pad)
		r = &ssd->crop[ssd->source_pad];
	else
		r = &ssd->sink_fmt;

	fmt->format.width = r->width;
	fmt->format.height = r->height;
	fmt->format.code =
		sensor->sensor_ds->csi_fmts[sensor->fmt_index].code;
	fmt->format.field = (ssd->field == V4L2_FIELD_ANY) ?
		V4L2_FIELD_NONE : ssd->field;
	return 0;
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Slightly modified based on the CRL Module changes
 */
static int crlmodule_enum_mbus_code(struct v4l2_subdev *subdev,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_mbus_code_enum *code)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);

	if (code->index >= sensor->sensor_ds->csi_fmts_items)
		return -EINVAL;

	code->code = sensor->sensor_ds->csi_fmts[code->index].code;

	return 0;
}

static int crlmodule_enum_frame_size(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_frame_size_enum *fse)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(sd);

	if (fse->index >= sensor->sensor_ds->modes_items)
		return -EINVAL;

	fse->min_width = sensor->sensor_ds->modes[fse->index].width;
	fse->max_width = fse->min_width;
	fse->min_height = sensor->sensor_ds->modes[fse->index].height;
	fse->max_height = fse->min_height;

	return 0;
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Slightly modified based on the CRL Module changes
 */
static int crlmodule_get_format(struct v4l2_subdev *subdev,
			     struct v4l2_subdev_pad_config *cfg,
			     struct v4l2_subdev_format *fmt)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	int rval;

	mutex_lock(&sensor->mutex);
	rval = __crlmodule_get_format(subdev, cfg, fmt);
	mutex_unlock(&sensor->mutex);

	return rval;
}

static int __crlmodule_sel_supported(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_selection *sel)
{
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);

	if (ssd == sensor->pixel_array
		    && sel->pad == CRL_PA_PAD_SRC) {
		switch (sel->target) {
		case V4L2_SEL_TGT_NATIVE_SIZE:
		case V4L2_SEL_TGT_CROP:
		case V4L2_SEL_TGT_CROP_BOUNDS:
			return 0;
		}
	}
	if (ssd == sensor->binner) {
		switch (sel->target) {
		case V4L2_SEL_TGT_COMPOSE:
		case V4L2_SEL_TGT_COMPOSE_BOUNDS:
			if (sel->pad == CRL_PAD_SINK)
				return 0;
		}
	}
	if (ssd == sensor->scaler) {
		switch (sel->target) {
		case V4L2_SEL_TGT_CROP:
		case V4L2_SEL_TGT_CROP_BOUNDS:
			if (sel->pad == CRL_PAD_SRC)
				return 0;
		break;
		case V4L2_SEL_TGT_COMPOSE:
		case V4L2_SEL_TGT_COMPOSE_BOUNDS:
			if (sel->pad == CRL_PAD_SINK)
				return 0;
		}
	}
	return -EINVAL;
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Slightly modified based on the CRL Module changes
 */
static void crlmodule_get_crop_compose(struct v4l2_subdev *subdev,
				    struct v4l2_subdev_pad_config *cfg,
				    struct v4l2_rect **crops,
				    struct v4l2_rect **comps, int which)
{
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	unsigned int i;

	/* Currently we support only 2 pads */
	BUG_ON(subdev->entity.num_pads > CRL_PADS);

	if (which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		if (crops)
			for (i = 0; i < subdev->entity.num_pads; i++)
				crops[i] = &ssd->crop[i];
		if (comps)
			*comps = &ssd->compose;
	} else {
		if (crops) {
			for (i = 0; i < subdev->entity.num_pads; i++) {
				crops[i] = v4l2_subdev_get_try_crop(subdev,
								    cfg, i);
				BUG_ON(!crops[i]);
			}
		}
		if (comps) {
			*comps = v4l2_subdev_get_try_compose(subdev, cfg,
							     CRL_PAD_SINK);
			BUG_ON(!*comps);
		}
	}
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Slightly modified based on the CRL Module changes
 */
static int crlmodule_get_selection(struct v4l2_subdev *subdev,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_selection *sel)
{
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct v4l2_rect *comp, *crops[CRL_PADS];
	struct v4l2_rect sink_fmt;
	int ret;

	ret = __crlmodule_sel_supported(subdev, sel);
	if (ret)
		return ret;

	crlmodule_get_crop_compose(subdev, cfg, crops, &comp, sel->which);

	if (sel->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		sink_fmt = ssd->sink_fmt;
	} else {
		struct v4l2_mbus_framefmt *fmt =
			v4l2_subdev_get_try_format(subdev, cfg, ssd->sink_pad);
		sink_fmt.left = 0;
		sink_fmt.top = 0;
		sink_fmt.width = fmt->width;
		sink_fmt.height = fmt->height;
	}

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_NATIVE_SIZE:
		if (ssd == sensor->pixel_array) {
			sel->r.left = sel->r.top = 0;
			sel->r.width =
				sensor->sensor_ds->sensor_limits->x_addr_max;
			sel->r.height =
				sensor->sensor_ds->sensor_limits->y_addr_max;
		} else if (sel->pad == ssd->sink_pad) {
			sel->r = sink_fmt;
		} else {
			sel->r = *comp;
		}
		break;
	case V4L2_SEL_TGT_CROP:
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
		sel->r = *crops[sel->pad];
		break;
	case V4L2_SEL_TGT_COMPOSE:
		sel->r = *comp;
		break;
	}
	return 0;
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Slightly modified based on the CRL Module changes
 */
static void crlmodule_propagate(struct v4l2_subdev *subdev,
			     struct v4l2_subdev_pad_config *cfg, int which,
			     int target)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	struct v4l2_rect *comp, *crops[CRL_PADS];

	crlmodule_get_crop_compose(subdev, cfg, crops, &comp, which);

	switch (target) {
	case V4L2_SEL_TGT_CROP:
		comp->width = crops[CRL_PAD_SINK]->width;
		comp->height = crops[CRL_PAD_SINK]->height;
		if (which == V4L2_SUBDEV_FORMAT_ACTIVE) {
			if (ssd == sensor->scaler) {
				sensor->scale_m = 1;
			} else if (ssd == sensor->binner) {
				sensor->binning_horizontal = 1;
				sensor->binning_vertical = 1;
			}
		}
		/* Fall through */
	case V4L2_SEL_TGT_COMPOSE:
		*crops[CRL_PAD_SRC] = *comp;
		break;
	default:
		BUG();
	}
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Slightly modified based on the CRL Module changes
 */
static int crlmodule_set_compose(struct v4l2_subdev *subdev,
			      struct v4l2_subdev_pad_config *cfg,
			      struct v4l2_subdev_selection *sel)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	struct v4l2_rect *comp, *crops[CRL_PADS];

	crlmodule_get_crop_compose(subdev, cfg, crops, &comp, sel->which);

	sel->r.top = 0;
	sel->r.left = 0;

	if (ssd == sensor->binner) {
		sensor->binning_horizontal = crops[CRL_PAD_SINK]->width /
					   sel->r.width;
		sensor->binning_vertical = crops[CRL_PAD_SINK]->height /
					   sel->r.height;
	} else {
		sensor->scale_m = crops[CRL_PAD_SINK]->width *
			sensor->sensor_ds->sensor_limits->scaler_m_min /
				 sel->r.width;
	}

	*comp = sel->r;

	crlmodule_propagate(subdev, cfg, sel->which,
			 V4L2_SEL_TGT_COMPOSE);

	if (sel->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		crlmodule_update_current_mode(sensor);

	return 0;
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Slightly modified based on the CRL Module changes
 */
static int crlmodule_set_crop(struct v4l2_subdev *subdev,
			      struct v4l2_subdev_pad_config *cfg,
			      struct v4l2_subdev_selection *sel)
{
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct v4l2_rect *src_size, *crops[CRL_PADS];
	struct v4l2_rect _r;

	crlmodule_get_crop_compose(subdev, cfg, crops, NULL, sel->which);

	if (sel->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		if (sel->pad == ssd->sink_pad)
			src_size = &ssd->sink_fmt;
		else
			src_size = &ssd->compose;
	} else {
		if (sel->pad == ssd->sink_pad) {
			_r.left = 0;
			_r.top = 0;
			_r.width = v4l2_subdev_get_try_format(subdev,
							      cfg, sel->pad)
				->width;
			_r.height = v4l2_subdev_get_try_format(subdev,
							       cfg, sel->pad)
				->height;
			src_size = &_r;
		} else {
			src_size =
				v4l2_subdev_get_try_compose(subdev, cfg,
							    ssd->sink_pad);
		}
	}

	if (ssd == sensor->src && sel->pad == CRL_PAD_SRC) {
		sel->r.left = 0;
		sel->r.top = 0;
	}

	sel->r.width = min(sel->r.width, src_size->width);
	sel->r.height = min(sel->r.height, src_size->height);

	sel->r.left = min_t(s32, sel->r.left, src_size->width - sel->r.width);
	sel->r.top = min_t(s32, sel->r.top, src_size->height - sel->r.height);

	*crops[sel->pad] = sel->r;

	if (ssd != sensor->pixel_array && sel->pad == CRL_PAD_SINK)
		crlmodule_propagate(subdev, cfg, sel->which,
				 V4L2_SEL_TGT_CROP);

	/* TODO! Should we short list supported mode? */

	return 0;
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Modified based on the CRL Module changes
 */
static int crlmodule_set_format(struct v4l2_subdev *subdev,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	struct v4l2_rect *crops[CRL_PADS];

	dev_dbg(&client->dev, "%s sd_name: %s pad: %d w: %d, h: %d code: 0x%x",
			       __func__, ssd->sd.name, fmt->pad,
			       fmt->format.width, fmt->format.height,
			       fmt->format.code);

	mutex_lock(&sensor->mutex);

	/* Currently we only support ALTERNATE interlaced mode. */
	if (fmt->format.field != V4L2_FIELD_ALTERNATE)
		fmt->format.field = V4L2_FIELD_NONE;
	ssd->field = fmt->format.field;

	if (fmt->pad == ssd->source_pad) {
		u32 code = fmt->format.code;
		int rval = __crlmodule_get_format(subdev, cfg, fmt);

		if (!rval && subdev == &sensor->src->sd) {
			/* Check if this code is supported, if yes get index */
			int idx = __crlmodule_get_data_fmt_index(sensor, code);

			if (idx < 0) {
				dev_err(&client->dev, "%s invalid format\n",
						       __func__);
				mutex_unlock(&sensor->mutex);
				return -EINVAL;
			}

			sensor->fmt_index = idx;
			/* TODO! validate PLL? */
		}
		mutex_unlock(&sensor->mutex);
		return rval;
	}

	fmt->format.width =
		clamp_t(uint32_t, fmt->format.width,
			sensor->sensor_ds->sensor_limits->x_addr_min,
			sensor->sensor_ds->sensor_limits->x_addr_max);
	fmt->format.height =
		clamp_t(uint32_t, fmt->format.height,
			sensor->sensor_ds->sensor_limits->y_addr_min,
			sensor->sensor_ds->sensor_limits->y_addr_max);

	crlmodule_get_crop_compose(subdev, cfg, crops, NULL, fmt->which);

	crops[ssd->sink_pad]->left = 0;
	crops[ssd->sink_pad]->top = 0;
	crops[ssd->sink_pad]->width = fmt->format.width;
	crops[ssd->sink_pad]->height = fmt->format.height;
	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		ssd->sink_fmt = *crops[ssd->sink_pad];

	crlmodule_propagate(subdev, cfg, fmt->which,
			 V4L2_SEL_TGT_CROP);

	crlmodule_update_current_mode(sensor);

	mutex_unlock(&sensor->mutex);

	return 0;
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Slightly modified based on the CRL Module changes
 */
static int crlmodule_set_selection(struct v4l2_subdev *subdev,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_selection *sel)
{
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	int ret;

	dev_dbg(&client->dev, "%s sd_name: %s sel w: %d, h: %d target: %d",
			       __func__, ssd->sd.name, sel->r.width,
			       sel->r.height, sel->target);

	ret = __crlmodule_sel_supported(subdev, sel);
	if (ret) {
		dev_dbg(&client->dev,
			"%s sd_name: %s w: %d, h: %d target: %d not supported",
			       __func__, ssd->sd.name, sel->r.width,
			       sel->r.height, sel->target);
		return ret;
	}

	mutex_lock(&sensor->mutex);

	sel->r.width = max_t(unsigned int,
			     sensor->sensor_ds->sensor_limits->x_addr_min,
			     sel->r.width);
	sel->r.height = max_t(unsigned int,
			      sensor->sensor_ds->sensor_limits->y_addr_min,
			      sel->r.height);
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		ret = crlmodule_set_crop(subdev, cfg, sel);
		break;
	case V4L2_SEL_TGT_COMPOSE:
		ret = crlmodule_set_compose(subdev, cfg, sel);
		break;
	default:
		ret = -EINVAL;
	}

	crlmodule_update_current_mode(sensor);

	mutex_unlock(&sensor->mutex);
	return ret;
}

static int crlmodule_get_skip_frames(struct v4l2_subdev *subdev, u32 *frames)
{
	/* TODO Handle this */
	return 0;
}

static int crlmodule_start_streaming(struct crl_sensor *sensor)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	const struct crl_pll_configuration *pll;
	const struct crl_csi_data_fmt *fmt;
	int rval;

	dev_dbg(&client->dev, "%s start streaming pll_idx: %d fmt_idx: %d\n",
			      __func__, sensor->pll_index,
			      sensor->fmt_index);

	pll = &sensor->sensor_ds->pll_configs[sensor->pll_index];
	fmt = &sensor->sensor_ds->csi_fmts[sensor->fmt_index];

	crlmodule_update_current_mode(sensor);

	rval = crlmodule_write_regs(sensor, fmt->regs, fmt->regs_items);
	if (rval) {
		dev_err(&client->dev, "%s failed to set format\n", __func__);
		return rval;
	}

	rval = crlmodule_write_regs(sensor, pll->pll_regs, pll->pll_regs_items);
	if (rval) {
		dev_err(&client->dev, "%s failed to set plls\n", __func__);
		return rval;
	}

	/* Write mode list */
	rval = crlmodule_write_regs(sensor,
			sensor->current_mode->mode_regs,
			sensor->current_mode->mode_regs_items);
	if (rval) {
		dev_err(&client->dev, "%s failed to set mode\n", __func__);
		return rval;
	}

	/* Write stream on list */
	rval = crlmodule_write_regs(sensor,
				   sensor->sensor_ds->streamon_regs,
				   sensor->sensor_ds->streamon_regs_items);
	if (rval) {
		dev_err(&client->dev, "%s failed to set stream\n", __func__);
		return rval;
	}

	return 0;
}

static int crlmodule_stop_streaming(struct crl_sensor *sensor)
{
	return crlmodule_write_regs(sensor,
				    sensor->sensor_ds->streamoff_regs,
				    sensor->sensor_ds->streamoff_regs_items);
}

static int crlmodule_set_stream(struct v4l2_subdev *subdev, int enable)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	int rval = 0;

	mutex_lock(&sensor->mutex);

	if (sensor->streaming == enable)
		goto out;

	if (enable) {

		if (sensor->msr_list) {
			rval = crlmodule_apply_msrlist(client,
					sensor->msr_list);
			if (rval)
				dev_warn(&client->dev, "msrlist write error %d\n",
						rval);
		}
		rval = crlmodule_start_streaming(sensor);
		if (!rval)
			sensor->streaming = 1;
	} else {
		rval = crlmodule_stop_streaming(sensor);
		sensor->streaming = 0;
	}

out:
	mutex_unlock(&sensor->mutex);

	/* SENSOR_IDLE control cannot be set when streaming*/
	__crlmodule_grab_v4l2_ctrl(sensor, SENSOR_IDLE, enable);

	/* SENSOR_STREAMING controls cannot be set when not streaming */
	__crlmodule_grab_v4l2_ctrl(sensor, SENSOR_STREAMING, !enable);

	/* SENSOR_POWERED_ON controls does not matter about streaming. */
	__crlmodule_grab_v4l2_ctrl(sensor, SENSOR_POWERED_ON, false);

	return rval;
}

static int crlmodule_identify_module(struct v4l2_subdev *subdev)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	unsigned int size = 0, pos;
	char *id_string;
	const char *expect_id;
	int i, ret;
	u32 val;

	for (i = 0; i < sensor->sensor_ds->id_reg_items; i++)
		size += sensor->sensor_ds->id_regs[i].width + 1;

	/* TODO! If no ID! return success? */
	if (!size)
		return 0;

	expect_id = sensor->platform_data->id_string;
	/* Create string variabel to append module ID */
	id_string = kzalloc(size, GFP_KERNEL);
	if (!id_string)
		return -ENOMEM;
	*id_string = '\0';

	/* Go through each regs in the list and append to id_string */
	for (i = 0; i < sensor->sensor_ds->id_reg_items; i++) {
		ret = crlmodule_read_reg(sensor,
					 sensor->sensor_ds->id_regs[i].reg,
					 &val);
		if (ret)
			goto out;

		if (i)
			pos += snprintf(id_string + pos, size - pos, " 0x%x", val);
		else
			pos = snprintf(id_string, size, "0x%x", val);
		if (pos >= size)
			break;
	}

	/* Check here if this module in the supported list
	 * Ideally the module manufacturer and id should be in platform
	 * data or ACPI and here the driver should read the value from the
	 * register and check if this matches to any in the supported
	 * platform data
	 */
	if (expect_id &&
	   (strnlen(id_string, size) != strnlen(expect_id, size + 1) ||
	    strncmp(id_string, expect_id, size))) {
		dev_err(&client->dev,
			"Sensor detection failed: expect \"%s\" actual \"%s\"",
			expect_id, id_string);
		ret = -ENODEV;
	}

out:
	dev_dbg(&client->dev, "%s module: %s expected id: %s\n",
		__func__, id_string,
		(expect_id) ? expect_id : "not specified");
	kfree(id_string);
	if (ret)
		dev_err(&client->dev, "sensor detection failed\n");
	return ret;
}

static int crlmodule_get_frame_desc(struct v4l2_subdev *subdev,
				    unsigned int pad,
				    struct v4l2_mbus_frame_desc *desc)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct crl_frame_desc *crl_desc = sensor->sensor_ds->frame_desc;
	unsigned int i;

	desc->num_entries = sensor->sensor_ds->frame_desc_entries;
	if (desc->num_entries)
		desc->type = sensor->sensor_ds->frame_desc_type;

	/*
	 * By any chance the sensor configuration has more than the maximum
	 * supported, clip the number of entries to the MAX supported.
	 */
	if (desc->num_entries > V4L2_FRAME_DESC_ENTRY_MAX)
		desc->num_entries = V4L2_FRAME_DESC_ENTRY_MAX;

	for (i = 0; i < desc->num_entries; i++) {
		int ret;
		u32 val;

		ret = __crlmodule_parse_dynamic_entity(sensor,
						       crl_desc[i].flags, &val);
		if (ret)
			return ret;
		desc->entry[i].flags = (u16)val;

		ret = __crlmodule_parse_dynamic_entity(sensor, crl_desc[i].bpp,
						       &val);
		if (ret)
			return ret;
		desc->entry[i].bpp = (u8)val;

		ret = __crlmodule_parse_dynamic_entity(
					sensor, crl_desc[i].pixelcode, &val);
		if (ret)
			return ret;
		desc->entry[i].pixelcode = val;

		if (desc->entry[i].flags & V4L2_MBUS_FRAME_DESC_FL_BLOB) {
			ret = __crlmodule_parse_dynamic_entity(
					sensor, crl_desc[i].length, &val);
			if (ret)
				return ret;
			desc->entry[i].size.length = val;
		} else {
			ret = __crlmodule_parse_dynamic_entity(
					sensor, crl_desc[i].start_line, &val);
			if (ret)
				return ret;
			desc->entry[i].size.two_dim.start_line =
								 (u16)val;

			ret = __crlmodule_parse_dynamic_entity(
					sensor, crl_desc[i].start_pixel, &val);
			if (ret)
				return ret;
			desc->entry[i].size.two_dim.start_pixel =
								 (u16)val;

			ret = __crlmodule_calc_dynamic_entity_values(
					sensor, crl_desc[i].height.ops_items,
					crl_desc[i].height.ops, &val);
			if (ret)
				return ret;
			desc->entry[i].size.two_dim.height = (u16)val;

			ret = __crlmodule_calc_dynamic_entity_values(
					sensor, crl_desc[i].width.ops_items,
					crl_desc[i].width.ops, &val);
			if (ret)
				return ret;
			desc->entry[i].size.two_dim.width = (u16)val;
		}

		if (desc->type == CRL_V4L2_MBUS_FRAME_DESC_TYPE_CSI2) {
			ret = __crlmodule_parse_dynamic_entity(
					sensor, crl_desc[i].csi2_channel, &val);
			if (ret)
				return ret;
			desc->entry[i].bus.csi2.channel = (u8)val;

			ret = __crlmodule_parse_dynamic_entity(
				      sensor, crl_desc[i].csi2_data_type, &val);
			if (ret)
				return ret;
			desc->entry[i].bus.csi2.data_type = (u8)val;
		}
	}

	return 0;
}


static int crlmodule_get_routing(struct v4l2_subdev *subdev,
				   struct v4l2_subdev_routing *route)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	int i;

	if (!route)
		return -EINVAL;

	if (ssd != sensor->src ||
		sensor->sensor_ds->frame_desc_entries <= 1)
		return -ENOIOCTLCMD;

	for (i = 0; i < min(sensor->sensor_ds->frame_desc_entries,
				route->num_routes); i++) {
		route->routes[i].sink_pad = CRL_PAD_SINK;
		route->routes[i].sink_stream = 0;
		route->routes[i].source_pad = CRL_PAD_SRC;
		route->routes[i].source_stream = i;
		route->routes[i].flags = sensor->src->route_flags[i];
	}

	route->num_routes = i;
	return 0;
}

static int crlmodule_set_routing(struct v4l2_subdev *subdev,
				   struct v4l2_subdev_routing *route)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	const unsigned int stream_nr = sensor->sensor_ds->frame_desc_entries;
	struct v4l2_subdev_route *t;
	int i, ret = 0;

	if (!route)
		return -EINVAL;

	if (ssd != sensor->src ||
		sensor->sensor_ds->frame_desc_entries <= 1)
		return -ENOIOCTLCMD;

	for (i = 0; i < min(stream_nr, route->num_routes); ++i) {
		t = &route->routes[i];

		if (t->source_stream > stream_nr - 1)
			continue;

		if (t->source_pad != CRL_PAD_SRC ||
		    t->sink_pad != CRL_PAD_SINK)
			continue;

		if (t->flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE)
			sensor->src->route_flags[t->source_stream] |=
				V4L2_SUBDEV_ROUTE_FL_ACTIVE;
		else if (!(t->flags & V4L2_SUBDEV_ROUTE_FL_ACTIVE))
			sensor->src->route_flags[t->source_stream] &=
				(~V4L2_SUBDEV_ROUTE_FL_ACTIVE);
	}

	return ret;
}

/*
 * This function executes the initialisation routines after the power on
 * is successfully completed. Following operations are done
 *
 *    Initiases registers after sensor power up - if any such list is configured
 *    V4l2 Ctrl handler framework intialisation
 */
static int crlmodule_run_poweron_init(struct crl_sensor *sensor)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	int rval;

	dev_dbg(&client->dev, "%s set power up registers: %d\n",
			       __func__, sensor->sensor_ds->powerup_regs_items);

	/* Write the power up registers */
	rval = crlmodule_write_regs(sensor, sensor->sensor_ds->powerup_regs,
				    sensor->sensor_ds->powerup_regs_items);
	if (rval) {
		dev_err(&client->dev, "%s failed to set powerup registers\n",
				      __func__);
		return rval;
	}

	/* Are we still initialising...? If yes, return here. */
	if (!sensor->pixel_array)
		return 0;

	dev_dbg(&client->dev, "%s init v4l2 controls", __func__);

	rval = v4l2_ctrl_handler_setup(
		&sensor->pixel_array->ctrl_handler);
	if (rval) {
		dev_err(&client->dev, "%s PA v4l2_ctrl_handler failed\n",
				      __func__);
		return rval;
	}

	rval = v4l2_ctrl_handler_setup(&sensor->src->ctrl_handler);
	if (rval)
		dev_err(&client->dev, "%s SRC v4l2_ctrl_handler failed\n",
				      __func__);

	/* SENSOR_IDLE control can be set only when not streaming*/
	__crlmodule_grab_v4l2_ctrl(sensor, SENSOR_IDLE, false);

	/* SENSOR_STREAMING controls can be set only when streaming */
	__crlmodule_grab_v4l2_ctrl(sensor, SENSOR_STREAMING, true);

	/* SENSOR_POWERED_ON controls can be set after power on */
	__crlmodule_grab_v4l2_ctrl(sensor, SENSOR_POWERED_ON, false);

	mutex_lock(&sensor->mutex);
	crlmodule_update_current_mode(sensor);
	mutex_unlock(&sensor->mutex);

	return rval;
}

static int custom_gpio_request(struct crl_sensor *sensor)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	int i;

	for (i = 0; i < CRL_MAX_CUSTOM_GPIO_AMOUNT; i++) {
		if (sensor->platform_data->custom_gpio[i].name[0] == '\0')
			break;
		if (devm_gpio_request_one(
			    &client->dev,
			    sensor->platform_data->custom_gpio[i].number, 0,
			    sensor->platform_data->custom_gpio[i].name) != 0) {
			dev_err(&client->dev,
				"unable to acquire %s %d\n",
				sensor->platform_data->custom_gpio[i].name,
				sensor->platform_data->custom_gpio[i].number);
			return -ENODEV;
		}
	}
	return 0;
}

static void custom_gpio_ctrl(struct crl_sensor *sensor, bool set)
{
	int i;
	unsigned int val;

	for (i = 0; i < CRL_MAX_CUSTOM_GPIO_AMOUNT; i++) {
		if (sensor->platform_data->custom_gpio[i].name[0] == '\0')
			break;
		if (set)
			val = sensor->platform_data->custom_gpio[i].val;
		else
			val = sensor->platform_data->custom_gpio[i].undo_val;

		gpio_set_value(
			sensor->platform_data->custom_gpio[i].number, val);
	}
}

/*
 * This function handles sensor power up routine failure because of any failed
 * step in the routine. The index "i" is the index to last successfull power
 * sequence entity successfull completed. This function executes the power
 * senquence entities in the reverse or with undo value.
 */
static void crlmodule_undo_poweron_entities(
				struct crl_sensor *sensor,
				int rev_idx)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	struct crl_power_seq_entity *entity;
	int idx;

	for (idx = rev_idx; idx >= 0; idx--) {
		entity = &sensor->pwr_entity[idx];
		dev_dbg(&client->dev, "%s power type %d index %d\n",
				__func__, entity->type, idx);

		switch (entity->type) {
		case CRL_POWER_ETY_GPIO_FROM_PDATA:
#ifdef CONFIG_INTEL_IPU4_OV13858
			if (!vcm_in_use) {
#endif
			gpio_set_value(sensor->platform_data->xshutdown,
						   entity->undo_val);
#ifdef CONFIG_INTEL_IPU4_OV13858
			}
#endif
			break;
		case CRL_POWER_ETY_GPIO_FROM_PDATA_BY_NUMBER:
			custom_gpio_ctrl(sensor, false);
			break;
		case CRL_POWER_ETY_GPIO_CUSTOM:
			if (entity->gpiod_priv) {
				if (gpiod_cansleep(entity->gpiod_priv))
					gpiod_set_raw_value_cansleep(
						entity->gpiod_priv,
						entity->undo_val);
				else
					gpiod_set_raw_value(entity->gpiod_priv,
							entity->undo_val);
			} else {
				gpio_set_value(entity->ent_number,
					entity->undo_val);
			}
			break;
		case CRL_POWER_ETY_REGULATOR_FRAMEWORK:
			regulator_disable(entity->regulator_priv);
			break;
		case CRL_POWER_ETY_CLK_FRAMEWORK:
			clk_disable_unprepare(sensor->xclk);
			break;
		default:
			dev_err(&client->dev, "%s Invalid power type\n",
					__func__);
			break;
		}

		if (entity->delay)
			usleep_range(entity->delay, entity->delay + 10);
	}
}

static int __crlmodule_powerup_sequence(struct crl_sensor *sensor)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	struct crl_power_seq_entity *entity;
	unsigned idx;
	int rval;

	for (idx = 0; idx < sensor->sensor_ds->power_items; idx++) {
		entity = &sensor->pwr_entity[idx];
		dev_dbg(&client->dev, "%s power type %d index %d\n",
				__func__, entity->type, idx);

		switch (entity->type) {
		case CRL_POWER_ETY_GPIO_FROM_PDATA:
			gpio_set_value(sensor->platform_data->xshutdown,
					entity->val);
			break;
		case CRL_POWER_ETY_GPIO_FROM_PDATA_BY_NUMBER:
			custom_gpio_ctrl(sensor, true);
			break;
		case CRL_POWER_ETY_GPIO_CUSTOM:
			if (entity->gpiod_priv) {
				if (gpiod_cansleep(entity->gpiod_priv))
					gpiod_set_raw_value_cansleep(
						entity->gpiod_priv,
						entity->val);
				else
					gpiod_set_raw_value(entity->gpiod_priv,
								entity->val);
			} else {
				gpio_set_value(entity->ent_number, entity->val);
			}
			break;
		case CRL_POWER_ETY_REGULATOR_FRAMEWORK:
			rval = regulator_enable(entity->regulator_priv);
			if (rval) {
				dev_err(&client->dev,
					"Failed to enable regulator: %d\n",
					rval);
				devm_regulator_put(entity->regulator_priv);
				entity->regulator_priv = NULL;
				goto error;
			}
			break;
		case CRL_POWER_ETY_CLK_FRAMEWORK:
			rval = clk_set_rate(sensor->xclk,
					sensor->platform_data->ext_clk);
			if (rval < 0) {
				dev_err(&client->dev,
				"unable to set clock freq to %u\n",
				sensor->platform_data->ext_clk);
				goto error;
			}
			if (clk_get_rate(sensor->xclk) !=
					sensor->platform_data->ext_clk)
					dev_warn(&client->dev,
						"warning: unable to set \
						accurate clock freq %u\n",
						sensor->platform_data->ext_clk);
			rval = clk_prepare_enable(sensor->xclk);
			if (rval) {
				dev_err(&client->dev, "Failed to enable \
						clock: %d\n", rval);
				goto error;
			}
			break;
		default:
			dev_err(&client->dev, "Invalid power type\n");
			rval = -ENODEV;
			goto error;
		}

		if (entity->delay)
			usleep_range(entity->delay, entity->delay + 10);
	}

	return 0;
error:
	dev_err(&client->dev, "Error:Power sequece failed\n");
	if (idx > 0)
		crlmodule_undo_poweron_entities(sensor, idx-1);
	return rval;
}

static int crlmodule_set_power(struct v4l2_subdev *subdev, int on)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	int ret = 0;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put(&client->dev);
			return ret;
		}
	}

	mutex_lock(&sensor->power_mutex);
	if (on && !sensor->power_count) {
		usleep_range(2000, 3000);
		ret = crlmodule_run_poweron_init(sensor);
		if (ret < 0) {
			pm_runtime_put(&client->dev);
			goto out;
		}
	}

	/* Update the power count. */
	sensor->power_count += on ? 1 : -1;
	WARN_ON(sensor->power_count < 0);

out:
	mutex_unlock(&sensor->power_mutex);

	if (!on)
		pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_subdev_ops crlmodule_ops;
static const struct v4l2_subdev_internal_ops crlmodule_internal_ops;
static const struct media_entity_operations crlmodule_entity_ops;

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Modified based on the CRL Module changes
 */
static int crlmodule_init_subdevs(struct v4l2_subdev *subdev)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	struct crl_subdev *prev_sd = NULL;
	int i = 0, j;
	struct crl_subdev *sd;
	int rval = 0;

	dev_dbg(&client->dev, "%s\n", __func__);

	/*
	 * The scaler, binner and PA order matters. Sensor configuration file
	 * must maintain this order. PA sub dev is a must and binner and
	 * scaler can be omitted based on the sensor. But if scaler is present
	 * it must be the first sd.
	 */
	if (sensor->sensor_ds->subdevs[i].subdev_type
	    == CRL_SUBDEV_TYPE_SCALER) {
		sensor->scaler = &sensor->ssds[sensor->ssds_used];
		sensor->ssds_used++;
		i++;
	}

	if (sensor->sensor_ds->subdevs[i].subdev_type
	    == CRL_SUBDEV_TYPE_BINNER) {
		sensor->binner = &sensor->ssds[sensor->ssds_used];
		sensor->ssds_used++;
		i++;
	}

	if (sensor->sensor_ds->subdevs[i].subdev_type
	    == CRL_SUBDEV_TYPE_PIXEL_ARRAY) {
		sensor->pixel_array = &sensor->ssds[sensor->ssds_used];
		sensor->ssds_used++;
		i++;
	}

	/* CRL MediaCTL IF driver can't handle if none of these sd's present! */
	if (!sensor->ssds_used) {
		dev_err(&client->dev, "%s no subdevs present\n", __func__);
		return -ENODEV;
	}

	if (!sensor->sensor_ds->pll_config_items) {
		dev_err(&client->dev, "%s no pll configurations\n", __func__);
		return -ENODEV;
	}

	/* TODO validate rest of the settings from the sensor definition file */

	dev_dbg(&client->dev, "%s subdevs: %d\n", __func__, i);

	for (i = 0; i < sensor->ssds_used; i++) {
		bool has_substreams = false;

		sd = &sensor->ssds[i];

		if (sd != sensor->src)
			v4l2_subdev_init(&sd->sd, &crlmodule_ops);
		else if (sensor->sensor_ds->frame_desc_entries > 1)
			has_substreams = true;

		sd->sensor = sensor;

		if (sd == sensor->pixel_array) {
			sd->npads = 1;
		} else {
			sd->npads = 2;
			sd->source_pad = 1;
		}

		snprintf(sd->sd.name,
			 sizeof(sd->sd.name), "%s %d-%4.4x",
			 sensor->sensor_ds->subdevs[i].name,
			 i2c_adapter_id(client->adapter), client->addr);

		sd->sink_fmt.width =
			sensor->sensor_ds->sensor_limits->x_addr_max;
		sd->sink_fmt.height =
			sensor->sensor_ds->sensor_limits->y_addr_max;
		sd->compose.width = sd->sink_fmt.width;
		sd->compose.height = sd->sink_fmt.height;
		sd->crop[sd->source_pad] = sd->compose;
		sd->pads[sd->source_pad].flags = MEDIA_PAD_FL_SOURCE |
			(has_substreams ? MEDIA_PAD_FL_MULTIPLEX : 0);
		if (sd != sensor->pixel_array) {
			sd->crop[sd->sink_pad] = sd->compose;
			sd->pads[sd->sink_pad].flags = MEDIA_PAD_FL_SINK;
		}

		if (has_substreams) {
			sd->route_flags = devm_kzalloc(&client->dev,
				sizeof(unsigned int) *
				sensor->sensor_ds->frame_desc_entries,
				GFP_KERNEL);
			if (!sd->route_flags)
				return -ENOMEM;
			for (j = 0; j < sensor->sensor_ds->frame_desc_entries;
				j++)
				sd->route_flags[j] =
					V4L2_SUBDEV_ROUTE_FL_SOURCE;
			sd->route_flags[0] |=
					V4L2_SUBDEV_ROUTE_FL_ACTIVE;
		}

		sd->sd.entity.ops = &crlmodule_entity_ops;

		if (prev_sd == NULL) {
			prev_sd = sd;
			continue;
		}

		sd->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
		sd->sd.internal_ops = &crlmodule_internal_ops;
		sd->sd.owner = THIS_MODULE;
		v4l2_set_subdevdata(&sd->sd, client);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
		rval = media_entity_init(&sd->sd.entity, sd->npads,
					 sd->pads, 0);
#else
		rval = media_entity_pads_init(&sd->sd.entity, sd->npads,
					      sd->pads);
#endif
		if (rval) {
			dev_err(&client->dev,
				"media_entity_init failed\n");
			return rval;
		}

		rval = v4l2_device_register_subdev(sensor->src->sd.v4l2_dev,
						   &sd->sd);
		if (rval) {
			dev_err(&client->dev,
				"v4l2_device_register_subdev failed\n");
			return rval;
		}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
		rval = media_entity_create_link(&sd->sd.entity,
#else
		rval = media_create_pad_link(&sd->sd.entity,
#endif
						sd->source_pad,
						&prev_sd->sd.entity,
						prev_sd->sink_pad,
						MEDIA_LNK_FL_ENABLED |
						MEDIA_LNK_FL_IMMUTABLE);
		if (rval) {
			dev_err(&client->dev,
				"media_entity_create_link failed\n");
			return rval;
		}

		prev_sd = sd;
	}

	return rval;
}

static int __init_power_resources(struct v4l2_subdev *subdev)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	struct crl_power_seq_entity *entity;
	unsigned idx;

	sensor->pwr_entity = devm_kzalloc(&client->dev,
		sizeof(struct crl_power_seq_entity) *
		 sensor->sensor_ds->power_items, GFP_KERNEL);

	if (!sensor->pwr_entity)
		return -ENOMEM;

	for (idx = 0; idx < sensor->sensor_ds->power_items; idx++)
		sensor->pwr_entity[idx] =
		 sensor->sensor_ds->power_entities[idx];

	dev_dbg(&client->dev, "%s\n", __func__);

	for (idx = 0; idx < sensor->sensor_ds->power_items; idx++) {
		int rval;

		entity = &sensor->pwr_entity[idx];

		switch (entity->type) {
		case CRL_POWER_ETY_GPIO_FROM_PDATA:
			if (devm_gpio_request_one(&client->dev,
				sensor->platform_data->xshutdown, 0,
				"CRL xshutdown") != 0) {
				dev_err(&client->dev,
					"unable to acquire xshutdown %d\n",
					sensor->platform_data->xshutdown);
				return -ENODEV;
			}
			break;
		case CRL_POWER_ETY_GPIO_FROM_PDATA_BY_NUMBER:
			rval = custom_gpio_request(sensor);
			if (rval < 0)
				return rval;
			break;
		case CRL_POWER_ETY_GPIO_CUSTOM:
			if (entity->ent_name[0]) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
				entity->gpiod_priv = gpiod_get(NULL,
					entity->ent_name);
#else
				entity->gpiod_priv = gpiod_get(NULL,
					entity->ent_name, GPIOD_OUT_LOW);
#endif
				if (IS_ERR(entity->gpiod_priv)) {
					dev_err(&client->dev,
						"unable to acquire custom gpio %s\n",
						entity->ent_name);
					entity->gpiod_priv = NULL;
					return -ENODEV;
				}
			} else {
				if (devm_gpio_request_one(&client->dev,
					entity->ent_number, 0,
					"CRL Custom") != 0) {
					dev_err(&client->dev,
						"unable to acquire custom gpio %d\n",
						entity->ent_number);
					return -ENODEV;
				}
			}
			break;
		case CRL_POWER_ETY_REGULATOR_FRAMEWORK:
			entity->regulator_priv = devm_regulator_get(
					&client->dev, entity->ent_name);
			if (IS_ERR(entity->regulator_priv)) {
				dev_err(&client->dev,
					"Failed to get regulator: %s\n",
					entity->ent_name);
					entity->regulator_priv = NULL;
				return -ENODEV;
			}
			rval = regulator_set_voltage(entity->regulator_priv,
						     entity->val,
						     entity->val);
			/* Not all regulator supports voltage change */
			if (rval  < 0)
				dev_info(&client->dev,
					"Failed to set voltage %s %d\n",
					entity->ent_name, entity->val);
			break;
		case CRL_POWER_ETY_CLK_FRAMEWORK:
			sensor->xclk = devm_clk_get(&client->dev,
				entity->ent_name[0] ? entity->ent_name : NULL);
			if (IS_ERR(sensor->xclk)) {
				dev_err(&client->dev,
					"Cannot get sensor clk\n");
				return -ENODEV;
			}
			break;
		default:
			dev_err(&client->dev, "Invalid Power item\n");
			return -ENODEV;
		}
	}

	return 0;
}

static int crl_request_gpio_irq(struct crl_sensor *sensor)
{
	int rval;
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	int irq_pin = sensor->platform_data->crl_irq_pin;

	if (!gpio_is_valid(irq_pin)) {
		dev_err(&client->dev, "%s: GPIO pin %d is invalid!\n",
			__func__, irq_pin);
		return -ENODEV;
	}
	dev_dbg(&client->dev,
			"%s: IRQ GPIO %d is valid.\n", __func__, irq_pin);

	rval = devm_gpio_request(&client->dev, irq_pin,
				 sensor->platform_data->irq_pin_name);
	if (rval) {
		dev_err(&client->dev,
			"%s:IRQ GPIO pin request failed!\n", __func__);
		return rval;
	}

	gpio_direction_input(irq_pin);
	sensor->irq = gpio_to_irq(irq_pin);
	rval = devm_request_threaded_irq(&client->dev, sensor->irq,
					 sensor->sensor_ds->crl_irq_fn,
					 sensor->sensor_ds->crl_threaded_irq_fn,
					 sensor->platform_data->irq_pin_flags,
					 sensor->platform_data->irq_pin_name,
					 sensor);

	dev_dbg(&client->dev, "%s: GPIO register GPIO IRQ result: %d\n",
		__func__, rval);

	return rval;
}

static int crlmodule_registered(struct v4l2_subdev *subdev)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);

	int rval;

	rval = __init_power_resources(subdev);
	if (rval)
		return -ENODEV;

	pm_runtime_enable(&client->dev);

	/* Power up the sensor */
	if (pm_runtime_get_sync(&client->dev) < 0) {
		rval = -ENODEV;
		goto out;
	}

	/* init GPIO IRQ */
	if (sensor->sensor_ds->irq_in_use == true) {
		rval = crl_request_gpio_irq(sensor);
		if (rval) {
			rval = -ENODEV;
			goto out;
		}
	}

	/* one time init */
	rval = crlmodule_write_regs(sensor,
				    sensor->sensor_ds->onetime_init_regs,
				    sensor->sensor_ds->onetime_init_regs_items);
	if (rval) {
		dev_err(&client->dev, "%s failed to set powerup registers\n",
				      __func__);
		rval = -ENODEV;
		goto out;
	}

	/* sensor specific init */
	if (sensor->sensor_ds->sensor_init) {
		rval = sensor->sensor_ds->sensor_init(client);

		if (rval) {
			dev_err(&client->dev,
				"%s failed to run sensor specific init\n",
				__func__);
			rval = -ENODEV;
			goto out;
		}
	}
	/* Identify the module */
	rval = crlmodule_identify_module(subdev);
	if (rval) {
		rval = -ENODEV;
		goto out;
	}

	rval = crlmodule_init_subdevs(subdev);
	if (rval)
		goto out;

	sensor->binning_horizontal = 1;
	sensor->binning_vertical = 1;
	sensor->scale_m = 1;
	sensor->flip_info = CRL_FLIP_DEFAULT_NONE;
	sensor->ext_ctrl_impacts_pll_selection = false;
	sensor->ext_ctrl_impacts_mode_selection = false;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	sensor->pixel_array->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
#else
	sensor->pixel_array->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
#endif

	rval = crlmodule_init_controls(sensor);
	if (rval)
		goto out;

	mutex_lock(&sensor->mutex);
	crlmodule_update_current_mode(sensor);
	mutex_unlock(&sensor->mutex);
	rval = crlmodule_nvm_init(sensor);

out:
	dev_dbg(&client->dev, "%s rval: %d\n", __func__, rval);
	/* crlmodule_power_off(sensor); */
	pm_runtime_put(&client->dev);

	return rval;
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Slightly modified based on the CRL Module changes
 */
static int crlmodule_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct crl_subdev *ssd = to_crlmodule_subdev(sd);
	struct crl_sensor *sensor = ssd->sensor;
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	u32 mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10;
	unsigned int i;
	int rval;

	dev_dbg(&client->dev, "%s\n", __func__);

	mutex_lock(&sensor->mutex);

	for (i = 0; i < ssd->npads; i++) {
		struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_get_try_format(sd, fh->pad, i);
		struct v4l2_rect *try_crop = v4l2_subdev_get_try_crop(sd,
								fh->pad, i);
		struct v4l2_rect *try_comp;

		try_fmt->width = sensor->sensor_ds->sensor_limits->x_addr_max;
		try_fmt->height = sensor->sensor_ds->sensor_limits->y_addr_max;
		try_fmt->code = mbus_code;

		try_crop->top = 0;
		try_crop->left = 0;
		try_crop->width = try_fmt->width;
		try_crop->height = try_fmt->height;

		if (ssd != sensor->pixel_array)
			continue;

		try_comp = v4l2_subdev_get_try_compose(sd, fh->pad, i);
		*try_comp = *try_crop;
	}

	mutex_unlock(&sensor->mutex);


	rval = pm_runtime_get_sync(&client->dev);
	if (rval < 0)
		pm_runtime_put(&client->dev);
	return rval;
}

static int crlmodule_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	pm_runtime_put(&client->dev);

	return 0;
}

static int crlmodule_get_registers(struct v4l2_subdev *sd, struct crl_registers_info *info)
{
	struct crl_subdev *ssd = to_crlmodule_subdev(sd);
	struct crl_sensor *sensor = ssd->sensor;
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	struct crl_register_read_rep reg;
	int i;
	int ret = 0;

	if (info->number > REGS_BUF_SIZE) {
		dev_err(&client->dev, "error: max register's numbers than %d\n", REGS_BUF_SIZE);
		return -1;
	}

	for (i = 0; i < info->number; i++) {
		reg.address = info->start_address + i;
		reg.dev_i2c_addr = CRL_I2C_ADDRESS_NO_OVERRIDE;
		reg.len = info->len;
		reg.mask = 0xff;
		ret = crlmodule_read_reg(sensor, reg, &info->regs[i]);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static int crlmodule_set_registers(struct v4l2_subdev *sd, struct crl_registers_info *info)
{
	struct crl_subdev *ssd = to_crlmodule_subdev(sd);
	struct crl_sensor *sensor = ssd->sensor;
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->src->sd);
	int i;
	int ret = 0;

	if (info->number > REGS_BUF_SIZE) {
		dev_err(&client->dev, "error: max register's numbers than %d\n", REGS_BUF_SIZE);
		return -1;
	}

	for (i = 0; i < info->number; i++) {
		ret = crlmodule_write_reg(sensor, CRL_I2C_ADDRESS_NO_OVERRIDE,
				info->start_address + i, info->len, 0xff, info->regs[i]);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static long crlmodule_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret;

	switch (cmd) {
	case CRL_G_REGISTERS:
		ret = crlmodule_get_registers(sd, arg);
		break;
	case CRL_S_REGISTERS:
		ret = crlmodule_set_registers(sd, arg);
		break;
	default:
		ret = -1;
		break;
	};

	return ret;
}

static const struct v4l2_subdev_video_ops crlmodule_video_ops = {
	.s_stream = crlmodule_set_stream,
};

static const struct v4l2_subdev_core_ops crlmodule_core_ops = {
	.s_power = crlmodule_set_power,
	.ioctl = crlmodule_ioctl,
};

static const struct v4l2_subdev_pad_ops crlmodule_pad_ops = {
	.enum_mbus_code = crlmodule_enum_mbus_code,
	.get_fmt = crlmodule_get_format,
	.set_fmt = crlmodule_set_format,
	.get_selection = crlmodule_get_selection,
	.set_selection = crlmodule_set_selection,
	.enum_frame_size = crlmodule_enum_frame_size,
	.get_frame_desc = crlmodule_get_frame_desc,
	.get_routing = crlmodule_get_routing,
	.set_routing = crlmodule_set_routing,
};

static const struct v4l2_subdev_sensor_ops crlmodule_sensor_ops = {
	.g_skip_frames = crlmodule_get_skip_frames,
};

static const struct v4l2_subdev_ops crlmodule_ops = {
	.core = &crlmodule_core_ops,
	.video = &crlmodule_video_ops,
	.pad = &crlmodule_pad_ops,
	.sensor = &crlmodule_sensor_ops,
};

static const struct media_entity_operations crlmodule_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_subdev_internal_ops crlmodule_internal_src_ops = {
	.registered = crlmodule_registered,
	.open = crlmodule_open,
	.close = crlmodule_close,
};

static const struct v4l2_subdev_internal_ops crlmodule_internal_ops = {
	.open = crlmodule_open,
	.close = crlmodule_close,
};

#ifdef CONFIG_PM

static int crlmodule_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct crl_sensor *sensor = to_crlmodule_sensor(sd);

	crlmodule_undo_poweron_entities(sensor,
					sensor->sensor_ds->power_items - 1);
	return 0;
}

static int crlmodule_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct crl_sensor *sensor = to_crlmodule_sensor(sd);

	return __crlmodule_powerup_sequence(sensor);
}

static int crlmodule_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct crl_subdev *ssd = to_crlmodule_subdev(sd);
	struct crl_sensor *sensor = ssd->sensor;

	if (sensor->streaming)
		crlmodule_stop_streaming(sensor);

	if (sensor->power_count > 0)
		crlmodule_undo_poweron_entities(sensor,
					sensor->sensor_ds->power_items - 1);
	return 0;
}

static int crlmodule_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct crl_subdev *ssd = to_crlmodule_subdev(sd);
	struct crl_sensor *sensor = ssd->sensor;
	int rval = 0;

	if (sensor->power_count > 0) {
		rval = __crlmodule_powerup_sequence(sensor);
		if (!rval)
			rval = crlmodule_run_poweron_init(sensor);
	}

	if (!rval && sensor->streaming)
		rval = crlmodule_start_streaming(sensor);

	return rval;
}
#else
#define crlmodule_runtime_suspend	NULL
#define crlmodule_runtime_resume	NULL
#define crlmodule_suspend	NULL
#define crlmodule_resume	NULL
#endif /* CONFIG_PM */

static int crlmodule_probe(struct i2c_client *client,
			   const struct i2c_device_id *devid)
{
	struct crl_sensor *sensor;
	int ret;
#ifdef CONFIG_INTEL_IPU4_OV13858
	vcm_in_use = false;
#endif

	if (client->dev.platform_data == NULL)
		return -ENODEV;

	/* TODO! Create the sensor based on the interface */
	sensor = devm_kzalloc(&client->dev, sizeof(*sensor), GFP_KERNEL);
	if (sensor == NULL)
		return -ENOMEM;

	sensor->platform_data = client->dev.platform_data;
	mutex_init(&sensor->mutex);
	mutex_init(&sensor->power_mutex);

	ret = crlmodule_populate_ds(sensor, &client->dev);
	if (ret)
		return -ENODEV;

	sensor->src = &sensor->ssds[sensor->ssds_used];

	v4l2_i2c_subdev_init(&sensor->src->sd, client, &crlmodule_ops);
	sensor->src->sd.internal_ops = &crlmodule_internal_src_ops;
	sensor->src->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	if (sensor->sensor_ds->frame_desc_entries > 1)
		sensor->src->sd.flags |= V4L2_SUBDEV_FL_HAS_SUBSTREAMS;

	sensor->src->sensor = sensor;

	sensor->src->pads[0].flags = MEDIA_PAD_FL_SOURCE;
	if (sensor->sensor_ds->frame_desc_entries > 1)
		sensor->src->sd.flags |= MEDIA_PAD_FL_MULTIPLEX;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	ret = media_entity_init(&sensor->src->sd.entity, 2,
				sensor->src->pads, 0);
#else
	ret = media_entity_pads_init(&sensor->src->sd.entity, 2,
				sensor->src->pads);
#endif
	if (ret < 0)
		goto cleanup;
	ret = v4l2_async_register_subdev(&sensor->src->sd);
	if (ret < 0)
		goto cleanup;

	/* Load IQ tuning registers from drvb file*/
	if (sensor->sensor_ds->msr_file_name) {
		ret = crlmodule_load_msrlist(client,
			sensor->sensor_ds->msr_file_name,
			&sensor->msr_list);
		if (ret)
			dev_warn(&client->dev,
				"msrlist loading failed. Ignore, move on\n");
	} else {
		/* sensor will still continue streaming */
		dev_warn(&client->dev, "No msrlists associated with sensor\n");
	}

	return 0;

cleanup:
	media_entity_cleanup(&sensor->src->sd.entity);
	crlmodule_release_ds(sensor);
	return ret;
}

static void crlmodule_free_controls(struct crl_sensor *sensor)
{
	unsigned int i;

	for (i = 0; i < sensor->ssds_used; i++)
		v4l2_ctrl_handler_free(&sensor->ssds[i].ctrl_handler);
}

static int crlmodule_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	unsigned int i;

	if (sensor->sensor_ds->sensor_cleanup)
		sensor->sensor_ds->sensor_cleanup(client);

	v4l2_async_unregister_subdev(&sensor->src->sd);
	for (i = 0; i < sensor->ssds_used; i++) {
		v4l2_device_unregister_subdev(&sensor->ssds[i].sd);
		media_entity_cleanup(&sensor->ssds[i].sd.entity);
	}

	for (i = 0; i < sensor->sensor_ds->power_items; i++) {
		struct crl_power_seq_entity *entity =
			&sensor->pwr_entity[i];

		if (entity->type == CRL_POWER_ETY_GPIO_CUSTOM  &&
				entity->gpiod_priv)
			gpiod_put(entity->gpiod_priv);
	}

	crlmodule_nvm_deinit(sensor);
	crlmodule_release_ds(sensor);
	crlmodule_free_controls(sensor);
	crlmodule_release_msrlist(&sensor->msr_list);

	pm_runtime_disable(&client->dev);

	return 0;
}


static const struct i2c_device_id crlmodule_id_table[] = {
	{ CRLMODULE_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, crlmodule_id_table);

static const struct dev_pm_ops crlmodule_pm_ops = {
	.runtime_suspend = crlmodule_runtime_suspend,
	.runtime_resume = crlmodule_runtime_resume,
	.suspend	= crlmodule_suspend,
	.resume		= crlmodule_resume,
};

static struct i2c_driver crlmodule_i2c_driver = {
	.driver	= {
		.name = CRLMODULE_NAME,
		.pm = &crlmodule_pm_ops,
	},
	.probe	= crlmodule_probe,
	.remove	= crlmodule_remove,
	.id_table = crlmodule_id_table,
};

module_i2c_driver(crlmodule_i2c_driver);

MODULE_AUTHOR("Vinod Govindapillai <vinod.govindapillai@intel.com>");
MODULE_AUTHOR("Jouni Ukkonen <jouni.ukkonen@intel.com>");
MODULE_AUTHOR("Tommi Franttila <tommi.franttila@intel.com>");
MODULE_DESCRIPTION("Generic driver for common register list based \
			camera sensor modules");
MODULE_LICENSE("GPL");
