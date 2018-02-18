/*
 * debugfs.c - debug fs access support
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define CAMERA_DEVICE_INTERNAL

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <media/nvc.h>
#include <media/camera.h>
#include "camera_common.h"

#define GET_POINTER(p, o) ((unsigned long)p + (unsigned long)o)

/* module size = 96 bytes in user space */
struct cam_module {
	u32 id;
	u32 reserved0;
	u64 sensor;
	u64 focuser;
	u64 flash;
	u64 rom;
	u32 reserved[13];
	u8 pos;
	u8 reserved3;
};

struct dev_profile {
	u64 off_name;
	u32 index;
	u32 type;
	u64 guid;
	u32 present;
	u32 pos;
	u32 bustype;
	u8 bus;
	u8 addr;
	u8 reserved1[2];
	u32 addr_byte;
	u32 reserved2[16];
	u64 off_drv_name;
	u32 reserved3[4];
	u32 dev_id;
};

static struct camera_platform_info *cam_desc;
static char *device_type[] = {
	"sensor",
	"focuser",
	"flash",
	"rom",
	"other1",
	"other2",
	"other3",
	"other4",
	"unsupported type"
};

static void camdbg_dev_prof(struct seq_file *s, void *pl, u32 offset)
{
	struct dev_profile *prf = (void *)GET_POINTER(pl, offset);
	int dtype = prf->type;

	if (dtype < 2 || dtype >= ARRAY_SIZE(device_type))
		dtype = ARRAY_SIZE(device_type) - 1;
	else
		dtype -= 2;

	seq_printf(s, "%016llx %.20s %.20s %1x %1x %.2x %1x %.8x %x %.20s\n",
		prf->guid, device_type[dtype],
		(char *)GET_POINTER(pl, prf->off_drv_name),
		prf->pos, prf->bus, prf->addr, prf->addr_byte, prf->dev_id,
		prf->index, (char *)GET_POINTER(pl, prf->off_name));
}

static void camdbg_dev_layout(struct seq_file *s, struct cam_device_layout *pl)
{
	const char *pt;

	if (pl->type < ARRAY_SIZE(device_type))
		pt = device_type[pl->type];
	else
		pt = device_type[ARRAY_SIZE(device_type) - 1];
	seq_printf(s, "%016llx %.20s %.20s %1x %1x %.2x %1x %.8x %x %.20s\n",
		pl->guid, pt, pl->alt_name, pl->pos, pl->bus, pl->addr,
		pl->addr_byte, pl->dev_id, pl->index, pl->name);
}

static int camera_debugfs_layout(struct seq_file *s)
{
	void *pl = cam_desc->layout;
	struct cam_layout_hdr *hdr = (struct cam_layout_hdr *)pl;
	struct cam_device_layout *layout;
	int num;

	if (!pl)
		return -EEXIST;

	if (*((u32 *)pl) == 2) { /* version 2 */
		struct cam_module_layout *pmod =
			(void *)GET_POINTER(pl, sizeof(*hdr));
		for (num = 0; num < hdr->mod_num; num++) {
			layout = (void *)GET_POINTER(pl, pmod[num].off_sensor);
			if (pmod[num].off_sensor)
				camdbg_dev_layout(s, layout);

			layout = (void *)GET_POINTER(pl, pmod[num].off_focuser);
			if (pmod[num].off_focuser)
				camdbg_dev_layout(s, layout);

			layout = (void *)GET_POINTER(pl, pmod[num].off_flash);
			if (pmod[num].off_flash)
				camdbg_dev_layout(s, layout);
		}
		return 0;
	}

	if (*((u32 *)pl) == 1) { /* version 1 */
		struct cam_module *pmod = (void *)GET_POINTER(pl, sizeof(*hdr));
		for (num = 0; num < hdr->mod_num; num++) {
			if (pmod[num].sensor)
				camdbg_dev_prof(s, pl, pmod[num].sensor);
			if (pmod[num].focuser)
				camdbg_dev_prof(s, pl, pmod[num].focuser);
			if (pmod[num].flash)
				camdbg_dev_prof(s, pl, pmod[num].flash);
		}
		return 0;
	}

	if (*((u32 *)pl) == 0) {
		layout = (void *)((unsigned long)pl +
			sizeof(struct cam_layout_hdr));
		num = ((struct cam_layout_hdr *)pl)->dev_num;
	} else {
		layout = pl;
		num = cam_desc->size_layout / sizeof(*layout);
		if (unlikely(num * sizeof(*layout) != cam_desc->size_layout)) {
			seq_printf(s, "WHAT? layout size mismatch: %zx vs %d\n",
				cam_desc->size_layout, num);
			return -EFAULT;
		}
	}

	while (num--) {
		camdbg_dev_layout(s, layout);
		layout++;
	}
	return 0;
}

static int camera_debugfs_platform_data(struct seq_file *s)
{
	struct camera_platform_data *pd = cam_desc->pdata;
	struct camera_module *md = pd->modules;
	struct i2c_board_info *bi;
	char ch;

	seq_printf(s, "\nplatform data: cfg = %x\n", pd->cfg);

	while (md) {
		if (!md->sensor.bi && !md->focuser.bi && !md->flash.bi)
			break;

		seq_puts(s, "        {");
		if ((md->sensor.flags & CAMERA_MODULE_FLAG_TRIED) &&
			!(md->sensor.flags & CAMERA_MODULE_FLAG_INSTALLED))
			ch = '#';
		else
			ch = ' ';
		bi = md->sensor.bi;
		if (bi)
			seq_printf(s, "%csensor %s @%02x,",
				ch, bi->type, bi->addr);

		if ((md->focuser.flags & CAMERA_MODULE_FLAG_TRIED) &&
			!(md->focuser.flags & CAMERA_MODULE_FLAG_INSTALLED))
			ch = '#';
		else
			ch = ' ';
		bi = md->focuser.bi;
		if (bi)
			seq_printf(s, "%cfocuser %s @%02x,",
				ch, bi->type, bi->addr);

		if ((md->flash.flags & CAMERA_MODULE_FLAG_TRIED) &&
			!(md->flash.flags & CAMERA_MODULE_FLAG_INSTALLED))
			ch = '#';
		else
			ch = ' ';
		bi = md->flash.bi;
		if (bi)
			seq_printf(s, "%cflash %s @%02x",
				ch, bi->type, bi->addr);

		if (md->flags & CAMERA_MODULE_FLAG_INSTALLED)
			ch = '*';
		else if (md->flags & CAMERA_MODULE_FLAG_TRIED)
			ch = '#';
		else
			ch = ' ';
		seq_printf(s, " }  %c\n", ch);
		md++;
	}

	return 0;
}

static int camera_debugfs_chips(struct seq_file *s)
{
	struct camera_chip *ccp;
	const struct regmap_config *rcfg;

	seq_printf(s, "\n%s: camera devices supported:\n", cam_desc->dname);
	mutex_lock(cam_desc->c_mutex);
	list_for_each_entry(ccp, cam_desc->chip_list, list) {
		rcfg = &ccp->regmap_cfg;
		seq_printf(s, "    %.16s: type %d, ref_cnt %d. ",
			ccp->name,
			ccp->type,
			atomic_read(&ccp->ref_cnt)
			);
		seq_printf(s, "%02d bit addr, %02d bit data, stride %d, pad %d",
			rcfg->reg_bits,
			rcfg->val_bits,
			rcfg->reg_stride,
			rcfg->pad_bits
			);
		seq_printf(s, " bits. cache type: %d, flags r %02x / w %02x\n",
			rcfg->cache_type,
			rcfg->read_flag_mask,
			rcfg->write_flag_mask
			);
	}
	mutex_unlock(cam_desc->c_mutex);

	return 0;
}

static int camera_debugfs_devices(struct seq_file *s)
{
	struct camera_device *cdev;

	if (list_empty(cam_desc->dev_list)) {
		seq_printf(s, "\n%s: No device installed.\n", cam_desc->dname);
		return 0;
	}

	seq_printf(s, "\n%s: activated devices:\n", cam_desc->dname);
	mutex_lock(cam_desc->d_mutex);
	list_for_each_entry(cdev, cam_desc->dev_list, list) {
		seq_printf(s, "    %s: on %s, %s power %s\n",
			cdev->name,
			cdev->chip->name,
			atomic_read(&cdev->in_use) ? "occupied" : "free",
			cdev->is_power_on ? "on" : "off"
			);
	}
	mutex_unlock(cam_desc->d_mutex);

	return 0;
}

static int camera_debugfs_apps(struct seq_file *s)
{
	struct camera_info *user;
	int num = 0;

	if (list_empty(cam_desc->app_list)) {
		seq_printf(s, "\n%s: No App running.\n", cam_desc->dname);
		return 0;
	}

	mutex_lock(cam_desc->u_mutex);
	list_for_each_entry(user, cam_desc->app_list, list) {
		struct camera_device *cdev = user->cdev;
		struct camera_chip *ccp = NULL;

		if (cdev)
			ccp = cdev->chip;
		seq_printf(s, "app #%02d: on %s chip type: %s\n", num,
			cdev ? (char *)cdev->name : "NULL",
			ccp ? (char *)ccp->name : "NULL");
		num++;
	}
	mutex_unlock(cam_desc->u_mutex);
	seq_printf(s, "\n%s: There are %d apps running.\n",
		cam_desc->dname, num);

	return 0;
}

static int camera_status_show(struct seq_file *s, void *data)
{
	pr_info("%s %s\n", __func__, cam_desc->dname);

	if (list_empty(cam_desc->chip_list)) {
		seq_printf(s, "%s: No devices supported.\n", cam_desc->dname);
		return 0;
	}

	camera_debugfs_layout(s);
	camera_debugfs_chips(s);
	camera_debugfs_devices(s);
	camera_debugfs_apps(s);
	camera_debugfs_platform_data(s);

	return 0;
}

static ssize_t camera_attr_set(struct file *s,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[24];
	int buf_size;
	u32 val = 0;

	pr_info("%s\n", __func__);

	if (!user_buf || count <= 1)
		return -EFAULT;

	memset(buf, 0, sizeof(buf));
	buf_size = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (sscanf(buf + 1, "0x%x", &val) == 1)
		goto set_attr;
	if (sscanf(buf + 1, "0X%x", &val) == 1)
		goto set_attr;
	if (sscanf(buf + 1, "%d", &val) == 1)
		goto set_attr;

	pr_info("SYNTAX ERROR: %s\n", buf);
	return -EFAULT;

set_attr:
	pr_info("new data = %x\n", val);
	switch (buf[0]) {
	case 'd':
		cam_desc->pdata->cfg = val;
		break;
	}

	return count;
}

static int camera_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, camera_status_show, inode->i_private);
}

static const struct file_operations camera_debugfs_fops = {
	.open = camera_debugfs_open,
	.read = seq_read,
	.write = camera_attr_set,
	.llseek = seq_lseek,
	.release = single_release,
};

int camera_debugfs_init(struct camera_platform_info *info)
{
	struct dentry *d;

	dev_dbg(info->dev, "%s %s\n", __func__, info->dname);
	info->d_entry = debugfs_create_dir(
		info->miscdev.this_device->kobj.name, NULL);
	if (info->d_entry == NULL) {
		dev_err(info->dev, "%s: create dir failed\n", __func__);
		return -ENOMEM;
	}

	d = debugfs_create_file("d", S_IRUGO|S_IWUSR, info->d_entry,
		(void *)info, &camera_debugfs_fops);
	if (!d) {
		dev_err(info->dev, "%s: create file failed\n", __func__);
		debugfs_remove_recursive(info->d_entry);
		info->d_entry = NULL;
	}

	cam_desc = info;
	return 0;
}

int camera_debugfs_remove(void)
{
	debugfs_remove_recursive(cam_desc->d_entry);
	cam_desc->d_entry = NULL;
	return 0;
}
