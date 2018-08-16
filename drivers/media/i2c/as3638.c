// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2016 - 2018 Intel Corporation

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>
#include "../../../include/media/as3638.h"
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>

/* Registers */
#define REG_DESIGN_INFO			0x00
#define REG_VERSION_CONTROL		0x01
#define REG_CURRENT_SET1		0x02
#define REG_CURRENT_SET2		0x03
#define REG_CURRENT_SET3		0x04
#define REG_CONFIG			0x05
#define REG_LOW_VOLTAGE			0x06
#define REG_TIMER			0x07
#define REG_CONTROL			0x08
#define REG_FAULT_AND_INFO		0x09
#define REG_FLASH_CURRENT_REACHED	0x20
#define REG_PROTECT			0x21
#define REG_HIG_OVERVOLTAGE_PROTECTION	0x22

/* Register fields */
#define VERSION_SHIFT			0
#define VERSION_MASK			0x0F

#define FLASH_CURRENT_SHIFT		0
#define FLASH_CURRENT_MASK		0x1F
#define TORCH_CURRENT_SHIFT		5
#define TORCH_CURRENT_MASK		0xE0

#define CONFIG_ASSIST_STROBE_SHIFT	1
#define CONFIG_ASSIST_STROBE_MASK	0x02
#define CONFIG_EXT_TORCH_ON_SHIFT	2
#define CONFIG_EXT_TORCH_ON_MASK	0x04
#define CONFIG_COIL_PEAK_SHIFT		3
#define CONFIG_COIL_PEAK_MASK		0x18
#define CONFIG_MUTE_POLARITY_SHIFT	5
#define CONFIG_MUTE_POLARITY_MASK	0x20
#define CONFIG_STROBE_TYPE_SHIFT	6
#define CONFIG_STROBE_TYPE_MASK		0x40
#define CONFIG_STROBE_ON_SHIFT		7
#define CONFIG_STROBE_ON_MASK		0x80

#define LOW_VOLTAGE_RESET_SHIFT		0
#define LOW_VOLTAGE_RESET_MASK		0x01
#define LOW_VOLTAGE_LOWV_RED_CURR_SHIFT	1
#define LOW_VOLTAGE_LOWV_RED_CURR_MASK	0x06
#define LOW_VOLTAGE_LOWV_SEL_SHIFT	3
#define LOW_VOLTAGE_LOWV_SEL_MASK	0x38
#define LOW_VOLTAGE_LOWV_ON_SHIFT	6
#define LOW_VOLTAGE_LOWV_ON_MASK	0x40
#define LOW_VOLTAGE_BOOST_SHIFT		7
#define LOW_VOLTAGE_BOOST_MASK		0x80

#define TIMER_FLASH_TIMEOUT_SHIFT	0
#define TIMER_FLASH_TIMEOUT_MASK	0x1F
#define TIMER_MUTE_THRESHOLD_SHIFT	5
#define TIMER_MUTE_THRESHOLD_MASK	0xE0

#define CONTROL_MODE_SETTING_SHIFT	0
#define CONTROL_MODE_SETTING_MASK	0x03
#define CONTROL_OUT_ON_SHIFT		2
#define CONTROL_OUT_ON_MASK		0x04
#define CONTROL_DCDC_SKIP_ENABLE_SHIFT	4
#define CONTROL_DCDC_SKIP_ENABLE_MASK	0x10
#define CONTROL_TXMASK_RED_CURR_SHIFT	5
#define CONTROL_TXMASK_RED_CURR_MASK	0x60
#define CONTROL_TXMASK_EN_SHIFT		7
#define CONTROL_TXMASK_EN_MASK		0x80

#define FAULT_UNDER_VOLTAGE_LO_SHIFT	0
#define FAULT_UNDER_VOLTAGE_LO_MASK	0x01
#define FAULT_LOW_VOLTAGE_SHIFT		1
#define FAULT_LOW_VOLTAGE_MASK		0x02
#define INFO_TORCH_DETECTED_SHIFT	2
#define INFO_TORCH_DETECTED_MASK	0x04
#define INFO_TXMASK_EVENT_SHIFT		3
#define INFO_TXMASK_EVENT_MASK		0x08
#define FAULT_TIMEOUT_SHIFT		4
#define FAULT_TIMEOUT_MASK		0x10
#define FAULT_OVERTEMP_SHIFT		5
#define FAULT_OVERTEMP_MASK		0x20
#define FAULT_LED_SHORT_SHIFT		6
#define FAULT_LED_SHORT_MASK		0x40
#define FAULT_LED_OPEN_SHIFT		7
#define FAULT_LED_OPEN_MASK		0x80

#define HOVP_SEL_HIGH_OVP_SHIFT		0
#define HOVP_SEL_HIGH_OVP_MASK		0x01
#define HOVP_LED_STATUS_ON_SHIFT	1
#define HOVP_LED_STATUS_ON_MASK		0x02
#define HOVP_LED_STATUS_1_SHIFT		2
#define HOVP_LED_STATUS_1_MASK		0x04
#define HOVP_LED_STATUS_2_SHIFT		3
#define HOVP_LED_STATUS_2_MASK		0x08
#define HOVP_LED_STATUS_3_SHIFT		4
#define HOVP_LED_STATUS_3_MASK		0x10

#define DESIGN_INFO_FIXED_ID		0x16
#define CONFIG_COIL_PEAK_1_3_AMP	0x0
#define CONFIG_COIL_PEAK_1_6_AMP	0x1
#define CONFIG_COIL_PEAK_1_95_AMP	0x2
#define CONFIG_COIL_PEAK_2_4_AMP	0x3
#define CONTROL_RED_CURR_100_MILLI_AMP	0x0
#define CONTROL_RED_CURR_200_MILLI_AMP	0x1
#define CONTROL_RED_CURR_300_MILLI_AMP	0x2
#define CONTROL_RED_CURR_400_MILLI_AMP	0x3
#define ALL_BITS_MASK			0xFF

#define AS3638_FLASH_TOUT_MIN		0
#define AS3638_FLASH_TOUT_STEP		1    /* Each step is 4ms */
#define AS3638_FLASH_TOUT_MAX		31   /* Max timeout is 128 ms */
#define AS3638_FLASH_TOUT_DEF		0x0F /* 64 ms timeout default */

#define AS3638_FLASH_INT_STEP		50000 /* uA */
#define AS3638_FLASH_INT_MILLI_A_TO_REG(a) \
	(((a) * 1000) / AS3638_FLASH_INT_STEP)

#define AS3638_TORCH_INT_STEP		12500 /* uA */
#define AS3638_TORCH_INT_MILLI_A_TO_REG(a) \
	(((a) * 1000) / AS3638_TORCH_INT_STEP)

const int torch_led1_intensity_table[] = {    0,  9400, 14100, 18100,
					  23500, 32900, 51800, 98800}; /* uA */

enum mode_setting {
	MODE_EXT_TORCH		= 0x0,
	MODE_MEM_INTERFACE	= 0x1, /* Not supported */
	MODE_TORCH		= 0x2,
	MODE_FLASH		= 0x3,
};

struct as3638_subdev {
	struct v4l2_subdev subdev;
	enum as3638_led_id led;
};

struct as3638_handler {
	struct v4l2_ctrl_handler handler;
	enum as3638_led_id led;
};

/*
 * struct as3638_flash
 *
 * @dev:             Device structure
 * @subdev_led:      V4L2 subdevices for each LED
 * @pdata:           Platform data
 * @regmap:          Register map for I2C accesses
 * @current_led:     The LED for which all HW registers currently is configured
 * @ctrls_led:       V4L2 control handlers for each LED
 * @led_mode:        V4L2 LED mode for each LED
 * @strobe_source:   V4L2 strobe source (SW or HW GPIO) for each LED
 * @timeout:         V4L2 timeout in steps of 4ms for each LED
 * @flash_intensity: Current flash intensity in steps of 50 mA for each LED
 * @torch_intensity: Current torch intensity in steps of 12.5 mA for each LED
 */
struct as3638_flash {
	struct device *dev;
	struct as3638_subdev subdev_led[AS3638_LED_MAX];
	struct as3638_platform_data *pdata;
	struct regmap *regmap;
	struct mutex lock;
	bool open[AS3638_LED_MAX];

	int current_led;

	struct as3638_handler ctrls_led[AS3638_LED_MAX];
	struct v4l2_ctrl *led_mode[AS3638_LED_MAX];
	struct v4l2_ctrl *strobe_source[AS3638_LED_MAX];
	struct v4l2_ctrl *timeout[AS3638_LED_MAX];
	struct v4l2_ctrl *flash_intensity[AS3638_LED_MAX];
	struct v4l2_ctrl *torch_intensity[AS3638_LED_MAX];
};

#define ctrl_to_as3638_handler(_ctrl) \
	container_of(_ctrl->handler, struct as3638_handler, handler)

#define ctrls_led_to_as3638_flash(_ctrls_led, _no) \
	container_of(_ctrls_led, struct as3638_flash, ctrls_led[_no])

#define subdev_to_as3638_subdev_led(_subdev) \
	container_of(_subdev, struct as3638_subdev, subdev)

#define subdev_led_to_as3638_flash(_subdev_led, _no) \
	container_of(_subdev_led, struct as3638_flash, subdev_led[_no])

static int as3638_dump_registers(struct as3638_flash *flash)
{
	int i;
	int reg[22];

	memset(&reg, 0, sizeof(reg));
	for (i = 0; i <= 9; i++)
		regmap_read(flash->regmap, i, &reg[i]);

	regmap_read(flash->regmap, 0x20, &reg[10]);
	regmap_read(flash->regmap, 0x21, &reg[11]);
	regmap_read(flash->regmap, 0x22, &reg[12]);

	dev_dbg(flash->dev,
		"Addr: 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x20 0x21 0x22\n");
	dev_dbg(flash->dev,
		"Val:  0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
		reg[0], reg[1], reg[2], reg[3], reg[4], reg[5], reg[6],
		reg[7], reg[8], reg[9], reg[10], reg[11], reg[12]);
	return 0;
}

static int as3638_disable_outputs(struct as3638_flash *flash)
{
	int rval;

	dev_dbg(flash->dev, "disable outputs\n");

	rval = regmap_update_bits(flash->regmap, REG_CONTROL,
				  CONTROL_OUT_ON_MASK, 0);
	if (rval < 0)
		dev_err(flash->dev, "Register write fail\n");

	return rval;
}

static int as3638_mode_ctrl(struct as3638_flash *flash,
			    enum as3638_led_id led_no)
{
	int rval;

	switch (flash->led_mode[led_no]->val) {
	case V4L2_FLASH_LED_MODE_NONE:
		dev_dbg(flash->dev, "LED%d: FLASH_LED_MODE_NONE\n", led_no + 1);
		rval = regmap_update_bits(
			flash->regmap, REG_CONTROL, CONTROL_MODE_SETTING_MASK,
			MODE_EXT_TORCH << CONTROL_MODE_SETTING_SHIFT);
		break;
	case V4L2_FLASH_LED_MODE_TORCH:
		dev_dbg(flash->dev, "LED%d: FLASH_LED_MODE_TORCH\n",
			led_no + 1);
		rval = regmap_update_bits(
			flash->regmap, REG_CONTROL, CONTROL_MODE_SETTING_MASK,
			MODE_TORCH << CONTROL_MODE_SETTING_SHIFT);
		break;
	case V4L2_FLASH_LED_MODE_FLASH:
		dev_dbg(flash->dev, "LED%d: FLASH_LED_MODE_FLASH\n",
			led_no + 1);
		rval = regmap_update_bits(
			flash->regmap, REG_CONTROL, CONTROL_MODE_SETTING_MASK,
			MODE_FLASH << CONTROL_MODE_SETTING_SHIFT);
		break;
	default:
		dev_err(flash->dev, "LED%d: Invalid mode %d\n",
			led_no + 1, flash->led_mode[led_no]->val);
		return -EINVAL;
	}
	return rval;
}

/*
 * The Infra-Red LED is connected to the LED1 output.
 *
 * For IR LED safety, be absolutely 100% sure that the HW current reduction
 * feature is enabled and setup correctly for using the maximum 400mA reduction.
 *
 * LED2 and LED3 currents must be set to zero to ensure the IR LED (LED1) is the
 * one with the highest current setting. Otherwise the reduction current will
 * be smaller than 400 mA for the IR LED.
 */
static int as3638_force_reduction_current(struct as3638_flash *flash)
{
	int rval;
	unsigned int reg_val;
	unsigned int reg_mask;

	reg_mask = CONTROL_TXMASK_RED_CURR_MASK | CONTROL_TXMASK_EN_MASK;

	reg_val = CONTROL_RED_CURR_400_MILLI_AMP <<
			CONTROL_TXMASK_RED_CURR_SHIFT;
	reg_val = reg_val | 1 << CONTROL_TXMASK_EN_SHIFT;

	rval = regmap_update_bits(flash->regmap, REG_CONTROL,
				  reg_mask, reg_val);
	if (rval < 0)
		return rval;

	rval = regmap_read(flash->regmap, REG_CONTROL, &reg_val);
	if (rval < 0) {
		dev_err(flash->dev, "Device read failed\n");
		return rval;
	}
	dev_dbg(flash->dev, "Control register = 0x%X\n", reg_val);

	if (!(reg_val & CONTROL_TXMASK_EN_MASK)) {
		dev_err(flash->dev, "Reduction enable fail\n");
		return -EINVAL;
	}

	if ((reg_val & CONTROL_TXMASK_RED_CURR_MASK) !=
	    (CONTROL_RED_CURR_400_MILLI_AMP << CONTROL_TXMASK_RED_CURR_SHIFT)) {
		dev_err(flash->dev, "Reduction init fail\n");
		return -EINVAL;
	}

	rval = regmap_update_bits(flash->regmap, REG_CURRENT_SET2,
				  ALL_BITS_MASK, 0); /* Clear reg */
	if (rval < 0)
		return rval;

	rval = regmap_update_bits(flash->regmap, REG_CURRENT_SET3,
				  ALL_BITS_MASK, 0); /* Clear reg */
	if (rval < 0)
		return rval;
	return 0;
}

/* The torch values for LED1 are non-linear, so use a table look-up instead. */
static int as3638_torch_intensity_to_register_led1(int value)
{
	int i;

	for (i = ARRAY_SIZE(torch_led1_intensity_table) - 1; i >= 0; i--) {
		if (torch_led1_intensity_table[i] <= value * 1000)
			break;
	}
	return i;
}

static int as3638_set_intensity(struct as3638_flash *flash,
				enum as3638_led_id led_no)
{
	int rval;
	unsigned int reg_val;
	unsigned int reg_mask;

	/*
	 * The HW is configured with an IR LED on the LED1 output and two
	 * visible light LEDs on LED2 and LED3.
	 * LED1 is always fired alone. LED2 and LED3 are always fired together.
	 */
	reg_mask = FLASH_CURRENT_MASK | TORCH_CURRENT_MASK;
	switch (led_no)	{
	case AS3638_LED1:
		dev_dbg(flash->dev, "LED1: flash = %d mA, torch = %d mA\n",
			flash->flash_intensity[AS3638_LED1]->val,
		       flash->torch_intensity[AS3638_LED1]->val);

		reg_val = as3638_torch_intensity_to_register_led1(
				flash->torch_intensity[AS3638_LED1]->val);
		reg_val = reg_val << TORCH_CURRENT_SHIFT;
		reg_val = reg_val | AS3638_FLASH_INT_MILLI_A_TO_REG(
				flash->flash_intensity[AS3638_LED1]->val);

		rval = regmap_update_bits(flash->regmap, REG_CURRENT_SET1,
					  reg_mask, reg_val);
		if (rval < 0)
			return rval;

		rval = regmap_update_bits(flash->regmap, REG_CURRENT_SET2,
					  ALL_BITS_MASK, 0); /* Clear reg */
		if (rval < 0)
			return rval;

		rval = regmap_update_bits(flash->regmap, REG_CURRENT_SET3,
					  ALL_BITS_MASK, 0); /* Clear reg */
		if (rval < 0)
			return rval;
		break;

	case AS3638_LED2:
	case AS3638_LED3:
		rval = regmap_update_bits(flash->regmap, REG_CURRENT_SET1,
					  ALL_BITS_MASK, 0); /* Clear reg */
		if (rval < 0)
			return rval;

		dev_dbg(flash->dev, "LED2: flash = %d mA, torch = %d mA\n",
			flash->flash_intensity[AS3638_LED2]->val,
		       flash->torch_intensity[AS3638_LED2]->val);

		reg_val = AS3638_TORCH_INT_MILLI_A_TO_REG(
				flash->torch_intensity[AS3638_LED2]->val);
		reg_val = reg_val << TORCH_CURRENT_SHIFT;
		reg_val = reg_val | AS3638_FLASH_INT_MILLI_A_TO_REG(
				flash->flash_intensity[AS3638_LED2]->val);

		rval = regmap_update_bits(flash->regmap, REG_CURRENT_SET2,
					  reg_mask, reg_val);
		if (rval < 0)
			return rval;

		dev_dbg(flash->dev, "LED3: flash = %d mA, torch = %d mA\n",
			flash->flash_intensity[AS3638_LED3]->val,
		       flash->torch_intensity[AS3638_LED3]->val);

		reg_val = AS3638_TORCH_INT_MILLI_A_TO_REG(
				flash->torch_intensity[AS3638_LED3]->val);
		reg_val = reg_val << TORCH_CURRENT_SHIFT;
		reg_val = reg_val | AS3638_FLASH_INT_MILLI_A_TO_REG(
				flash->flash_intensity[AS3638_LED3]->val);

		rval = regmap_update_bits(flash->regmap, REG_CURRENT_SET3,
					  reg_mask, reg_val);
		if (rval < 0)
			return rval;
		break;

	default:
		dev_warn(flash->dev, "Invalid led_no %d\n", led_no + 1);
		return -EINVAL;
	}
	return rval;
}

static int as3638_turn_on_leds(struct as3638_flash *flash,
			       enum as3638_led_id led_no)
{
	int rval;

	dev_dbg(flash->dev, "turn on LED%d\n", led_no + 1);

	if (led_no == AS3638_LED1 &&
	    flash->led_mode[led_no]->val == V4L2_FLASH_LED_MODE_FLASH) {
		/* For safety reasons ensure current reduction is enabled */
		rval = as3638_force_reduction_current(flash);
		if (rval < 0)
			return rval;
	}

	dev_dbg(flash->dev, "%s\n",
		as3638_dump_registers(flash) ? " " : " ");

	return regmap_update_bits(flash->regmap, REG_CONTROL,
				  CONTROL_OUT_ON_MASK,
				  1 << CONTROL_OUT_ON_SHIFT);
}

/*
 * This init function must be called everytime the controller is powered on or
 * when the driver detects that a different LED is being configured/used than
 * what the controller is currently setup for.
 */
static int as3638_init_device(struct as3638_flash *flash,
			      enum as3638_led_id led_no)
{
	int rval;
	unsigned int reg_val;
	unsigned int reg_mask;

	dev_dbg(flash->dev, "LED%d\n", led_no + 1);

	rval = as3638_set_intensity(flash, led_no);
	if (rval < 0)
		return rval;

	/*
	 * Max coil peak current set to 2.4A to match the HW design.
	 * Initialize also the strobe mode (SW or HW).
	 */
	reg_val = CONFIG_COIL_PEAK_2_4_AMP << CONFIG_COIL_PEAK_SHIFT |
		  flash->strobe_source[led_no]->val << CONFIG_STROBE_ON_SHIFT;
	reg_mask = CONFIG_COIL_PEAK_MASK | CONFIG_STROBE_ON_MASK;
	rval = regmap_update_bits(flash->regmap, REG_CONFIG,
				  reg_mask, reg_val);
	if (rval < 0)
		return rval;

	rval = regmap_update_bits(flash->regmap, REG_TIMER,
				  TIMER_FLASH_TIMEOUT_MASK,
				  flash->timeout[led_no]->val);
	if (rval < 0)
		return rval;

	rval = as3638_mode_ctrl(flash, led_no);
	if (rval < 0)
		return rval;

	flash->current_led = led_no;

	dev_dbg(flash->dev, "Resetting fault flags\n");
	return regmap_read(flash->regmap, REG_FAULT_AND_INFO, &reg_val);
}

static int as3638_get_ctrl(struct v4l2_ctrl *ctrl)
{
	struct as3638_handler *ctrls_led = ctrl_to_as3638_handler(ctrl);
	enum as3638_led_id led_no = ctrls_led->led;
	struct as3638_flash *flash = ctrls_led_to_as3638_flash(ctrls_led,
							       led_no);
	unsigned int fault_val;
	int rval = -EINVAL;

	dev_dbg(flash->dev, "LED%d: ctrl->id = 0x%X\n", led_no + 1, ctrl->id);

	if (ctrl->id != V4L2_CID_FLASH_FAULT) {
		dev_warn(flash->dev, "Invalid control\n");
		return rval;
	}

	rval = regmap_read(flash->regmap, REG_FAULT_AND_INFO, &fault_val);
	if (rval < 0) {
		dev_err(flash->dev, "Register read fail\n");
		return rval;
	}
	dev_dbg(flash->dev, "fault_and_info = 0x%X\n", fault_val);

	ctrl->val = 0;
	if (fault_val & FAULT_UNDER_VOLTAGE_LO_MASK)
		ctrl->val |= V4L2_FLASH_FAULT_UNDER_VOLTAGE;
	if (fault_val & FAULT_LOW_VOLTAGE_MASK)
		ctrl->val |= V4L2_FLASH_FAULT_INPUT_VOLTAGE;
	if (fault_val & FAULT_TIMEOUT_MASK)
		ctrl->val |= V4L2_FLASH_FAULT_TIMEOUT;
	if (fault_val & FAULT_OVERTEMP_MASK)
		ctrl->val |= V4L2_FLASH_FAULT_OVER_TEMPERATURE;
	if (fault_val & FAULT_LED_SHORT_MASK)
		ctrl->val |= V4L2_FLASH_FAULT_SHORT_CIRCUIT;
	if (fault_val & FAULT_LED_OPEN_MASK)
		ctrl->val |= V4L2_CID_FLASH_FAULT;

	return rval;
}

static int as3638_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct as3638_handler *ctrls_led = ctrl_to_as3638_handler(ctrl);
	enum as3638_led_id led_no = ctrls_led->led;
	struct as3638_flash *flash = ctrls_led_to_as3638_flash(ctrls_led,
							       led_no);
	int rval = 0;

	mutex_lock(&flash->lock);

	if (flash->current_led != led_no) {
		rval = as3638_init_device(flash, led_no);
		if (rval < 0) {
			dev_err(flash->dev, "Init device fail\n");
			goto leave;
		}
	}

	switch (ctrl->id) {
	case V4L2_CID_FLASH_LED_MODE:
		dev_dbg(flash->dev,
			"LED%d: V4L2_CID_FLASH_LED_MODE, val = %d\n",
			led_no + 1, ctrl->val);
		rval = as3638_mode_ctrl(flash, led_no);
		break;

	case V4L2_CID_FLASH_STROBE:
		dev_dbg(flash->dev,
			"LED%d: V4L2_CID_FLASH_STROBE, val = %d\n",
			led_no + 1, ctrl->val);
		rval = as3638_turn_on_leds(flash, led_no);
		break;

	case V4L2_CID_FLASH_STROBE_SOURCE:
		dev_dbg(flash->dev,
			"LED%d: V4L2_CID_FLASH_STROBE_SOURCE, val = %d\n",
			led_no + 1, ctrl->val);
		rval = regmap_update_bits(
			flash->regmap, REG_CONFIG, CONFIG_STROBE_ON_MASK,
			flash->strobe_source[led_no]->val <<
				CONFIG_STROBE_ON_SHIFT);
		break;

	case V4L2_CID_FLASH_STROBE_STOP:
		dev_dbg(flash->dev,
			"LED%d: V4L2_CID_FLASH_STROBE_STOP, val = %d\n",
			led_no + 1, ctrl->val);
		rval = as3638_disable_outputs(flash);
		break;

	case V4L2_CID_FLASH_TIMEOUT:
		dev_dbg(flash->dev,
			"LED%d: V4L2_CID_FLASH_TIMEOUT, val = %d\n",
			led_no + 1, ctrl->val);
		rval = regmap_update_bits(flash->regmap, REG_TIMER,
					  TIMER_FLASH_TIMEOUT_MASK,
					  flash->timeout[led_no]->val);
		break;

	case V4L2_CID_FLASH_INTENSITY:
		dev_dbg(flash->dev,
			"LED%d: V4L2_CID_FLASH_INTENSITY, val = %d\n",
			led_no + 1, ctrl->val);
		rval = as3638_disable_outputs(flash);
		if (rval < 0)
			goto leave;
		rval = as3638_set_intensity(flash, led_no);
		if (rval < 0)
			goto leave;
		break;

	case V4L2_CID_FLASH_TORCH_INTENSITY:
		dev_dbg(flash->dev,
			"LED%d: V4L2_CID_FLASH_TORCH_INTENSITY, val = %d\n",
			led_no + 1, ctrl->val);
		rval = as3638_set_intensity(flash, led_no);
		if (rval < 0)
			goto leave;
		break;

	case V4L2_CID_FLASH_FAULT:
		dev_dbg(flash->dev,
			"LED%d: V4L2_CID_FLASH_FAULT, val = %d\n",
			led_no + 1, ctrl->val);
		rval = 0;
		break;
	default:
		dev_warn(flash->dev,
			 "LED%d: Invalid control, id = 0x%X, val = %d\n",
			 led_no + 1, ctrl->id, ctrl->val);
		rval = -EINVAL;
		break;
	}
leave:
	mutex_unlock(&flash->lock);
	return rval;
}

static const struct v4l2_ctrl_ops as3638_led_ctrl_ops = {
	.g_volatile_ctrl = as3638_get_ctrl,
	.s_ctrl = as3638_set_ctrl,
};

static int as3638_init_controls(struct as3638_flash *flash,
				enum as3638_led_id led_no)
{
	struct v4l2_ctrl *fault;
	u32 max_flash_brt = flash->pdata->flash_max_brightness[led_no];
	u32 max_torch_brt = flash->pdata->torch_max_brightness[led_no];
	struct v4l2_ctrl_handler *hdl = &flash->ctrls_led[led_no].handler;
	const struct v4l2_ctrl_ops *ops = &as3638_led_ctrl_ops;

	dev_dbg(flash->dev, "Init controls: LED%d\n", led_no + 1);

	flash->ctrls_led[led_no].led = led_no;
	v4l2_ctrl_handler_init(hdl, 8);

	flash->led_mode[led_no] = v4l2_ctrl_new_std_menu(
		hdl, ops, V4L2_CID_FLASH_LED_MODE,
		V4L2_FLASH_LED_MODE_TORCH, ~0x7,
		V4L2_FLASH_LED_MODE_NONE);

	flash->strobe_source[led_no] = v4l2_ctrl_new_std_menu(
		hdl, ops, V4L2_CID_FLASH_STROBE_SOURCE,
		0x1, ~0x3, V4L2_FLASH_STROBE_SOURCE_SOFTWARE);

	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_STROBE, 0, 0, 0, 0);

	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_STROBE_STOP, 0, 0, 0, 0);

	flash->timeout[led_no] = v4l2_ctrl_new_std(
		hdl, ops, V4L2_CID_FLASH_TIMEOUT,
		AS3638_FLASH_TOUT_MIN, AS3638_FLASH_TOUT_MAX,
		AS3638_FLASH_TOUT_STEP, AS3638_FLASH_TOUT_DEF);

	flash->flash_intensity[led_no] = v4l2_ctrl_new_std(
		hdl, ops, V4L2_CID_FLASH_INTENSITY,
		0, max_flash_brt, 1, 0);

	flash->torch_intensity[led_no] = v4l2_ctrl_new_std(
		hdl, ops, V4L2_CID_FLASH_TORCH_INTENSITY,
		0, max_torch_brt, 1, 0);

	fault = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_FAULT, 0,
				  V4L2_FLASH_FAULT_UNDER_VOLTAGE
				  | V4L2_FLASH_FAULT_INPUT_VOLTAGE
				  | V4L2_FLASH_FAULT_TIMEOUT
				  | V4L2_FLASH_FAULT_OVER_TEMPERATURE
				  | V4L2_FLASH_FAULT_SHORT_CIRCUIT
				  | V4L2_CID_FLASH_FAULT, 0, 0);
	if (fault)
		fault->flags |= V4L2_CTRL_FLAG_VOLATILE;

	if (hdl->error) {
		dev_err(flash->dev, "Fail, LED = %d\n", led_no + 1);
		return hdl->error;
	}
	flash->subdev_led[led_no].subdev.ctrl_handler = hdl;
	return 0;
}

static const struct v4l2_subdev_ops as3638_ops = {
	.core = NULL,
};

static const struct regmap_config as3638_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REG_HIG_OVERVOLTAGE_PROTECTION,
};

static const
struct v4l2_subdev_internal_ops as3638_internal_ops;

static int as3638_subdev_init(struct as3638_flash *flash,
			      enum as3638_led_id led_no, char *led_name)
{
	struct i2c_client *client = to_i2c_client(flash->dev);
	int rval = -ENODEV;

	dev_dbg(flash->dev, "LED = %d, led_name = %s\n", led_no + 1, led_name);

	v4l2_subdev_init(&flash->subdev_led[led_no].subdev, &as3638_ops);
	flash->subdev_led[led_no].subdev.internal_ops = &as3638_internal_ops;
	flash->subdev_led[led_no].subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(flash->subdev_led[led_no].subdev.name,
		 sizeof(flash->subdev_led[led_no].subdev.name),
		 AS3638_NAME " LED%d %d-%4.4x",
		 led_no + 1,
		 i2c_adapter_id(client->adapter),
		 client->addr);
	flash->subdev_led[led_no].led = led_no;

	if (flash->subdev_led[AS3638_LED1].subdev.v4l2_dev)
		rval = v4l2_device_register_subdev(
			flash->subdev_led[AS3638_LED1].subdev.v4l2_dev,
			&flash->subdev_led[led_no].subdev);
	if (rval) {
		dev_err(flash->dev, "Register subdev fail LED%d\n", led_no + 1);
		goto err_out;
	}

	rval = as3638_init_controls(flash, led_no);
	if (rval) {
		dev_err(flash->dev, "Init controls fail LED%d\n", led_no + 1);
		goto err_out;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	rval = media_entity_init(&flash->subdev_led[led_no].subdev.entity,
				 0, NULL, 0);
#else
	rval = media_entity_pads_init(&flash->subdev_led[led_no].subdev.entity,
				 0, NULL);
#endif
	if (rval < 0) {
		dev_err(flash->dev, "Media init fail LED%d\n", led_no + 1);
		goto err_out;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	flash->subdev_led[led_no].subdev.entity.type =
		MEDIA_ENT_T_V4L2_SUBDEV_FLASH;
#else
	flash->subdev_led[led_no].subdev.entity.function =
		MEDIA_ENT_F_FLASH;
#endif
err_out:
	return rval;
}

static int as3638_registered(struct v4l2_subdev *subdev)
{
	struct as3638_subdev *subdev_led = subdev_to_as3638_subdev_led(subdev);
	enum as3638_led_id led_no = subdev_led->led;
	struct as3638_flash *flash = subdev_led_to_as3638_flash(subdev_led,
								led_no);
	int rval;

	/*
	 * Only initialize the additional subdevices for LED2 and LED3 as a
	 * result of the registration of the subdevice for LED1. The
	 * registration for LED2 and LED3 will also end up in this function.
	 */
	if (led_no != AS3638_LED1)
		return 0;

	dev_dbg(flash->dev, "register LED%d\n", led_no + 1);

	/* The LED1 subdevice was already initialized during the probe call */

	rval = as3638_subdev_init(flash, AS3638_LED2, "as3638-led2");
	if (rval < 0) {
		dev_err(flash->dev, "Subdev init LED2 fail\n");
		return rval;
	}
	rval = as3638_subdev_init(flash, AS3638_LED3, "as3638-led3");
	if (rval < 0) {
		dev_err(flash->dev, "Subdev init LED3 fail\n");
		return rval;
	}
	return rval;
}

static void as3638_unregistered(struct v4l2_subdev *subdev)
{
	struct as3638_subdev *subdev_led = subdev_to_as3638_subdev_led(subdev);
	enum as3638_led_id led_no = subdev_led->led;
	struct as3638_flash *flash = subdev_led_to_as3638_flash(subdev_led,
								led_no);
	dev_dbg(flash->dev, "Unregistered\n");
}

static int as3638_open(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh)
{
	struct as3638_subdev *subdev_led = subdev_to_as3638_subdev_led(subdev);
	enum as3638_led_id led_no = subdev_led->led;
	struct as3638_flash *flash = subdev_led_to_as3638_flash(subdev_led,
								led_no);
	int rval = 0;

	dev_dbg(flash->dev, "open LED%d\n", led_no + 1);

	pm_runtime_get_sync(flash->dev);

	mutex_lock(&flash->lock);

	if ((led_no == AS3638_LED1 && (flash->open[AS3638_LED2] ||
				       flash->open[AS3638_LED3])) ||
	    (led_no != AS3638_LED1 && (flash->open[AS3638_LED1]))) {
		dev_info(flash->dev,
			 "led 1 and leds 2&3 can't be controlled in parallel");
		rval = -EBUSY;
		goto error;
	}

	rval = as3638_init_device(flash, led_no);
	if (rval < 0) {
		dev_err(flash->dev, "Init device fail\n");
		goto error;
	}

	flash->open[led_no] = true;
	mutex_unlock(&flash->lock);
	return rval;
error:
	mutex_unlock(&flash->lock);
	pm_runtime_put(flash->dev);
	return rval;
}

static int as3638_close(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh)
{
	struct as3638_subdev *subdev_led = subdev_to_as3638_subdev_led(subdev);
	enum as3638_led_id led_no = subdev_led->led;
	struct as3638_flash *flash = subdev_led_to_as3638_flash(subdev_led,
								led_no);
	dev_dbg(flash->dev, "close LED%d\n", led_no + 1);

	mutex_lock(&flash->lock);
	flash->open[led_no] = false;
	mutex_unlock(&flash->lock);

	pm_runtime_put(flash->dev);

	return 0;
}

static const struct v4l2_subdev_internal_ops as3638_internal_ops = {
	.registered = as3638_registered,
	.unregistered = as3638_unregistered,
	.open = as3638_open,
	.close = as3638_close,
};

static int as3638_i2c_subdev_init(struct as3638_flash *flash,
				  enum as3638_led_id led_no, char *led_name)
{
	struct i2c_client *client = to_i2c_client(flash->dev);
	int rval;

	dev_dbg(flash->dev, "subdev init LED = %d, led_name = %s\n",
		led_no + 1, led_name);

	v4l2_i2c_subdev_init(&flash->subdev_led[led_no].subdev,
			     client, &as3638_ops);
	flash->subdev_led[led_no].subdev.internal_ops = &as3638_internal_ops;
	flash->subdev_led[led_no].subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(flash->subdev_led[led_no].subdev.name,
		 sizeof(flash->subdev_led[led_no].subdev.name),
		 AS3638_NAME " LED%d %d-%4.4x",
		 led_no + 1,
		 i2c_adapter_id(client->adapter),
		 client->addr);
	flash->subdev_led[led_no].led = led_no;

	rval = as3638_init_controls(flash, AS3638_LED1);
	if (rval) {
		dev_err(flash->dev, "Init controls fail LED1\n");
		return rval;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	rval = media_entity_init(&flash->subdev_led[led_no].subdev.entity,
				 0, NULL, 0);
#else
	rval = media_entity_pads_init(&flash->subdev_led[led_no].subdev.entity,
				 0, NULL);
#endif
	if (rval < 0) {
		dev_err(flash->dev, "Media init fail LED1\n");
		goto err_out;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	flash->subdev_led[led_no].subdev.entity.type =
		MEDIA_ENT_T_V4L2_SUBDEV_FLASH;
#else
	flash->subdev_led[led_no].subdev.entity.function =
		MEDIA_ENT_F_FLASH;
#endif
err_out:
	return rval;
}

static int as3638_probe(struct i2c_client *client,
			const struct i2c_device_id *devid)
{
	struct as3638_flash *flash;
	struct as3638_platform_data *pdata = dev_get_platdata(&client->dev);
	unsigned int reg_val;
	int rval;

	flash = devm_kzalloc(&client->dev, sizeof(*flash), GFP_KERNEL);
	if (!flash)
		return -ENOMEM;

	mutex_init(&flash->lock);

	flash->regmap = devm_regmap_init_i2c(client, &as3638_regmap);
	if (IS_ERR(flash->regmap))
		return PTR_ERR(flash->regmap);

	if (!pdata) {
		dev_err(flash->dev, "Missing platform data\n");
		return -ENXIO;
	}
	flash->pdata = pdata;
	flash->dev = &client->dev;

	rval = gpio_request(flash->pdata->gpio_reset, "flash reset");
	if (rval < 0) {
		dev_err(flash->dev, "Request reset GPIO fail\n");
		goto error;
	}
	rval = gpio_direction_output(flash->pdata->gpio_reset, 1);
	if (rval < 0) {
		dev_err(flash->dev, "Setting reset GPIO fail\n");
		goto error;
	}

	rval = regmap_read(flash->regmap, REG_DESIGN_INFO, &reg_val);
	if (rval < 0) {
		dev_err(flash->dev, "Device read failed\n");
		goto error;
	}
	if (reg_val != DESIGN_INFO_FIXED_ID) {
		dev_err(flash->dev,
			"Wrong ID returned (0x%X). Must be 0x%X\n",
			reg_val, DESIGN_INFO_FIXED_ID);
		goto error;
	}

	rval = regmap_read(flash->regmap, REG_VERSION_CONTROL, &reg_val);
	if (rval < 0) {
		dev_err(flash->dev, "Device read failed\n");
		goto error;
	}
	dev_dbg(flash->dev, "AS3638 chip version 0x%4X\n",
		reg_val & VERSION_MASK);

	rval = as3638_i2c_subdev_init(flash, AS3638_LED1, "as3638-led1");
	if (rval < 0) {
		dev_err(flash->dev, "Subdev init LED1 fail\n");
		goto error2;
	}

	gpio_set_value(flash->pdata->gpio_reset, 0);

	flash->current_led = AS3638_NO_LED;
	pm_runtime_enable(flash->dev);

	dev_dbg(flash->dev, "Success\n");
	return 0;
error2:
	v4l2_device_unregister_subdev(&flash->subdev_led[AS3638_LED1].subdev);
	media_entity_cleanup(&flash->subdev_led[AS3638_LED1].subdev.entity);
error:
	gpio_free(flash->pdata->gpio_reset);
	return rval;
}

static void as3638_subdev_cleanup(struct as3638_flash *flash)
{
	int i;

	dev_dbg(flash->dev, "Clean up\n");

	for (i = AS3638_LED1; i < AS3638_LED_MAX; i++) {
		v4l2_device_unregister_subdev(&flash->subdev_led[i].subdev);
		v4l2_ctrl_handler_free(&flash->ctrls_led[i].handler);
		media_entity_cleanup(&flash->subdev_led[i].subdev.entity);
	}
}

static int as3638_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct as3638_flash *flash =
		container_of(sd, struct as3638_flash,
			     subdev_led[AS3638_LED1].subdev);

	dev_dbg(flash->dev, "remove\n");

	as3638_subdev_cleanup(flash);
	gpio_free(flash->pdata->gpio_reset);
	return 0;
}

#ifdef CONFIG_PM
static int as3638_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct as3638_subdev *sd_led = subdev_to_as3638_subdev_led(sd);
	struct as3638_flash *flash = subdev_led_to_as3638_flash(sd_led,
								sd_led->led);
	gpio_set_value(flash->pdata->gpio_reset, 0);
	return 0;
}

static int as3638_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct as3638_subdev *sd_led = subdev_to_as3638_subdev_led(sd);
	struct as3638_flash *flash = subdev_led_to_as3638_flash(sd_led,
								sd_led->led);
	int rval;

	gpio_set_value(flash->pdata->gpio_reset, 1);

	if (flash->current_led == AS3638_NO_LED)
		return 0;

	rval = v4l2_ctrl_handler_setup(
			&flash->ctrls_led[flash->current_led].handler);

	return rval;
}
#else
#define as3638_suspend NULL
#define as3638_resume  NULL
#endif

static const struct i2c_device_id as3638_id_table[] = {
	{AS3638_NAME, 0},
	{}
};

static const struct dev_pm_ops as3638_pm_ops = {
	.suspend	= as3638_suspend,
	.resume		= as3638_resume,
	.runtime_suspend = as3638_suspend,
	.runtime_resume = as3638_resume,
};

MODULE_DEVICE_TABLE(i2c, as3638_id_table);

static struct i2c_driver as3638_i2c_driver = {
	.driver = {
		  .name = AS3638_NAME,
		  .pm = &as3638_pm_ops,
		  },
	.probe = as3638_probe,
	.remove = as3638_remove,
	.id_table = as3638_id_table,
};

module_i2c_driver(as3638_i2c_driver);

MODULE_AUTHOR("Soren Friis <soren.friis@intel.com>");
MODULE_DESCRIPTION("AMS AS3638 Triple Flash LED driver");
MODULE_LICENSE("GPL");
