/*
 * cyttsp6_i2c.c
 * Cypress TrueTouch(TM) Standard Product V4 I2C Module.
 * For use with Cypress touchscreen controllers.
 * Supported parts include:
 * CY8CTMA46X
 * CY8CTMA1036/768
 * CYTMA445
 *
 * Copyright (C) 2012-2015 Cypress Semiconductor
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
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include "cyttsp6_regs.h"
#include <linux/cyttsp6_platform.h>
#include <linux/i2c.h>
#include <linux/version.h>

#define CY_I2C_DATA_SIZE  (2 * 256)

#define M3_TOUCH_BUS 2

#define CYTTSP6_I2C_TCH_ADR 0x24
#define CYTTSP6_LDR_TCH_ADR 0x24
#define CYTTSP6_I2C_IRQ_GPIO 452
#define CYTTSP6_I2C_RST_GPIO 454
#define CYTTSP6_I2C_ERR_GPIO 454

#define CY_VKEYS_X 1366
#define CY_VKEYS_Y 768
#define CY_MAXX 1526
#define CY_MAXY 768
#define CY_MINX 0
#define CY_MINY 0

#define CY_ABS_MIN_X CY_MINX
#define CY_ABS_MIN_Y CY_MINY
#define CY_ABS_MAX_X CY_MAXX
#define CY_ABS_MAX_Y CY_MAXY
#define CY_ABS_MIN_P 0
#define CY_ABS_MAX_P 255
#define CY_ABS_MIN_W 0
#define CY_ABS_MAX_W 255
#define CY_PROXIMITY_MIN_VAL    0
#define CY_PROXIMITY_MAX_VAL    1
#define CY_ABS_MIN_T 0
#define CY_ABS_MAX_T 15

#define CYTTSP6_I2C_MAX_XFER_LEN 0x100


static int cyttsp6_i2c_read_block_data(struct device *dev, u16 addr,
	int length, void *values, int max_xfer)
{
	struct i2c_client *client = to_i2c_client(dev);
	int trans_len;
	u16 slave_addr = client->addr;
	u8 client_addr;
	u8 addr_lo;
	struct i2c_msg msgs[2];
	int rc = -EINVAL;
	int msg_cnt = 0;

	while (length > 0) {
		client_addr = slave_addr | ((addr >> 8) & 0x1);
		addr_lo = addr & 0xFF;
		trans_len = min(length, max_xfer);

		msg_cnt = 0;
		memset(msgs, 0, sizeof(msgs));
		msgs[msg_cnt].addr = client_addr;
		msgs[msg_cnt].flags = 0;
		msgs[msg_cnt].len = 1;
		msgs[msg_cnt].buf = &addr_lo;
		msg_cnt++;

		rc = i2c_transfer(client->adapter, msgs, msg_cnt);
		if (rc != msg_cnt)
			goto exit;

		msg_cnt = 0;
		msgs[msg_cnt].addr = client_addr;
		msgs[msg_cnt].flags = I2C_M_RD;
		msgs[msg_cnt].len = trans_len;
		msgs[msg_cnt].buf = values;
		msg_cnt++;

		rc = i2c_transfer(client->adapter, msgs, msg_cnt);
		if (rc != msg_cnt)
			goto exit;

		length -= trans_len;
		values += trans_len;
		addr += trans_len;
	}

exit:
	return (rc < 0) ? rc : rc != msg_cnt ? -EIO : 0;
}

static int cyttsp6_i2c_write_block_data(struct device *dev, u16 addr,
	u8 *wr_buf, int length, const void *values, int max_xfer)
{
	struct i2c_client *client = to_i2c_client(dev);
	u16 slave_addr = client->addr;
	u8 client_addr;
	u8 addr_lo;
	int trans_len;
	struct i2c_msg msg;
	int rc = -EINVAL;

	while (length > 0) {
		client_addr = slave_addr | ((addr >> 8) & 0x1);
		addr_lo = addr & 0xFF;
		trans_len = min(length, max_xfer);

		memset(&msg, 0, sizeof(msg));
		msg.addr = client_addr;
		msg.flags = 0;
		msg.len = trans_len + 1;
		msg.buf = wr_buf;

		wr_buf[0] = addr_lo;
		memcpy(&wr_buf[1], values, trans_len);

		/* write data */
		rc = i2c_transfer(client->adapter, &msg, 1);
		if (rc != 1)
			goto exit;

		length -= trans_len;
		values += trans_len;
		addr += trans_len;
	}

exit:
	return (rc < 0) ? rc : rc != 1 ? -EIO : 0;
}

static int cyttsp6_i2c_write(struct device *dev, u16 addr, u8 *wr_buf,
	const void *buf, int size, int max_xfer)
{
	int rc;

	pm_runtime_get_noresume(dev);
	rc = cyttsp6_i2c_write_block_data(dev, addr, wr_buf, size, buf,
		max_xfer);
	pm_runtime_put_noidle(dev);

	return rc;
}

static int cyttsp6_i2c_read(struct device *dev, u16 addr, void *buf, int size,
	int max_xfer)
{
	int rc;

	pm_runtime_get_noresume(dev);
	rc = cyttsp6_i2c_read_block_data(dev, addr, size, buf, max_xfer);
	pm_runtime_put_noidle(dev);

	return rc;
}

static struct cyttsp6_bus_ops cyttsp6_i2c_bus_ops = {
	.write = cyttsp6_i2c_write,
	.read = cyttsp6_i2c_read,
};

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_DEVICETREE_SUPPORT
static struct of_device_id cyttsp6_i2c_of_match[] = {
	{ .compatible = "cy,cyttsp6_i2c_adapter", }, { }
};
MODULE_DEVICE_TABLE(of, cyttsp6_i2c_of_match);
#endif

static int cyttsp6_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *i2c_id)
{
	struct device *dev = &client->dev;
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_DEVICETREE_SUPPORT
	const struct of_device_id *match;
#endif
	int rc;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(dev, "I2C functionality not Supported\n");
		return -EIO;
	}

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_DEVICETREE_SUPPORT
	match = of_match_device(of_match_ptr(cyttsp6_i2c_of_match), dev);
	if (match) {
		rc = cyttsp6_devtree_create_and_get_pdata(dev);
		if (rc < 0)
			return rc;
	}
#endif

	rc = cyttsp6_probe(&cyttsp6_i2c_bus_ops, &client->dev, client->irq,
			CY_I2C_DATA_SIZE);

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_DEVICETREE_SUPPORT
	if (rc && match)
		cyttsp6_devtree_clean_pdata(dev);
#endif

	return rc;
}

static int cyttsp6_i2c_remove(struct i2c_client *client)
{
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_DEVICETREE_SUPPORT
	struct device *dev = &client->dev;
	const struct of_device_id *match;
#endif
	struct cyttsp6_core_data *cd = i2c_get_clientdata(client);

	cyttsp6_release(cd);

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_DEVICETREE_SUPPORT
	match = of_match_device(of_match_ptr(cyttsp6_i2c_of_match), dev);
	if (match)
		cyttsp6_devtree_clean_pdata(dev);
#endif

	return 0;
}

static const struct i2c_device_id cyttsp6_i2c_id[] = {
	{ CYTTSP6_I2C_NAME, 0 },  { }
};
MODULE_DEVICE_TABLE(i2c, cyttsp6_i2c_id);

static struct i2c_driver cyttsp6_i2c_driver = {
	.driver = {
		.name = CYTTSP6_I2C_NAME,
		.owner = THIS_MODULE,
		.pm = &cyttsp6_pm_ops,
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_DEVICETREE_SUPPORT
		.of_match_table = cyttsp6_i2c_of_match,
#endif
	},
	.probe = cyttsp6_i2c_probe,
	.remove = cyttsp6_i2c_remove,
	.id_table = cyttsp6_i2c_id,
};

/* Cypress driver's platform specific data  */

static struct cyttsp6_core_platform_data _cyttsp6_core_platform_data = {
        .irq_gpio = CYTTSP6_I2C_IRQ_GPIO,
        .rst_gpio = CYTTSP6_I2C_RST_GPIO,
        .err_gpio = CYTTSP6_I2C_RST_GPIO,
		.max_xfer_len = CYTTSP6_I2C_MAX_XFER_LEN,
        .xres = cyttsp6_xres,
        .init = cyttsp6_init,
        .power = cyttsp6_power,
        .detect = cyttsp6_detect,
        .irq_stat = cyttsp6_irq_stat,
        .error_stat = cyttsp6_error_stat,
        .sett = {
                NULL,   /* Reserved */
                NULL,   /* Command Registers */
                NULL,   /* Touch Report */
                NULL,   /* Cypress Data Record */
                NULL,   /* Test Record */
                NULL,   /* Panel Configuration Record */
                NULL,   /* &cyttsp6_sett_param_regs, */
                NULL,   /* &cyttsp6_sett_param_size, */
                NULL,   /* Reserved */
                NULL,   /* Reserved */
                NULL,   /* Operational Configuration Record */
                NULL, 	/* &cyttsp6_sett_ddata, *//* Design Data Record */
                NULL, 	/* &cyttsp6_sett_mdata, *//* Manufacturing Data Record */
                NULL,   /* Config and Test Registers */
                NULL, 	/*&cyttsp6_sett_btn_keys,*/ /* button-to-keycode table */
        },
        /* XXX: Gestures disabled by default by Cypress */
        /*	.flags = CY_CORE_FLAG_WAKE_ON_GESTURE,
 	    	.easy_wakeup_gesture = CY_CORE_EWG_TAP_TAP
                | CY_CORE_EWG_TWO_FINGER_SLIDE,*/
};

static const int16_t cyttsp6_abs[] = {
        ABS_MT_POSITION_X, CY_ABS_MIN_X, CY_ABS_MAX_X, 0, 0,
        ABS_MT_POSITION_Y, CY_ABS_MIN_Y, CY_ABS_MAX_Y, 0, 0,
        ABS_MT_PRESSURE, CY_ABS_MIN_P, CY_ABS_MAX_P, 0, 0,
        CY_IGNORE_VALUE, CY_ABS_MIN_W, CY_ABS_MAX_W, 0, 0,
        ABS_MT_TRACKING_ID, CY_ABS_MIN_T, CY_ABS_MAX_T, 0, 0,
        ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0,
        ABS_MT_TOUCH_MINOR, 0, 255, 0, 0,
        ABS_MT_ORIENTATION, -127, 127, 0, 0,
        ABS_MT_TOOL_TYPE, 0, MT_TOOL_MAX, 0, 0,
        ABS_DISTANCE, 0, 255, 0, 0,     /* Used with hover */
};

struct touch_framework cyttsp6_framework = {
        .abs = cyttsp6_abs,
        .size = ARRAY_SIZE(cyttsp6_abs),
        .enable_vkeys = 0,
};

static struct cyttsp6_mt_platform_data _cyttsp6_mt_platform_data = {
        .frmwrk = &cyttsp6_framework,
        /* XXX: Invert flag disabled by default by Cypress */
        /*.flags = CY_MT_FLAG_INV_Y,*/
        .inp_dev_name = CYTTSP6_MT_NAME,
        .vkeys_x = CY_VKEYS_X,
        .vkeys_y = CY_VKEYS_Y,
};

static struct cyttsp6_btn_platform_data _cyttsp6_btn_platform_data = {
        .inp_dev_name = CYTTSP6_BTN_NAME,
};

static const int16_t cyttsp6_prox_abs[] = {
        ABS_DISTANCE, CY_PROXIMITY_MIN_VAL, CY_PROXIMITY_MAX_VAL, 0, 0,
};

struct touch_framework cyttsp6_prox_framework = {
        .abs = cyttsp6_prox_abs,
        .size = ARRAY_SIZE(cyttsp6_prox_abs),
};

static struct cyttsp6_proximity_platform_data
                _cyttsp6_proximity_platform_data = {
        .frmwrk = &cyttsp6_prox_framework,
        .inp_dev_name = CYTTSP6_PROXIMITY_NAME,
};

static struct cyttsp6_platform_data _cyttsp6_platform_data = {
        .core_pdata = &_cyttsp6_core_platform_data,
        .mt_pdata = &_cyttsp6_mt_platform_data,
        .btn_pdata = &_cyttsp6_btn_platform_data,
        .prox_pdata = &_cyttsp6_proximity_platform_data,
        .loader_pdata = &_cyttsp6_loader_platform_data,
};

struct i2c_client * cyttsp6_client;
static struct i2c_board_info apl_i2c_devices[] = {
        {
                I2C_BOARD_INFO(CYTTSP6_I2C_NAME, CYTTSP6_I2C_TCH_ADR),
                .irq = CYTTSP6_I2C_IRQ_GPIO,
                .platform_data = &_cyttsp6_platform_data,
        },
};

static int __init cyttsp6_i2c_init(void)
{
	pr_info("%s: Registering i2c device\n", __func__);

	/* Instantiate the i2c touch device explicitly when the module is loaded. 
	 * This method is used because we don't have a fully featured board-config 
	 * file (apl-board.c) from Intel. 
	 * 
	 * Explicit instantiation is documented here: 
	 * https://www.kernel.org/doc/Documentation/i2c/instantiating-devices
	*/

	struct i2c_adapter *adapter;
	adapter = i2c_get_adapter(M3_TOUCH_BUS);
	cyttsp6_client = i2c_new_device(adapter, apl_i2c_devices);
	i2c_put_adapter(adapter);

	int rc = i2c_add_driver(&cyttsp6_i2c_driver);

	pr_info("%s: Cypress TTSP I2C Driver (Built %s) rc=%d\n",
		 __func__, CY_DRIVER_VERSION, rc);

	return rc;
}
module_init(cyttsp6_i2c_init);

static void __exit cyttsp6_i2c_exit(void)
{
	i2c_unregister_device(cyttsp6_client);
	i2c_del_driver(&cyttsp6_i2c_driver);
}
module_exit(cyttsp6_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard Product I2C driver");
MODULE_AUTHOR("Cypress Semiconductor <ttdrivers@cypress.com>");
