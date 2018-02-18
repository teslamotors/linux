/*
 * of_camera.c - device tree retrieve
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

static struct camera_platform_info *cam_desc;
static struct device_node *cam_alias;

#if defined(DEBUG)
static int of_camera_get_strings(char *s, int length)
{
	int len, sn = 0;
	char ch;

	for (len = 0; len < length; len++) {
		ch = s[len];
		/* if \0 and not the 1st char, new string */
		if (len && !ch) {
			sn++;
			continue;
		}
		/* check \r \n */
		if (ch == 0x0D || ch == 0x0A)
			continue;
		/* check charactors */
		if (ch >= 0x20 && ch <= 0x7E)
			continue;
		return 0;
	}

	/* last charactor has to be a '\0' */
	if (ch)
		return 0;

	return sn;
}

static void of_camera_dump_strings(
	struct device *dev, const char *t, const char *s, int len)
{
	char *ptr, *buf;

	if (!len || !s)
		return;

	buf = kzalloc(len + 1, GFP_KERNEL);
	if (!buf)
		return;

	memcpy(buf, s, len);
	ptr = buf;
	while (--len) {
		if (!*ptr)
			*ptr = ',';
		ptr++;
	}
	dev_info(dev, "     %s = %s\n", t, buf);
	kfree(buf);
}

static void of_camera_dump_array(
	struct device *dev, char *t, const u8 *s, int len)
{
	char *ptr, *buf;
	int i;

	if (!len || !s)
		return;

	buf = kzalloc(len * 4 + 20, GFP_KERNEL);
	if (!buf)
		return;

	ptr = buf;
	for (i = 0; i < len; i++, s++) {
		sprintf(ptr, " %02x", *s);
		ptr += 3;
		if (i % 16 == 15) {
			strcat(ptr, "\n");
			ptr += strlen(ptr);
		} else if (i % 4 == 3) {
			strcat(ptr, ", ");
			ptr += strlen(ptr);
		}
	}
	dev_info(dev, "     %s(%d) = %s\n", t, len, buf);
	kfree(buf);
}

static void of_camera_dump_property(struct device *dev, struct property *pp)
{
	if (of_camera_get_strings(pp->value, pp->length))
		of_camera_dump_strings(dev, pp->name, pp->value, pp->length);
	else
		of_camera_dump_array(dev, pp->name, pp->value, pp->length);
}
#else
static inline void of_camera_dump_array(
	struct device *d, char *t, const u8 *n, int l) {}
static inline void of_camera_dump_property(
	struct device *d, struct property *p) {}
#endif

static struct device_node *of_camera_child_by_index(
	const struct device_node *np, int index)
{
	struct device_node *child;
	int i = 0;

	for_each_child_of_node(np, child) {
		if (index == i)
			break;
		i++;
	}

	return child;
}

static uint of_camera_scan_maxlen(
	struct platform_device *dev, struct device_node *np)
{
	struct device_node *child;
	struct property *pp;
	uint max_s = 0, mc;

	for_each_child_of_node(np, child) {
		dev_dbg(&dev->dev, " child %s\n", child->name);
		for (pp = child->properties; pp; pp = pp->next) {
			if (max_s < pp->length)
				max_s = pp->length;
			of_camera_dump_property(&dev->dev, pp);
		}
		mc = of_camera_scan_maxlen(dev, child);
		if (mc > max_s)
			max_s = mc;
	}

	return max_s;
}

static uint of_camera_get_child_count(
	struct platform_device *dev, struct device_node *np)
{
	struct device_node *child;
	uint cnt = 0;

	for_each_child_of_node(np, child) {
		cnt++;
		dev_dbg(&dev->dev, " child %s\n", child->name);
	}

	return cnt;
}

static uint of_camera_profile_statistic(
	struct platform_device *dev, struct device_node **prof, uint *num_prof)
{
	dev_dbg(&dev->dev, "%s\n", __func__);

	if (prof) {
		*prof = of_find_node_by_name(dev->dev.of_node, "profiles");
		if (*prof && num_prof) {
			*num_prof = of_camera_get_child_count(dev, *prof);
			dev_dbg(&dev->dev, "        profiles %d\n", *num_prof);
		}
	}

	return of_camera_scan_maxlen(dev, dev->dev.of_node);
}

int of_camera_get_property(
	struct camera_info *cam, struct nvc_param *prm, void *pcvt)
{
	struct camera_platform_data *pdata = cam_desc->pdata;
	struct device_node *np = NULL;
	char ref_name[CAMERA_MAX_NAME_LENGTH];
	const void *prop;
	int len = 0, err = 0, i;

	if (prm == NULL) {
		dev_err(cam->dev, "%s EMPTY param\n", __func__);
		return -EFAULT;
	}
	dev_dbg(cam->dev, "%s %x", __func__, prm->param);
	if ((prm->param & CAMERA_DT_TYPE_MASK) == CAMERA_DT_QUERY) {
		prm->sizeofvalue = pdata->max_blob_size;
		prm->variant = pdata->prof_num;
		prm->param = pdata->mod_num;
		prm->p_value = 0;
		return 0;
	}

	/* sanity check */
	if (!prm->sizeofvalue) {
		dev_err(cam->dev, "%s invalid property name length %d\n",
			__func__, prm->sizeofvalue);
		return -EBADF;
	}

	dev_dbg(cam->dev, "%s params: %x, %x, %lx, %d\n", __func__,
		prm->param, prm->variant, prm->p_value, prm->sizeofvalue);
	/* use bit mask to determine if it's a profile or a module query */
	switch (prm->param & CAMERA_DT_HANDLE_MASK) {
	case CAMERA_DT_HANDLE_PROFILE:
		/* looking for a profile */
		if (!pdata->of_profiles) {
			dev_dbg(cam->dev, "%s DT has no profile node.\n",
				__func__);
			return -EEXIST;
		}
		np = of_camera_child_by_index(
			pdata->of_profiles, prm->variant);
		break;
	case CAMERA_DT_HANDLE_PHANDLE:
		np = of_find_node_by_phandle(prm->variant);
		break;
	case CAMERA_DT_HANDLE_MODULE:
		/* module of_node */
		if (prm->variant >= pdata->mod_num) {
			dev_err(cam->dev, "%s has no sub module node %d\n",
				__func__, prm->variant);
			return -EEXIST;
		}
		np = pdata->modules[prm->variant].of_node;
		break;
	case CAMERA_DT_HANDLE_SENSOR:
		/* sensor of_node */
		if (prm->variant >= pdata->mod_num) {
			dev_err(cam->dev, "%s has no sub module node %d\n",
				__func__, prm->variant);
			return -EEXIST;
		}
		if (pdata->modules[prm->variant].sensor.bi)
			np = pdata->modules[prm->variant].sensor.bi->of_node;
		break;
	case CAMERA_DT_HANDLE_FOCUSER:
		/* focuser of_node */
		if (prm->variant >= pdata->mod_num) {
			dev_err(cam->dev, "%s has no sub module node %d\n",
				__func__, prm->variant);
			return -EEXIST;
		}
		if (pdata->modules[prm->variant].focuser.bi)
			np = pdata->modules[prm->variant].focuser.bi->of_node;
		break;
	case CAMERA_DT_HANDLE_FLASH:
		/* flash of_node */
		if (prm->variant >= pdata->mod_num) {
			dev_err(cam->dev, "%s has no sub module node %d\n",
				__func__, prm->variant);
			return -EEXIST;
		}
		if (pdata->modules[prm->variant].flash.bi)
			np = pdata->modules[prm->variant].flash.bi->of_node;
		break;
	default:
		dev_err(cam->dev, "%s unsupported handle type %x\n",
			__func__, prm->param);
		return -EINVAL;
	}
	if (!np) {
		dev_dbg(cam->dev, "%s module %d has no such DT node\n",
			__func__, prm->variant);
		return -ECHILD;
	}

	dev_dbg(cam->dev, "%s now on %s of [%d]\n",
		__func__, np->full_name, prm->variant);

	memcpy(ref_name, pcvt, prm->sizeofvalue < CAMERA_MAX_NAME_LENGTH - 1 ?
		prm->sizeofvalue : CAMERA_MAX_NAME_LENGTH - 1);

	dev_dbg(cam->dev, "%s looking for %s\n", __func__, ref_name);
	prop = of_get_property(np, ref_name, &len);
	if (len > prm->sizeofvalue) {
		dev_err(cam->dev, "%s return buffer size %d too small %d\n",
			__func__, prm->sizeofvalue, len);
		return -E2BIG;
	}
	if (!prop) {
		dev_dbg(cam->dev, "%s property %s not exists\n",
			__func__, ref_name);
		prm->sizeofvalue = 0;
		goto get_property_end;
	}
	prm->sizeofvalue = len;
	dev_dbg(cam->dev, "%s %p size %d\n", __func__, prop, len);

	switch (prm->param & CAMERA_DT_TYPE_MASK) {
	case CAMERA_DT_STRING:
		if (len)
			prm->variant = of_property_count_strings(
				np, ref_name);
		else
			prm->variant = 0;
		memcpy(pcvt, prop, len);
		break;
	case CAMERA_DT_DATA_U8:
		if (len != sizeof(u8)) {
			dev_err(cam->dev, "%s data_u8: size mismatch %d\n",
				__func__, len);
			err = -EINVAL;
			break;
		}
		*(u8 *)pcvt = *(u8 *)prop;
		break;
	case CAMERA_DT_DATA_U16:
		if (len != sizeof(u16)) {
			dev_err(cam->dev, "%s data_u8: size mismatch %d\n",
				__func__, len);
			err = -EINVAL;
			break;
		}
		*(u16 *)pcvt = be16_to_cpup(prop);
		break;
	case CAMERA_DT_DATA_U32:
		if (len != sizeof(u32)) {
			dev_err(cam->dev, "%s data_u8: size mismatch %d\n",
				__func__, len);
			err = -EINVAL;
			break;
		}
		*(u32 *)pcvt = be32_to_cpup(prop);
		break;
	case CAMERA_DT_DATA_U64:
		if (len != sizeof(u64)) {
			dev_err(cam->dev, "%s data_u8: size mismatch %d\n",
				__func__, len);
			err = -EINVAL;
			break;
		}
		*(u64 *)pcvt = be64_to_cpup(prop);
		break;
	case CAMERA_DT_ARRAY_U8:
		memcpy(pcvt, prop, len);
		break;
	case CAMERA_DT_ARRAY_U16:
		if (len % sizeof(__be16)) {
			dev_err(cam->dev, "%s array_u16: size mismatch %d\n",
				__func__, len);
			err = -EINVAL;
			break;
		}
		for (i = 0; i < len / sizeof(__be16); i++) {
			((u16 *)pcvt)[i] = be16_to_cpup(prop);
			prop += sizeof(__be16);
		}
		break;
	case CAMERA_DT_ARRAY_U32:
		if (len % sizeof(__be32)) {
			dev_err(cam->dev, "%s array_u16: size mismatch %d\n",
				__func__, len);
			err = -EINVAL;
			break;
		}
		for (i = 0; i < len / sizeof(__be32); i++) {
			((u32 *)pcvt)[i] = be32_to_cpup(prop);
			prop += sizeof(__be32);
		}
		break;
	default:
		dev_err(cam->dev, "%s unsupported request %x\n",
			__func__, prm->param);
		err = -ENOENT;
	}

get_property_end:
	of_camera_dump_array(cam->dev, ref_name, pcvt, len);
	return err;
}

static struct device_node *of_camera_match_device(
	struct camera_device_info *info,
	struct camera_board *brd)
{
	if (brd == NULL || brd->bi == NULL || brd->chip == NULL)
		return NULL;

	if (brd->busnum == info->bus &&
		brd->bi->addr == info->addr &&
		!strcmp(((struct camera_chip *)brd->chip)->name, info->name))
		return brd->of_node;

	return NULL;
}

struct device_node *of_camera_get_node(
	struct device *dev,
	struct camera_device_info *info,
	struct camera_platform_data *pdata)
{
	struct camera_module *md;
	struct device_node *of_node = NULL;

	if (!pdata || !pdata->modules)
		return NULL;
	md = pdata->modules;
	while (md->of_node) {
		of_node = of_camera_match_device(info, &md->sensor);
		if (of_node)
			return of_node;
		of_node = of_camera_match_device(info, &md->focuser);
		if (of_node)
			return of_node;
		of_node = of_camera_match_device(info, &md->flash);
		if (of_node)
			return of_node;
		md++;
	}

	return NULL;
}

static const char *of_camera_alias_regulator(struct device *dev, char *alias)
{
	struct device_node *np;
	phandle ph;
	int err;

	if (cam_alias == NULL)
		return ERR_PTR(-ENODEV);
	err = of_property_read_u32(cam_alias, alias, &ph);
	if (err) {
		dev_dbg(dev, "%s datalen invalid.\n", __func__);
		return NULL;
	}
	np = of_find_node_by_phandle(ph);
	return of_get_property(np, "regulator-name", NULL);
}

int of_camera_get_regulators(
	struct device *dev,
	struct device_node *prf,
	const char **regs,
	int reg_num)
{
	const void *prop;
	const char *sreg;
	int len, n, num = 0;

	/* first search for power-rails, otherwise fall back to regulators. */
	prop = of_get_property(prf, "power-rails", &len);
	if (prop) {
		while (len) {
			sreg = of_camera_alias_regulator(dev, (char *)prop);
			if (sreg) {
				regs[num++] = sreg;
				if (num >= reg_num)
					break;
			}
			n = strlen(prop) + 1;
			prop += n;
			len -= n;
		}
		if (num >= reg_num) {
			dev_notice(dev, "%s: THERE MAY BE REGULATOR NOT READ\n",
				__func__);
			dev_notice(dev, "%s: INCREASE THE BUF SIZE TO SOLVE\n",
				__func__);
		}
		return num;
	}

	prop = of_get_property(prf, "regulators", &len);
	if (prop == NULL)
		return 0;

	while (len) {
		regs[num++] = prop;
		if (num >= reg_num)
			break;
		n = strlen(prop) + 1;
		prop += n;
		len -= n;
	}
	return num;
}

void *of_camera_get_pwrseq(
	struct device *dev, struct device_node *prf, int *pon, int *poff)
{
	void *pwr_seq = NULL;
	const void *prop, *prop2;
	int i, len, len2;
	u32 *preg;

	dev_dbg(dev, "%s - %s\n", __func__, prf->full_name);
	prop = of_get_property(prf, "poweron", &len);
	if (!prop)
		dev_dbg(dev, "%s No power on %s\n", __func__, prf->full_name);
	prop2 = of_get_property(prf, "poweroff", &len2);
	if (!prop2)
		dev_dbg(dev, "%s No power on %s\n", __func__, prf->full_name);
	if (len || len2) {
		pwr_seq = kzalloc(len + len2, GFP_KERNEL);
		if (pwr_seq == NULL) {
			dev_err(dev, "%s cannot allocate memory!\n", __func__);
			return ERR_PTR(-ENOMEM);
		}

		preg = pwr_seq;
		for (i = 0; i < len / sizeof(u32); i++, preg++) {
			*preg = be32_to_cpup((__be32 *)prop);
			prop += sizeof(__be32);
		}

		for (i = 0; i < len2 / sizeof(u32); i++, preg++) {
			*preg = be32_to_cpup((__be32 *)prop2);
			prop2 += sizeof(__be32);
		}
	}
	*pon = len / sizeof(struct camera_reg);
	*poff = len2 / sizeof(struct camera_reg);
	dev_dbg(dev, "%p - %u + %u\n", pwr_seq, *pon, *poff);

	return pwr_seq;
}

void of_camera_put_pwrseq(struct device *dev, void *ps)
{
	kfree(ps);
}

const void *of_camera_get_clocks(
	struct device *dev, struct device_node *np, int *num)
{
	const void *prop;

	prop = of_get_property(np, "clocks", NULL);
	if (prop == NULL)
		return (const void *)-EEXIST;
	*num = of_property_count_strings(np, "clocks");
	dev_dbg(dev, "%s - %d, %s ...\n", __func__, *num, (char *)prop);
	return prop;
}

int of_camera_get_detect(
	struct device *dev,
	struct device_node *np,
	int idx,
	struct cam_detect *pdt)
{
	const void *prop;
	int num;

	prop = of_get_property(np, "detect", &num);
	dev_dbg(dev, "%s - %p %d\n", __func__, prop, num);
	if (prop == NULL || pdt == NULL)
		return -EEXIST;
	if ((idx + 1) * sizeof(u32) * 4 > num)
		return -ENOENT;

	memset(pdt, 0, sizeof(*pdt));
	prop += sizeof(u32) * 4 * idx;
	num = be32_to_cpup((__be32 *)prop);
	pdt->bytes = num;
	prop += sizeof(u32);

	num = be32_to_cpup((__be32 *)prop);
	pdt->offset = num;
	prop += sizeof(u32);

	num = be32_to_cpup((__be32 *)prop);
	pdt->mask = num;
	prop += sizeof(u32);

	num = be32_to_cpup((__be32 *)prop);
	pdt->val = num;
	dev_dbg(dev, "%s %d - %x %x %x %x\n", __func__,
		idx, pdt->bytes, pdt->offset, pdt->mask, pdt->val);

	return 0;
}

int of_camera_get_dev_layout(
	struct device *dev,
	struct device_node *np,
	struct cam_device_layout *pl)
{
	const void *prop;
	int val, err;

	prop = of_get_property(np, "guid", &val);
	if (prop == NULL) {
		dev_err(dev, "missing guid in DT %s\n", np->full_name);
		return -EEXIST;
	}
	pl->guid = be64_to_cpup(prop);

	err = of_property_read_u32(np, "position", &val);
	if (err) {
		dev_err(dev, "missing postion in DT %s\n", np->full_name);
		return -EEXIST;
	}
	pl->pos = val;

	err = of_property_read_u32(np, "devid", &val);
	if (err) {
		dev_err(dev, "missing devid in DT %s\n", np->full_name);
		return -EEXIST;
	}
	pl->dev_id = val;

	err = of_property_read_u32(np, "phandle", &val);
	if (err) {
		dev_err(dev, "missing phandle in DT %s\n", np->full_name);
		return -EEXIST;
	}
	pl->index = val;

	prop = of_get_property(np, "drivername", &val);
	if (prop) {
		if (val > sizeof(pl->alt_name))
			val = sizeof(pl->alt_name);
		memcpy(pl->alt_name, prop, val);
	}

	prop = of_get_property(np, "type", &val);
	if (prop == NULL) {
		dev_err(dev, "missing type in DT %s\n", np->full_name);
		return -EEXIST;
	}
	if (!strcmp(prop, "sensor"))
		pl->type = DEVICE_SENSOR;
	else if (!strcmp(prop, "focuser"))
		pl->type = DEVICE_FOCUSER;
	else if (!strcmp(prop, "flash"))
		pl->type = DEVICE_FLASH;
	else if (!strcmp(prop, "rom"))
		pl->type = DEVICE_ROM;
	else
		pl->type = DEVICE_OTHER;

	dev_dbg(dev, "%s - %s, %s %s\n", __func__,
		np->full_name, pl->name, pl->alt_name);
	return 0;
}

static struct device_node *of_camera_node_lookup(
	struct camera_board *cb, struct i2c_board_info *bi, int bus_num)
{
	struct camera_chip *chip = cb->chip;

	if (!cb->bi || !cb->bi->of_node || !chip)
		return NULL;

	if ((bus_num == cb->busnum) &&
		(bi->addr == cb->bi->addr) &&
		!strcmp(bi->type, chip->name))
		return cb->bi->of_node;

	return NULL;
}

int of_camera_find_node(
	struct device *dev, int bus_num, struct i2c_board_info *bi)
{
	struct camera_platform_data *pdata = cam_desc->pdata;
	struct camera_module *md;
	int i;

	dev_dbg(dev, "%s: %s %x %x @ %d\n", __func__,
		bi->type, bi->addr, bi->flags, bus_num);
	if (!pdata || !pdata->modules || !bi)
		return 0;

	if (bi->of_node)
		goto node_validation;

	md = pdata->modules;
	for (i = 0; i < pdata->mod_num; i++, md++) {
		bi->of_node = of_camera_node_lookup(&md->sensor, bi, bus_num);
		if (bi->of_node)
			break;

		bi->of_node = of_camera_node_lookup(&md->focuser, bi, bus_num);
		if (bi->of_node)
			break;

		bi->of_node = of_camera_node_lookup(&md->flash, bi, bus_num);
		if (bi->of_node)
			break;
	}

node_validation:
	/* only attach device of_node when keyword compatible presents */
	if (bi->of_node &&
		!of_get_property(bi->of_node, "use_of_node", NULL))
		bi->of_node = NULL;

	return 0;
}

static struct device_node *of_camera_get_brdinfo(
	struct platform_device *dev,
	struct device_node *np,
	struct i2c_board_info *bi,
	int *busnum)
{
	struct device_node *prfdev;
	phandle ph;
	const char *sname;
	int err;

	if (!np || !bi)
		return ERR_PTR(-EFAULT);

	dev_dbg(&dev->dev, "%s - %s\n", __func__, np->name);
	err = of_property_read_u32(np, "profile", &ph);
	if (err) {
		dev_err(&dev->dev, "%s no phandle in %s\n",
			__func__, np->name);
		return ERR_PTR(err);
	}
	prfdev = of_find_node_by_phandle(ph);
	if (!prfdev) {
		dev_err(&dev->dev, "%s can't find profile %d\n",
			__func__, ph);
		return ERR_PTR(-ENOENT);
	}
	sname = of_get_property(prfdev, "drivername", NULL);
	if (!sname) {
		dev_err(&dev->dev, "%s no devicename in %s\n",
			__func__, prfdev->name);
		return ERR_PTR(-ENOENT);
	}
	snprintf(bi->type, sizeof(bi->type), "%s", sname);
	if (of_property_read_u32(prfdev, "addr", (void *)&bi->addr)) {
		dev_err(&dev->dev, "%s missing addr in %s\n",
			__func__, prfdev->name);
		return ERR_PTR(-ENOENT);
	}
	if (busnum && of_property_read_u32(prfdev, "busnum", (void *)busnum)) {
		dev_err(&dev->dev, "%s missing busnum in %s\n",
			__func__, prfdev->name);
		return ERR_PTR(-ENOENT);
	}

	return prfdev;
}

static void *of_camera_add_new_chip(
	struct platform_device *dev, struct device_node *prf)
{
	struct virtual_device chp_info;
	struct camera_chip *c_chip;
	const void *prop;
	int i, len, err = 0;
	u32 val;

	memset(&chp_info, 0, sizeof(chp_info));
	prop = of_get_property(prf, "chipname", &len);
	if (!prop) {
		dev_err(&dev->dev, "%s No chip name on %s\n",
			__func__, prf->full_name);
		return ERR_PTR(-ENODEV);
	}
	if (len > sizeof(chp_info.name) - 1)
		len = sizeof(chp_info.name) - 1;
	strncpy(chp_info.name, prop, len);
	c_chip = camera_chip_chk(chp_info.name);
	if (c_chip) {
		dev_dbg(&dev->dev, "%s chip %s already exists.\n",
			__func__, chp_info.name);
		return c_chip;
	}

	prop = of_get_property(prf, "bustype", &len);
	if (!prop) {
		dev_err(&dev->dev, "%s No bustype on %s\n",
			__func__, prf->full_name);
		return ERR_PTR(-ENODEV);
	}
	if (!strncmp(prop, "i2c", len))
		chp_info.bus_type = CAMERA_DEVICE_TYPE_I2C;
	else {
		dev_err(&dev->dev, "%s bustype not supported.\n", __func__);
		return ERR_PTR(-ENODEV);
	}

	err = of_property_read_u32(prf, "datalen", &val);
	if (err) {
		dev_err(&dev->dev, "%s datalen invalid.\n", __func__);
		return ERR_PTR(err);
	}
	chp_info.regmap_cfg.addr_bits = val * 8; /* convert to bits */
	chp_info.regmap_cfg.val_bits = 8;
	chp_info.regmap_cfg.cache_type = REGCACHE_NONE;

	err = of_property_read_u32(prf, "gpios", &val);
	if (err) {
		dev_err(&dev->dev, "%s datalen invalid.\n", __func__);
		return ERR_PTR(err);
	}
	chp_info.gpio_num = val;

	len = of_property_count_strings(prf, "clocks");
	if (len >= 0)
		chp_info.clk_num = len;

	len = of_property_count_strings(prf, "regulators");
	if (len > 0) {
		chp_info.reg_num = len;
		for (i = 0; i < chp_info.reg_num; i++) {
			err = of_property_read_string_index(
				prf, "regulators", i, (const char **)&prop);
			if (err) {
				dev_err(&dev->dev, "%s get regulator %d error\n",
					__func__, i);
				return ERR_PTR(err);
			}
			len = strlen(prop);
			if (len > CAMERA_MAX_NAME_LENGTH - 1)
				len = CAMERA_MAX_NAME_LENGTH - 1;
			strncpy(chp_info.reg_names + i * CAMERA_MAX_NAME_LENGTH,
				prop, len);
		}
	}

	return virtual_chip_add(&dev->dev, &chp_info);
}

static int of_module_sanity_check(struct device *dev, struct camera_module *md)
{
	struct camera_board *brd;

	dev_dbg(dev, "%s\n", __func__);
	if (md->of_node == NULL) {
		dev_err(dev, "%s: MODULE ERROR %p\n", __func__, md->of_node);
		return -EINVAL;
	}

	brd = &md->sensor;
	if (brd->bi && (brd->of_node == NULL || brd->chip == NULL)) {
		dev_err(dev, "%s: sensor ERR %p %p %p\n", __func__,
			brd->of_node, brd->bi, brd->chip);
		return -EINVAL;
	}

	brd = &md->focuser;
	if (brd->bi && (brd->of_node == NULL || brd->chip == NULL)) {
		dev_err(dev, "%s: focuser ERR %p %p %p\n", __func__,
			brd->of_node, brd->bi, brd->chip);
		return -EINVAL;
	}

	brd = &md->flash;
	if (brd->bi && (brd->of_node == NULL || brd->chip == NULL)) {
		dev_err(dev, "%s: flash ERR %p %p %p\n", __func__,
			brd->of_node, brd->bi, brd->chip);
		return -EINVAL;
	}

	return 0;
}

static void *of_parse_dpd(struct device *dev, struct device_node *np, bool *enb)
{
	struct device_node *np_dpd, *sub_dpd;
	struct tegra_io_dpd *dpd_tbl = NULL;
	u32 data[3];
	int err = 0, num = 0, i = 0;

	if (!np)
		return NULL;

	np_dpd = of_find_node_by_name(np, "dpd");
	if (!np_dpd) {
		dev_err(dev, "%s can't find dpd table\n", __func__);
		return NULL;
	}

	of_property_read_u32(np_dpd, "num", &num);
	if (!num) {
		dev_dbg(dev, "%s: num = 0\n", __func__);
		return NULL;
	}

	dpd_tbl = devm_kzalloc(dev, sizeof(*dpd_tbl) * (num + 1), GFP_KERNEL);
	if (!dpd_tbl) {
		dev_err(dev, "%s: can't allocate memory\n", __func__);
		return NULL;
	}

	for_each_child_of_node(np_dpd, sub_dpd) {
		if (i >= num)
			break;
		dpd_tbl[i].name = sub_dpd->name;
		err = of_property_read_u32_array(
			sub_dpd, "reg", data, ARRAY_SIZE(data));
		if (err) {
			devm_kfree(dev, dpd_tbl);
			dev_err(dev, "%s: can't get reg of %s\n",
				__func__, sub_dpd->full_name);
			return NULL;
		}
		dpd_tbl[i].io_dpd_reg_index = data[0];
		dpd_tbl[i].io_dpd_bit = data[1];
		dpd_tbl[i].need_delay_dpd = data[2];
		dev_dbg(dev, "%s: dpd[%d] = %s %x %x %x\n", __func__, i,
			dpd_tbl[i].name,
			dpd_tbl[i].io_dpd_reg_index,
			dpd_tbl[i].io_dpd_bit,
			dpd_tbl[i].need_delay_dpd);
		i++;
	}
	if (enb)
		*enb = of_property_read_bool(np_dpd, "default-enable");

	dev_dbg(dev, "%s: dpd @ %s, num = %d default %d\n", __func__,
		np_dpd->full_name, num, enb ? *enb : 0);
	return dpd_tbl;
}

struct camera_platform_data *of_camera_create_pdata(
	struct platform_device *dev)
{
	struct device_node *modules, *submod, *subdev, *pdev;
	struct camera_platform_data *pd = NULL;
	struct i2c_board_info *bi = NULL;
	struct camera_data_blob *cb;
	struct camera_board *cbrd;
	struct camera_chip *chip;
	const char *sname;
	int num, nr;

	dev_dbg(&dev->dev, "%s\n", __func__);
	modules = of_find_node_by_name(dev->dev.of_node, "modules");
	if (!modules) {
		dev_info(&dev->dev, "module node not found\n");
		return ERR_PTR(-ENODEV);
	}

	num = of_camera_get_child_count(dev, modules);

	pd = devm_kzalloc(&dev->dev,
		sizeof(*pd) +
		(num + 1) * (sizeof(*pd->modules) +
		sizeof(struct i2c_board_info) * 3),
		GFP_KERNEL);
	if (!pd) {
		dev_err(&dev->dev, "%s allocate memory failed!\n", __func__);
		return ERR_PTR(-ENOMEM);
	}
	/* layout */
	pd->modules = (void *)&pd[1];
	pd->mod_num = num;
	bi = (void *)&pd->modules[num + 1];
	num = 0;
	for_each_child_of_node(modules, submod) {
		for_each_child_of_node(submod, subdev) {
			pdev = of_camera_get_brdinfo(dev, subdev, bi, &nr);
			if (IS_ERR(pdev))
				break;
			chip = of_camera_add_new_chip(dev, pdev);
			if (IS_ERR(chip)) {
				devm_kfree(&dev->dev, pd);
				return (void *)chip;
			}
			/* looking for matching pdata in lut */
			if (dev->dev.platform_data) {
				cb = dev->dev.platform_data;
				sname = of_get_property(
					subdev, "platformdata", NULL);
				/* searching for match signature */
				while (sname && cb && cb->name) {
					if (!strcmp(sname, cb->name)) {
						bi->platform_data = cb->data;
						break;
					}
					cb++;
				}
			}
			bi->of_node = pdev;

			sname = of_get_property(pdev, "chipname", NULL);
			if (!sname) {
				dev_err(&dev->dev, "%s no chipname in %s\n",
					__func__, pdev->name);
				break;
			}

			dev_dbg(&dev->dev,
				"    device %s, subdev %s @ %d-%04x %p, %s\n",
				subdev->name, bi->type, nr,
				bi->addr, bi->platform_data, sname);
			if (!strcmp(subdev->name, "sensor"))
				cbrd = &pd->modules[num].sensor;
			else if (!strcmp(subdev->name, "focuser"))
				cbrd = &pd->modules[num].focuser;
			else if (!strcmp(subdev->name, "flash"))
				cbrd = &pd->modules[num].flash;
			else
				cbrd = NULL;

			if (cbrd) {
				const u32 *ret = NULL;

				cbrd->bi = bi;
				cbrd->busnum = nr;
				cbrd->of_node = pdev;
				cbrd->chip = chip;
				ret = of_get_property(pdev,
					"skip_camera_install", NULL);
				if (ret != NULL)
					cbrd->flags |=
					 CAMERA_MODULE_FLAG_SKIP_INSTALLD;
			}
			bi++;
		}
		pd->modules[num].of_node = submod;
		if (!of_module_sanity_check(&dev->dev, &pd->modules[num]))
			num++;
		else
			memset(&pd->modules[num], 0, sizeof(pd->modules[0]));
	}

	/* dpd config */
	pd->dpd_tbl = of_parse_dpd(
		&dev->dev, dev->dev.of_node, &pd->enable_dpd);

	/* generic info */
	cam_alias = of_find_node_by_name(NULL, "camera-alias");
	of_property_read_u32(dev->dev.of_node, "configuration", &pd->cfg);
	pd->max_blob_size = of_camera_profile_statistic(
		dev, &pd->of_profiles, &pd->prof_num);
	pd->lut = dev->dev.platform_data;

	return pd;
}

int of_camera_init(struct camera_platform_info *info)
{
	cam_desc = info;
	return 0;
}

