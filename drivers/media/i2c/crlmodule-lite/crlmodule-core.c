// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <media/ici.h>

#include "crlmodule.h"
#include "crlmodule-nvm.h"
#include "crlmodule-regs.h"
#include "crlmodule-msrlist.h"

static int init_ext_sd(struct i2c_client *client, 
	struct crl_subdev *ssd, int idx);
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
 * Find the index of the ctrl pointer from the array of ctrls
 * maintained by the CRL module based on the ctrl id.
 */
static int __crlmodule_get_crl_ctrl_index(struct crl_sensor *sensor,
					  u32 id, unsigned int *index)
{
	unsigned int i;

	for (i = 0; i < sensor->sensor_ds->ctrl_items; i++)
		if (sensor->ctrl_bank[i].ctrl_id == id)
			break;

	if (i >= sensor->sensor_ds->ctrl_items)
		return -EINVAL;

	*index = i;
	return 0;
}

/*
 * Finds the value of a specific ctrl based on the ctrl-id
 */
static int __crlmodule_get_param_value(struct crl_sensor *sensor,
				      u32 id, u32 *val)
{
	struct i2c_client *client = sensor->src->sd.client;
	unsigned int i;
	int ret;
	struct ici_ext_sd_param* param;
	
	ret = __crlmodule_get_crl_ctrl_index(sensor, id, &i);
	if (ret)
		return ret;

	/* If no corresponding ctrl created, return */
	if (sensor->ctrl_bank[i].param.id != id) {
		dev_dbg(&client->dev,
			"%s ctrl_id: 0x%x desc: %s not ready\n", __func__, id,
			sensor->ctrl_bank[i].name);
		return -ENODATA;
	}

	param = &sensor->ctrl_bank[i].param;
	switch (sensor->ctrl_bank[i].type) {
	case CRL_CTRL_TYPE_MENU_INT:
		if (param->val <= sensor->ctrl_bank[i].data.int_menu.max)
			*val = sensor->ctrl_bank[i].data.int_menu.menu[param->val];
		else
			*val = 0;
		break;
	case CRL_CTRL_TYPE_INTEGER:
	default:
		*val = param->val;
		break;
	}


	dev_dbg(&client->dev, "%s ctrl_id: 0x%x desc: %s val: %d\n",
			       __func__, id,
			       sensor->ctrl_bank[i].name, *val);
	return 0;
}

/*
 * Finds the v4l2 based on the control id
 */
static struct crl_ctrl_data *__crlmodule_get_ctrl(
	struct crl_sensor *sensor,
	u32 id)
{
	unsigned int i;

	if (__crlmodule_get_crl_ctrl_index(sensor, id, &i))
		return NULL;

	return &sensor->ctrl_bank[i];
}

/*
 * Grab / Release controls based on the ctrl update context
 */
static void __crlmodule_enable_param(struct crl_sensor *sensor,
				     enum crl_ctrl_update_context ctxt,
				     bool enable)
{
	struct crl_ctrl_data *crl_ctrl;
	unsigned int i;

	for (i = 0; i < sensor->sensor_ds->ctrl_items; i++) {
		crl_ctrl = &sensor->ctrl_bank[i];

		if (crl_ctrl->context == ctxt)
			crl_ctrl->enabled = enable;
	}
}

/*
 * Checks if the ctrl sepecific data is satisfied in the mode and PLL
 * selection logic.
 */
static bool __crlmodule_compare_ctrl_specific_data(
			struct crl_sensor *sensor,
			unsigned int items,
			struct crl_ctrl_data_pair *ctrl_val)
{
	struct i2c_client *client = sensor->src->sd.client;
	unsigned int i;
	u32 val;
	int ret;

	/* Go through all the controls associated with this config */
	for (i = 0; i < items; i++) {
		/* Get the value set for the control */
		ret = __crlmodule_get_param_value(sensor, ctrl_val[i].ctrl_id,
						 &val);
		if (ret) {
			dev_err(&client->dev, "%s ctrl_id: 0x%x not found\n",
				__func__, ctrl_val[i].ctrl_id);
			return false;
		}

		/* Compare the value from the sensor definition file config */
		if (val != ctrl_val[i].data) {
			dev_err(&client->dev,
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
static int __crlmodule_update_pll_index(struct crl_sensor *sensor,
	struct crl_ctrl_data *crl_ctrl)
{
	struct i2c_client *client = sensor->src->sd.client;
	const struct crl_pll_configuration *pll_config;
	const struct crl_csi_data_fmt *fmts =
		     &sensor->sensor_ds->csi_fmts[sensor->fmt_index];
	unsigned int i;
	u32 link_freq = 0;
	
	if (!sensor->link_freq ||
		sensor->link_freq->type != CRL_CTRL_TYPE_MENU_INT) {
		dev_err(&client->dev, "%s Invalid link freq ctrl\n",
			__func__);
		return -EINVAL;
	}
			
	sensor->link_freq->param.val = crl_ctrl->param.val;
	if (crl_ctrl->param.val <=
		sensor->link_freq->data.int_menu.max) {
		link_freq =sensor->link_freq->data.int_menu.menu[
			crl_ctrl->param.val];
	}
	
	dev_dbg(&client->dev, "%s PLL Items: %d link_freq: %d\n",
		__func__, sensor->sensor_ds->pll_config_items,
		link_freq);

	for (i = 0; i < sensor->sensor_ds->pll_config_items; i++) {
		pll_config = &sensor->sensor_ds->pll_configs[i];

		if (pll_config->op_sys_clk != link_freq)
			continue;

		if (pll_config->input_clk != sensor->platform_data->ext_clk)
			continue;

		/* if pll_config->csi_lanes == 0, lanes do not matter */
		if (pll_config->csi_lanes)
			if (sensor->platform_data->lanes != pll_config->csi_lanes)
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
		sensor->pixel_rate_pa->param.s64val = pll_config->pixel_rate_pa;
		sensor->pixel_rate_csi->param.s64val = pll_config->pixel_rate_csi;
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
		return __crlmodule_get_param_value(sensor,
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
	struct i2c_client *client = sensor->src->sd.client;
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
			if(operand==0) {
				dev_err(&client->dev, "CRL_DIV error for operand returned is zero.");
				return -EINVAL;
			}
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
 * This kind of registers are mainly used in crlmodule's ctrl logic.
 *
 * This is to handle cases like the below examples, where mutliple registers
 * need to be modified based on the input value "val"
 * R3000 = val & 0xff and R3001 = val >> 8 & 0xff and R3002 = val >> 16 & 0xff
 * R4001 = val and R4002 = val or
 * R2800 = FLL - val and R2802 = LLP - val
 */
static int __crlmodule_update_dynamic_regs(struct crl_sensor *sensor,
					struct crl_ctrl_data *crl_ctrl,
					unsigned int val)
{
	unsigned int i;

	for (i = 0; i < crl_ctrl->regs_items; i++) {
		struct crl_dynamic_register_access *reg = &crl_ctrl->regs[i];
		/*
		 * Each register group must start from the initial value, not
		 * as a continuation of the previous calculations. The sensor
		 * configurations must take care of this restriction.
		 */
		u32 val_t = val;
		int ret;

		/* Get the value associated with the dynamic entity */
		ret = __crlmodule_calc_dynamic_entity_values(sensor,
							     reg->ops_items,
							     reg->ops, &val_t);
		if (ret)
			return ret;

		/* Now ready to write the value */
		ret = crlmodule_write_reg(sensor, reg->dev_i2c_addr,
					reg->address, reg->len,
					reg->mask, val_t);
		if (ret)
			return ret;
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
					   struct crl_ctrl_data *crl_ctrl,
					   unsigned int *val,
					   enum crl_dep_ctrl_action_type type)
{
	struct i2c_client *client = sensor->src->sd.client;
	struct crl_ctrl_data *dep_crl_ctrl;
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
		ret = __crlmodule_get_param_value(sensor, dep_prov->ctrl_id,
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

			dep_crl_ctrl = &sensor->ctrl_bank[idx];
			dev_dbg(&client->dev,
				"%s crl_ctrl: 0x%p 0x%p\n", __func__,
				&sensor->ctrl_bank[idx],
				dep_crl_ctrl);

			ret = __crlmodule_update_dynamic_regs(sensor,
							dep_crl_ctrl, dep_val);
			if (ret)
				continue;
		}
	}
	return 0;
}


static int crlmodule_get_fmt_index(struct crl_sensor *sensor,
				   u8 pixel_order, u8 bpp)
{
	struct i2c_client *client = sensor->src->sd.client;
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
					struct crl_ctrl_data *crl_ctrl,
					struct ici_ext_sd_param *param)
{
	struct i2c_client *client = sensor->src->sd.client;
	const struct crl_csi_data_fmt *fmt =
		&sensor->sensor_ds->csi_fmts[sensor->fmt_index];
	u8 bpp = fmt->bits_per_pixel;
	u8 flip_info = sensor->flip_info;
	u8 new_order = 0;
	int i, ret;

	dev_dbg(&client->dev, "%s current flip_info: %d curr index: %d\n",
			       __func__, flip_info, sensor->fmt_index);

	switch (param->id) {
	case ICI_EXT_SD_PARAM_ID_HFLIP:
		flip_info &= CRL_FLIP_HFLIP_MASK;
		flip_info |= param->val > 0 ? CRL_FLIP_HFLIP : 0;
		break;
	case ICI_EXT_SD_PARAM_ID_VFLIP:
		flip_info &= CRL_FLIP_VFLIP_MASK;
		flip_info |= param->val > 0 ? CRL_FLIP_VFLIP : 0;
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

	ret = __crlmodule_update_dynamic_regs(sensor, crl_ctrl, param->val);
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
					struct crl_ctrl_data *crl_ctrl,
					struct ici_ext_sd_param *param)
{
	const struct crl_mode_rep *mode = sensor->current_mode;
	unsigned int val;

	switch (param->id) {
	case ICI_EXT_SD_PARAM_ID_FRAME_LENGTH_LINES:
		val = max(param->val, mode->min_fll);
		break;
	case ICI_EXT_SD_PARAM_ID_LINE_LENGTH_PIXELS:
		val = max(param->val, mode->min_llp);
		break;
	default:
		return -EINVAL;
	}

	return __crlmodule_update_dynamic_regs(sensor, crl_ctrl, val);
}
static int __crlmodule_update_blanking(struct crl_sensor *sensor,
					struct crl_ctrl_data *crl_ctrl,
					struct ici_ext_sd_param *param)
{
	unsigned int val;

	switch (param->id) {
	case ICI_EXT_SD_PARAM_ID_HBLANK:
		val = sensor->pixel_array->crop[CRL_PA_PAD_SRC].width +
		      param->val;
		break;
	case ICI_EXT_SD_PARAM_ID_VBLANK:
		val = sensor->pixel_array->crop[CRL_PA_PAD_SRC].height +
		      param->val;
		break;
	default:
		return -EINVAL;
	}

	return __crlmodule_update_dynamic_regs(sensor, crl_ctrl, val);
}

static void __crlmodule_update_selection_impact_flags(
				struct crl_sensor *sensor,
				struct crl_ctrl_data *crl_ctrl)
{
	if (crl_ctrl->impact & CRL_IMPACTS_PLL_SELECTION)
		sensor->ext_ctrl_impacts_pll_selection = true;

	if (crl_ctrl->impact & CRL_IMPACTS_MODE_SELECTION)
		sensor->ext_ctrl_impacts_mode_selection = true;
}

static struct crl_ctrl_data *__crlmodule_find_crlctrl(
						struct crl_sensor *sensor,
						struct ici_ext_sd_param *param)
{
	struct crl_ctrl_data *crl_ctrl;
	unsigned int i;

	for (i = 0; i < sensor->sensor_ds->ctrl_items; i++) {
		crl_ctrl = &sensor->ctrl_bank[i];
		if (crl_ctrl->param.sd == param->sd &&
		    crl_ctrl->ctrl_id == param->id)
			return crl_ctrl;
	}

	return NULL;
}

static int crlmodule_set_param(struct ici_ext_sd_param *param)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(param->sd);
	struct i2c_client *client = sensor->src->sd.client;
	struct crl_ctrl_data *crl_ctrl = NULL;
	int ret = 0;

	dev_dbg(&client->dev, "%s id:%d val:%d\n", __func__, param->id,
			      param->val);

	/*
	 * Need to find the corresponding crlmodule wrapper for this param.
	 */
	crl_ctrl = __crlmodule_find_crlctrl(sensor, param);
	if (!crl_ctrl) {
		dev_err(&client->dev, "%s ctrl :0x%x not supported\n",
				      __func__, param->id);
		return -EINVAL;
	}

	dev_dbg(&client->dev, "%s id:0x%x name:%s\n", __func__, param->id,
			      crl_ctrl->name);

	if (!crl_ctrl->enabled ||
		crl_ctrl->flags & CRL_CTRL_FLAG_READ_ONLY) {
		dev_err(&client->dev, "%s Control id:0x%x is not writeable\n",
				      __func__, param->id);
		return -EINVAL;
	}

	if (param->type != ICI_EXT_SD_PARAM_TYPE_INT32) {
		dev_err(&client->dev, "%s Control id:0x%x only INT32 is supported\n",
				      __func__, param->id);
		return -EINVAL;
	}

	crl_ctrl->param.val = param->val;
	
	/* Then go through the mandatory controls */
	switch (param->id) {
	case ICI_EXT_SD_PARAM_ID_LINK_FREQ:
		/* Go through the supported list and compare the values */
		ret = __crlmodule_update_pll_index(sensor, crl_ctrl);
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
	__crlmodule_handle_dependency_ctrl(sensor, crl_ctrl, &param->val,
				     CRL_DEP_CTRL_ACTION_TYPE_SELF);

	/* Handle specific controls */
	switch (param->id) {
	case ICI_EXT_SD_PARAM_ID_HFLIP:
	case ICI_EXT_SD_PARAM_ID_VFLIP:
		ret = __crlmodule_update_flip_info(sensor, crl_ctrl, param);
		goto out;

	case ICI_EXT_SD_PARAM_ID_VBLANK:
	case ICI_EXT_SD_PARAM_ID_HBLANK:
		if (sensor->blanking_ctrl_not_use) {
			dev_info(&client->dev, "%s Blanking controls are not used \
			in this configuration, setting them has no effect\n", __func__);
			/* Disable control*/
			crl_ctrl->enabled = false;

		} else {
			ret = __crlmodule_update_blanking(sensor, crl_ctrl, param);
		}
		goto out;

	case ICI_EXT_SD_PARAM_ID_FRAME_LENGTH_LINES:
	case ICI_EXT_SD_PARAM_ID_LINE_LENGTH_PIXELS:
		ret = __crlmodule_update_framesize(sensor, crl_ctrl, param);
		goto out;

	case ICI_EXT_SD_PARAM_ID_SENSOR_MODE:
		sensor->sensor_mode = param->val;
		crlmodule_update_current_mode(sensor);
		goto out;
	}

	ret = __crlmodule_update_dynamic_regs(sensor, crl_ctrl, param->val);

out:
	/*
	 * Now check in the dependency control list, if the action type is
	 * "dependency control" and update the value accordingly now
	 */
	if (!ret && crl_ctrl)
		__crlmodule_handle_dependency_ctrl(sensor, crl_ctrl, &param->val,
					     CRL_DEP_CTRL_ACTION_TYPE_DEP_CTRL);

	return ret;
}

static int crlmodule_get_param(struct ici_ext_sd_param *param)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(param->sd);
	struct i2c_client *client = sensor->src->sd.client;
	struct crl_ctrl_data *crl_ctrl;
	struct crl_dynamic_register_access *reg;

	/*
	 * Need to find the corresponding crlmodule wrapper for this param.
	 */
	crl_ctrl = __crlmodule_find_crlctrl(sensor, param);
	if (!crl_ctrl) {
		dev_err(&client->dev, "%s ctrl :0x%x not supported\n",
				      __func__, param->id);
		return -EINVAL;
	}

	dev_dbg(&client->dev, "%s id:0x%x name:%s\n", __func__, param->id,
			      crl_ctrl->name);

	if (crl_ctrl->flags & CRL_CTRL_FLAG_WRITE_ONLY) {
		dev_err(&client->dev, "%s Control id:0x%x is not readable\n",
				      __func__, param->id);
		return -EINVAL;
	}
	
	param->type = ICI_EXT_SD_PARAM_TYPE_INT32;
	if (!(crl_ctrl->flags & CRL_CTRL_FLAG_READ_ONLY)) {
		param->val = crl_ctrl->param.val;
		return 0;
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
	if (!crl_ctrl->regs || !crl_ctrl->regs_items) {
		dev_err(&client->dev, "%s no dynamic entities found\n",
			 __func__);
		return -EINVAL;
	}
	if (crl_ctrl->regs_items > 1)
		dev_warn(&client->dev,
			 "%s multiple dynamic entities, will skip the rest\n",
			 __func__);
	reg = &crl_ctrl->regs[0];

	/* Get the value associated with the dynamic entity */
	return  __crlmodule_calc_dynamic_entity_values(sensor, reg->ops_items,
						       reg->ops, &param->val);
}

static int crlmodule_get_menu_item(
	struct ici_ext_sd_param *param, u32 idx)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(param->sd);
	struct i2c_client *client = sensor->src->sd.client;
	struct crl_ctrl_data *crl_ctrl;
	
	crl_ctrl = __crlmodule_find_crlctrl(sensor, param);
	if (!crl_ctrl) {
		dev_err(&client->dev, "%s ctrl :0x%x not supported\n",
				      __func__, param->id);
		return -EINVAL;
	}

	if (idx > crl_ctrl->max) {
		dev_err(&client->dev, "%s Control id:0x%x has invalid index %u\n",
				      __func__, param->id, idx);
		return -EINVAL;
	}
	switch (crl_ctrl->type)
	{
	case CRL_CTRL_TYPE_MENU_INT:
		param->type = ICI_EXT_SD_PARAM_TYPE_INT64;
		param->s64val = crl_ctrl->data.int_menu.menu[idx];
		break;
	case CRL_CTRL_TYPE_MENU_ITEMS:
		if (!param->custom.size || !param->custom.data) {
			dev_err(&client->dev, "%s Control id:0x%x param->custom.data must be preallocated by caller\n",
					__func__, param->id);
			return -EINVAL;
		}
		param->type = ICI_EXT_SD_PARAM_TYPE_STR;
		strncpy(param->custom.data,
			crl_ctrl->data.menu_items.menu[idx],
			param->custom.size - 1);
		param->custom.data[param->custom.size - 1] = '\0';
		break;
	default:
		dev_err(&client->dev, "%s Control id:0x%x does not have a menu\n",
				      __func__, param->id);
		return -EINVAL;
	}
	return 0;
}		

static int __crlmodule_init_link_freq_ctrl_menu(
					struct crl_sensor *sensor,
					struct crl_ctrl_data *crl_ctrl)
{
	struct i2c_client *client = sensor->src->sd.client;
	unsigned int items = 0;
	unsigned int i;

	/* Cannot handle if the control type is not integer menu */
	if (crl_ctrl->type != CRL_CTRL_TYPE_MENU_INT)
		return 0;

	/* If the menu contents exist, skip filling it dynamically */
	if (crl_ctrl->data.int_menu.menu)
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

	crl_ctrl->data.int_menu.menu = sensor->link_freq_menu;

	/* items will not be 0 as there will be atleast one pll_config_item */
	crl_ctrl->data.int_menu.max = items - 1;

	return 0;
}

static int crlmodule_init_controls(struct crl_sensor *sensor)
{
	struct i2c_client *client = sensor->src->sd.client;
	unsigned int pa_ctrls = 0;
	unsigned int src_ctrls = 0;
	struct crl_ctrl_data *crl_ctrl;
	unsigned int i;
	int rval;

	sensor->ctrl_bank = devm_kzalloc(&client->dev,
		sizeof(struct crl_ctrl_data) *
		 sensor->sensor_ds->ctrl_items,
		 GFP_KERNEL);
	if (!sensor->ctrl_bank)
		return -ENOMEM;

	/* Prepare to initialise the ctrls from the crl wrapper */
	for (i = 0; i < sensor->sensor_ds->ctrl_items; i++) {
		/*
		 * First copy the ctrls to the sensor as there could be
		 * more than one similar sensors in a product which could share
		 * the same configuration files
		 */
		sensor->ctrl_bank[i] =
				sensor->sensor_ds->ctrl_bank[i];

		crl_ctrl = &sensor->ctrl_bank[i];
		crl_ctrl->param.id = crl_ctrl->ctrl_id;
		if (crl_ctrl->sd_type == CRL_SUBDEV_TYPE_PIXEL_ARRAY) {
			if (sensor->pixel_array) {
				crl_ctrl->param.sd =
					&sensor->pixel_array->sd;
			}
			pa_ctrls++;
		}

		if (crl_ctrl->sd_type == CRL_SUBDEV_TYPE_SCALER) {
			if (sensor->scaler) {
				crl_ctrl->param.sd =
					&sensor->scaler->sd;
			}
			src_ctrls++;
		}
		if (crl_ctrl->sd_type == CRL_SUBDEV_TYPE_BINNER) {
			if (sensor->binner) {
				crl_ctrl->param.sd =
					&sensor->binner->sd;
			}
			src_ctrls++;
		}

		/* populate the ctrl for the Link_freq dynamically */
		if (crl_ctrl->ctrl_id == ICI_EXT_SD_PARAM_ID_LINK_FREQ &&
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
	for (i = 0; i < sensor->sensor_ds->ctrl_items; i++) {
		crl_ctrl = &sensor->ctrl_bank[i];
		switch (crl_ctrl->type) {
		case CRL_CTRL_TYPE_MENU_ITEMS:
			crl_ctrl->max = crl_ctrl->data.menu_items.size - 1;
			break;
		case CRL_CTRL_TYPE_MENU_INT:
			crl_ctrl->max = crl_ctrl->data.int_menu.max;
			crl_ctrl->def = crl_ctrl->data.int_menu.def;
			break;
		case CRL_CTRL_TYPE_INTEGER64:
		case CRL_CTRL_TYPE_INTEGER:
		case CRL_CTRL_TYPE_CUSTOM:
			crl_ctrl->min = crl_ctrl->data.std_data.min;
			crl_ctrl->max = crl_ctrl->data.std_data.max;
			crl_ctrl->step = crl_ctrl->data.std_data.step;
			crl_ctrl->def = crl_ctrl->data.std_data.def;
			break;
		case CRL_CTRL_TYPE_BOOLEAN:
		case CRL_CTRL_TYPE_BUTTON:
		case CRL_CTRL_TYPE_CTRL_CLASS:
		default:
			dev_err(&client->dev,
				"%s Invalid control type\n", __func__);
			continue;
			break;
		}

		/*
		 * Blanking and framesize controls access to same register,
		 * Blank controls are disabled if framesize controls exists.
		 */
		if (crl_ctrl->ctrl_id == ICI_EXT_SD_PARAM_ID_FRAME_LENGTH_LINES ||
		    crl_ctrl->ctrl_id == ICI_EXT_SD_PARAM_ID_LINE_LENGTH_PIXELS)
		    sensor->blanking_ctrl_not_use = 1;

		if (crl_ctrl->ctrl_id == ICI_EXT_SD_PARAM_ID_SENSOR_MODE)
			sensor->direct_mode_in_use = 1;

		/* Save mandatory control references - link_freq in src sd */
		if (crl_ctrl->ctrl_id == ICI_EXT_SD_PARAM_ID_LINK_FREQ &&
		    (crl_ctrl->sd_type == CRL_SUBDEV_TYPE_SCALER ||
		     crl_ctrl->sd_type == CRL_SUBDEV_TYPE_BINNER))
		     sensor->link_freq = crl_ctrl;

		/* Save mandatory control references - pixel_rate_pa PA sd */
		if (crl_ctrl->ctrl_id == ICI_EXT_SD_PARAM_ID_PIXEL_RATE &&
		    crl_ctrl->sd_type == CRL_SUBDEV_TYPE_PIXEL_ARRAY)
			sensor->pixel_rate_pa = crl_ctrl;

		/* Save mandatory control references - pixel_rate_csi src sd */
		if (crl_ctrl->ctrl_id == ICI_EXT_SD_PARAM_ID_PIXEL_RATE &&
		    (crl_ctrl->sd_type == CRL_SUBDEV_TYPE_SCALER ||
		     crl_ctrl->sd_type == CRL_SUBDEV_TYPE_BINNER))
			sensor->pixel_rate_csi = crl_ctrl;

		dev_dbg(&client->dev,
			"%s idx: %d ctrl_id: 0x%x ctrl_name: %s\n",
			__func__, i, crl_ctrl->ctrl_id, crl_ctrl->name);
	}

	return 0;
}


static bool __crlmodule_rect_matches(struct i2c_client *client,
				     const struct ici_rect *const rect1,
				     const struct ici_rect *const rect2)
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

static int __crlmodule_update_hblank(struct crl_sensor *sensor,
				      struct crl_ctrl_data *hblank)
{
	const struct crl_mode_rep *mode = sensor->current_mode;
	const struct crl_sensor_limits *limits = sensor->sensor_ds->sensor_limits;
	unsigned int width = sensor->pixel_array->crop[CRL_PA_PAD_SRC].width;
	unsigned int min_llp, max_llp;

	if (mode->min_llp)
		min_llp = mode->min_llp; /* mode specific limit */
	else if (limits->min_line_length_pixels)
		min_llp = limits->min_line_length_pixels; /* sensor limit */
	else /* No restrictions */
		min_llp = width;

	if (mode->max_llp)
		max_llp = mode->max_llp; /* mode specific limit */
	else if (limits->min_line_length_pixels)
		max_llp = limits->max_line_length_pixels; /* sensor limit */
	else /* No restrictions */
		max_llp = USHRT_MAX;

	hblank->min = min_llp - width;
	hblank->max = max_llp - width;
	hblank->def = hblank->min;
	return 0;
}

static int __crlmodule_update_vblank(struct crl_sensor *sensor,
				      struct crl_ctrl_data *vblank)
{
	const struct crl_mode_rep *mode = sensor->current_mode;
	const struct crl_sensor_limits *limits = sensor->sensor_ds->sensor_limits;
	unsigned int height = sensor->pixel_array->crop[CRL_PA_PAD_SRC].height;
	unsigned int min_fll, max_fll;

	if (mode->min_fll)
		min_fll = mode->min_fll; /* mode specific limit */
	else if (limits->min_frame_length_lines)
		min_fll = limits->min_frame_length_lines; /* sensor limit */
	else /* No restrictions */
		min_fll = height;

	if (mode->max_fll)
		max_fll = mode->max_fll; /* mode specific limit */
	else if (limits->min_line_length_pixels)
		max_fll = limits->max_line_length_pixels; /* sensor limit */
	else /* No restrictions */
		max_fll = USHRT_MAX;

	vblank->min = min_fll - height;
	vblank->max = max_fll - height;
	vblank->def = vblank->min;
	return 0;
}

static void crlmodule_update_framesize(struct crl_sensor *sensor)
{
	const struct crl_mode_rep *mode = sensor->current_mode;
	struct crl_ctrl_data *llength;
	struct crl_ctrl_data *flength;

	llength = __crlmodule_get_ctrl(sensor, ICI_EXT_SD_PARAM_ID_LINE_LENGTH_PIXELS);
	flength = __crlmodule_get_ctrl(sensor, ICI_EXT_SD_PARAM_ID_FRAME_LENGTH_LINES);

	if (llength) {
		llength->min = mode->min_llp;
		llength->def = llength->min;
	}

	if (flength) {
		flength->min = mode->min_fll;
		flength->def = flength->min;
	}
}

static int crlmodule_update_frame_blanking(struct crl_sensor *sensor)
{
	struct i2c_client *client = sensor->src->sd.client;
	struct crl_ctrl_data *vblank;
	struct crl_ctrl_data *hblank;
	int ret;

	vblank = __crlmodule_get_ctrl(sensor, ICI_EXT_SD_PARAM_ID_VBLANK);
	hblank = __crlmodule_get_ctrl(sensor, ICI_EXT_SD_PARAM_ID_HBLANK);

	if (hblank) {
		ret = __crlmodule_update_hblank(sensor, hblank);
		if (ret)
			return ret;
		dev_dbg(&client->dev, "%s hblank:%d\n", __func__, hblank->param.val);
	}

	if (vblank) {
		ret = __crlmodule_update_vblank(sensor, vblank);
		if (ret)
			return ret;
		dev_dbg(&client->dev, "%s vblank:%d\n", __func__, vblank->param.val);
	}

	return 0;
}

static void crlmodule_update_mode_bysel(struct crl_sensor *sensor)
{
	struct i2c_client *client = sensor->src->sd.client;
	const struct crl_mode_rep *this;
	unsigned int i;

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
			dev_dbg(&client->dev, "%s Compare PA out rect\n", __func__);
			if (!__crlmodule_rect_matches(client,
				&sensor->pixel_array->crop[CRL_PA_PAD_SRC],
				&this->sd_rects[CRL_SD_PA_INDEX].out_rect))
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
			if (!__crlmodule_rect_matches(client,
				&sensor->binner->crop[CRL_PAD_SINK],
				&this->sd_rects[CRL_SD_BINNER_INDEX].in_rect))
				continue;

			dev_dbg(&client->dev, "%s binner out rect\n", __func__);
			if (!__crlmodule_rect_matches(client,
				&sensor->binner->crop[CRL_PAD_SRC],
				&this->sd_rects[CRL_SD_BINNER_INDEX].out_rect))
				continue;
		}

		if (sensor->scaler) {
			dev_dbg(&client->dev, "%s scaler scale_m %d vs. %d\n",
					      __func__, sensor->scale_m,
					      this->scale_m);
			if (sensor->scale_m != this->scale_m)
				continue;

			dev_dbg(&client->dev, "%s scaler in rect\n", __func__);
			if (!__crlmodule_rect_matches(client,
				&sensor->scaler->crop[CRL_PAD_SINK],
				&this->sd_rects[CRL_SD_SCALER_INDEX].in_rect))
				continue;

			dev_dbg(&client->dev, "%s scaler out rect\n", __func__);
			if (!__crlmodule_rect_matches(client,
				&sensor->scaler->crop[CRL_PAD_SRC],
				&this->sd_rects[CRL_SD_SCALER_INDEX].out_rect))
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
		dev_info(&client->dev,
			 "%s no matching mode, set to default: %d\n",
			 __func__, i);
	}

	sensor->current_mode = this;
}

static void crlmodule_update_mode_ctrl(struct crl_sensor *sensor)
{
	struct i2c_client *client = sensor->src->sd.client;
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
			 this->sd_rects[CRL_SD_PA_INDEX].out_rect;
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
		crlmodule_update_mode_ctrl(sensor);
	else
		crlmodule_update_mode_bysel(sensor);

	/*
	 * We have a valid mode now. If there are any mode specific "get"
	 * controls defined in the configuration it could be queried by the
	 * user space for any mode specific information. So go through the
	 * mode specific ctrls and update its value from the selected mode.
	 */

	this = sensor->current_mode;

	for (i = 0; i < this->comp_items; i++) {
		struct crl_ctrl_data_pair *ctrl_comp = &this->ctrl_data[i];
		unsigned int idx;

		/* Get the ctl_ctrl pointer corresponding ctrl id */
		if (__crlmodule_get_crl_ctrl_index(sensor, ctrl_comp->ctrl_id,
						   &idx))
			/* If not found, move to the next ctrl */
			continue;

		/* No need to update this control, if this is a set op ctrl */
		if (sensor->ctrl_bank[idx].op_type == CRL_CTRL_SET_OP)
			continue;

		/* Update the control value */
		sensor->ctrl_bank[idx].param.val = ctrl_comp->data;
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
static int __crlmodule_get_format(
	struct ici_ext_subdev* subdev,
	struct ici_pad_framefmt* pff)
{
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct ici_rect *r;

	if (pff->pad.pad_idx == ssd->source_pad)
		r = &ssd->crop[ssd->source_pad];
	else
		r = &ssd->sink_fmt;

	pff->ffmt.width = r->width;
	pff->ffmt.height = r->height;
	pff->ffmt.pixelformat =
		sensor->sensor_ds->csi_fmts[sensor->fmt_index].code;
	pff->ffmt.field =
		((ssd->field == ICI_FIELD_ANY) ?
		ICI_FIELD_NONE : ssd->field);
	return 0;
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Slightly modified based on the CRL Module changes
 */
static int crlmodule_enum_pixelformat(
	struct ici_isys_node* node,
	struct ici_pad_supported_format_desc* psfd)
{
	struct ici_ext_subdev* subdev = node->sd;
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	
	if (psfd->idx >= sensor->sensor_ds->csi_fmts_items)
		return -EINVAL;

	psfd->color_format =
		sensor->sensor_ds->csi_fmts[psfd->idx].code;
	psfd->min_width = sensor->sensor_ds->sensor_limits->x_addr_min;
	psfd->max_width = sensor->sensor_ds->sensor_limits->x_addr_max;
	psfd->min_height = sensor->sensor_ds->sensor_limits->y_addr_min;
	psfd->max_height = sensor->sensor_ds->sensor_limits->y_addr_max;
	return 0;
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Slightly modified based on the CRL Module changes
 */
static int crlmodule_get_format(
	struct ici_isys_node* node,
	struct ici_pad_framefmt* pff)
{
	struct ici_ext_subdev* subdev = node->sd;
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	int rval;

	mutex_lock(&sensor->mutex);
	rval = __crlmodule_get_format(subdev, pff);
	mutex_unlock(&sensor->mutex);

	return rval;
}

static int __crlmodule_sel_supported(
	struct ici_ext_subdev *subdev,
	u32 pad,
	u32 type)
{
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);

	if (ssd == sensor->pixel_array && pad == CRL_PA_PAD_SRC) {
		switch (type) {
		case ICI_EXT_SEL_TYPE_NATIVE:
		case ICI_EXT_SEL_TYPE_CROP:
		case ICI_EXT_SEL_TYPE_CROP_BOUNDS:
			return 0;
		}
	}
	if (ssd == sensor->binner) {
		switch (type) {
		case ICI_EXT_SEL_TYPE_COMPOSE:
		case ICI_EXT_SEL_TYPE_COMPOSE_BOUNDS:
			if (pad == CRL_PAD_SINK)
				return 0;
			break;
		}
	}
	if (ssd == sensor->scaler) {
		switch (type) {
		case ICI_EXT_SEL_TYPE_CROP:
		case ICI_EXT_SEL_TYPE_CROP_BOUNDS:
			if (pad == CRL_PAD_SRC)
				return 0;
			break;
		case ICI_EXT_SEL_TYPE_COMPOSE:
		case ICI_EXT_SEL_TYPE_COMPOSE_BOUNDS:
			if (pad == CRL_PAD_SINK)
				return 0;
			break;
		}
	}
	return -EINVAL;
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Slightly modified based on the CRL Module changes
 */
static void crlmodule_get_crop_compose(
	struct ici_ext_subdev *subdev,
	struct ici_rect **crops,
	struct ici_rect **comps)
{
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	unsigned int i;

	/* Currently we support only 2 pads */
	BUG_ON(subdev->num_pads > CRL_PADS);

	if (crops)
		for (i = 0; i < subdev->num_pads; i++)
			crops[i] = &ssd->crop[i];
	if (comps)
		*comps = &ssd->compose;
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Slightly modified based on the CRL Module changes
 */
static int crlmodule_get_selection(
	struct ici_isys_node *node,
	struct ici_pad_selection* ps)
{
	struct ici_ext_subdev *subdev = node->sd;
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct ici_rect *comp, *crops[CRL_PADS];
	struct ici_rect sink_fmt;
	int ret;

	ret = __crlmodule_sel_supported(subdev, ps->pad.pad_idx,
		ps->sel_type);
	if (ret)
		return ret;

	crlmodule_get_crop_compose(subdev, crops, &comp);

	sink_fmt = ssd->sink_fmt;

	switch (ps->sel_type) {
	case ICI_EXT_SEL_TYPE_CROP_BOUNDS:
	case ICI_EXT_SEL_TYPE_NATIVE:
		if (ssd == sensor->pixel_array) {
			ps->rect.left = ps->rect.top = 0;
			ps->rect.width =
				sensor->sensor_ds->sensor_limits->x_addr_max;
			ps->rect.height =
				sensor->sensor_ds->sensor_limits->y_addr_max;
		} else if (ps->pad.pad_idx == ssd->sink_pad) {
			ps->rect = sink_fmt;
		} else {
			ps->rect = *comp;
		}
		break;
	case ICI_EXT_SEL_TYPE_CROP:
	case ICI_EXT_SEL_TYPE_COMPOSE_BOUNDS:
		ps->rect = *crops[ps->pad.pad_idx];
		break;
	case ICI_EXT_SEL_TYPE_COMPOSE:
		ps->rect = *comp;
		break;
	}
	return 0;
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Slightly modified based on the CRL Module changes
 */
static void crlmodule_propagate(
	struct ici_ext_subdev *subdev,
	u32 type)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	struct ici_rect *comp, *crops[CRL_PADS];

	crlmodule_get_crop_compose(subdev, crops, &comp);

	switch (type) {
	case ICI_EXT_SEL_TYPE_CROP:
		comp->width = crops[CRL_PAD_SINK]->width;
		comp->height = crops[CRL_PAD_SINK]->height;
		if (ssd == sensor->scaler) {
			sensor->scale_m = 1;
		} else if (ssd == sensor->binner) {
			sensor->binning_horizontal = 1;
			sensor->binning_vertical = 1;
		}
		/* Fall through */
	case ICI_EXT_SEL_TYPE_COMPOSE:
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
static int crlmodule_set_compose(
	struct ici_ext_subdev *subdev,
	struct ici_rect *r)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	struct ici_rect *comp, *crops[CRL_PADS];

	crlmodule_get_crop_compose(subdev, crops, &comp);

	r->top = 0;
	r->left = 0;

	if (ssd == sensor->binner) {
		sensor->binning_horizontal = crops[CRL_PAD_SINK]->width /
					   r->width;
		sensor->binning_vertical = crops[CRL_PAD_SINK]->height /
					   r->height;
	} else {
		sensor->scale_m = crops[CRL_PAD_SINK]->width *
				 sensor->sensor_ds->sensor_limits->scaler_m_min /
				 r->width;
	}

	*comp = *r;

	crlmodule_propagate(subdev,
		ICI_EXT_SEL_TYPE_COMPOSE);

	crlmodule_update_current_mode(sensor);
	return 0;
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Slightly modified based on the CRL Module changes
 */
static int crlmodule_set_crop(
	struct ici_ext_subdev *subdev,
	u32 pad,
	struct ici_rect *r)
{
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct ici_rect *src_size, *crops[CRL_PADS];

	crlmodule_get_crop_compose(subdev, crops, NULL);

	if (pad == ssd->sink_pad)
		src_size = &ssd->sink_fmt;
	else
		src_size = &ssd->compose;

	if (ssd == sensor->src && pad == CRL_PAD_SRC) {
		r->left = 0;
		r->top = 0;
	}

	r->width = min(r->width, src_size->width);
	r->height = min(r->height, src_size->height);

	r->left = min_t(s32, r->left, src_size->width - r->width);
	r->top = min_t(s32, r->top, src_size->height - r->height);

	*crops[pad] = *r;

	if (ssd != sensor->pixel_array && pad == CRL_PAD_SINK)
		crlmodule_propagate(subdev,
			ICI_EXT_SEL_TYPE_CROP);

	/* TODO! Should we short list supported mode? */

	return 0;
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Modified based on the CRL Module changes
 */
static int crlmodule_set_format(
	struct ici_isys_node *node,
	struct ici_pad_framefmt *pff)
{
	struct ici_ext_subdev *subdev = node->sd;
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct crl_subdev *ssd = to_crlmodule_subdev(subdev);
	struct i2c_client *client = sensor->src->sd.client;
	struct ici_rect *crops[CRL_PADS];

	dev_dbg(&client->dev, "%s sd_name: %s pad: %d w: %d, h: %d code: 0x%x",
			       __func__, node->name, pff->pad.pad_idx,
			       pff->ffmt.width,
			       pff->ffmt.height,
			       pff->ffmt.pixelformat);

	mutex_lock(&sensor->mutex);

	/* Currently we only support ALTERNATE interlaced mode. */
	if (pff->ffmt.field != ICI_FIELD_ALTERNATE)
		pff->ffmt.field = ICI_FIELD_NONE;
	pff->ffmt.colorspace = 0;
	memset(pff->ffmt.reserved, 0, sizeof(pff->ffmt.reserved));
	ssd->field = pff->ffmt.field;

	if (pff->pad.pad_idx == ssd->source_pad) {
		u32 code = pff->ffmt.pixelformat;
		int rval = __crlmodule_get_format(subdev, pff);

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
			rval = __crlmodule_get_format(subdev, pff);
			/* TODO! validate PLL? */
		}
		mutex_unlock(&sensor->mutex);
		return rval;
	}

	pff->ffmt.width =
		clamp_t(uint32_t, pff->ffmt.width,
			sensor->sensor_ds->sensor_limits->x_addr_min,
			sensor->sensor_ds->sensor_limits->x_addr_max);
	pff->ffmt.height =
		clamp_t(uint32_t, pff->ffmt.height,
			sensor->sensor_ds->sensor_limits->y_addr_min,
			sensor->sensor_ds->sensor_limits->y_addr_max);

	crlmodule_get_crop_compose(subdev, crops, NULL);

	crops[ssd->sink_pad]->left = 0;
	crops[ssd->sink_pad]->top = 0;
	crops[ssd->sink_pad]->width = pff->ffmt.width;
	crops[ssd->sink_pad]->height = pff->ffmt.height;
	ssd->sink_fmt = *crops[ssd->sink_pad];

	crlmodule_propagate(subdev, ICI_EXT_SEL_TYPE_CROP);

	crlmodule_update_current_mode(sensor);

	mutex_unlock(&sensor->mutex);

	return 0;
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Slightly modified based on the CRL Module changes
 */
static int crlmodule_set_selection(
	struct ici_isys_node *node,
	struct ici_pad_selection* ps)
{
	struct ici_ext_subdev *subdev = node->sd;
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct i2c_client *client = sensor->src->sd.client;
	int ret;

	dev_dbg(&client->dev, "%s sd_name: %s sel w: %d, h: %d",
			       __func__, node->name, ps->rect.width,
			       ps->rect.height);

	ret = __crlmodule_sel_supported(subdev, ps->pad.pad_idx,
		ps->sel_type);
	if (ret) {
		dev_dbg(&client->dev,
			"%s sd_name: %s w: %d, h: %d not supported",
			       __func__, node->name, ps->rect.width,
			       ps->rect.height);
		return ret;
	}

	mutex_lock(&sensor->mutex);

	ps->rect.width = max_t(unsigned int,
			     sensor->sensor_ds->sensor_limits->x_addr_min,
			     ps->rect.width);
	ps->rect.height = max_t(unsigned int,
			      sensor->sensor_ds->sensor_limits->y_addr_min,
			      ps->rect.height);
	switch (ps->sel_type) {
	case ICI_EXT_SEL_TYPE_CROP:
		ret = crlmodule_set_crop(subdev, ps->pad.pad_idx,
			&ps->rect);
		break;
	case ICI_EXT_SEL_TYPE_COMPOSE:
		ret = crlmodule_set_compose(subdev, &ps->rect);
		break;
	default:
		ret = -EINVAL;
	}

	crlmodule_update_current_mode(sensor);

	mutex_unlock(&sensor->mutex);
	return ret;
}

static int crlmodule_start_streaming(struct crl_sensor *sensor)
{
	struct i2c_client *client = sensor->src->sd.client;
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

static int crlmodule_set_stream(
	struct ici_isys_node* node,
	void* ip,
	int enable)
{
	struct ici_ext_subdev *subdev = node->sd;
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct i2c_client *client = sensor->src->sd.client;
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
	__crlmodule_enable_param(sensor, SENSOR_IDLE, enable);

	/* SENSOR_STREAMING controls cannot be set when not streaming */
	__crlmodule_enable_param(sensor, SENSOR_STREAMING, !enable);

	/* SENSOR_POWERED_ON controls does not matter about streaming. */
	__crlmodule_enable_param(sensor, SENSOR_POWERED_ON, false);

	return rval;
}

static int crlmodule_identify_module(
	struct ici_ext_subdev *subdev)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct i2c_client *client = sensor->src->sd.client;
	unsigned int size = 0;
	char *id_string;
	char *temp;
	int i, ret;
	u32 val;

	for (i = 0; i < sensor->sensor_ds->id_reg_items; i++)
		size += sensor->sensor_ds->id_regs[i].width + 1;

	/* TODO! If no ID! return success? */
	if (!size)
		return 0;

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

		temp = kzalloc(sensor->sensor_ds->id_regs[i].width, GFP_KERNEL);
		if (!temp) {
			ret = -ENOMEM;
			goto out;
		}
		snprintf(temp, sensor->sensor_ds->id_regs[i].width, "0x%x ",
			 val);
		strcat(id_string, temp);
		snprintf(id_string, sensor->sensor_ds->id_regs[i].width,
			 "%s 0x%x ", temp, val);

		kfree(temp);
	}

	/* TODO! Check here if this module in the supported list
	 * Ideally the module manufacturer and id should be in platform
	 * data or ACPI and here the driver should read the value from the
	 * register and check if this matches to any in the supported
	 * platform data */

out:
	dev_dbg(&client->dev, "%s module: %s", __func__, id_string);
	kfree(id_string);
	if (ret)
		dev_err(&client->dev, "sensor detection failed\n");
	return ret;
}

/*
 * This function executes the initialisation routines after the power on
 * is successfully completed. Following operations are done
 *
 *    Initiases registers after sensor power up - if any such list is configured
 *    Ctrl handler framework intialisation
 */
static int crlmodule_run_poweron_init(struct crl_sensor *sensor)
{
	struct i2c_client *client = sensor->src->sd.client;
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

	dev_dbg(&client->dev, "%s init controls", __func__);


	/* SENSOR_IDLE control can be set only when not streaming*/
	__crlmodule_enable_param(sensor, SENSOR_IDLE, false);

	/* SENSOR_STREAMING controls can be set only when streaming */
	__crlmodule_enable_param(sensor, SENSOR_STREAMING, true);

	/* SENSOR_POWERED_ON controls can be set after power on */
	__crlmodule_enable_param(sensor, SENSOR_POWERED_ON, false);

	mutex_lock(&sensor->mutex);
	crlmodule_update_current_mode(sensor);
	mutex_unlock(&sensor->mutex);

	return rval;
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
	struct i2c_client *client = sensor->src->sd.client;
	struct crl_power_seq_entity *entity;
	int idx;

	for (idx = rev_idx; idx >= 0; idx--) {
		entity = &sensor->pwr_entity[idx];
		dev_dbg(&client->dev, "%s power type %d index %d\n",
				__func__, entity->type, idx);

		switch (entity->type) {
		case CRL_POWER_ETY_GPIO_FROM_PDATA:
			gpio_set_value(sensor->platform_data->xshutdown,
						   entity->undo_val);
			break;
		case CRL_POWER_ETY_GPIO_CUSTOM:
			gpio_set_value(entity->ent_number, entity->undo_val);
			break;
		case CRL_POWER_ETY_REGULATOR_FRAMEWORK:
			regulator_disable(entity->regulator_priv);
			break;
		case CRL_POWER_ETY_CLK_FRAMEWORK:
			clk_disable_unprepare(sensor->xclk);
			break;
		default:
			dev_err(&client->dev, "%s Invalid power type\n", __func__);
			break;
		}

		if (entity->delay)
			usleep_range(entity->delay, entity->delay + 10);
	}
}

static int __crlmodule_powerup_sequence(struct crl_sensor *sensor)
{
	struct i2c_client *client = sensor->src->sd.client;
	struct crl_power_seq_entity *entity;
	unsigned idx;
	int rval;

	for (idx = 0; idx < sensor->sensor_ds->power_items; idx++) {
		entity = &sensor->pwr_entity[idx];
		dev_dbg(&client->dev, "%s power type %d index %d\n",
				__func__, entity->type, idx);

		switch (entity->type) {
		case CRL_POWER_ETY_GPIO_FROM_PDATA:
			gpio_set_value(sensor->platform_data->xshutdown, entity->val);
			break;
		case CRL_POWER_ETY_GPIO_CUSTOM:
			gpio_set_value(entity->ent_number, entity->val);
			break;
		case CRL_POWER_ETY_REGULATOR_FRAMEWORK:
			rval = regulator_enable(entity->regulator_priv);
			if (rval) {
				dev_err(&client->dev, "Failed to enable regulator: %d\n",
						rval);
				devm_regulator_put(entity->regulator_priv);
				entity->regulator_priv = NULL;
				goto error;
			}
			break;
		case CRL_POWER_ETY_CLK_FRAMEWORK:
			rval = clk_set_rate(sensor->xclk, sensor->platform_data->ext_clk);
			if (rval < 0) {
				dev_err(&client->dev,
				"unable to set clock freq to %u\n",
				sensor->platform_data->ext_clk);
				goto error;
			}
			if (clk_get_rate(sensor->xclk) != sensor->platform_data->ext_clk)
					dev_warn(&client->dev,
						"warning: unable to set accurate clock freq %u\n",
						sensor->platform_data->ext_clk);
			rval = clk_prepare_enable(sensor->xclk);
			if (rval) {
				dev_err(&client->dev, "Failed to enable clock: %d\n", rval);
				goto error;
			}
			break;
		default:
			dev_err(&client->dev, "Invalid power type\n");
			rval = -ENODEV;
			goto error;
			break;
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

static int crlmodule_set_power(
	struct ici_isys_node* node,
	int on)
{
	struct ici_ext_subdev *subdev = node->sd;
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct i2c_client *client = sensor->src->sd.client;
	int ret = 0;

	pr_err("crlmodule_set_power %d\n", on);
	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		pr_err("crlmodule_set_power val %d\n", ret);
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
			pr_err("crlmodule_set_power err (2) %d\n", ret);
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

	pr_err("crlmodule_set_power ret %d\n", ret);
	return ret;
}

/*
 * Function main code replicated from /drivers/media/i2c/smiapp/smiapp-core.c
 * Modified based on the CRL Module changes
 */
static int crlmodule_init_subdevs(
	struct ici_ext_subdev *subdev)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct i2c_client *client = sensor->src->sd.client;
	struct crl_subdev *prev_sd = NULL;
	int i = 0;
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
		sd = &sensor->ssds[i];

		sd->sensor = sensor;
		if (sd == sensor->pixel_array) {
			sd->npads = 1;
		} else {
			sd->npads = 2;
			sd->source_pad = 1;
		}

		sd->sink_fmt.width =
			sensor->sensor_ds->sensor_limits->x_addr_max;
		sd->sink_fmt.height =
			sensor->sensor_ds->sensor_limits->y_addr_max;
		sd->compose.width = sd->sink_fmt.width;
		sd->compose.height = sd->sink_fmt.height;
		sd->crop[sd->source_pad] = sd->compose;
		//sd->pads[sd->source_pad].flags = ICI_PAD_FLAGS_SOURCE;
		if (sd != sensor->pixel_array) {
			sd->crop[sd->sink_pad] = sd->compose;
			//sd->pads[sd->sink_pad].flags = ICI_PAD_FLAGS_SINK;
		}

		rval = init_ext_sd(client, sd, i);
		if (rval)
			return rval;
		
		if (prev_sd == NULL) {
			prev_sd = sd;
			continue;
		}

		if (sensor->reg.create_link) { 
			rval = sensor->reg.create_link(&sd->sd.node,
				sd->source_pad,
				&prev_sd->sd.node,
				prev_sd->sink_pad,
				0);
			if (rval)
				return rval;
		}
		prev_sd = sd;
	}

	return rval;
}

static int __init_power_resources(
	struct ici_ext_subdev *subdev)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct i2c_client *client = sensor->src->sd.client;
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
				dev_err(&client->dev, "unable to acquire xshutdown %d\n",
				sensor->platform_data->xshutdown);
				return -ENODEV;
			}
		break;
		case CRL_POWER_ETY_GPIO_CUSTOM:
			if (devm_gpio_request_one(&client->dev,
				entity->ent_number, 0,
				"CRL Custom") != 0) {
				dev_err(&client->dev, "unable to acquire custom gpio %d\n",
				entity->ent_number);
				return -ENODEV;
			}
		break;
		case CRL_POWER_ETY_REGULATOR_FRAMEWORK:
			entity->regulator_priv = devm_regulator_get(&client->dev,
			entity->ent_name);
			if (IS_ERR(entity->regulator_priv)) {
				dev_err(&client->dev, "Failed to get regulator: %s\n",
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
			sensor->xclk = devm_clk_get(&client->dev, NULL);
			if (IS_ERR(sensor->xclk)) {
				dev_err(&client->dev, "Cannot get sensor clk\n");
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

static int crlmodule_registered(
	struct ici_ext_subdev_register *reg)
{
	struct ici_ext_subdev* subdev = reg->sd;
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct i2c_client *client = sensor->src->sd.client;

	int rval;

	if (!reg->sd || !reg->setup_node || !reg->create_link)
		return -EINVAL;

	rval = __init_power_resources(subdev);
	if (rval)
		return -ENODEV;


	/* Power up the sensor */
	if (pm_runtime_get_sync(&client->dev) < 0) {
		pm_runtime_put(&client->dev);
		return -ENODEV;
	}

	/* one time init */
	rval = crlmodule_write_regs(sensor, sensor->sensor_ds->onetime_init_regs,
				    sensor->sensor_ds->onetime_init_regs_items);
	if (rval) {
		dev_err(&client->dev, "%s failed to set powerup registers\n",
				      __func__);
		return -ENODEV;
	}

	/* sensor specific init */
	if (sensor->sensor_ds->sensor_init) {
		rval = sensor->sensor_ds->sensor_init(client);

		if (rval) {
			dev_err(&client->dev, "%s failed to run sensor specific init\n",
				      __func__);
			return -ENODEV;
		}
	}
	/* Identify the module */
	rval = crlmodule_identify_module(subdev);
	if (rval) {
		rval = -ENODEV;
		goto out;
	}

	sensor->reg = *reg;

	rval = crlmodule_init_subdevs(subdev);
	if (rval)
		goto out;

	sensor->binning_horizontal = 1;
	sensor->binning_vertical = 1;
	sensor->scale_m = 1;
	sensor->flip_info = CRL_FLIP_DEFAULT_NONE;
	sensor->ext_ctrl_impacts_pll_selection = false;
	sensor->ext_ctrl_impacts_mode_selection = false;

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

static void crlmodule_unregistered(
	struct ici_ext_subdev *subdev)
{
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	struct i2c_client *client = sensor->src->sd.client;
	dev_dbg(&client->dev, "%s\n", __func__);
}

static int init_ext_sd(struct i2c_client *client, 
	struct crl_subdev *ssd, int idx)
{
	int rval;
	struct ici_ext_subdev* sd = &ssd->sd;
	struct crl_sensor *sensor = ssd->sensor;
	char name[ICI_MAX_NODE_NAME];
	if (sensor->platform_data->suffix)
		snprintf(name,
			sizeof(name), "%s %c",
			sensor->sensor_ds->subdevs[idx].name,
			sensor->platform_data->suffix);
	else
		snprintf(name,
			sizeof(name), "%s",
			sensor->sensor_ds->subdevs[idx].name);

	sd->client = client;
	sd->num_pads = ssd->npads;
	sd->src_pad = ssd->source_pad;
	sd->set_param = crlmodule_set_param;
	sd->get_param = crlmodule_get_param;
	sd->get_menu_item = crlmodule_get_menu_item;
	if (sensor->reg.setup_node) {
		rval = sensor->reg.setup_node(sensor->reg.ipu_data,
			sd, name);
		if (rval)
			return rval;
	}
	sd->node.node_set_power = crlmodule_set_power;
	sd->node.node_set_streaming = crlmodule_set_stream;
	sd->node.node_get_pad_supported_format =
		crlmodule_enum_pixelformat;
	sd->node.node_set_pad_ffmt = crlmodule_set_format;
	sd->node.node_get_pad_ffmt = crlmodule_get_format;
	sd->node.node_set_pad_sel = crlmodule_set_selection;
	sd->node.node_get_pad_sel = crlmodule_get_selection;

	return 0;
}

#ifdef CONFIG_PM

static int crlmodule_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ici_ext_subdev *sd =
		i2c_get_clientdata(client);
	struct crl_sensor *sensor = to_crlmodule_sensor(sd);

	crlmodule_undo_poweron_entities(sensor,
					sensor->sensor_ds->power_items - 1);
	return 0;
}

static int crlmodule_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ici_ext_subdev *sd =
		i2c_get_clientdata(client);
	struct crl_subdev *ssd = to_crlmodule_subdev(sd);
	struct crl_sensor *sensor = ssd->sensor;

	if (sensor->streaming)
		crlmodule_stop_streaming(sensor);

	crlmodule_undo_poweron_entities(sensor,
					sensor->sensor_ds->power_items - 1);
	return 0;
}

static int crlmodule_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ici_ext_subdev *sd =
		i2c_get_clientdata(client);
	struct crl_sensor *sensor = to_crlmodule_sensor(sd);

	return __crlmodule_powerup_sequence(sensor);
}

static int crlmodule_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ici_ext_subdev *sd =
		i2c_get_clientdata(client);
	struct crl_subdev *ssd = to_crlmodule_subdev(sd);
	struct crl_sensor *sensor = ssd->sensor;
	int rval;

	rval = __crlmodule_powerup_sequence(sensor);
	if (!rval && sensor->power_count)
		rval = crlmodule_run_poweron_init(sensor);
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
	sensor->src->sensor = sensor;

	sensor->src->sd.client = client;
	sensor->src->sd.do_register = crlmodule_registered;
	sensor->src->sd.do_unregister = crlmodule_unregistered;
	i2c_set_clientdata(client, &sensor->src->sd);

	pm_runtime_enable(&client->dev);

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
}

static int crlmodule_remove(struct i2c_client *client)
{
	struct ici_ext_subdev *subdev =
		i2c_get_clientdata(client);
	struct crl_sensor *sensor = to_crlmodule_sensor(subdev);
	unsigned int i;

	if (sensor->sensor_ds->sensor_cleanup)
		sensor->sensor_ds->sensor_cleanup(client);

	for (i = 0; i < sensor->ssds_used; i++) {
		struct ici_ext_subdev *sd =
			&sensor->ssds[i].sd;
		if (sd->do_unregister)
			sd->do_unregister(sd);
	}

	i2c_set_clientdata(client, NULL);

	crlmodule_nvm_deinit(sensor);
	crlmodule_release_ds(sensor);
	crlmodule_release_msrlist(&sensor->msr_list);

	pm_runtime_disable(&client->dev);

	return 0;
}


static const struct i2c_device_id crlmodule_id_table[] = {
	{ CRLMODULE_LITE_NAME, 0 },
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
		.name = CRLMODULE_LITE_NAME,
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
MODULE_DESCRIPTION("Generic driver for common register list based camera sensor modules");
MODULE_LICENSE("GPL");
