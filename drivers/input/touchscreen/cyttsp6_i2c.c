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
#include <linux/i2c.h>
#include <linux/version.h>

#include <linux/acpi.h>
#include <linux/cyttsp6_acpi.h>

#define CY_I2C_DATA_SIZE  (2 * 256)

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

static const struct acpi_device_id cyttsp6_acpi_match[] = {
	{ "CYTTSP6", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, cyttsp6_acpi_match);

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

	/* ACPI_HANDLE(dev) will be NULL when CONFIG_ACPI is not enabled */
	if (ACPI_HANDLE(dev)) {
		rc = cyttsp6_acpi_get_pdata(dev);
		if (rc < 0)
			return rc;
	}

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
		.acpi_match_table = ACPI_PTR(cyttsp6_acpi_match),
	},
	.probe = cyttsp6_i2c_probe,
	.remove = cyttsp6_i2c_remove,
	.id_table = cyttsp6_i2c_id,
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0))
module_i2c_driver(cyttsp6_i2c_driver);
#else
static int __init cyttsp6_i2c_init(void)
{
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
#endif

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard Product I2C driver");
MODULE_AUTHOR("Cypress Semiconductor <ttdrivers@cypress.com>");
