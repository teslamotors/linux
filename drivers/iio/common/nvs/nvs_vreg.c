/* Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/device.h>
#include <linux/regulator/consumer.h>


int nvs_vreg_dis(struct device *dev, struct regulator_bulk_data *vreg)
{
	int ret = 0;

	if (vreg->ret && (vreg->consumer != NULL)) {
		ret = regulator_disable(vreg->consumer);
		if (ret) {
			dev_err(dev, "%s %s ERR\n", __func__, vreg->supply);
		} else {
			vreg->ret = 0;
			dev_dbg(dev, "%s %s\n", __func__, vreg->supply);
		}
	}
	return ret;
}

int nvs_vregs_disable(struct device *dev, struct regulator_bulk_data *vregs,
		      unsigned int vregs_n)
{
	unsigned int i;
	int ret = 0;

	for (i = vregs_n; i > 0; i--)
		ret |= nvs_vreg_dis(dev, &vregs[i - 1]);
	return ret;
}

int nvs_vreg_en(struct device *dev, struct regulator_bulk_data *vreg)
{
	int ret = 0;

	if (vreg->consumer != NULL && !vreg->ret) {
		ret = regulator_enable(vreg->consumer);
		if (ret) {
			dev_err(dev, "%s %s ERR\n", __func__, vreg->supply);
		} else {
			vreg->ret = 1;
			dev_dbg(dev, "%s %s\n", __func__, vreg->supply);
			ret = 1; /* flag regulator state change */
		}
	}
	return ret;
}

int nvs_vregs_enable(struct device *dev, struct regulator_bulk_data *vregs,
		     unsigned int vregs_n)
{
	unsigned int i;
	int ret = 0;

	if (vregs_n) {
		for (i = 0; i < vregs_n; i++)
			ret |= nvs_vreg_en(dev, &vregs[i]);
	}
	return ret;
}

void nvs_vregs_exit(struct device *dev, struct regulator_bulk_data *vregs,
		    unsigned int vregs_n)
{
	unsigned int i;

	for (i = 0; i < vregs_n; i++) {
		if (vregs[i].consumer != NULL) {
			devm_regulator_put(vregs[i].consumer);
			vregs[i].consumer = NULL;
			dev_dbg(dev, "%s %s\n", __func__, vregs[i].supply);
		}
	}
}

int nvs_vregs_init(struct device *dev, struct regulator_bulk_data *vregs,
		   unsigned int vregs_n, char **vregs_name)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < vregs_n; i++) {
		vregs[i].supply = vregs_name[i];
		vregs[i].ret = 0;
		vregs[i].consumer = devm_regulator_get(dev, vregs[i].supply);
		if (IS_ERR(vregs[i].consumer)) {
			ret |= PTR_ERR(vregs[i].consumer);
			dev_err(dev, "%s %s err=%d\n",
				__func__, vregs[i].supply, ret);
			vregs[i].consumer = NULL;
		} else {
			dev_dbg(dev, "%s %s\n", __func__, vregs[i].supply);
		}
	}
	return ret;
}

int nvs_vregs_sts(struct regulator_bulk_data *vregs, unsigned int vregs_n)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < vregs_n; i++) {
		if (vregs[i].consumer != NULL)
			break;
	}
	if (i < vregs_n) {
		/* ret == number of regulators on */
		for (i = 0; i < vregs_n; i++) {
			if (vregs[i].ret)
				ret++;
		}
	} else {
		/* no regulator support (can assume always on) */
		ret = -EINVAL;
	}
	return ret;
}

