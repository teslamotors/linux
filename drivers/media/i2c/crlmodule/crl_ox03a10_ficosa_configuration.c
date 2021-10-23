/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2020 Intel Corporation
 *
 * Author: Chen Meng J <meng.j.chen@intel.com>
 *
 */

#include <linux/i2c.h>

#include "crlmodule.h"
#include "crlmodule-regs.h"
#include "crlmodule-sensor-ds.h"
#include "crl_ox03a10_1280_dcg.h"

static struct crl_register_write_rep ox03a10_ficosa_reset_regs[] = {
	{ 0x00, CRL_REG_LEN_DELAY, 0x64 }, /* Delay 100 ms */
	{ 0x0103, CRL_REG_LEN_08BIT, 0x01 }, /* software reset */
	{ 0x00, CRL_REG_LEN_DELAY, 0x64 } /* Delay 100 ms */
};

int ox03a10_ficosa_sensor_init(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct crl_subdev *ssd = to_crlmodule_subdev(sd);
	struct crl_sensor *sensor = ssd->sensor;
	int rval;

	rval = crlmodule_write_regs(sensor,
			ox03a10_ficosa_reset_regs,
			ARRAY_SIZE(ox03a10_ficosa_reset_regs));
	if (rval) {
		dev_err(&client->dev, "%s failed to reset\n", __func__);
		return rval;
	}

	dev_dbg(&client->dev,
			"%s apply mode setting 1920x1280 12 bit DCG\n",
			__func__);

	rval = crlmodule_write_regs(sensor,
			ox03a10_1920_1280_12DCG,
			ARRAY_SIZE(ox03a10_1920_1280_12DCG));
	if (rval) {
		dev_err(&client->dev, "%s failed to set mode\n", __func__);
		return rval;
	}

	rval = crlmodule_write_regs(sensor,
			sensor->sensor_ds->streamoff_regs,
			sensor->sensor_ds->streamoff_regs_items);
	if (rval) {
		dev_err(&client->dev, "%s failed to stream off\n", __func__);
		return rval;
	}

	return 0;
}
