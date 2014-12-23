// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
/*
 * Copyright(c) 2015 - 2018 Intel Corporation. All rights reserved.
 */
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/debugfs.h>

#include "cmd.h"
#include "spd.h"

static ssize_t mei_spd_dbgfs_read_info(struct file *fp, char __user *ubuf,
				       size_t cnt, loff_t *ppos)
{
	struct mei_spd *spd = fp->private_data;
	size_t bufsz = 4095;
	char *buf;
	int pos = 0;
	int ret;

	buf = kzalloc(bufsz, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	pos += scnprintf(buf + pos, bufsz - pos, "DEV STATE: [%d] %s\n",
			spd->state, mei_spd_state_str(spd->state));
	pos += scnprintf(buf + pos, bufsz - pos, "DEV TYPE    : [%d] %s\n",
			spd->dev_type, mei_spd_dev_str(spd->dev_type));
	pos += scnprintf(buf + pos, bufsz - pos, "    ID SIZE : %d\n",
			spd->dev_id_sz);
	pos += scnprintf(buf + pos, bufsz - pos, "    ID      : '%s'\n", "N/A");
	pos += scnprintf(buf + pos, bufsz - pos, "GPP\n");
	pos += scnprintf(buf + pos, bufsz - pos, "  id     : %d\n",
			spd->gpp_partition_id);
	pos += scnprintf(buf + pos, bufsz - pos, "  opened : %1d\n",
			mei_spd_gpp_is_open(spd));

	ret = simple_read_from_buffer(ubuf, cnt, ppos, buf, pos);
	kfree(buf);
	return ret;
}

static const struct file_operations mei_spd_dbgfs_fops_info = {
	.open   = simple_open,
	.read   = mei_spd_dbgfs_read_info,
	.llseek = generic_file_llseek,
};

void mei_spd_dbgfs_deregister(struct mei_spd *spd)
{
	if (!spd->dbgfs_dir)
		return;
	debugfs_remove_recursive(spd->dbgfs_dir);
	spd->dbgfs_dir = NULL;
}

int mei_spd_dbgfs_register(struct mei_spd *spd, const char *name)
{
	struct dentry *dir, *f;

	dir = debugfs_create_dir(name, NULL);
	if (!dir)
		return -ENOMEM;

	spd->dbgfs_dir = dir;

	f = debugfs_create_file("info", 0400, dir,
				spd, &mei_spd_dbgfs_fops_info);
	if (!f) {
		spd_err(spd, "info: registration failed\n");
		goto err;
	}

	return 0;
err:
	mei_spd_dbgfs_deregister(spd);
	return -ENODEV;
}
