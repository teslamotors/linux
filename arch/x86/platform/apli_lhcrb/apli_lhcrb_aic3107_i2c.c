/*
 * I2C Platform initialize for Apollo Lake Machine Driver
 * Copyright (c) 2016, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>


#define AIC3107_I2C_ADDRESS	0x18
#define AUDIO_I2C_CHANNEL	3

static struct i2c_client *client;

static const struct i2c_board_info i2c_bus_info[] __initconst = {
	{ I2C_BOARD_INFO("tlv320aic3007", AIC3107_I2C_ADDRESS) },
};

static int __init apli_lhcrb_aic3107_i2c_init(void)
{
	struct i2c_adapter *adapter = NULL;

	adapter = i2c_get_adapter(AUDIO_I2C_CHANNEL);
	if (!adapter) {
		pr_warn("i2c adapter not found\n");
		goto exit;
	}

	client = i2c_new_device(adapter, &i2c_bus_info[0]);
	if (client == NULL) {
		pr_warn("i2c new device failed\n");
		goto exit;
	}

	i2c_put_adapter(adapter);

	return 0;

exit:
	i2c_put_adapter(adapter);
	if (client)
		i2c_unregister_device(client);
	return -EIO;

}

static void __exit apli_lhcrb_aic3107_i2c_exit(void)
{
	if (client)
		i2c_unregister_device(client);
}

device_initcall(apli_lhcrb_aic3107_i2c_init);
module_exit(apli_lhcrb_aic3107_i2c_exit);

MODULE_DESCRIPTION("Intel Apollo Lake-I Leaf Hill ASoC Machine Driver Init");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:lhcrb_aic3107M_i2s");
MODULE_ALIAS("platform:lhcrb_aic3107S_i2s");
