// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2014 - 2018 Intel Corporation
 *
 * Lm3560 and other TI drivers used for this, written by
 * Daniel Jeong <gshark.jeong@gmail.com>"
 *
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include "../../../include/media/lm3643.h"
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>

/* registers definitions */
#define ENABLE_REG		0x01
#define MODE_SHIFT		2
#define TORCH_NTC_EN_SHIFT	4
#define STROBE_EN_SHIFT		5
#define STROBE_TYPE_SHIFT	6
#define TX_PIN_MASK		0x80
#define TX_EN_SHIFT		7
#define MODE_MASK		0xc
#define MASK_STROBE_SRC		1
#define ENABLE_MASK		0x3

#define IVFM_REG		0x02
#define IVFM_SELECTION_SHIFT	0
#define IVFM_DISABLE		0
#define IVFM_STOP_AND_HOLD	1
#define IVFM_DONW		2
#define IVFM_UP_AND_DONW	3
#define IVFM_HYSTERESIS_SHIFT	2
#define IVFM_LEVEL_SHIFT	3
#define IVFM_UVLO_SHIFT		6

#define LED1_FLASH_BR_REG	0x03
#define LED2_FLASH_BR_REG	0x04
#define LED1_TORCH_BR_REG	0x05
#define LED2_TORCH_BR_REG	0x06
#define MASK_TORCH_BR		0x7f
#define MASK_FLASH_BR		0x7f
#define LED2_OVERDRIVE_SHIFT	7

#define BOOST_REG		0x07
#define BOOST_CURRENT_LIMIT_SHIFT 0
#define BOOST_FREQ_SHIFT	1
#define BOOST_MODE_SHIFT	2
#define BOOST_FAULT_DETEC_SHIFT	3
#define BOOST_SW_RESET_SHIFT	7

#define TIMING_REG		0x08
#define MASK_FLASH_TOUT		0xf

#define FLASH_TIMEOUT_SHIFT	0
#define TORCH_CURRENT_RAMP_SHIFT	4
#define TEMP_REG			0x08
#define FLAGS1_REG		0x0a
#define FLAGS2_REG		0x0b
#define FAULT_TIMEOUT		(1 << 0)
#define FAULT_UVLO		(1 << 1)
#define FAULT_THERMAL_SHUTDOWN	(1 << 2)
#define FAULT_SHORT_CIRCUIT_LED1	(1 << 5)
#define FAULT_SHORT_CIRCUIT_LED2	(1 << 4)
#define FAULT_SHORT_CIRCUIT_VOUT	(1 << 6)
#define FAULT_OVP		(1 << 1)
#define FAULT_OCP		(1 << 3)
#define FAULT_IVFM		(1 << 2)
#define FAULT_OVERTEMP		(1 << 0)
#define FAULT_NTC_TRIP		(3 << 3)


#define FAULT_SHORT (FAULT_SHORT_CIRCUIT_LED1 | FAULT_SHORT_CIRCUIT_LED2 \
		| FAULT_SHORT_CIRCUIT_VOUT)

enum led_mode {
	MODE_SHDN	= 0x0,
	MODE_IR_DRIVE	= 0x1,
	MODE_TORCH	= 0x2,
	MODE_FLASH	= 0x3,
};

#ifndef V4L2_FLASH_FAULT_UNDER_VOLTAGE
 #define V4L2_FLASH_FAULT_UNDER_VOLTAGE          (1 << 6)
#endif
#ifndef V4L2_FLASH_FAULT_INPUT_VOLTAGE
 #define V4L2_FLASH_FAULT_INPUT_VOLTAGE          (1 << 7)
#endif
#ifndef V4L2_FLASH_FAULT_LED_OVER_TEMPERATURE
 #define V4L2_FLASH_FAULT_LED_OVER_TEMPERATURE   (1 << 8)
#endif

/*
  struct lm3643_gpio
  *
  * @list: list node
  * @mutex: serialize access
  * @gpio: gpio number
  * @num_users: number of lm3643 devices which shares this gpio
  * @use_count: Number of requests to turn gpio on
 */

struct lm3643_gpio {
	struct list_head list;
	struct mutex mutex;
	int gpio;
	int num_users;
	int use_count;
};

/*
 * struct lm3643_flash
 *
 * @pdata: platform data
 * @regmap: reg. map for i2c
 * @lock: muxtex for serial access.
 * @led_mode: V4L2 LED mode
 * @ctrls_led: V4L2 contols
 * @subdev_led: V4L2 subdev
 * @mode_reg : mode register value
 * @lmgpio: Gpio access struct
 */
struct lm3643_flash {
	struct device *dev;
	struct lm3643_platform_data *pdata;
	struct regmap *regmap;
	struct v4l2_ctrl_handler ctrls_led;
	struct v4l2_subdev subdev_led;
	enum v4l2_flash_led_mode led_mode;
	struct lm3643_gpio *lmgpio;
};

#define to_lm3643_flash(_ctrl)	\
	container_of(_ctrl->handler, struct lm3643_flash, ctrls_led)

static LIST_HEAD(lm3643_gpios);
static DEFINE_MUTEX(lm3643_gpio_mutex);

static int lm3643_suspend(struct device *dev);
static int lm3643_resume(struct device *dev);
static int lm3643_init_device(struct lm3643_flash *flash);

/* Call this from probe */
static struct lm3643_gpio *lm3643_get_gpio(int gpio_num, struct device *dev)
{

	struct lm3643_gpio *gpio;
	struct lm3643_gpio *tmp;
	int rval;

	if (gpio_num < 0) {
		dev_err(dev, "Invalid GPIO\n");
		return NULL;
	}

	mutex_lock(&lm3643_gpio_mutex);

	list_for_each_entry_safe(gpio, tmp, &lm3643_gpios, list) {
		if (gpio_num == gpio->gpio)
			goto out;
	}
	/* New */
	gpio = kzalloc(sizeof(*gpio), GFP_KERNEL);
	if (!gpio) {
		dev_err(dev, "No memory\n");
		goto error;
	}

	mutex_init(&gpio->mutex);

	rval = gpio_request(gpio_num, "flash reset");
	if (rval) {
		dev_err(dev, "Can't get gpio %d\n", gpio_num);
		goto freemem;
	}

	rval = gpio_direction_output(gpio_num, 0);
	if (rval) {
		dev_err(dev, "Can't set gpio to output\n");
		goto freegpio;
	}

	gpio->gpio = gpio_num;
	list_add(&gpio->list, &lm3643_gpios);

out:
	mutex_unlock(&lm3643_gpio_mutex);

	if (gpio) {
		mutex_lock(&gpio->mutex);
		gpio->num_users++;
		mutex_unlock(&gpio->mutex);
	}

	return gpio;
freegpio:
	gpio_free(gpio_num);
freemem:
	kfree(gpio);
error:
	mutex_unlock(&lm3643_gpio_mutex);
	return NULL;
}

static void lm3643_gpio_ctrl(struct lm3643_gpio *gpio, bool state)
{
	mutex_lock(&gpio->mutex);
	if (state) {
		gpio_set_value(gpio->gpio, 1);
		gpio->use_count++;
	} else {
		gpio->use_count--;
		if (!gpio->use_count)
			gpio_set_value(gpio->gpio, 0);
	}
	mutex_unlock(&gpio->mutex);
}

static void lm3643_gpio_remove(struct lm3643_gpio *gpio, int gpio_num)
{
	struct lm3643_gpio *tmp;

	mutex_lock(&lm3643_gpio_mutex);

	list_for_each_entry_safe(gpio, tmp, &lm3643_gpios, list) {
		if (gpio_num != gpio->gpio)
			continue;
		mutex_lock(&gpio->mutex);
		gpio->num_users--;
		if (!gpio->num_users) {
			gpio_free(gpio->gpio);
			list_del(&gpio->list);
			mutex_unlock(&gpio->mutex);
			mutex_destroy(&gpio->mutex);
			kfree(gpio);
		} else {
			mutex_unlock(&gpio->mutex);
		}
	}
	mutex_unlock(&lm3643_gpio_mutex);
}

/* enable mode control */
static int lm3643_mode_ctrl(struct lm3643_flash *flash)
{
	switch (flash->led_mode) {
	case V4L2_FLASH_LED_MODE_NONE:
		regmap_update_bits(flash->regmap,
				   ENABLE_REG,
				   MODE_MASK,
				   MODE_SHDN << MODE_SHIFT);
		break;
	case V4L2_FLASH_LED_MODE_TORCH:
		regmap_update_bits(flash->regmap,
				   ENABLE_REG,
				   MODE_MASK,
				   MODE_TORCH << MODE_SHIFT);
		break;
	case V4L2_FLASH_LED_MODE_FLASH:
		regmap_update_bits(flash->regmap,
				   ENABLE_REG,
				   MODE_MASK,
				   MODE_FLASH << MODE_SHIFT);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* V4L2 controls  */
static int lm3643_get_ctrl(struct v4l2_ctrl *ctrl)
{
	struct lm3643_flash *flash = to_lm3643_flash(ctrl);
	unsigned int reg_val1, reg_val2;
	int rval;

	if (ctrl->id != V4L2_CID_FLASH_FAULT)
		return -EINVAL;

	rval = regmap_read(flash->regmap, FLAGS1_REG, &reg_val1);
	if (rval < 0)
		return rval;

	rval = regmap_read(flash->regmap, FLAGS2_REG, &reg_val2);
	if (rval < 0)
		return rval;

	dev_dbg(flash->dev, "Flags1 = 0x%x, Flags2 = 0x%x\n", reg_val1,
			reg_val2);
	ctrl->val = 0;
	if (reg_val1 & FAULT_TIMEOUT)
		ctrl->val |= V4L2_FLASH_FAULT_TIMEOUT;
	if (reg_val1 & FAULT_SHORT)
		ctrl->val |= V4L2_FLASH_FAULT_SHORT_CIRCUIT;
	if (reg_val1 & FAULT_UVLO)
		ctrl->val |= V4L2_FLASH_FAULT_UNDER_VOLTAGE;
	if (reg_val2 & FAULT_IVFM)
		ctrl->val |= V4L2_FLASH_FAULT_INPUT_VOLTAGE;
	if (reg_val1 & FAULT_OCP)
		ctrl->val |= V4L2_FLASH_FAULT_OVER_CURRENT;
	if (reg_val1 & FAULT_THERMAL_SHUTDOWN)
		ctrl->val |= V4L2_FLASH_FAULT_OVER_TEMPERATURE;
	if (reg_val2 & FAULT_OVERTEMP)
		ctrl->val |= V4L2_FLASH_FAULT_OVER_TEMPERATURE;
	if (reg_val2 & FAULT_NTC_TRIP)
		ctrl->val |= V4L2_FLASH_FAULT_LED_OVER_TEMPERATURE;
	if (reg_val2 & FAULT_OVP)
		ctrl->val |= V4L2_FLASH_FAULT_OVER_VOLTAGE;
	return 0;
}

static int lm3643_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct lm3643_flash *flash = to_lm3643_flash(ctrl);
	int rval = 0;

	dev_dbg(flash->dev, "lm3643 control 0x%x\n", ctrl->id);

	switch (ctrl->id) {
	case V4L2_CID_FLASH_LED_MODE:
		if (flash->led_mode == ctrl->val)
			break;
		flash->led_mode = ctrl->val;
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH)
			rval = lm3643_mode_ctrl(flash);
		break;
	case V4L2_CID_FLASH_STROBE:
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH) {
			rval = -EBUSY;
			break;
			}
		flash->led_mode = V4L2_FLASH_LED_MODE_FLASH;
		rval = lm3643_mode_ctrl(flash);
		break;
	case V4L2_CID_FLASH_STROBE_SOURCE:
		if (ctrl->val == V4L2_FLASH_STROBE_SOURCE_EXTERNAL) {
				flash->led_mode = V4L2_FLASH_LED_MODE_NONE;
				lm3643_mode_ctrl(flash);
		}
		rval = regmap_update_bits(flash->regmap,
					  ENABLE_REG,
					  MASK_STROBE_SRC,
					  (ctrl->val) << STROBE_EN_SHIFT);
		break;
	case V4L2_CID_FLASH_STROBE_STOP:
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH) {
			rval = -EBUSY;
			break;
		}
		flash->led_mode = V4L2_FLASH_LED_MODE_NONE;
		rval = lm3643_mode_ctrl(flash);
		break;
	case V4L2_CID_FLASH_TIMEOUT:
		rval = regmap_update_bits(flash->regmap,
					  TIMING_REG, MASK_FLASH_TOUT,
					  ctrl->val);
		break;
	/*
	 * Intensity: flash intensity in given in mA
	*/
	case V4L2_CID_FLASH_INTENSITY:
		rval = regmap_update_bits(flash->regmap,
					  LED1_FLASH_BR_REG,
					  MASK_FLASH_BR,
					  LM3643_FLASH_BRT_mA_TO_REG(
						  ctrl->val));
		break;
	case V4L2_CID_FLASH_TORCH_INTENSITY:
		rval = regmap_update_bits(flash->regmap,
					  LED1_TORCH_BR_REG,
					  MASK_TORCH_BR,
					  LM3643_TORCH_BRT_mA_TO_REG(
						  ctrl->val));
		break;
	case V4L2_CID_FLASH_FAULT:
		break;
	default:
		dev_err(flash->dev, "lm3643 invalid control 0x%x\n", ctrl->id);
		rval = -EINVAL;
		break;
	}
	return rval;
}

static const struct v4l2_ctrl_ops lm3643_led_ctrl_ops = {
	.g_volatile_ctrl = lm3643_get_ctrl,
	.s_ctrl = lm3643_set_ctrl,
};
static int lm3643_init_controls(struct lm3643_flash *flash)
{
	struct v4l2_ctrl *fault;

	struct v4l2_ctrl_handler *hdl = &flash->ctrls_led;
	const struct v4l2_ctrl_ops *ops = &lm3643_led_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 8);

	/* flash mode */
	v4l2_ctrl_new_std_menu(hdl, ops,
				V4L2_CID_FLASH_LED_MODE,
				V4L2_FLASH_LED_MODE_TORCH,
				~0x7,
				V4L2_FLASH_LED_MODE_NONE);

	flash->led_mode = V4L2_FLASH_LED_MODE_NONE;

	/* flash source */
	v4l2_ctrl_new_std_menu(hdl, ops,
				V4L2_CID_FLASH_STROBE_SOURCE,
				0x1,
				~0x1,
				V4L2_FLASH_STROBE_SOURCE_SOFTWARE);

	/* flash strobe */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_STROBE, 0, 0, 0, 0);

	/* flash strobe stop */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_STROBE_STOP, 0, 0, 0, 0);

	/* flash strobe timeout */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_TIMEOUT,
			  LM3643_FLASH_TOUT_MIN,
			  LM3643_FLASH_TOUT_MAX,
			  LM3643_FLASH_TOUT_STEP, LM3643_FLASH_TOUT_DEF);
	/*max flash current in uA*/
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_INTENSITY,
			  0,
			  flash->pdata->flash_max_brightness,
			  1,
			  flash->pdata->flash_max_brightness);

	/* max torch current uA*/
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_TORCH_INTENSITY,
			  0,
			  flash->pdata->torch_max_brightness,
			  1,
			  flash->pdata->torch_max_brightness);

	/* fault */
	fault = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_FAULT, 0,
				  V4L2_FLASH_FAULT_OVER_VOLTAGE
				  | V4L2_FLASH_FAULT_OVER_TEMPERATURE
				  | V4L2_FLASH_FAULT_SHORT_CIRCUIT
				  | V4L2_FLASH_FAULT_TIMEOUT, 0, 0);
	if (fault != NULL)
		fault->flags |= V4L2_CTRL_FLAG_VOLATILE;
	if (hdl->error)
		dev_err(flash->dev, "lm3643_init_controls fail\n");
	flash->subdev_led.ctrl_handler = hdl;
	return hdl->error;
}

static int lm3643_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct lm3643_flash *flash = container_of(sd, struct lm3643_flash,
						  subdev_led);
	int rval;

	rval = pm_runtime_get_sync(flash->dev);
	dev_dbg(flash->dev, "%s rval = %d\n", __func__, rval);
	return rval;
}

static int lm3643_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct lm3643_flash *flash = container_of(sd, struct lm3643_flash,
						  subdev_led);
	dev_dbg(flash->dev, "%s\n", __func__);
	pm_runtime_put(flash->dev);

	return 0;
}
static int lm3643_s_power(struct v4l2_subdev *sd, int on)
{
	struct lm3643_flash *flash = container_of(sd, struct lm3643_flash,
						  subdev_led);
	dev_dbg(flash->dev, "%s value = %d\n", __func__, on);

	if (!on)
		pm_runtime_put(flash->dev);
	else
		pm_runtime_get_sync(flash->dev);
	return 0;
}
static const struct v4l2_subdev_core_ops lm3643_core_ops = {
	.s_power = lm3643_s_power,
};

static const struct v4l2_subdev_internal_ops lm3643_int_ops = {
	.open = lm3643_open,
	.close = lm3643_close,
};
static const struct v4l2_subdev_ops lm3643_ops = {
	.core = &lm3643_core_ops,
};

static const struct regmap_config lm3643_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xFF,
};

static int lm3643_subdev_init(struct lm3643_flash *flash)
{
	struct i2c_client *client = to_i2c_client(flash->dev);
	int rval;

	dev_dbg(flash->dev, "lm3643 subdev init\n");
	v4l2_i2c_subdev_init(&flash->subdev_led, client, &lm3643_ops);
	flash->subdev_led.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(flash->subdev_led.name, sizeof(flash->subdev_led.name),
		 LM3643_NAME " %d-%4.4x", i2c_adapter_id(client->adapter),
		 client->addr);
	flash->subdev_led.internal_ops = &lm3643_int_ops;
	rval = lm3643_init_controls(flash);
	if (rval)
		goto err_out;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	rval = media_entity_init(&flash->subdev_led.entity, 0, NULL, 0);
#else
	rval = media_entity_pads_init(&flash->subdev_led.entity, 0, NULL);
#endif
	if (rval < 0)
		goto err_out;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	flash->subdev_led.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_FLASH;
#else
	flash->subdev_led.entity.function = MEDIA_ENT_F_FLASH;
#endif
err_out:
	return rval;
}

static void lm3646_subdev_cleanup(struct lm3643_flash *flash)
{
	v4l2_ctrl_handler_free(&flash->ctrls_led);
	v4l2_device_unregister_subdev(&flash->subdev_led);
	media_entity_cleanup(&flash->subdev_led.entity);
	lm3643_gpio_ctrl(flash->lmgpio, 0);
	lm3643_gpio_remove(flash->lmgpio, flash->lmgpio->gpio);
}

static int lm3643_init_device(struct lm3643_flash *flash)
{
	unsigned int reg_val;
	int rval;

	/* output disable */
	flash->led_mode = V4L2_FLASH_LED_MODE_NONE;
	lm3643_mode_ctrl(flash);

	/* Enable both leds*/
	rval = regmap_update_bits(flash->regmap,
				  ENABLE_REG,
				  ENABLE_MASK,
				  0x3);
	if (rval < 0)
		return rval;

	rval = regmap_update_bits(flash->regmap,
				  LED1_FLASH_BR_REG,
				  MASK_FLASH_BR,
				  LM3643_FLASH_BRT_mA_TO_REG(
					flash->pdata->flash_max_brightness));
	if (rval < 0)
		return rval;

	rval = regmap_update_bits(flash->regmap,
				  LED1_TORCH_BR_REG,
				  MASK_TORCH_BR,
				  LM3643_TORCH_BRT_mA_TO_REG(
					flash->pdata->torch_max_brightness));
	if (rval < 0)
		return rval;

	/* Reset flag registers */
	dev_dbg(flash->dev, "%s reset flag registers\n", __func__);
	rval = regmap_read(flash->regmap, FLAGS1_REG, &reg_val);
	if (rval < 0)
		return rval;

	return regmap_read(flash->regmap, FLAGS2_REG, &reg_val);
}

static int lm3643_probe(struct i2c_client *client,
			const struct i2c_device_id *devid)
{
	struct lm3643_flash *flash;
	struct lm3643_platform_data *pdata = dev_get_platdata(&client->dev);
	int rval;

	flash = devm_kzalloc(&client->dev, sizeof(*flash), GFP_KERNEL);
	if (flash == NULL)
		return -ENOMEM;

	flash->regmap = devm_regmap_init_i2c(client, &lm3643_regmap);
	if (IS_ERR(flash->regmap))
		return PTR_ERR(flash->regmap);

	/* check device tree if there is no platform data */
	if (pdata == NULL) {
		pdata = devm_kzalloc(&client->dev,
				     sizeof(struct lm3643_platform_data),
				     GFP_KERNEL);
		if (pdata == NULL)
			return -ENOMEM;

		pdata->flash_max_brightness = 500;
		pdata->torch_max_brightness = 89;
	}

	flash->pdata = pdata;
	flash->dev = &client->dev;

	flash->lmgpio = lm3643_get_gpio(flash->pdata->gpio_reset, flash->dev);
	if (!flash->lmgpio)
		return -ENODEV;
	lm3643_gpio_ctrl(flash->lmgpio, 1);
	rval = lm3643_init_device(flash);
	if (rval < 0) {
		dev_err(flash->dev, "%s initdevice fail\n", __func__);
		lm3643_gpio_ctrl(flash->lmgpio, 0);
		lm3643_gpio_remove(flash->lmgpio, flash->lmgpio->gpio);
		return rval;
	}
	rval = lm3643_subdev_init(flash);
	if (rval < 0) {
		dev_err(flash->dev, "%s subdev init fail\n", __func__);
		lm3646_subdev_cleanup(flash);
		return rval;
	}
	dev_dbg(flash->dev, "%s Success\n", __func__);
	lm3643_gpio_ctrl(flash->lmgpio, 0);
	pm_runtime_enable(flash->dev);
	return 0;
}

static int lm3643_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lm3643_flash *flash = container_of(sd, struct lm3643_flash,
						  subdev_led);
	lm3646_subdev_cleanup(flash);
	pm_runtime_disable(flash->dev);
	dev_info(flash->dev, "%s\n", __func__);
	return 0;
}

#ifdef CONFIG_PM

static int lm3643_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lm3643_flash *flash = container_of(sd, struct lm3643_flash,
						  subdev_led);
	dev_dbg(flash->dev, "%s\n", __func__);

	lm3643_gpio_ctrl(flash->lmgpio, 0);

	return 0;
}

static int lm3643_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lm3643_flash *flash = container_of(sd, struct lm3643_flash,
						  subdev_led);
	int rval;

	lm3643_gpio_ctrl(flash->lmgpio, 1);
	rval = lm3643_init_device(flash);
	if (rval)
		goto out;

	/* restore v4l2 control values */
	rval = v4l2_ctrl_handler_setup(&flash->ctrls_led);
out:
	dev_dbg(flash->dev, "%s rval = %d\n", __func__, rval);
	return rval;
}

#else

#define lm3643_suspend	NULL
#define lm3643_resume	NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id lm3643_id_table[] = {
	{LM3643_NAME, 0},
	{}
};

static const struct dev_pm_ops lm3643_pm_ops = {
	.suspend	= lm3643_suspend,
	.resume		= lm3643_resume,
	.runtime_suspend = lm3643_suspend,
	.runtime_resume = lm3643_resume,
};

MODULE_DEVICE_TABLE(i2c, lm3643_id_table);

static struct i2c_driver lm3643_i2c_driver = {
	.driver = {
		   .name = LM3643_NAME,
		   .pm = &lm3643_pm_ops,
		   },
	.probe = lm3643_probe,
	.remove = lm3643_remove,
	.id_table = lm3643_id_table,
};

module_i2c_driver(lm3643_i2c_driver);

MODULE_AUTHOR("Jouni Ukkonen <jouni.ukkonen@intel.com>");
MODULE_DESCRIPTION("TI LM3643 Dual Flash LED driver");
MODULE_LICENSE("GPL");
