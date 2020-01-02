/*
 * cyttsp6_test_device_access_api.c
 * Cypress TrueTouch(TM) Standard Product V4 Device Access API test module.
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

#include <linux/module.h>
#include <linux/cyttsp6_device_access-api.h>

#define CY_DEFAULT_CORE_ID	"cyttsp6_core0"
#define CONFIG_VER_OFFSET	8
#define CONFIG_VER_SIZE		2

#define BUFFER_SIZE		256

static u8 buffer[BUFFER_SIZE];
static int active_refresh_interval;

static void cyttsp6_write_async_cont(const char *core_name, int ic_grpnum,
		int ic_grpoffset, u8 *buf, int length, int rc)
{
	pr_info("write command async completed\n");
	if (!core_name)
		core_name = CY_DEFAULT_CORE_ID;
	pr_info("core_name:%s ic_grpnum:%d ic_grpoffset:%d rc:%d\n",
		core_name, ic_grpnum, ic_grpoffset, rc);
}

static int __init cyttsp6_test_device_access_api_init(void)
{
	int i;
	int j;
	int value;
	int rc;
	u16 config_ver;

	/*
	 * CASE 1 - Get CONFIG_VER and update it
	 */

	/*
	 * Get CONFIG_VER
	 * Group 6 read requires to fetch from offset to the end of row
	 * The return buffer should be at least read config block command
	 * return size + config row size bytes
	 */
	rc = cyttsp6_device_access_read_command(NULL,
			GRPNUM_TOUCH_CONFIG, CONFIG_VER_OFFSET,
			buffer, BUFFER_SIZE);
	if (rc < 0) {
		pr_err("%s: cyttsp6_device_access_read_command failed, rc=%d\n",
			__func__, rc);
		return rc;
	}

	pr_info("%s: cyttsp6_device_access_read_command returned %d bytes\n",
		__func__, rc);

	/* Calculate CONFIG_VER (Little Endian) */
	config_ver = buffer[0] + (buffer[1] << 8);

	pr_info("%s: Old CONFIG_VER:%04X New CONFIG_VER:%04X\n", __func__,
		config_ver, config_ver + 1);

	config_ver++;

	/* Store CONFIG_VER (Little Endian) */
	buffer[0] = config_ver & 0xFF;
	buffer[1] = config_ver >> 8;

	/*
	 * Set CONFIG_VER
	 * Group 6 write supports writing arbitrary number of bytes
	 */
	rc = cyttsp6_device_access_write_command(NULL,
			GRPNUM_TOUCH_CONFIG, CONFIG_VER_OFFSET,
			buffer, CONFIG_VER_SIZE);
	if (rc < 0) {
		pr_err("%s: cyttsp6_device_access_write_command failed, rc=%d\n",
			__func__, rc);
		return rc;
	}

	/*
	 * CASE 2 - Get Operational mode parameters
	 */
	for (i = OP_PARAM_MIN; i <= OP_PARAM_MAX; i++) {
		buffer[0] = OP_CMD_GET_PARAMETER;
		buffer[1] = i;

		rc = cyttsp6_device_access_write_command(NULL,
				GRPNUM_OP_COMMAND, 0, buffer, 2);
		if (rc < 0) {
			pr_err("%s: cyttsp6_device_access_write_command failed, rc=%d\n",
				__func__, rc);
			return rc;
		}

		/*
		 * The return buffer should be at least
		 * number of command data registers + 1
		 */
		rc = cyttsp6_device_access_read_command(NULL,
				GRPNUM_OP_COMMAND, 0, buffer, 7);
		if (rc < 0) {
			pr_err("%s: cyttsp6_device_access_read_command failed, rc=%d\n",
				__func__, rc);
			return rc;
		}

		if (buffer[0] != OP_CMD_GET_PARAMETER || buffer[1] != i) {
			pr_err("%s: Invalid response\n", __func__);
			return -EINVAL;
		}

		/*
		 * Get value stored starting at &buffer[3] whose
		 * size (in bytes) is specified at buffer[2]
		 */
		value = 0;
		j = 0;
		while (buffer[2]--)
			value += buffer[3 + j++] << (8 * buffer[2]);

		/* Store Active mode refresh interval to restore */
		if (i == OP_PARAM_REFRESH_INTERVAL)
			active_refresh_interval = value;

		pr_info("%s: Parameter %02X: %d\n", __func__, i, value);
	}

	/*
	 * CASE 3 - Set Active mode refresh interval to 200 ms asynchronously
	 */
	buffer[0] = OP_CMD_SET_PARAMETER; /* Set Parameter */
	buffer[1] = OP_PARAM_REFRESH_INTERVAL; /* Refresh Interval parameter */
	buffer[2] = 1; /* Parameter length - 1 byte */
	buffer[3] = 200; /* 200 ms */

	preempt_disable();
	rc = cyttsp6_device_access_write_command_async(NULL,
			GRPNUM_OP_COMMAND, 0, buffer, 4,
			cyttsp6_write_async_cont);
	preempt_enable();
	if (rc < 0) {
		pr_err("%s: cyttsp6_device_access_write_command failed, rc=%d\n",
			__func__, rc);
		return rc;
	}

	return 0;
}
module_init(cyttsp6_test_device_access_api_init);

static void __exit cyttsp6_test_device_access_api_exit(void)
{
	int rc;

	/*
	 * CASE 4 - Restore Active mode refresh interval to original
	 */
	if (active_refresh_interval) {
		buffer[0] = OP_CMD_SET_PARAMETER; /* Set Parameter */
		buffer[1] = OP_PARAM_REFRESH_INTERVAL;
					/* Refresh Interval parameter */
		buffer[2] = 1; /* Parameter length - 1 byte */
		buffer[3] = (u8)active_refresh_interval;

		rc = cyttsp6_device_access_write_command(NULL,
				GRPNUM_OP_COMMAND, 0, buffer, 4);
		if (rc < 0) {
			pr_err("%s: cyttsp6_device_access_write_command failed, rc=%d\n",
				__func__, rc);
		}
	}
}
module_exit(cyttsp6_test_device_access_api_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard Product Device Access Driver API Tester");
MODULE_AUTHOR("Cypress Semiconductor <ttdrivers@cypress.com>");
