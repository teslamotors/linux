// SPDX-License-Identifier: GPL-2.0-only
/*
 * MAXIM 20444 Backlight LED Driver
 *
 * Copyright (C) 2020 Tesla Motors, Inc.
 *
 */
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/slab.h>

/* Register address */
#define LP8863_REG_BRT_MODE 0x20	// Brightness Mode
#define LP8863_REG_DISP_BRT 0x28	// Display Brightness
#define LP8863_REG_GROUPING1 0x30	// Brightness Grouping 1
#define LP8863_REG_GROUPING2 0x32	// Brightness Grouping 2
#define LP8863_REG_USER_CONFIG1 0x40	// User Config 1
#define LP8863_REG_USER_CONFIG2 0x42	// User Config 2
#define LP8863_REG_INTERRUPT_ENABLE_3 0x4E	// Fault Interrupt Enable 3
#define LP8863_REG_INTERRUPT_ENABLE_1 0x50	// Fault Interrupt Enable 1
#define LP8863_REG_INTERRUPT_ENABLE_2 0x52	// Fault Interrupt Enable 2
#define LP8863_REG_INTERRUPT_STATUS_1 0x54	// Fault Interrupt Status 1
#define LP8863_REG_INTERRUPT_STATUS_2 0x56	// Fault Interrupt Status 2
#define LP8863_REG_INTERRUPT_STATUS_3 0x58	// Fault Interrupt Status 3
#define LP8863_REG_JUNCTION_TEMPERATURE 0xE8	// Die Junction Temperature
#define LP8863_REG_TEMPERATURE_LIMIT_HIGH 0xEC	// Temperature High Limit
#define LP8863_REG_TEMPERATURE_LIMIT_LOW 0xEE	// Temperature Low Limit
#define LP8863_REG_CLUSTER1_BRT 0x13C	// Cluster 1 Brightness
#define LP8863_REG_CLUSTER2_BRT 0x148	// Cluster 2 Brightness
#define LP8863_REG_CLUSTER3_BRT 0x154	// Cluster 3 Brightness
#define LP8863_REG_CLUSTER4_BRT 0x160	// Cluster 4 Brightness
#define LP8863_REG_CLUSTER5_BRT 0x16C	// Cluster 5 Brightness
#define LP8863_REG_BRT_DB_CONTROL 0x178	// Cluster Brightness and Current Load
#define LP8863_REG_LED0_CURRENT 0x1C2	// LED0 Current
#define LP8863_REG_LED1_CURRENT 0x1C4	// LED1 Current
#define LP8863_REG_LED2_CURRENT 0x1C6	// LED2 Current
#define LP8863_REG_LED3_CURRENT 0x1C8	// LED3 Current
#define LP8863_REG_LED4_CURRENT 0x1CA	// LED4 Current
#define LP8863_REG_LED5_CURRENT 0x1CC	// LED5 Current
#define LP8863_REG_BOOST_CONTROL 0x288	// Boost Control
#define LP8863_REG_SHORT_THRESH 0x28A	// Shorted LED Threshold
#define LP8863_REG_FSM_DIAGNOSTICS 0x2A4	// Device State Diagnostics
#define LP8863_REG_PWM_INPUT_DIAGNOSTICS 0x2A6	// PWM Input Diagnostics
#define LP8863_REG_PWM_OUTPUT_DIAGNOSTICS 0x2A8	// PWM Output Diagnostics
#define LP8863_REG_LED_CURR_DIAGNOSTICS 0x2AA	// LED Current Diagnostics
#define LP8863_REG_ADAPT_BOOST_DIAGNOSTICS 0x2AC // Adaptive Boost Diagnostics
#define LP8863_REG_AUTO_DETECT_DIAGNOSTICS 0x2AE // Auto Detect Diagnstics

#define LP8863_DEFAULT_BRT 0xFF
#define LP8863_MAX_BRIGHTNESS 65535

#define DISPLAY_BRT_CTRL_PWM 0x00
#define DISPLAY_BRT_CTRL_DISPLAY_BRT 0x02

// INTERRUPT_STATUS_1 fields
#define INTERRUPT_STATUS_1_BSTOVPH_STATUS  BIT(15)
#define INTERRUPT_STATUS_1_TEMPHIGH_STATUS BIT(13)
#define INTERRUPT_STATUS_1_BSTOVPL_STATUS  BIT(7)
#define INTERRUPT_STATUS_1_BSTOCP_STATUS   BIT(5)
#define INTERRUPT_STATUS_1_TSD_STATUS      BIT(1)

// INTERRUPT_STATUS_2 fields
#define INTERRUPT_STATUS_2_OPEN_LED   BIT(9)
#define INTERRUPT_STATUS_2_SHORT_LED  BIT(8)
#define INTERRUPT_STATUS_2_LED_STATUS BIT(7)
#define INTERRUPT_STATUS_2_LED_CLEAR  BIT(6)

struct lp8863 {
	struct i2c_client *client;
	struct i2c_client *analog_client;
	struct i2c_client *diagnostics_client;
	struct i2c_client *unused_client;
	struct backlight_device *backlight;
	int power;
};

/*
 * The LP8863-Q1 uses a 10-bit register address space. The 10-bit
 * register address space is accessed as four separate 8-bit address
 * spaces. Four different slave addresses are used to access each
 * of the four 8-bit address register spaces.
 */
static inline struct i2c_client* reg2client(struct lp8863* lp, u16 reg)
{
	switch((reg & 0xF00) >> 8){
	case 0x1:
		return lp->analog_client;
	case 0x2:
		return lp->diagnostics_client;
	case 0x3:
		return lp->unused_client; // Should never really be called.
	case 0x0:
		/* FALL-THRU */
	default:
		return lp->client;
	}
}

static int lp8863_write(struct lp8863 *lp, u16 reg, u16 data)
{
	/* Write I2C transactions are made up of 4 bytes. The first byte
	 * includes the 7-bit slave address and Write bit. The 7-bit slave
	 * address selects the LP8863-Q1 slave device and one of four 8-bit
	 * register address sections. The second byte includes eight LSB bits
	 * of the 10-bit register address. The last two bytes are the 16-bit*
	 * register value.
	 */
	struct i2c_client *i2c = reg2client(lp, reg);
	u8 buf[2];
	s32 ret;

	dev_dbg(&i2c->dev, "i2c_write [%02X]=>%02X\n", reg, data);

	buf[0] = data & 0xFF;
	buf[1] = ((data & 0xFF00) >> 8);

	ret = i2c_smbus_write_i2c_block_data(i2c, reg & 0xFF, sizeof(buf), buf);
	if (ret != 0) {
		dev_err(&i2c->dev, "i2c_write failed 0x%02X to 0x%02X, rc=%d\n",
			data, reg, ret);
	}
	return ret;
}

/*
 * Read I2C transactions are made up of five bytes. The first byte
 * includes the 7-bit slave address and Write bit. The 7-bit slave
 * address selects the LP8863-Q1 slave device and one of four 8-bit
 * register address sections. The second byte includes eight LSB bits
 * of the 10-bit register address. The third byte includes the 7-bit
 * slave address and Read bit. The last two bytes are the 16-bit
 * register value returned from the slave.
 */
static int lp8863_read(struct lp8863 *lp, u16 reg, u16 *data)
{
	struct i2c_client *i2c = reg2client(lp, reg);
	u8 buf[2];
	s32 ret;

	memset(buf, 0, sizeof(buf));
	ret = i2c_smbus_read_i2c_block_data(i2c, reg & 0xFF, sizeof(buf), buf);
	if (ret != 2) {
		dev_err(&i2c->dev, "i2c_read failed 0x%X, rc=%d\n", reg, ret);
	} else {
		*data = (buf[1] << 8) | (buf[0]);
		dev_dbg(&i2c->dev, "i2c_read [%02X]=>%02X\n", reg, *data);
	}
	return ret;
}

static inline u16 lp8863_read_isr1(struct lp8863* lp)
{
	u16 int_status = 0x0;
	int ret;

	ret = lp8863_read(lp, LP8863_REG_INTERRUPT_STATUS_1, &int_status);
	return ret < 0 ? 0 : int_status;
}

static inline u16 lp8863_read_isr2(struct lp8863* lp)
{
	u16 int_status = 0x0;
	int ret;

	ret = lp8863_read(lp, LP8863_REG_INTERRUPT_STATUS_2, &int_status);
	return ret < 0 ? 0 : int_status;
}


static int lp8863_backlight_configure(struct lp8863 *lp)
{
	s32 ret;

	/* Brightness control mode */
	ret = lp8863_write(lp, LP8863_REG_BRT_MODE,
			   DISPLAY_BRT_CTRL_DISPLAY_BRT);
	if (ret < 0) {
		dev_err(&lp->client->dev,
			"Failed to configure brightness control mode\n");
		return ret;
	}

	ret = lp8863_write(lp, LP8863_REG_DISP_BRT, LP8863_DEFAULT_BRT);
	if (ret < 0) {
		dev_err(&lp->client->dev,
			"Failed to set default brightness reg\n");
	}
	return ret;
}

static int lp8863_backlight_update_status(struct backlight_device *bl)
{
	struct lp8863 *lp = bl_get_data(bl);
	int brightness = bl->props.brightness;
	int ret = 0;

	/* Re-configure backlight from power off -> on (>0 -> 0) */
	if (bl->props.power == FB_BLANK_UNBLANK &&
	    lp->power != FB_BLANK_UNBLANK) {
		dev_info(&lp->client->dev, "Powering on");
		ret = lp8863_backlight_configure(lp);
		if (ret < 0) {
			dev_err(&lp->client->dev,
				"Failed to enable backlight rc=%d\n", ret);
			return ret;
		}
	} else if (bl->props.power != FB_BLANK_UNBLANK) {
		/* turn off backlight if powering off */
		brightness = 0;
	}

	if (bl->props.state & BL_CORE_SUSPENDED) {
		dev_info(&lp->client->dev,
			 "Device suspended, not updating backlight\n");
		return 0;
	}

	/* update internal power state */
	lp->power = bl->props.power;

	clamp_t(int, brightness, 0, bl->props.max_brightness);

	dev_dbg(&lp->client->dev, "Update Brightness=%d\n", brightness);
	ret = lp8863_write(lp, LP8863_REG_DISP_BRT, brightness);
	if (ret < 0) {
		dev_err(&lp->client->dev,
			"Failed to update brightness reg, rc=%d\n", ret);
	}

	return 0;
}

static int lp8863_backlight_get_brightness(struct backlight_device *bl)
{
	struct lp8863 *lp = bl_get_data(bl);
	u16 current_brightness;
	int ret;

	ret = lp8863_read(lp, LP8863_REG_DISP_BRT, &current_brightness);
	if (ret < 0) {
		dev_err(&lp->client->dev,
			"Failed to get brightness, rc=%d\n", ret);
		return -EINVAL;
	}

	dev_dbg(&lp->client->dev, "get brightness=%u\n",
		current_brightness);

	return current_brightness;
}

static ssize_t lp8863_fsm_diagnostics_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	struct lp8863 *lp = dev_get_drvdata(dev);
	u16 fsm = 0x0;
	int ret;

	ret = lp8863_read(lp, LP8863_REG_FSM_DIAGNOSTICS, &fsm);
	if (ret < 0)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "0x%0X\n", fsm);
}

static ssize_t lp8863_auto_detect_diagnostics_show(struct device *dev,
						   struct device_attribute *attr,
						   char *buf)
{
	struct lp8863 *lp = dev_get_drvdata(dev);
	u16 auto_detect = 0x0;
	int ret;

	ret = lp8863_read(lp, LP8863_REG_AUTO_DETECT_DIAGNOSTICS, &auto_detect);
	if (ret < 0)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "0x%0X\n", auto_detect);
}

static ssize_t lp8863_open_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct lp8863 *lp = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 lp8863_read_isr2(lp) & INTERRUPT_STATUS_2_OPEN_LED ? "open" : "none");
}

static ssize_t lp8863_short_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct lp8863 *lp = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 lp8863_read_isr2(lp) & INTERRUPT_STATUS_2_SHORT_LED ? "short" : "none");
}

static ssize_t lp8863_fault_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct lp8863 *lp = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 lp8863_read_isr2(lp) & INTERRUPT_STATUS_2_LED_STATUS ?
			 "fault" : "none");
}

static ssize_t lp8863_ovp_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct lp8863 *lp = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 lp8863_read_isr1(lp) &
			 (INTERRUPT_STATUS_1_BSTOVPH_STATUS |
			  INTERRUPT_STATUS_1_BSTOVPL_STATUS) ?
			 "over-voltage" : "none");
}

static ssize_t lp8863_ocp_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct lp8863 *lp = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 lp8863_read_isr1(lp) & INTERRUPT_STATUS_1_BSTOCP_STATUS ?
			 "over-current" : "none");
}

static ssize_t lp8863_thermal_shutdown_show(struct device *dev,
					    struct device_attribute *attr, char *buf)
{
	struct lp8863 *lp = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 lp8863_read_isr1(lp) &
			 (INTERRUPT_STATUS_1_TEMPHIGH_STATUS | INTERRUPT_STATUS_1_TSD_STATUS) ?
			 "thermal-shutdown" : "none");
}

static DEVICE_ATTR(fsm_diagnostics, 0444, lp8863_fsm_diagnostics_show ,NULL);
static DEVICE_ATTR(auto_detect_diagnostics, 0444, lp8863_auto_detect_diagnostics_show ,NULL);
static DEVICE_ATTR(open, 0444, lp8863_open_show, NULL);
static DEVICE_ATTR(short, 0444, lp8863_short_show, NULL);
static DEVICE_ATTR(fault, 0444, lp8863_fault_show, NULL);
static DEVICE_ATTR(ovp, 0444, lp8863_ovp_show, NULL);
static DEVICE_ATTR(ocp, 0444, lp8863_ocp_show, NULL);
static DEVICE_ATTR(thermal_shutdown, 0444, lp8863_thermal_shutdown_show, NULL);

static struct attribute *lp8863_attrs[] = {
	&dev_attr_fsm_diagnostics.attr,
	&dev_attr_auto_detect_diagnostics.attr,
	&dev_attr_open.attr,
	&dev_attr_short.attr,
	&dev_attr_fault.attr,
	&dev_attr_ovp.attr,
	&dev_attr_ocp.attr,
	&dev_attr_thermal_shutdown.attr,
	NULL,
};

static const struct attribute_group lp8863_attr_group = {
	.name = "diagnostics",
	.attrs = lp8863_attrs,
};

static const struct backlight_ops lp8863_backlight_ops = {
	.options = 0, /* no suspend resume support, done in userspace */
	.update_status = lp8863_backlight_update_status,
	.get_brightness = lp8863_backlight_get_brightness,
};

static int lp8863_backlight_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct lp8863 *lp;
	struct backlight_properties props;
	struct backlight_device *backlight;
	int ret;

	lp = devm_kzalloc(&client->dev, sizeof(struct lp8863), GFP_KERNEL);
	if (!lp)
		return -ENOMEM;

	lp->client = client;

	// Create auxillary diagnostics device clients that the device will
	// respond to.
	// Client for 0x1HH registers
	lp->analog_client = devm_i2c_new_dummy_device(&client->dev,
						      client->adapter,
						      client->addr + 1);
	if (IS_ERR(lp->analog_client)) {
		ret = PTR_ERR(lp->analog_client);
		dev_err(&client->dev,
			"failed to register secondary i2c device, err: %d\n",
			ret);
		goto err_exit;
	}

	lp->diagnostics_client = devm_i2c_new_dummy_device(&client->dev,
							   client->adapter,
							   client->addr + 2);
	// Client of 0x2HH registers
	if (IS_ERR(lp->diagnostics_client)) {
		ret = PTR_ERR(lp->diagnostics_client);
		dev_err(&client->dev,
			"failed to register tertiary i2c device, err: %d\n",
			ret);
		goto err_exit;
	}
	// Client for 0x3HH registers, however none are documented. Protect
	// them anyway.
	lp->unused_client = devm_i2c_new_dummy_device(&client->dev,
						      client->adapter,
						      client->addr + 3);
	if (IS_ERR(lp->unused_client)) {
		ret = PTR_ERR(lp->unused_client);
		dev_err(&client->dev,
			"failed register quaternary i2c device, err: %d\n",
			ret);
		goto err_exit;
	}

	dev_info(&client->dev, "Configuring lp8863 backlight\n");

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;	/* Controlled by hardware registers */
	props.max_brightness = LP8863_MAX_BRIGHTNESS;

	ret = lp8863_backlight_configure(lp);
	if (ret) {
		dev_err(&client->dev, "backlight config err: %d\n", ret);
		goto err_exit;
	}

	/* Start un-powered, will be modified in update_status */
	lp->power = FB_BLANK_POWERDOWN;
	backlight = devm_backlight_device_register(&client->dev,
						   dev_name(&client->dev),
						   &lp->client->dev, lp,
						   &lp8863_backlight_ops,
						   &props);
	if (IS_ERR(backlight)) {
		dev_err(&client->dev, "failed to register backlight err: %d\n",
			ret);
		return PTR_ERR(backlight);
	}

	pr_err("Putting group on bd->dev.kobj\n");
	ret = sysfs_create_group(&backlight->dev.kobj, &lp8863_attr_group);
	if (ret) {
		dev_err(&client->dev, "failed to register sysfs err: %d\n",
			ret);
		goto err_exit;
	}

	ret = backlight_update_status(backlight);
	i2c_set_clientdata(client, backlight);
err_exit:
	return ret;
}

static int lp8863_backlight_remove(struct i2c_client *client)
{
	struct backlight_device *backlight = i2c_get_clientdata(client);
	int ret;

	backlight->props.brightness = 0;
	ret = backlight_update_status(backlight);

	sysfs_remove_group(&backlight->dev.kobj, &lp8863_attr_group);
	return ret;
}

static const struct i2c_device_id lp8863_ids[] = {
	{"lp8863", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, lp8863_ids);

static struct i2c_driver lp8863_driver = {
	.driver = {
		   .name = "lp8863",
		   },
	.probe = lp8863_backlight_probe,
	.remove = lp8863_backlight_remove,
	.id_table = lp8863_ids,
};

module_i2c_driver(lp8863_driver);

MODULE_DESCRIPTION("Texas Instruments LP8863 Backlight Driver");
MODULE_AUTHOR("Tyler Rider");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:lp8863-backlight");
