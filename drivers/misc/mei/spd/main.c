// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
/*
 * Copyright(c) 2015 - 2018 Intel Corporation. All rights reserved.
 */
#include <linux/module.h>

#include "spd.h"

static void mei_spd_rx_cb(struct mei_cl_device *cldev)
{
	struct mei_spd *spd = mei_cldev_get_drvdata(cldev);

	mutex_lock(&spd->lock);
	mei_spd_cmd(spd);
	mutex_unlock(&spd->lock);
}

static int mei_spd_probe(struct mei_cl_device *cldev,
			 const struct mei_cl_device_id *id)
{
	struct mei_spd *spd;
	u8 ver = mei_cldev_ver(cldev);
	int ret;

	dev_dbg(&cldev->dev, "probing mei spd ver = %d\n", ver);

	if (ver < 2) {
		dev_warn(&cldev->dev, "unuspported protocol version %d\n", ver);
		return -ENODEV;
	}

	spd = mei_spd_alloc(cldev);
	if (!spd)
		return -ENOMEM;

	mei_cldev_set_drvdata(cldev, spd);

	ret = mei_spd_dbgfs_register(spd, "spd");
	if (ret)
		goto free;

	ret = mei_cldev_enable(cldev);
	if (ret < 0) {
		dev_err(&cldev->dev, "Could not enable device ret = %d\n", ret);
		goto free;
	}

	ret = mei_cldev_register_rx_cb(cldev, mei_spd_rx_cb);
	if (ret) {
		dev_err(&cldev->dev, "Error register event %d\n", ret);
		goto disable;
	}

	spd_dbg(spd, "protocol version %d\n", ver);
	mei_spd_gpp_prepare(spd);
	mutex_lock(&spd->lock);
	ret = mei_spd_cmd_init_req(spd);
	mutex_unlock(&spd->lock);
	if (ret) {
		dev_err(&cldev->dev, "Could not start ret = %d\n", ret);
		goto disable;
	}

	return 0;

disable:
	mei_cldev_disable(cldev);

free:
	mei_spd_dbgfs_deregister(spd);
	mei_cldev_set_drvdata(cldev, NULL);
	mei_spd_free(spd);
	return ret;
}

static int mei_spd_remove(struct mei_cl_device *cldev)
{
	struct mei_spd *spd = mei_cldev_get_drvdata(cldev);

	if (spd->state == MEI_SPD_STATE_RUNNING) {
		spd->state = MEI_SPD_STATE_STOPPING;
		mei_spd_gpp_exit(spd);
		mutex_lock(&spd->lock);
		mei_spd_cmd_storage_status_req(spd);
		mutex_unlock(&spd->lock);
	}

	mei_cldev_disable(cldev);
	mei_spd_dbgfs_deregister(spd);
	mei_cldev_set_drvdata(cldev, NULL);
	mei_spd_free(spd);

	return 0;
}

#define MEI_SPD_UUID UUID_LE(0x2a39291f, 0x5551, 0x482f, \
			     0x99, 0xcb, 0x9e, 0x22, 0x74, 0x97, 0x8c, 0xa8)

static struct mei_cl_device_id mei_spd_tbl[] = {
	{ .uuid = MEI_SPD_UUID,  .version = MEI_CL_VERSION_ANY},
	/* required last entry */
	{ }
};
MODULE_DEVICE_TABLE(mei, mei_spd_tbl);

static struct mei_cl_driver mei_spd_driver = {
	.id_table = mei_spd_tbl,
	.name = "mei_spd",

	.probe = mei_spd_probe,
	.remove = mei_spd_remove,
};

module_mei_cl_driver(mei_spd_driver);

MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Storage Proxy driver based on mei bus");
