/*
 * auto_detect.c - camera devices auto detection
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

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

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/seq_file.h>

#include <media/nvc.h>
#include <media/camera.h>
#include "camera_common.h"

static struct camera_platform_info *cam_desc;

static void *detect_cam_dev(
	struct device *dev,
	struct camera_board *pdev,
	struct cam_device_layout *pl)
{
	struct camera_device_info d_info;
	struct camera_device *new_dev;
	struct camera_chip *chip = pdev->chip;
	struct cam_detect detect;
	struct camera_reg tbl[sizeof(u32) + 1], *ptr;
	int len, idx, i;
	int err = 0;

	dev_dbg(dev, "%s - %s %d %x\n", __func__,
		chip->name, pdev->busnum, pdev->bi->addr);
	while (pl->name[0]) {
		/* search in detected list for matching device */
		if (!strcmp(pl->name, chip->name) &&
			(pl->bus == pdev->busnum) &&
			(pl->addr == pdev->bi->addr)) {
			dev_dbg(dev, "%s: already detected.\n", __func__);
			return ERR_PTR(-EEXIST);
		}
		pl++;
	}

	err = of_camera_get_detect(dev, pdev->of_node, 0, &detect);
	/* if no detect condition, we consider the device as forced enabled. */
	if (err) {
		err = 0;
		goto auto_detect_update;
	}

	len = strlen(chip->name);
	if (sizeof(d_info.name) - 1 < len) {
		dev_dbg(dev, "%s: chip name too long %d\n", __func__, len);
		return ERR_PTR(-EFAULT);
	}

	memset(&d_info, 0, sizeof(d_info));
	strncpy(d_info.name, chip->name, len);
	d_info.bus = pdev->busnum;
	d_info.addr = pdev->bi->addr;
	new_dev = camera_new_device(dev, &d_info);
	if (IS_ERR(new_dev))
		return new_dev;
	err = chip->power_on(new_dev);
	if (err)
		goto auto_detect_done;

	idx = 1;
	do {
		dev_dbg(new_dev->dev, "    bytes: %d\n", detect.bytes);
		for (ptr = tbl, i = 0; i < detect.bytes; i++, ptr++) {
			if (i >= sizeof(u32)) {
				dev_err(new_dev->dev,
					"%s: detect read > 4 bytes.\n",
					__func__);
				err = -EINVAL;
				goto auto_detect_pwr_off;
			} else
				ptr->addr = detect.offset + i;
		}
		ptr->addr = CAMERA_TABLE_END;

		err = camera_dev_rd_table(new_dev, tbl);
		if (err)
			break;

		for (detect.offset = 0, i = 0; i < detect.bytes; i++) {
			detect.offset = detect.offset << 8 | tbl[i].val;
			dev_dbg(new_dev->dev, "  %x - %02x\n",
				tbl[i].addr, tbl[i].val);
		}
		if ((detect.offset & detect.mask) != detect.val) {
			/* not match */
			err = -ENODEV;
			break;
		}
		err = of_camera_get_detect(
			new_dev->dev, pdev->of_node, idx++, &detect);
		if (err == -ENOENT) {
			/* end of detect records */
			err = 0;
			break;
		}
	} while (!err);

auto_detect_pwr_off:
	if (err)
		chip->power_off(new_dev);
	else {
		err = chip->power_off(new_dev);
		dev_dbg(new_dev->dev, "%s: %s@%d--%04x detected.\n", __func__,
			chip->name, d_info.bus, d_info.addr);
	}

auto_detect_done:
	camera_free_device(new_dev);
	camera_remove_device(new_dev);

auto_detect_update:
	if (!err) {
		strcpy(pl->name, chip->name);
		pl->bus = pdev->busnum;
		pl->addr = pdev->bi->addr;
		pl->addr_byte = chip->regmap_cfg.reg_bits / 8;
		of_camera_get_dev_layout(dev, pdev->of_node, pl);
		return pl;
	}
	return ERR_PTR(err);
}

static void camera_auto_detect(struct work_struct *work)
{
	struct camera_platform_data *pd;
	struct camera_module *md;
	struct cam_layout_hdr *phdr, *pheader;
	struct cam_module_layout *pmod, *pml;
	struct cam_device_layout *pdev, *pl;
	uint dev_num, i;

	if (cam_desc == NULL || cam_desc->pdata == NULL) {
		pr_err("%s NULL pointer %p\n", __func__, cam_desc);
		return;
	}
	dev_dbg(cam_desc->dev, "%s\n", __func__);

	mutex_lock(cam_desc->u_mutex);
	if (cam_desc->layout) {
		dev_notice(cam_desc->dev, "layout already created.\n");
		mutex_unlock(cam_desc->u_mutex);
		return;
	}

	pd = cam_desc->pdata;
	md = pd->modules;
	phdr = kzalloc(sizeof(*phdr) + sizeof(*pmod) * pd->mod_num +
		sizeof(*pdev) * pd->prof_num, GFP_KERNEL);
	if (phdr == NULL) {
		mutex_unlock(cam_desc->u_mutex);
		dev_err(cam_desc->dev, "%s cannot alloc memory!\n", __func__);
		return;
	}
	pmod = (void *)(phdr + 1);
	pdev = (void *)(pmod + pd->mod_num);

	phdr->version = 2;

	for (i = 0; i < pd->mod_num; i++, md++) {
		memset(pmod, 0, sizeof(*pmod));
		dev_num = 0;
		if (md->sensor.bi) {
			pl = detect_cam_dev(cam_desc->dev, &md->sensor, pdev);
			if (IS_ERR(pl))
				continue;
			pmod->off_sensor =
				(unsigned long)pl - (unsigned long)phdr;
			dev_num++;
		}

		if (md->focuser.bi) {
			pl = detect_cam_dev(cam_desc->dev, &md->focuser, pdev);
			if (IS_ERR(pl))
				continue;
			pmod->off_focuser =
				(unsigned long)pl - (unsigned long)phdr;
			dev_num++;
		}

		if (md->flash.bi) {
			pl = detect_cam_dev(cam_desc->dev, &md->flash, pdev);
			if (IS_ERR(pl))
				continue;
			pmod->off_flash =
				(unsigned long)pl - (unsigned long)phdr;
			dev_num++;
		}
		pmod->index = i;

		/* Try to add drivers by module */
		if (camera_add_drv_by_module(cam_desc->dev, pmod->index)) {
			dev_notice(cam_desc->dev, "module %d not enabled.\n",
				pmod->index);
			continue;
		}

		pmod++;
		phdr->mod_num++;
		phdr->dev_num += dev_num;
		dev_dbg(cam_desc->dev, "%s: module %u(%u devices) detected\n",
			__func__, i, dev_num);
	}

	dev_dbg(cam_desc->dev, "%s: %d modules %d devices detected.\n",
		__func__, phdr->mod_num, phdr->dev_num);

	pheader = kzalloc(sizeof(*pheader) + sizeof(*pmod) * phdr->mod_num +
		sizeof(*pdev) * phdr->dev_num, GFP_KERNEL);
	if (pheader == NULL) {
		mutex_unlock(cam_desc->u_mutex);
		kfree(phdr);
		dev_err(cam_desc->dev, "%s cannot alloc memory!\n", __func__);
		return;
	}
	*pheader = *phdr;
	pheader->size = sizeof(*pheader);
	pml = (void *)(pheader + 1);
	pmod = (void *)(phdr + 1);
	for (i = 0; i < pheader->mod_num; i++) {
		*pml++ = *pmod++;
		pheader->size += sizeof(*pml);
	}

	pl = (void *)pml;
	pml = (void *)(pheader + 1);

	for (i = 0; i < pheader->mod_num; i++) {
		if (pml->off_sensor) {
			pdev = (void *)((unsigned long)phdr + pml->off_sensor);
			*pl = *pdev;
			pml->off_sensor = (void *)pl - (void *)pheader;
			pheader->size += sizeof(*pl);
			dev_dbg(cam_desc->dev, "%d: %s %s %d %x %x\n",
				pml->off_sensor, pl->name, pl->alt_name,
				pl->bus, pl->addr, pl->dev_id);
			pl++;
		}
		if (pml->off_focuser) {
			pdev = (void *)((unsigned long)phdr + pml->off_focuser);
			*pl = *pdev;
			pml->off_focuser = (void *)pl - (void *)pheader;
			pheader->size += sizeof(*pl);
			dev_dbg(cam_desc->dev, "%d: %s %s %d %x %x\n",
				pml->off_focuser, pl->name, pl->alt_name,
				pl->bus, pl->addr, pl->dev_id);
			pl++;
		}
		if (pml->off_flash) {
			pdev = (void *)((unsigned long)phdr + pml->off_flash);
			*pl = *pdev;
			pml->off_flash = (void *)pl - (void *)pheader;
			pheader->size += sizeof(*pl);
			dev_dbg(cam_desc->dev, "%d: %s %s %d %x %x\n",
				pml->off_flash, pl->name, pl->alt_name,
				pl->bus, pl->addr, pl->dev_id);
			pl++;
		}
		pml++;
	}
	kfree(phdr);

	cam_desc->layout = pheader;
	cam_desc->size_layout = pheader->size;
	mutex_unlock(cam_desc->u_mutex);
	dev_dbg(cam_desc->dev, "HEADER: %d %d %d %d\n", pheader->version,
		pheader->size, pheader->mod_num, pheader->dev_num);
}

static DECLARE_WORK(auto_detect_work, camera_auto_detect);
int camera_module_detect(struct camera_platform_info *camd)
{
	cam_desc = camd;
	schedule_work(&auto_detect_work);
	dev_dbg(cam_desc->dev, "%s scheduled.\n", __func__);
	return 0;
}
