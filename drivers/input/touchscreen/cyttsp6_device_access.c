/*
 * cyttsp6_device_access.c
 * Cypress TrueTouch(TM) Standard Product V4 Device Access Module.
 * Configuration and Test command/status user interface.
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

#define CY_MAX_CONFIG_BYTES    256
#define CY_CMD_INDEX             0
#define CY_NULL_CMD_INDEX        1
#define CY_NULL_CMD_MODE_INDEX   2
#define CY_NULL_CMD_SIZE_INDEX   3
#define CY_NULL_CMD_SIZEL_INDEX  2
#define CY_NULL_CMD_SIZEH_INDEX  3

#define CYTTSP6_DEVICE_ACCESS_NAME "cyttsp6_device_access"

#define CYTTSP6_INPUT_ELEM_SZ (sizeof("0xHH") + 1)
#define CYTTSP6_TCH_PARAM_SIZE_BLK_SZ 128

#define CAP_TEST_TYPE_MUTUAL		0
#define CAP_TEST_TYPE_SELF		1

#define CAP_TARGET_PANEL		0
#define CAP_TARGET_BUTTON		1


#define STATUS_SUCCESS	0
#define STATUS_FAIL	-1

#define MAX_SYSFS_BUF_LEN	650

enum cyttsp6_scan_data_type {
	CY_MUT_RAW,
	CY_MUT_BASE,
	CY_MUT_DIFF,
	CY_SELF_RAW,
	CY_SELF_BASE,
	CY_SELF_DIFF,
	CY_BAL_RAW,
	CY_BAL_BASE,
	CY_BAL_DIFF,
};

struct heatmap_param {
	bool scan_start;
	enum cyttsp6_scan_data_type data_type; /* raw, base, diff */
	int num_element;
	int input_offset;
};

struct cyttsp6_device_access_data {
	struct device *dev;
	struct cyttsp6_sysinfo *si;
	struct cyttsp6_test_mode_params test;
	struct mutex sysfs_lock;
	uint32_t ic_grpnum;
	uint32_t ic_grpoffset;
	bool own_exclusive;
	bool sysfs_nodes_created;
	struct kobject mfg_test;
	wait_queue_head_t wait_q;
	struct heatmap_param heatmap;
	u8 panel_scan_data_id;
	u8 cap_test_type;
	u8 cap_target;
	u8 get_idac_data_id;
	u8 calibrate_sensing_mode;
	u8 calibrate_initialize_baselines;
	u8 baseline_sensing_mode;
	u8 ic_buf[CY_MAX_PRBUF_SIZE];
	u8 return_buf[CY_MAX_PRBUF_SIZE];
};

static struct cyttsp6_core_commands *cmd;

static inline struct cyttsp6_device_access_data *cyttsp6_get_device_access_data(
		struct device *dev)
{
	return cyttsp6_get_dynamic_data(dev, CY_MODULE_DEVICE_ACCESS);
}

struct cyttsp6_attribute {
	struct attribute attr;
	ssize_t (*show)(struct device *dev, struct cyttsp6_attribute *attr,
			char *buf);
	ssize_t (*store)(struct device *dev, struct cyttsp6_attribute *attr,
			const char *buf, size_t count);
};

#define CY_ATTR(_name, _mode, _show, _store)  \
	struct cyttsp6_attribute cy_attr_##_name = \
		__ATTR(_name, _mode, _show, _store)

static inline int cyttsp6_create_file(struct device *dev,
		const struct cyttsp6_attribute *attr)
{
	struct cyttsp6_device_access_data *dad;
	int error = 0;

	if (dev) {
		dad = cyttsp6_get_device_access_data(dev);
		error = sysfs_create_file(&dad->mfg_test, &attr->attr);
	}

	return error;
}

static inline void cyttsp6_remove_file(struct device *dev,
		const struct cyttsp6_attribute *attr)
{
	struct cyttsp6_device_access_data *dad;

	if (dev) {
		dad = cyttsp6_get_device_access_data(dev);
		sysfs_remove_file(&dad->mfg_test, &attr->attr);
	}
}

static ssize_t cyttsp6_attr_show(struct kobject *kobj, struct attribute *attr,
		char *buf)
{
	struct cyttsp6_attribute *cyttsp6_attr = container_of(attr,
			struct cyttsp6_attribute, attr);
	struct device *dev = container_of(kobj->parent, struct device, kobj);
	ssize_t ret = -EIO;

	if (cyttsp6_attr->show)
		ret = cyttsp6_attr->show(dev, cyttsp6_attr, buf);

	return ret;
}

static ssize_t cyttsp6_attr_store(struct kobject *kobj, struct attribute *attr,
		const char *buf, size_t count)
{
	struct cyttsp6_attribute *cyttsp6_attr = container_of(attr,
			struct cyttsp6_attribute, attr);
	struct device *dev = container_of(kobj->parent, struct device, kobj);
	ssize_t ret = -EIO;

	if (cyttsp6_attr->store)
		ret = cyttsp6_attr->store(dev, cyttsp6_attr, buf, count);

	return ret;
}

static const struct sysfs_ops cyttsp6_sysfs_ops = {
	.show = cyttsp6_attr_show,
	.store = cyttsp6_attr_store,
};

static struct kobj_type cyttsp6_ktype = {
	.sysfs_ops = &cyttsp6_sysfs_ops,
};

/*
 * Show function prototype.
 * Returns response length or Linux error code on error.
 */
typedef int (*cyttsp6_show_function)(struct device *dev, u8 *ic_buf,
		size_t length);

/*
 * Store function prototype.
 * Returns Linux error code on error.
 */
typedef int (*cyttsp6_store_function)(struct device *dev, u8 *ic_buf,
		size_t length);

/*
 * grpdata show function to be used by
 * reserved and not implemented ic group numbers.
 */
static int cyttsp6_grpdata_show_void(struct device *dev, u8 *ic_buf,
		size_t length)
{
	return -ENOSYS;
}

/*
 * grpdata store function to be used by
 * reserved and not implemented ic group numbers.
 */
static int cyttsp6_grpdata_store_void(struct device *dev, u8 *ic_buf,
		size_t length)
{
	return -ENOSYS;
}

/*
 * SysFs group number entry show function.
 */
static ssize_t cyttsp6_ic_grpnum_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int val = 0;

	mutex_lock(&dad->sysfs_lock);
	val = dad->ic_grpnum;
	mutex_unlock(&dad->sysfs_lock);

	return scnprintf(buf, CY_MAX_PRBUF_SIZE, "Current Group: %d\n", val);
}

/*
 * SysFs group number entry store function.
 */
static ssize_t cyttsp6_ic_grpnum_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	unsigned long value;
	int rc;

	rc = kstrtoul(buf, 10, &value);
	if (rc < 0) {
		dev_err(dev, "%s: Invalid value\n", __func__);
		return size;
	}

	if (value >= CY_IC_GRPNUM_NUM) {
		dev_err(dev, "%s: Group %lu does not exist.\n",
				__func__, value);
		return size;
	}

	if (value > 0xFF)
		value = 0xFF;

	mutex_lock(&dad->sysfs_lock);
	/*
	 * Block grpnum change when own_exclusive flag is set
	 * which means the current grpnum implementation requires
	 * running exclusively on some consecutive grpdata operations
	 */
	if (dad->own_exclusive) {
		mutex_unlock(&dad->sysfs_lock);
		dev_err(dev, "%s: own_exclusive\n", __func__);
		return -EBUSY;
	}
	dad->ic_grpnum = (int) value;
	mutex_unlock(&dad->sysfs_lock);

	dev_vdbg(dev, "%s: ic_grpnum=%d, return size=%d\n",
			__func__, (int)value, (int)size);

	return size;
}

static DEVICE_ATTR(ic_grpnum, S_IRUSR | S_IWUSR,
		   cyttsp6_ic_grpnum_show, cyttsp6_ic_grpnum_store);

/*
 * SysFs group offset entry show function.
 */
static ssize_t cyttsp6_ic_grpoffset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int val = 0;

	mutex_lock(&dad->sysfs_lock);
	val = dad->ic_grpoffset;
	mutex_unlock(&dad->sysfs_lock);

	return scnprintf(buf, CY_MAX_PRBUF_SIZE, "Current Offset: %d\n", val);
}

/*
 * SysFs group offset entry store function.
 */
static ssize_t cyttsp6_ic_grpoffset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	unsigned long value;
	int ret;

	ret = kstrtoul(buf, 10, &value);
	if (ret < 0) {
		dev_err(dev, "%s: Invalid value\n", __func__);
		return size;
	}

	if (value > 0xFFFF)
		value = 0xFFFF;

	mutex_lock(&dad->sysfs_lock);
	dad->ic_grpoffset = (int)value;
	mutex_unlock(&dad->sysfs_lock);

	dev_vdbg(dev, "%s: ic_grpoffset=%d, return size=%d\n", __func__,
			(int)value, (int)size);

	return size;
}

static DEVICE_ATTR(ic_grpoffset, S_IRUSR | S_IWUSR,
		   cyttsp6_ic_grpoffset_show, cyttsp6_ic_grpoffset_store);

static int cyttsp6_grpdata_show_registers_(struct device *dev, u8 *ic_buf,
		size_t length, int num_read, int offset, int mode)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);

	return cmd->read(dev, mode, offset + dad->ic_grpoffset, ic_buf,
		num_read);
}

/*
 * Prints part of communication registers.
 */
static int cyttsp6_grpdata_show_registers(struct device *dev, u8 *ic_buf,
		size_t length, int num_read, int offset, int mode)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int rc;
	int ret = 0;

	if (dad->ic_grpoffset >= num_read)
		return -EINVAL;

	num_read -= dad->ic_grpoffset;

	if (length < num_read) {
		dev_err(dev, "%s: not sufficient buffer req_bug_len=%d, length=%zu\n",
				__func__, num_read, length);
		return -EINVAL;
	}

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		ret = rc;
		goto err_put;
	}

	rc = cyttsp6_grpdata_show_registers_(dev, ic_buf, length, num_read,
			offset, mode);
	if (rc < 0)
		ret = rc;

	rc = cmd->release_exclusive(dev);
	if (rc < 0) {
		dev_err(dev, "%s: Error on release exclusive r=%d\n",
				__func__, rc);
		if (!ret)
			ret = rc;
	}

err_put:
	pm_runtime_put(dev);

	if (ret < 0)
		return ret;

	return num_read;
}

/*
 * SysFs grpdata show function implementation of group 1.
 * Prints status register contents of Operational mode registers.
 */
static int cyttsp6_grpdata_show_operational_regs(struct device *dev, u8 *ic_buf,
		size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int num_read = dad->si->si_ofs.rep_ofs - dad->si->si_ofs.cmd_ofs;
	int i;

	if (dad->ic_grpoffset >= num_read) {
		dev_err(dev, "%s: ic_grpoffset bigger than command registers, cmd_registers=%d\n",
			__func__, num_read);
		return -EINVAL;
	}

	num_read -= dad->ic_grpoffset;

	if (length < num_read) {
		dev_err(dev, "%s: not sufficient buffer req_bug_len=%d, length=%zu\n",
			__func__, num_read, length);
		return -EINVAL;
	}

	if (dad->ic_grpoffset + num_read > CY_MAX_PRBUF_SIZE) {
		dev_err(dev, "%s: not sufficient source buffer req_bug_len=%d, length=%d\n",
			__func__, dad->ic_grpoffset + num_read,
			CY_MAX_PRBUF_SIZE);
		return -EINVAL;
	}


	/* cmd result already put into dad->return_buf */
	for (i = 0; i < num_read; i++)
		ic_buf[i] = dad->return_buf[dad->ic_grpoffset + i];

	return num_read;
}

/*
 * SysFs grpdata show function implementation of group 2.
 * Prints current contents of the touch registers (full set).
 */
static int cyttsp6_grpdata_show_touch_regs(struct device *dev, u8 *ic_buf,
		size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int num_read = dad->si->si_ofs.rep_sz;
	int offset = dad->si->si_ofs.rep_ofs;

	return cyttsp6_grpdata_show_registers(dev, ic_buf, length, num_read,
			offset, CY_MODE_OPERATIONAL);
}

static int cyttsp6_grpdata_show_sysinfo_(struct device *dev, u8 *ic_buf,
		size_t length, int num_read, int offset)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int rc;
	int ret = 0;

	rc = cmd->request_set_mode(dev, CY_MODE_SYSINFO);
	if (rc < 0)
		return rc;

	rc = cmd->read(dev, CY_MODE_SYSINFO, offset + dad->ic_grpoffset,
			ic_buf, num_read);
	if (rc < 0) {
		dev_err(dev, "%s: Fail read cmd regs r=%d\n",
				__func__, rc);
		ret = rc;
	}

	rc = cmd->request_set_mode(dev, CY_MODE_OPERATIONAL);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request set mode 2 r=%d\n",
				__func__, rc);
		if (!ret)
			ret = rc;
	}

	return ret;
}

/*
 * Prints some content of the system information
 */
static int cyttsp6_grpdata_show_sysinfo(struct device *dev, u8 *ic_buf,
		size_t length, int num_read, int offset)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int rc;
	int ret = 0;

	if (dad->ic_grpoffset >= num_read)
		return -EINVAL;

	num_read -= dad->ic_grpoffset;

	if (length < num_read) {
		dev_err(dev, "%s: not sufficient buffer req_bug_len=%d, length=%zu\n",
				__func__, num_read, length);
		return -EINVAL;
	}

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		ret = rc;
		goto err_put;
	}

	rc = cyttsp6_grpdata_show_sysinfo_(dev, ic_buf, length, num_read,
			offset);
	if (rc < 0)
		ret = rc;

	rc = cmd->release_exclusive(dev);
	if (rc < 0) {
		dev_err(dev, "%s: Error on release exclusive r=%d\n",
				__func__, rc);
		if (!ret)
			ret = rc;
	}

err_put:
	pm_runtime_put(dev);

	if (ret < 0)
		return ret;

	return num_read;
}

/*
 * SysFs grpdata show function implementation of group 3.
 * Prints content of the system information DATA record.
 */
static int cyttsp6_grpdata_show_sysinfo_data_rec(struct device *dev, u8 *ic_buf,
		size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int num_read = dad->si->si_ofs.cydata_size;
	int offset = dad->si->si_ofs.cydata_ofs;

	return cyttsp6_grpdata_show_sysinfo(dev, ic_buf, length, num_read,
			offset);
}

/*
 * SysFs grpdata show function implementation of group 4.
 * Prints content of the system information TEST record.
 */
static int cyttsp6_grpdata_show_sysinfo_test_rec(struct device *dev, u8 *ic_buf,
		size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int num_read = dad->si->si_ofs.test_size;
	int offset = dad->si->si_ofs.test_ofs;

	return cyttsp6_grpdata_show_sysinfo(dev, ic_buf, length, num_read,
			offset);
}

/*
 * SysFs grpdata show function implementation of group 5.
 * Prints content of the system information PANEL data.
 */
static int cyttsp6_grpdata_show_sysinfo_panel(struct device *dev, u8 *ic_buf,
		size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int num_read = dad->si->si_ofs.pcfg_size;
	int offset = dad->si->si_ofs.pcfg_ofs;

	return cyttsp6_grpdata_show_sysinfo(dev, ic_buf, length, num_read,
			offset);
}

static int cyttsp6_grpdata_show_touch_params__(struct device *dev, u8 *ic_buf,
		size_t length, int *size)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	u8 cmd_buf[CY_CMD_CAT_READ_CFG_BLK_CMD_SZ];
	int return_buf_size = CY_CMD_CAT_READ_CFG_BLK_RET_SZ;
	u16 config_row_size;
	int row_offset;
	int offset_in_single_row = 0;
	int rc;
	int i, j;

	rc = cmd->request_config_row_size(dev, &config_row_size);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request config row size r=%d\n",
				__func__, rc);
		return rc;
	}

	/* Perform buffer size check since we have just acquired row size */
	return_buf_size += config_row_size;

	if (length < return_buf_size) {
		dev_err(dev, "%s: not sufficient buffer req_buf_len=%d, length=%zu\n",
				__func__, return_buf_size, length);
		return -EINVAL;
	}

	row_offset = dad->ic_grpoffset / config_row_size;

	cmd_buf[0] = CY_CMD_CAT_READ_CFG_BLK;
	cmd_buf[1] = HI_BYTE(row_offset);
	cmd_buf[2] = LO_BYTE(row_offset);
	cmd_buf[3] = HI_BYTE(config_row_size);
	cmd_buf[4] = LO_BYTE(config_row_size);
	cmd_buf[5] = CY_TCH_PARM_EBID;
	rc = cmd->request_exec_cmd(dev, CY_MODE_CAT,
			cmd_buf, CY_CMD_CAT_READ_CFG_BLK_CMD_SZ,
			ic_buf, return_buf_size,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc < 0)
		return rc;

	offset_in_single_row = dad->ic_grpoffset % config_row_size;

	/* Remove Header data from return buffer */
	for (i = 0, j = CY_CMD_CAT_READ_CFG_BLK_RET_HDR_SZ
				+ offset_in_single_row;
			i < (config_row_size - offset_in_single_row);
			i++, j++)
		ic_buf[i] = ic_buf[j];

	*size = config_row_size - offset_in_single_row;

	return rc;
}

static int cyttsp6_grpdata_show_touch_params_(struct device *dev, u8 *ic_buf,
		size_t length, int *size)
{
	int rc;
	int ret = 0;

	rc = cmd->request_set_mode(dev, CY_MODE_CAT);
	if (rc < 0)
		return rc;

	rc = cyttsp6_grpdata_show_touch_params__(dev, ic_buf, length, size);
	if (rc < 0)
		ret = rc;

	rc = cmd->request_set_mode(dev, CY_MODE_OPERATIONAL);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request set mode r=%d\n",
				__func__, rc);
		if (!ret)
			ret = rc;
	}

	return ret;
}

/*
 * SysFs grpdata show function implementation of group 6.
 * Prints contents of the touch parameters a row at a time.
 */
static int cyttsp6_grpdata_show_touch_params(struct device *dev, u8 *ic_buf,
		size_t length)
{
	int rc;
	int ret = 0;
	int size = 0;

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request exclusive r=%d\n",
				__func__, rc);
		ret = rc;
		goto err_put;
	}

	rc = cyttsp6_grpdata_show_touch_params_(dev, ic_buf, length, &size);
	if (rc < 0)
		ret = rc;

	rc = cmd->release_exclusive(dev);
	if (rc < 0) {
		dev_err(dev, "%s: Error on release exclusive r=%d\n",
				__func__, rc);
		if (!ret)
			ret = rc;
	}

	if (!ret)
		ret = size;

err_put:
	pm_runtime_put(dev);

	return ret;
}

/*
 * SysFs grpdata show function implementation of group 7.
 * Prints contents of the touch parameters sizes.
 */
static int cyttsp6_grpdata_show_touch_params_sizes(struct device *dev,
		u8 *ic_buf, size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	struct cyttsp6_platform_data *_pdata = dev_get_platdata(dev);
	struct cyttsp6_core_platform_data *pdata;
	int max_size;
	int block_start;
	int block_end;
	int num_read;

	if (!_pdata || !_pdata->core_pdata)
		return -ENODEV;
	pdata = _pdata->core_pdata;

	if (pdata->sett[CY_IC_GRPNUM_TCH_PARM_SIZE] == NULL) {
		dev_err(dev, "%s: Missing platform data Touch Parameters Sizes table\n",
				__func__);
		return -EINVAL;
	}

	if (pdata->sett[CY_IC_GRPNUM_TCH_PARM_SIZE]->data == NULL) {
		dev_err(dev, "%s: Missing platform data Touch Parameters Sizes table data\n",
				__func__);
		return -EINVAL;
	}

	max_size = pdata->sett[CY_IC_GRPNUM_TCH_PARM_SIZE]->size;
	max_size *= sizeof(uint16_t);
	if (dad->ic_grpoffset >= max_size)
		return -EINVAL;

	block_start = (dad->ic_grpoffset / CYTTSP6_TCH_PARAM_SIZE_BLK_SZ)
			* CYTTSP6_TCH_PARAM_SIZE_BLK_SZ;
	block_end = CYTTSP6_TCH_PARAM_SIZE_BLK_SZ + block_start;
	if (block_end > max_size)
		block_end = max_size;
	num_read = block_end - dad->ic_grpoffset;
	if (length < num_read) {
		dev_err(dev, "%s: not sufficient buffer %s=%d, %s=%zu\n",
				__func__, "req_buf_len", num_read, "length",
				length);
		return -EINVAL;
	}

	memcpy(ic_buf, (u8 *)pdata->sett[CY_IC_GRPNUM_TCH_PARM_SIZE]->data
			+ dad->ic_grpoffset, num_read);

	return num_read;
}

/*
 * SysFs grpdata show function implementation of group 10.
 * Prints content of the system information Operational Configuration data.
 */
static int cyttsp6_grpdata_show_sysinfo_opcfg(struct device *dev, u8 *ic_buf,
		size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int num_read = dad->si->si_ofs.opcfg_size;
	int offset = dad->si->si_ofs.opcfg_ofs;

	return cyttsp6_grpdata_show_sysinfo(dev, ic_buf, length, num_read,
			offset);
}

/*
 * SysFs grpdata show function implementation of group 11.
 * Prints content of the system information Design data.
 */
static int cyttsp6_grpdata_show_sysinfo_design(struct device *dev, u8 *ic_buf,
		size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int num_read = dad->si->si_ofs.ddata_size;
	int offset = dad->si->si_ofs.ddata_ofs;

	return cyttsp6_grpdata_show_sysinfo(dev, ic_buf, length, num_read,
			offset);
}

/*
 * SysFs grpdata show function implementation of group 12.
 * Prints content of the system information Manufacturing data.
 */
static int cyttsp6_grpdata_show_sysinfo_manufacturing(struct device *dev,
		u8 *ic_buf, size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int num_read = dad->si->si_ofs.mdata_size;
	int offset = dad->si->si_ofs.mdata_ofs;

	return cyttsp6_grpdata_show_sysinfo(dev, ic_buf, length, num_read,
			offset);
}

/*
 * SysFs grpdata show function implementation of group 13.
 * Prints status register contents of Configuration and Test registers.
 */
static int cyttsp6_grpdata_show_test_regs(struct device *dev, u8 *ic_buf,
		size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	u8 mode;
	int rc = 0;
	int num_read = 0;
	int i;

	dev_vdbg(dev, "%s: test.cur_cmd=%d test.cur_mode=%d\n",
			__func__, dad->test.cur_cmd, dad->test.cur_mode);

	if (dad->test.cur_cmd == CY_CMD_CAT_NULL) {
		num_read = 1;
		if (length < num_read) {
			dev_err(dev, "%s: not sufficient buffer %s=%d, %s=%zu\n",
					__func__, "req_buf_len", num_read,
					"length", length);
			return -EINVAL;
		}

		dev_vdbg(dev, "%s: GRP=TEST_REGS: NULL CMD: host_mode=%02X\n",
				__func__, ic_buf[0]);
		pm_runtime_get_sync(dev);
		rc = cmd->read(dev, dad->test.cur_mode,
				CY_REG_BASE, &mode, sizeof(mode));
		pm_runtime_put(dev);
		if (rc < 0) {
			ic_buf[0] = 0xFF;
			dev_err(dev, "%s: failed to read host mode r=%d\n",
					__func__, rc);
		} else {
			ic_buf[0] = mode;
		}
	} else if (dad->test.cur_mode == CY_MODE_CAT) {
		num_read = dad->test.cur_status_size;
		if (length < num_read) {
			dev_err(dev, "%s: not sufficient buffer %s=%d, %s=%zu\n",
					__func__, "req_buf_len", num_read,
					"length", length);
			return -EINVAL;
		}
		if (dad->ic_grpoffset + num_read > CY_MAX_PRBUF_SIZE) {
			dev_err(dev,
				"%s: not sufficient source buffer req_bug_len=%d, length=%d\n",
				__func__, dad->ic_grpoffset + num_read,
				CY_MAX_PRBUF_SIZE);
			return -EINVAL;
		}

		dev_vdbg(dev, "%s: GRP=TEST_REGS: num_rd=%d at ofs=%zu + grpofs=%d\n",
				__func__, num_read, dad->si->si_ofs.cmd_ofs,
				dad->ic_grpoffset);

		/* compensate for the command byte */
		num_read++;
		/* cmd result already put into dad->return_buf */
		for (i = 0; i < num_read; i++)
			ic_buf[i] = dad->return_buf[dad->ic_grpoffset + i];
	} else {
		dev_err(dev, "%s: Not in Config/Test mode\n", __func__);
	}

	return num_read;
}

/*
 * SysFs grpdata show function implementation of group 14.
 * Prints CapSense button keycodes.
 */
static int cyttsp6_grpdata_show_btn_keycodes(struct device *dev, u8 *ic_buf,
		size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	struct cyttsp6_btn *btn = dad->si->btn;
	int num_btns = dad->si->si_ofs.num_btns - dad->ic_grpoffset;
	int n;

	if (num_btns <= 0 || btn == NULL || length < num_btns)
		return -EINVAL;

	for (n = 0; n < num_btns; n++)
		ic_buf[n] = (u8) btn[dad->ic_grpoffset + n].key_code;

	return n;
}

/*
 * SysFs grpdata show function implementation of group 15.
 * Prints status register contents of Configuration and Test registers.
 */
static int cyttsp6_grpdata_show_tthe_test_regs(struct device *dev, u8 *ic_buf,
		size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int rc = 0;
	int num_read = 0;

	dev_vdbg(dev, "%s: test.cur_cmd=%d test.cur_mode=%d\n",
			__func__, dad->test.cur_cmd, dad->test.cur_mode);

	if (dad->test.cur_cmd == CY_CMD_CAT_NULL) {
		num_read = dad->test.cur_status_size;
		if (length < num_read) {
			dev_err(dev, "%s: not sufficient buffer %s=%d, %s=%zu\n",
					__func__, "req_buf_len", num_read,
					"length", length);
			return -EINVAL;
		}

		dev_vdbg(dev, "%s: GRP=TEST_REGS: NULL CMD: host_mode=%02X\n",
				__func__, dad->test.cur_mode);
		pm_runtime_get_sync(dev);
		rc = cmd->read(dev, dad->test.cur_mode,
				CY_REG_BASE, ic_buf, num_read);
		pm_runtime_put(dev);
		if (rc < 0) {
			ic_buf[0] = 0xFF;
			dev_err(dev, "%s: failed to read host mode r=%d\n",
					__func__, rc);
		}
	} else if (dad->test.cur_mode == CY_MODE_CAT
			|| dad->test.cur_mode == CY_MODE_SYSINFO) {
		num_read = dad->test.cur_status_size;
		if (length < num_read) {
			dev_err(dev, "%s: not sufficient buffer %s=%d, %s=%zu\n",
					__func__, "req_buf_len", num_read,
					"length", length);
			return -EINVAL;
		}
		dev_vdbg(dev, "%s: GRP=TEST_REGS: num_rd=%d at ofs=%zu + grpofs=%d\n",
				__func__, num_read, dad->si->si_ofs.cmd_ofs,
				dad->ic_grpoffset);
		pm_runtime_get_sync(dev);
		rc = cmd->read(dev, dad->test.cur_mode,
				CY_REG_BASE, ic_buf, num_read);
		pm_runtime_put(dev);
		if (rc < 0)
			return rc;
	} else {
		dev_err(dev, "%s: In unsupported mode\n", __func__);
	}

	return num_read;
}

static cyttsp6_show_function
		cyttsp6_grpdata_show_functions[CY_IC_GRPNUM_NUM] = {
	[CY_IC_GRPNUM_RESERVED] = cyttsp6_grpdata_show_void,
	[CY_IC_GRPNUM_CMD_REGS] = cyttsp6_grpdata_show_operational_regs,
	[CY_IC_GRPNUM_TCH_REP] = cyttsp6_grpdata_show_touch_regs,
	[CY_IC_GRPNUM_DATA_REC] = cyttsp6_grpdata_show_sysinfo_data_rec,
	[CY_IC_GRPNUM_TEST_REC] = cyttsp6_grpdata_show_sysinfo_test_rec,
	[CY_IC_GRPNUM_PCFG_REC] = cyttsp6_grpdata_show_sysinfo_panel,
	[CY_IC_GRPNUM_TCH_PARM_VAL] = cyttsp6_grpdata_show_touch_params,
	[CY_IC_GRPNUM_TCH_PARM_SIZE] = cyttsp6_grpdata_show_touch_params_sizes,
	[CY_IC_GRPNUM_RESERVED1] = cyttsp6_grpdata_show_void,
	[CY_IC_GRPNUM_RESERVED2] = cyttsp6_grpdata_show_void,
	[CY_IC_GRPNUM_OPCFG_REC] = cyttsp6_grpdata_show_sysinfo_opcfg,
	[CY_IC_GRPNUM_DDATA_REC] = cyttsp6_grpdata_show_sysinfo_design,
	[CY_IC_GRPNUM_MDATA_REC] = cyttsp6_grpdata_show_sysinfo_manufacturing,
	[CY_IC_GRPNUM_TEST_REGS] = cyttsp6_grpdata_show_test_regs,
	[CY_IC_GRPNUM_BTN_KEYS] = cyttsp6_grpdata_show_btn_keycodes,
	[CY_IC_GRPNUM_TTHE_REGS] = cyttsp6_grpdata_show_tthe_test_regs,
};

static ssize_t cyttsp6_ic_grpdata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int i;
	ssize_t num_read;
	int index;

	mutex_lock(&dad->sysfs_lock);
	dev_vdbg(dev, "%s: grpnum=%d grpoffset=%u\n",
			__func__, dad->ic_grpnum, dad->ic_grpoffset);

	index = scnprintf(buf, CY_MAX_PRBUF_SIZE,
			"Group %d, Offset %u:\n", dad->ic_grpnum,
			dad->ic_grpoffset);

	num_read = cyttsp6_grpdata_show_functions[dad->ic_grpnum] (dev,
			dad->ic_buf, CY_MAX_PRBUF_SIZE);
	if (num_read < 0) {
		index = num_read;
		if (num_read == -ENOSYS) {
			dev_err(dev, "%s: Group %d is not implemented.\n",
				__func__, dad->ic_grpnum);
			goto cyttsp6_ic_grpdata_show_error;
		}
		dev_err(dev, "%s: Cannot read Group %d Data.\n",
				__func__, dad->ic_grpnum);
		goto cyttsp6_ic_grpdata_show_error;
	}

	for (i = 0; i < num_read; i++) {
		index += scnprintf(buf + index, CY_MAX_PRBUF_SIZE - index,
				"0x%02X\n", dad->ic_buf[i]);
	}

	index += scnprintf(buf + index, CY_MAX_PRBUF_SIZE - index,
			"(%zd bytes)\n", num_read);

cyttsp6_ic_grpdata_show_error:
	mutex_unlock(&dad->sysfs_lock);

	return index;
}

static int _cyttsp6_cat_cmd_handshake(struct cyttsp6_device_access_data *dad)
{
	struct device *dev = dad->dev;
	u8 mode;
	int rc;

	rc = cmd->read(dev, CY_MODE_CAT, CY_REG_BASE, &mode, sizeof(mode));
	if (rc < 0) {
		dev_err(dev, "%s: Fail read host mode r=%d\n", __func__, rc);
		return rc;
	}

	rc = cmd->request_handshake(dev, mode);
	if (rc < 0)
		dev_err(dev, "%s: Fail cmd handshake r=%d\n", __func__, rc);

	return rc;
}

static int _cyttsp6_cmd_toggle_lowpower(struct cyttsp6_device_access_data *dad)
{
	struct device *dev = dad->dev;
	u8 mode;
	int rc;

	pm_runtime_get_sync(dev);

	rc = cmd->read(dev, dad->test.cur_mode,
			CY_REG_BASE, &mode, sizeof(mode));
	if (rc < 0) {
		dev_err(dev, "%s: Fail read host mode r=%d\n", __func__, rc);
		goto err_put;
	}

	rc = cmd->request_toggle_lowpower(dev, mode);
	if (rc < 0)
		dev_err(dev, "%s: Fail cmd handshake r=%d\n", __func__, rc);

err_put:
	pm_runtime_put(dev);

	return rc;
}

static int cyttsp6_test_cmd_mode(struct cyttsp6_device_access_data *dad,
		u8 *ic_buf, size_t length)
{
	struct device *dev = dad->dev;
	int rc = -ENOSYS;
	u8 mode;

	if (length < CY_NULL_CMD_MODE_INDEX + 1)  {
		dev_err(dev, "%s: %s length=%zu\n", __func__,
				"Buffer length is not valid", length);
		return -EINVAL;
	}
	mode = ic_buf[CY_NULL_CMD_MODE_INDEX];

	if (mode == CY_HST_CAT) {
		pm_runtime_get_sync(dev);
		rc = cmd->request_exclusive(dev,
				CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
		if (rc < 0) {
			dev_err(dev, "%s: Fail rqst exclusive r=%d\n",
					__func__, rc);
			pm_runtime_put(dev);
			goto cyttsp6_test_cmd_mode_exit;
		}
		rc = cmd->request_set_mode(dev, CY_MODE_CAT);
		if (rc < 0) {
			dev_err(dev, "%s: Fail rqst set mode=%02X r=%d\n",
					__func__, mode, rc);
			rc = cmd->release_exclusive(dev);
			if (rc < 0)
				dev_err(dev, "%s: %s r=%d\n", __func__,
						"Fail release exclusive", rc);
			pm_runtime_put(dev);
			goto cyttsp6_test_cmd_mode_exit;
		}
		dad->test.cur_mode = CY_MODE_CAT;
		dad->own_exclusive = true;
		dev_vdbg(dev, "%s: %s=%d %s=%02X %s=%d(CaT)\n", __func__,
				"own_exclusive", dad->own_exclusive == true,
				"mode", mode, "test.cur_mode",
				dad->test.cur_mode);
	} else if (mode == CY_HST_OPERATE) {
		if (dad->own_exclusive) {
			rc = cmd->request_set_mode(dev,
					CY_MODE_OPERATIONAL);
			if (rc < 0)
				dev_err(dev, "%s: %s=%02X r=%d\n", __func__,
						"Fail rqst set mode", mode, rc);
				/* continue anyway */

			rc = cmd->release_exclusive(dev);
			if (rc < 0) {
				dev_err(dev, "%s: %s r=%d\n", __func__,
						"Fail release exclusive", rc);
				/* continue anyway */
				rc = 0;
			}
			dad->test.cur_mode = CY_MODE_OPERATIONAL;
			dad->own_exclusive = false;
			pm_runtime_put(dev);
			dev_vdbg(dev, "%s: %s=%d %s=%02X %s=%d(Operate)\n",
					__func__, "own_exclusive",
					dad->own_exclusive == true,
					"mode", mode,
					"test.cur_mode", dad->test.cur_mode);
		} else
			dev_vdbg(dev, "%s: %s mode=%02X(Operate)\n", __func__,
					"do not own exclusive; cannot switch",
					mode);
	} else
		dev_vdbg(dev, "%s: unsupported mode switch=%02X\n",
				__func__, mode);

cyttsp6_test_cmd_mode_exit:
	return rc;
}

static int cyttsp6_test_tthe_cmd_mode(struct cyttsp6_device_access_data *dad,
		u8 *ic_buf, size_t length)
{
	struct device *dev = dad->dev;
	int rc = -ENOSYS;
	u8 mode;
	enum cyttsp6_mode new_mode;

	if (length < CY_NULL_CMD_MODE_INDEX + 1)  {
		dev_err(dev, "%s: %s length=%zu\n", __func__,
				"Buffer length is not valid", length);
		return -EINVAL;
	}
	mode = ic_buf[CY_NULL_CMD_MODE_INDEX];

	switch (mode) {
	case CY_HST_CAT:
		new_mode = CY_MODE_CAT;
		break;
	case CY_HST_OPERATE:
		new_mode = CY_MODE_OPERATIONAL;
		break;
	case CY_HST_SYSINFO:
		new_mode = CY_MODE_SYSINFO;
		break;
	default:
		dev_vdbg(dev, "%s: unsupported mode switch=%02X\n",
				__func__, mode);
		goto cyttsp6_test_tthe_cmd_mode_exit;
	}

	rc = cmd->request_exclusive(dev, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Fail rqst exclusive r=%d\n", __func__, rc);
		goto cyttsp6_test_tthe_cmd_mode_exit;
	}
	rc = cmd->request_set_mode(dev, new_mode);
	if (rc < 0)
		dev_err(dev, "%s: Fail rqst set mode=%02X r=%d\n",
				__func__, mode, rc);
	rc = cmd->release_exclusive(dev);
	if (rc < 0) {
		dev_err(dev, "%s: %s r=%d\n", __func__,
				"Fail release exclusive", rc);
		if (mode == CY_HST_OPERATE)
			rc = 0;
		else
			goto cyttsp6_test_tthe_cmd_mode_exit;
	}
	dad->test.cur_mode = new_mode;
	dev_vdbg(dev, "%s: %s=%d %s=%02X %s=%d\n", __func__,
			"own_exclusive", dad->own_exclusive == true,
			"mode", mode,
			"test.cur_mode", dad->test.cur_mode);

cyttsp6_test_tthe_cmd_mode_exit:
	return rc;
}

static int cyttsp6_grpdata_store_operational_regs_(struct device *dev,
		u8 *ic_buf, size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int num_read = dad->si->si_ofs.rep_ofs - dad->si->si_ofs.cmd_ofs - 1;
	u8 *return_buf = dad->return_buf;
	int rc;

	return_buf[0] = ic_buf[0];
	rc = cmd->request_exec_cmd(dev, CY_MODE_OPERATIONAL,
			ic_buf, length,
			return_buf + 1, num_read,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc < 0)
		dev_err(dev, "%s: Fail to execute cmd r=%d\n", __func__, rc);

	return rc;
}

/*
 * SysFs grpdata store function implementation of group 1.
 * Stores to command and parameter registers of Operational mode.
 */
static int cyttsp6_grpdata_store_operational_regs(struct device *dev,
		u8 *ic_buf, size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	size_t cmd_ofs = dad->si->si_ofs.cmd_ofs;
	int rc;
	int ret = 0;

	if ((cmd_ofs + length) > dad->si->si_ofs.rep_ofs) {
		dev_err(dev, "%s: %s length=%zu\n", __func__,
				"Buffer length is not valid", length);
		return -EINVAL;
	}

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request exclusive r=%d\n",
				__func__, rc);
		ret = rc;
		goto err_put;
	}

	rc = cyttsp6_grpdata_store_operational_regs_(dev, ic_buf, length);
	if (rc < 0)
		ret = rc;

	rc = cmd->release_exclusive(dev);
	if (rc < 0) {
		dev_err(dev, "%s: Error on release exclusive r=%d\n",
				__func__, rc);
		if (!ret)
			ret = rc;
	}

err_put:
	pm_runtime_put(dev);

	return ret;
}

/*
 * SysFs grpdata store function implementation of group 13.
 * Run CAT commands
 */
static int cyttsp6_grpdata_store_test_regs(struct device *dev, u8 *ic_buf,
		size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int rc;
	u8 *return_buf = dad->return_buf;

	/* Caller function guaranties, length is not bigger than ic_buf size */
	if (length < CY_CMD_INDEX + 1) {
		dev_err(dev, "%s: %s length=%zu\n", __func__,
				"Buffer length is not valid", length);
		return -EINVAL;
	}

	dad->test.cur_cmd = ic_buf[CY_CMD_INDEX];
	if (dad->test.cur_cmd == CY_CMD_CAT_NULL) {
		if (length < CY_NULL_CMD_INDEX + 1) {
			dev_err(dev, "%s: %s length=%zu\n", __func__,
					"Buffer length is not valid", length);
			return -EINVAL;
		}
		dev_vdbg(dev, "%s: test-cur_cmd=%d null-cmd=%d\n", __func__,
				dad->test.cur_cmd, ic_buf[CY_NULL_CMD_INDEX]);
		switch (ic_buf[CY_NULL_CMD_INDEX]) {
		case CY_NULL_CMD_NULL:
			dev_err(dev, "%s: empty NULL cmd\n", __func__);
			break;
		case CY_NULL_CMD_MODE:
			if (length < CY_NULL_CMD_MODE_INDEX + 1) {
				dev_err(dev, "%s: %s length=%zu\n", __func__,
						"Buffer length is not valid",
						length);
				return -EINVAL;
			}
			dev_vdbg(dev, "%s: Set cmd mode=%02X\n", __func__,
					ic_buf[CY_NULL_CMD_MODE_INDEX]);
			cyttsp6_test_cmd_mode(dad, ic_buf, length);
			break;
		case CY_NULL_CMD_STATUS_SIZE:
			if (length < CY_NULL_CMD_SIZE_INDEX + 1) {
				dev_err(dev, "%s: %s length=%zu\n", __func__,
						"Buffer length is not valid",
						length);
				return -EINVAL;
			}
			dad->test.cur_status_size =
				ic_buf[CY_NULL_CMD_SIZEL_INDEX]
				+ (ic_buf[CY_NULL_CMD_SIZEH_INDEX] << 8);
			dev_vdbg(dev, "%s: test-cur_status_size=%zu\n",
					__func__, dad->test.cur_status_size);
			break;
		case CY_NULL_CMD_HANDSHAKE:
			dev_vdbg(dev, "%s: try null cmd handshake\n",
					__func__);
			rc = _cyttsp6_cat_cmd_handshake(dad);
			if (rc < 0)
				dev_err(dev, "%s: %s r=%d\n", __func__,
						"Fail test cmd handshake", rc);
			break;
		default:
			break;
		}
	} else {
		dev_dbg(dev, "%s: TEST CMD=0x%02X length=%zu %s%zu\n",
				__func__, ic_buf[0], length, "cmd_ofs+grpofs=",
				dad->ic_grpoffset + dad->si->si_ofs.cmd_ofs);
		cyttsp6_pr_buf(dev, NULL, ic_buf, length, "test_cmd");

		/* exclusive access get and pm_runtime_get() already called when
		 * swithched to CAT mode */
		rc = cmd->request_exec_cmd(dev, CY_MODE_CAT,
				ic_buf, length,
				return_buf + 1, dad->test.cur_status_size,
				max(CY_COMMAND_COMPLETE_TIMEOUT,
					CY_CALIBRATE_COMPLETE_TIMEOUT));
		if (rc < 0)
			dev_err(dev, "%s: Fail to execute cmd r=%d\n",
					__func__, rc);
		/* read command byte */
		cmd->read(dev, CY_MODE_CAT, dad->si->si_ofs.cmd_ofs,
				&return_buf[0], 1);

	}

	return 0;
}

/*
 * SysFs grpdata store function implementation of group 15.
 * Run CAT/OP commands for TTHE
 */
static int cyttsp6_grpdata_store_tthe_test_regs(struct device *dev, u8 *ic_buf,
		size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int rc;

	/* Caller function guaranties, length is not bigger than ic_buf size */
	if (length < CY_CMD_INDEX + 1) {
		dev_err(dev, "%s: %s length=%zu\n", __func__,
				"Buffer length is not valid", length);
		return -EINVAL;
	}

	dad->test.cur_cmd = ic_buf[CY_CMD_INDEX];
	if (dad->test.cur_cmd == CY_CMD_CAT_NULL) {
		if (length < CY_NULL_CMD_INDEX + 1) {
			dev_err(dev, "%s: %s length=%zu\n", __func__,
					"Buffer length is not valid", length);
			return -EINVAL;
		}
		dev_vdbg(dev, "%s: test-cur_cmd=%d null-cmd=%d\n", __func__,
				dad->test.cur_cmd, ic_buf[CY_NULL_CMD_INDEX]);
		switch (ic_buf[CY_NULL_CMD_INDEX]) {
		case CY_NULL_CMD_NULL:
			dev_err(dev, "%s: empty NULL cmd\n", __func__);
			break;
		case CY_NULL_CMD_MODE:
			if (length < CY_NULL_CMD_MODE_INDEX + 1) {
				dev_err(dev, "%s: %s length=%zu\n", __func__,
						"Buffer length is not valid",
						length);
				return -EINVAL;
			}
			dev_vdbg(dev, "%s: Set cmd mode=%02X\n", __func__,
					ic_buf[CY_NULL_CMD_MODE_INDEX]);
			cyttsp6_test_tthe_cmd_mode(dad, ic_buf, length);
			break;
		case CY_NULL_CMD_STATUS_SIZE:
			if (length < CY_NULL_CMD_SIZE_INDEX + 1) {
				dev_err(dev, "%s: %s length=%zu\n", __func__,
						"Buffer length is not valid",
						length);
				return -EINVAL;
			}
			dad->test.cur_status_size =
				ic_buf[CY_NULL_CMD_SIZEL_INDEX]
				+ (ic_buf[CY_NULL_CMD_SIZEH_INDEX] << 8);
			dev_vdbg(dev, "%s: test-cur_status_size=%zu\n",
					__func__, dad->test.cur_status_size);
			break;
		case CY_NULL_CMD_HANDSHAKE:
			dev_vdbg(dev, "%s: try null cmd handshake\n",
					__func__);
			rc = _cyttsp6_cat_cmd_handshake(dad);
			if (rc < 0)
				dev_err(dev, "%s: %s r=%d\n", __func__,
						"Fail test cmd handshake", rc);
			break;
		case CY_NULL_CMD_LOW_POWER:
			dev_vdbg(dev, "%s: try null cmd low power\n", __func__);
			rc = _cyttsp6_cmd_toggle_lowpower(dad);
			if (rc < 0)
				dev_err(dev, "%s: Fail test cmd toggle low power r=%d\n",
						__func__, rc);
			break;
		default:
			break;
		}
	} else {
		dev_dbg(dev, "%s: TEST CMD=0x%02X length=%zu %s%zu\n",
				__func__, ic_buf[0], length, "cmd_ofs+grpofs=",
				dad->ic_grpoffset + dad->si->si_ofs.cmd_ofs);
		cyttsp6_pr_buf(dev, NULL, ic_buf, length, "test_cmd");

		if (dad->test.cur_mode == CY_MODE_OPERATIONAL)
			pm_runtime_get_sync(dev);

		/* Support Operating mode command. */
		/* Write command parameters first */
		if (length > 1) {
			rc = cmd->write(dev, dad->test.cur_mode,
				dad->ic_grpoffset + dad->si->si_ofs.cmd_ofs + 1,
				ic_buf + 1, length - 1);
			if (rc < 0) {
				dev_err(dev, "%s: Fail write cmd param regs r=%d\n",
					__func__, rc);
				if (dad->test.cur_mode == CY_MODE_OPERATIONAL)
					pm_runtime_put(dev);
				return 0;
			}
		}
		/* Write command */
		rc = cmd->write(dev, dad->test.cur_mode,
				dad->ic_grpoffset + dad->si->si_ofs.cmd_ofs,
				ic_buf, 1);
		if (rc < 0)
			dev_err(dev, "%s: Fail write cmd reg r=%d\n",
					__func__, rc);

		if (dad->test.cur_mode == CY_MODE_OPERATIONAL)
			pm_runtime_put(dev);
	}

	return 0;
}

static int cyttsp6_device_access_write_config_(struct device *dev,
	u8 ebid,  u8 *ic_buf, int offset, size_t length)
{
	int ret = 0;
	int rc;

	rc = cmd->request_set_mode(dev, CY_MODE_CAT);
	if (rc < 0)
		return rc;

	rc = cmd->request_write_config(dev, ebid, offset, ic_buf, length);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request write config ebid:%d r=%d\n",
				__func__, ebid, rc);
		ret = rc;
	}

	rc = cmd->request_set_mode(dev, CY_MODE_OPERATIONAL);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request set mode r=%d\n",
				__func__, rc);
		if (!ret)
			ret = rc;
	}

	return ret;
}

static int cyttsp6_device_access_write_config(struct device *dev,
	u8 ebid,  u8 *ic_buf, int offset, size_t length)
{
	int rc;
	int ret = 0;

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request exclusive r=%d\n",
				__func__, rc);
		ret = rc;
		goto err_put;
	}

	rc = cyttsp6_device_access_write_config_(dev, ebid, ic_buf, offset,
			length);
	if (rc < 0)
		ret = rc;

	rc = cmd->release_exclusive(dev);
	if (rc < 0) {
		dev_err(dev, "%s: Error on release exclusive r=%d\n",
				__func__, rc);
		if (!ret)
			ret = rc;
	}

err_put:
	pm_runtime_put(dev);

	if (!ret)
		cmd->request_restart(dev, true);

	return ret;
}

/*
 * SysFs grpdata store function implementation of group 6.
 * Stores the contents of the touch parameters.
 */
static int cyttsp6_grpdata_store_touch_params(struct device *dev, u8 *ic_buf,
	size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);

	return cyttsp6_device_access_write_config(dev, CY_TCH_PARM_EBID,
			ic_buf, dad->ic_grpoffset, length);
}

/*
 * SysFs grpdata store function implementation of group 11.
 * Stores the contents of the design data.
 */
static int cyttsp6_grpdata_store_ddata(struct device *dev, u8 *ic_buf,
	size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);

	/*
	 *  +4 offset is to bypass 2 byte length and 2 byte max length fields
	 */
	return cyttsp6_device_access_write_config(dev, CY_DDATA_EBID,
			ic_buf, dad->ic_grpoffset + 4, length);
}

/*
 * SysFs grpdata store function implementation of group 12.
 * Stores the contents of the manufacturing data.
 */
static int cyttsp6_grpdata_store_mdata(struct device *dev, u8 *ic_buf,
	size_t length)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);

	/*
	 *  +4 offset is to bypass 2 byte length and 2 byte max length fields
	 */
	return cyttsp6_device_access_write_config(dev, CY_MDATA_EBID,
			ic_buf, dad->ic_grpoffset + 4, length);
}

/*
 * Gets user input from sysfs and parse it
 * return size of parsed output buffer
 */
static int cyttsp6_ic_parse_input(struct device *dev, const char *buf,
		size_t buf_size, u8 *ic_buf, size_t ic_buf_size)
{
	const char *pbuf = buf;
	unsigned long value;
	char scan_buf[CYTTSP6_INPUT_ELEM_SZ];
	int i = 0;
	int j;
	int last = 0;
	int ret;

	dev_dbg(dev, "%s: pbuf=%p buf=%p size=%d %s=%zu buf=%s\n", __func__,
			pbuf, buf, (int) buf_size, "scan buf size",
			CYTTSP6_INPUT_ELEM_SZ, buf);

	while (pbuf <= (buf + buf_size)) {
		if (i >= CY_MAX_CONFIG_BYTES) {
			dev_err(dev, "%s: %s size=%d max=%d\n", __func__,
					"Max cmd size exceeded", i,
					CY_MAX_CONFIG_BYTES);
			return -EINVAL;
		}
		if (i >= ic_buf_size) {
			dev_err(dev, "%s: %s size=%d buf_size=%zu\n", __func__,
					"Buffer size exceeded", i, ic_buf_size);
			return -EINVAL;
		}
		while (((*pbuf == ' ') || (*pbuf == ','))
				&& (pbuf < (buf + buf_size))) {
			last = *pbuf;
			pbuf++;
		}

		if (pbuf >= (buf + buf_size))
			break;

		memset(scan_buf, 0, CYTTSP6_INPUT_ELEM_SZ);
		if ((last == ',') && (*pbuf == ',')) {
			dev_err(dev, "%s: %s \",,\" not allowed.\n", __func__,
					"Invalid data format.");
			return -EINVAL;
		}
		for (j = 0; j < (CYTTSP6_INPUT_ELEM_SZ - 1)
				&& (pbuf < (buf + buf_size))
				&& (*pbuf != ' ')
				&& (*pbuf != ','); j++) {
			last = *pbuf;
			scan_buf[j] = *pbuf++;
		}

		ret = kstrtoul(scan_buf, 16, &value);
		if (ret < 0) {
			dev_err(dev, "%s: %s '%s' %s%s i=%d r=%d\n", __func__,
					"Invalid data format. ", scan_buf,
					"Use \"0xHH,...,0xHH\"", " instead.",
					i, ret);
			return ret;
		}

		ic_buf[i] = value;
		i++;
	}

	return i;
}

/*
 * SysFs store functions of each group member.
 */
static cyttsp6_store_function
		cyttsp6_grpdata_store_functions[CY_IC_GRPNUM_NUM] = {
	[CY_IC_GRPNUM_RESERVED] = cyttsp6_grpdata_store_void,
	[CY_IC_GRPNUM_CMD_REGS] = cyttsp6_grpdata_store_operational_regs,
	[CY_IC_GRPNUM_TCH_REP] = cyttsp6_grpdata_store_void,
	[CY_IC_GRPNUM_DATA_REC] = cyttsp6_grpdata_store_void,
	[CY_IC_GRPNUM_TEST_REC] = cyttsp6_grpdata_store_void,
	[CY_IC_GRPNUM_PCFG_REC] = cyttsp6_grpdata_store_void,
	[CY_IC_GRPNUM_TCH_PARM_VAL] = cyttsp6_grpdata_store_touch_params,
	[CY_IC_GRPNUM_TCH_PARM_SIZE] = cyttsp6_grpdata_store_void,
	[CY_IC_GRPNUM_RESERVED1] = cyttsp6_grpdata_store_void,
	[CY_IC_GRPNUM_RESERVED2] = cyttsp6_grpdata_store_void,
	[CY_IC_GRPNUM_OPCFG_REC] = cyttsp6_grpdata_store_void,
	[CY_IC_GRPNUM_DDATA_REC] = cyttsp6_grpdata_store_ddata,
	[CY_IC_GRPNUM_MDATA_REC] = cyttsp6_grpdata_store_mdata,
	[CY_IC_GRPNUM_TEST_REGS] = cyttsp6_grpdata_store_test_regs,
	[CY_IC_GRPNUM_BTN_KEYS] = cyttsp6_grpdata_store_void,
	[CY_IC_GRPNUM_TTHE_REGS] = cyttsp6_grpdata_store_tthe_test_regs,
};

static ssize_t cyttsp6_ic_grpdata_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	ssize_t length;
	int rc;

	mutex_lock(&dad->sysfs_lock);
	length = cyttsp6_ic_parse_input(dev, buf, size, dad->ic_buf,
			CY_MAX_PRBUF_SIZE);
	if (length <= 0) {
		dev_err(dev, "%s: %s Group Data store\n", __func__,
				"Malformed input for");
		goto cyttsp6_ic_grpdata_store_exit;
	}

	dev_vdbg(dev, "%s: grpnum=%d grpoffset=%u\n",
			__func__, dad->ic_grpnum, dad->ic_grpoffset);

	if (dad->ic_grpnum >= CY_IC_GRPNUM_NUM) {
		dev_err(dev, "%s: Group %d does not exist.\n",
				__func__, dad->ic_grpnum);
		goto cyttsp6_ic_grpdata_store_exit;
	}

	/* write ic_buf to log */
	cyttsp6_pr_buf(dev, NULL, dad->ic_buf, length, "ic_buf");

	/* Call relevant store handler. */
	rc = cyttsp6_grpdata_store_functions[dad->ic_grpnum] (dev, dad->ic_buf,
			length);
	if (rc < 0)
		dev_err(dev, "%s: Failed to store for grpnum=%d.\n",
				__func__, dad->ic_grpnum);

cyttsp6_ic_grpdata_store_exit:
	mutex_unlock(&dad->sysfs_lock);
	dev_vdbg(dev, "%s: return size=%zu\n", __func__, size);

	return size;
}

static DEVICE_ATTR(ic_grpdata, S_IRUSR | S_IWUSR,
	cyttsp6_ic_grpdata_show, cyttsp6_ic_grpdata_store);

/*
 * Execute Panel Scan command
 */
static int _cyttsp6_execute_panel_scan_cmd(struct device *dev)
{
	u8 cmd_buf[CY_CMD_CAT_EXECUTE_PANEL_SCAN_CMD_SZ];
	u8 ret_buf[CY_CMD_CAT_EXECUTE_PANEL_SCAN_RET_SZ];
	int rc;

	cmd_buf[0] = CY_CMD_CAT_EXEC_PANEL_SCAN;

	rc = cmd->request_exec_cmd(dev, CY_MODE_CAT,
			cmd_buf, CY_CMD_CAT_EXECUTE_PANEL_SCAN_CMD_SZ,
			ret_buf, CY_CMD_CAT_EXECUTE_PANEL_SCAN_RET_SZ,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc)
		goto exit;

	if (ret_buf[0] != CY_CMD_STATUS_SUCCESS) {
		rc = -EINVAL;
		goto exit;
	}

exit:
	return rc;
}

/*
 * Retrieve Panel Scan command
 */
static int _cyttsp6_retrieve_panel_scan_cmd(struct device *dev,
		u16 offset, u16 length, u8 data_id, u8 *buf, int *size)
{
	u8 cmd_buf[CY_CMD_CAT_RETRIEVE_PANEL_SCAN_CMD_SZ];
	u8 ret_buf[CY_CMD_CAT_RETRIEVE_PANEL_SCAN_RET_SZ];
	u16 total_read_length = 0;
	u16 read_length;
	u16 off_buf = 0;
	u8 element_size = 0;
	int rc;

again:
	cmd_buf[0] = CY_CMD_CAT_RETRIEVE_PANEL_SCAN;
	cmd_buf[1] = HI_BYTE(offset);
	cmd_buf[2] = LO_BYTE(offset);
	cmd_buf[3] = HI_BYTE(length);
	cmd_buf[4] = LO_BYTE(length);
	cmd_buf[5] = data_id;

	rc = cmd->request_exec_cmd(dev, CY_MODE_CAT,
			cmd_buf, CY_CMD_CAT_RETRIEVE_PANEL_SCAN_CMD_SZ,
			ret_buf, CY_CMD_CAT_RETRIEVE_PANEL_SCAN_RET_SZ,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc)
		goto exit;

	if (ret_buf[0] != CY_CMD_STATUS_SUCCESS
			|| ret_buf[1] != data_id) {
		rc = -EINVAL;
		goto exit;
	}

	read_length = (ret_buf[2] << 8) + ret_buf[3];
	if (read_length) {
		element_size = ret_buf[4] & 0x07;
		/* Read panel scan data */
		rc = cmd->read(dev, CY_MODE_CAT, CY_REG_CAT_CMD + 1 +
				CY_CMD_CAT_RETRIEVE_PANEL_SCAN_RET_SZ,
				&buf[CY_CMD_CAT_RETRIEVE_PANEL_SCAN_RET_SZ +
					off_buf * element_size],
				read_length * element_size);
		if (rc)
			goto exit;

		total_read_length += read_length;

		if (read_length < length) {
			offset += read_length;
			off_buf += read_length;
			length -= read_length;
			goto again;
		}
	}

	/* Form response buffer */
	buf[0] = ret_buf[0]; /* Status */
	buf[1] = ret_buf[1]; /* Data ID */
	buf[2] = HI_BYTE(total_read_length);
	buf[3] = LO_BYTE(total_read_length);
	buf[4] = ret_buf[4]; /* Data Format */

	*size = CY_CMD_CAT_RETRIEVE_PANEL_SCAN_RET_SZ +
			total_read_length * element_size;

exit:
	return rc;
}

/*
 * Retrieve Data Structure command
 */
static int _cyttsp6_retrieve_data_structure_cmd(struct device *dev,
		u16 offset, u16 length, u8 data_id, u8 *status,
		u8 *data_format, u16 *act_length, u8 *data)

{
	u8 cmd_buf[CY_CMD_CAT_RETRIEVE_DATA_STRUCT_CMD_SZ];
	u8 ret_buf[CY_CMD_CAT_RETRIEVE_DATA_STRUCT_RET_SZ];
	u16 total_read_length = 0;
	u16 read_length;
	u16 off_buf = 0;
	int rc;

again:
	cmd_buf[0] = CY_CMD_CAT_RETRIEVE_DATA_STRUCTURE;
	cmd_buf[1] = HI_BYTE(offset);
	cmd_buf[2] = LO_BYTE(offset);
	cmd_buf[3] = HI_BYTE(length);
	cmd_buf[4] = LO_BYTE(length);
	cmd_buf[5] = data_id;

	rc = cmd->request_exec_cmd(dev, CY_MODE_CAT,
			cmd_buf, CY_CMD_CAT_RETRIEVE_DATA_STRUCT_CMD_SZ,
			ret_buf, CY_CMD_CAT_RETRIEVE_DATA_STRUCT_RET_SZ,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc)
		goto exit;

	read_length = (ret_buf[2] << 8) + ret_buf[3];
	if (read_length && data) {
		/* Read data */
		rc = cmd->read(dev, CY_MODE_CAT, CY_REG_CAT_CMD + 1 +
				CY_CMD_CAT_RETRIEVE_DATA_STRUCT_RET_SZ,
				&data[off_buf], read_length);
		if (rc)
			goto exit;

		total_read_length += read_length;

		if (read_length < length) {
			offset += read_length;
			off_buf += read_length;
			length -= read_length;
			goto again;
		}
	}

	if (status)
		*status = ret_buf[0];
	if (data_format)
		*data_format = ret_buf[4];
	if (act_length)
		*act_length = total_read_length;

exit:
	return rc;
}

/*
 * Run Auto Shorts Self Test command
 */
static int _cyttsp6_run_autoshorts_self_test_cmd(struct device *dev,
		u8 *status, u8 *summary_result, u8 *results_available)
{
	u8 cmd_buf[CY_CMD_CAT_RUN_AUTOSHORTS_ST_CMD_SZ];
	u8 ret_buf[CY_CMD_CAT_RUN_AUTOSHORTS_ST_RET_SZ];
	int rc;

	cmd_buf[0] = CY_CMD_CAT_RUN_SELF_TEST;
	cmd_buf[1] = CY_ST_ID_AUTOSHORTS;

	rc = cmd->request_exec_cmd(dev, CY_MODE_CAT,
			cmd_buf, CY_CMD_CAT_RUN_AUTOSHORTS_ST_CMD_SZ,
			ret_buf, CY_CMD_CAT_RUN_AUTOSHORTS_ST_RET_SZ,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc)
		goto exit;

	if (status)
		*status = ret_buf[0];
	if (summary_result)
		*summary_result = ret_buf[1];
	if (results_available)
		*results_available = ret_buf[2];

exit:
	return rc;
}

/*
 * Run Capacitance Self Tests
 */
static int _cyttsp6_run_cap_self_test_cmd(struct device *dev,
		u8 test_id, u8 *status, u8 *summary_result,
		u8 *results_available)
{
	u8 cmd_buf[CY_CMD_CAT_RUN_CAP_ST_CMD_SZ];
	u8 ret_buf[CY_CMD_CAT_RUN_CAP_ST_RET_SZ];
	int rc;

	cmd_buf[0] = CY_CMD_CAT_RUN_SELF_TEST;
	cmd_buf[1] = test_id;

	rc = cmd->request_exec_cmd(dev, CY_MODE_CAT,
			cmd_buf, CY_CMD_CAT_RUN_CAP_ST_CMD_SZ,
			ret_buf, CY_CMD_CAT_RUN_CAP_ST_RET_SZ,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc)
		goto exit;

	if (status)
		*status = ret_buf[0];
	if (summary_result)
		*summary_result = ret_buf[1];
	if (results_available)
		*results_available = ret_buf[2];

exit:
	return rc;
}

/*
 * Get Auto Shorts Self Test Results command
 */
static int _cyttsp6_get_autoshorts_self_test_results_cmd(struct device *dev,
		u16 offset, u16 length, u8 *status, u16 *act_length,
		u8 *test_result, u8 *data)
{
	u8 cmd_buf[CY_CMD_CAT_GET_AUTOSHORTS_ST_RES_CMD_SZ];
	u8 ret_buf[CY_CMD_CAT_GET_AUTOSHORTS_ST_RES_RET_SZ];
	u16 read_length;
	int rc;

	cmd_buf[0] = CY_CMD_CAT_GET_SELF_TEST_RESULT;
	cmd_buf[1] = HI_BYTE(offset);
	cmd_buf[2] = LO_BYTE(offset);
	cmd_buf[3] = HI_BYTE(length);
	cmd_buf[4] = LO_BYTE(length);
	cmd_buf[5] = CY_ST_ID_AUTOSHORTS;

	rc = cmd->request_exec_cmd(dev, CY_MODE_CAT,
			cmd_buf, CY_CMD_CAT_GET_AUTOSHORTS_ST_RES_CMD_SZ,
			ret_buf, CY_CMD_CAT_GET_AUTOSHORTS_ST_RES_RET_SZ,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc)
		goto exit;

	read_length = (ret_buf[2] << 8) + ret_buf[3];
	if (read_length && data) {
		/* Read test result data */
		rc = cmd->read(dev, CY_MODE_CAT, CY_REG_CAT_CMD + 1 +
				CY_CMD_CAT_GET_AUTOSHORTS_ST_RES_RET_SZ,
				data, read_length);
		if (rc)
			goto exit;
	}

	if (status)
		*status = ret_buf[0];
	if (act_length)
		*act_length = read_length;
	if (test_result)
		*test_result = ret_buf[5];

exit:
	return rc;
}

/*
 * Get Capacitance Self Test Results command
 */
static int _cyttsp6_get_cap_self_test_results_cmd(struct device *dev,
		u8 test_id, u16 offset, u16 length, u8 *status, u16 *act_length, u8 *data)
{
	u8 cmd_buf[CY_CMD_CAT_GET_CAP_ST_RES_CMD_SZ];
	u8 ret_buf[CY_CMD_CAT_GET_CAP_ST_RES_RET_SZ];
	u16 read_length;
	int rc;

	cmd_buf[0] = CY_CMD_CAT_GET_SELF_TEST_RESULT;
	cmd_buf[1] = HI_BYTE(offset);
	cmd_buf[2] = LO_BYTE(offset);
	cmd_buf[3] = HI_BYTE(length);
	cmd_buf[4] = LO_BYTE(length);
	cmd_buf[5] = test_id;

	rc = cmd->request_exec_cmd(dev, CY_MODE_CAT,
			cmd_buf, CY_CMD_CAT_GET_CAP_ST_RES_CMD_SZ,
			ret_buf, CY_CMD_CAT_GET_CAP_ST_RES_RET_SZ,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc)
		goto exit;

	read_length = (ret_buf[2] << 8) + ret_buf[3];
	if (read_length && data) {
		/* Read test result data */
		rc = cmd->read(dev, CY_MODE_CAT, CY_REG_CAT_CMD + 1 +
				CY_CMD_CAT_GET_CAP_ST_RES_RET_SZ,
				data, read_length);
		if (rc)
			goto exit;
	}

	if (status)
		*status = ret_buf[0];
	if (act_length)
		*act_length = read_length;

exit:
	return rc;
}

/*
 * Calibrate IDACs command
 */
static int _cyttsp6_calibrate_idacs_cmd(struct device *dev,
		u8 sensing_mode, u8 *status)
{
	u8 cmd_buf[CY_CMD_CAT_CALIBRATE_IDAC_CMD_SZ];
	u8 ret_buf[CY_CMD_CAT_CALIBRATE_IDAC_RET_SZ];
	int rc;

	cmd_buf[0] = CY_CMD_CAT_CALIBRATE_IDACS;
	cmd_buf[1] = sensing_mode;

	rc = cmd->request_exec_cmd(dev, CY_MODE_CAT,
			cmd_buf, CY_CMD_CAT_CALIBRATE_IDAC_CMD_SZ,
			ret_buf, CY_CMD_CAT_CALIBRATE_IDAC_RET_SZ,
			CY_CALIBRATE_COMPLETE_TIMEOUT);
	if (rc)
		goto exit;

	if (status)
		*status = ret_buf[0];

exit:
	return rc;
}

/*
 * Initialize Baselines command
 */
static int _cyttsp6_initialize_baselines_cmd(struct device *dev,
		u8 sensing_mode, u8 *status)
{
	u8 cmd_buf[CY_CMD_CAT_INIT_BASELINE_CMD_SZ];
	u8 ret_buf[CY_CMD_CAT_INIT_BASELINE_RET_SZ];
	int rc;

	cmd_buf[0] = CY_CMD_CAT_INIT_BASELINES;
	cmd_buf[1] = sensing_mode;

	rc = cmd->request_exec_cmd(dev, CY_MODE_CAT,
			cmd_buf, CY_CMD_CAT_INIT_BASELINE_CMD_SZ,
			ret_buf, CY_CMD_CAT_INIT_BASELINE_RET_SZ,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc)
		goto exit;

	if (status)
		*status = ret_buf[0];

exit:
	return rc;
}

static ssize_t cyttsp6_get_panel_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int data_idx = 0;
	int print_idx = -1;
	int i = 0;
	int rc;

	rc = cmd->request_exclusive(dev, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request exclusive r=%d\n",
				__func__, rc);
		goto exit;
	}

	if (dad->heatmap.scan_start) {
		/* Start scan */
		rc = _cyttsp6_execute_panel_scan_cmd(dev);
		if (rc < 0) {
			dev_err(dev, "%s: Error on execute panel scan\n",
				__func__);
			goto release_exclusive;
		}
	}

	/* retrieve panel_scan data */
	rc = _cyttsp6_retrieve_panel_scan_cmd(dev, dad->heatmap.input_offset,
			dad->heatmap.num_element, dad->heatmap.data_type,
			&dad->ic_buf[CY_REG_CAT_CMD + 1], &data_idx);
	if (rc < 0) {
		dev_err(dev, "%s: Error on retrieve panel scan\n", __func__);
		goto release_exclusive;
	}

	/* Read HST_MODE and COMMAND registers */
	rc = cmd->read(dev, CY_MODE_CAT, 0, dad->ic_buf, CY_REG_CAT_CMD + 1);
	if (rc < 0)
		goto release_exclusive;

	data_idx += CY_REG_CAT_CMD + 1;

release_exclusive:
	cmd->release_exclusive(dev);

	if (rc < 0)
		goto exit;

	print_idx = 0;
	print_idx += scnprintf(buf, CY_MAX_PRBUF_SIZE, "CY_DATA:");
	for (i = 0; i < data_idx; i++) {
		print_idx += scnprintf(buf + print_idx,
				CY_MAX_PRBUF_SIZE - print_idx,
				"%02X ", dad->ic_buf[i]);
	}
	print_idx += scnprintf(buf + print_idx, CY_MAX_PRBUF_SIZE - print_idx,
			":(%d bytes)\n", data_idx);

exit:
	return print_idx;
}

static ssize_t cyttsp6_get_panel_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	ssize_t length;

	mutex_lock(&dad->sysfs_lock);

	length = cyttsp6_ic_parse_input(dev, buf, size, dad->ic_buf,
			CY_MAX_PRBUF_SIZE);
	if (length <= 0) {
		dev_err(dev, "%s: %s Group Data store\n", __func__,
				"Malformed input for");
		goto cyttsp6_get_panel_data_store_exit;
	}

	dev_vdbg(dev, "%s: grpnum=%d grpoffset=%u\n",
			__func__, dad->ic_grpnum, dad->ic_grpoffset);

	if (dad->ic_grpnum >= CY_IC_GRPNUM_NUM) {
		dev_err(dev, "%s: Group %d does not exist.\n",
				__func__, dad->ic_grpnum);
		goto cyttsp6_get_panel_data_store_exit;
	}

	/*update parameter value */
	dad->heatmap.input_offset = dad->ic_buf[2] + (dad->ic_buf[1] << 8);
	dad->heatmap.num_element = dad->ic_buf[4] + (dad->ic_buf[3] << 8);
	dad->heatmap.data_type = dad->ic_buf[5];

	if (dad->ic_buf[6] > 0)
		dad->heatmap.scan_start = true;
	else
		dad->heatmap.scan_start = false;

cyttsp6_get_panel_data_store_exit:
	mutex_unlock(&dad->sysfs_lock);
	dev_vdbg(dev, "%s: return size=%zu\n", __func__, size);

	return size;
}

static DEVICE_ATTR(get_panel_data, S_IRUSR | S_IWUSR,
	cyttsp6_get_panel_data_show, cyttsp6_get_panel_data_store);

static int prepare_print_buffer(int status, u8 *in_buf, int length,
		u8 *out_buf)
{
	int index = 0;
	int i;
	char *p = "\n";

	index += scnprintf(out_buf, CY_MAX_PRBUF_SIZE, "status %d\n", status);

	/*
	 * workaround for sysfs buffer limit (4096 bytes).
	 * num_elems X 2 (an elem can be represented at most with 2 bytes) X
	 * 3 (a byte is represented with 2 chars + \n) has to be lower
	 * than sysfs buffer limit. and also command header (5 bytes) must be
	 * accounted in this calculation. finally, maximum number of elements
	 * can approximately be 650.
	 */
	if (length > MAX_SYSFS_BUF_LEN)
		p = "";


	for (i = 0; i < length; i++) {
		index += scnprintf(&out_buf[index],
				CY_MAX_PRBUF_SIZE - index,
				"%02X%s", in_buf[i], p);
	}

	if (length > MAX_SYSFS_BUF_LEN)
		index += scnprintf(&out_buf[index],
				CY_MAX_PRBUF_SIZE - index, "\n");

	return index;
}

static ssize_t cyttsp6_panel_scan_show(struct device *dev,
		struct cyttsp6_attribute *attr, char *buf)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int status = STATUS_FAIL;
	int length;
	int size;
	int rc;

	mutex_lock(&dad->sysfs_lock);

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request exclusive r=%d\n",
				__func__, rc);
		goto put_pm_runtime;
	}

	rc = cmd->request_set_mode(dev, CY_MODE_CAT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request set mode to CAT r=%d\n",
				__func__, rc);
		goto release_exclusive;
	}

	rc = _cyttsp6_execute_panel_scan_cmd(dev);
	if (rc < 0) {
		dev_err(dev, "%s: Error on execute panel scan r=%d\n",
				__func__, rc);
		goto set_mode_to_operational;
	}

	/* Set length to max to read all */
	rc = _cyttsp6_retrieve_panel_scan_cmd(dev, 0, 65535,
			dad->panel_scan_data_id, dad->ic_buf, &length);
	if (rc < 0) {
		dev_err(dev, "%s: Error on retrieve panel scan r=%d\n",
				__func__, rc);
		goto set_mode_to_operational;
	}

	status = STATUS_SUCCESS;

set_mode_to_operational:
	cmd->request_set_mode(dev, CY_MODE_OPERATIONAL);

release_exclusive:
	cmd->release_exclusive(dev);

put_pm_runtime:
	pm_runtime_put(dev);

	if (status == STATUS_FAIL)
		length = 0;

	size = prepare_print_buffer(status, dad->ic_buf, length, buf);

	mutex_unlock(&dad->sysfs_lock);

	return size;
}

static ssize_t cyttsp6_panel_scan_store(struct device *dev,
		struct cyttsp6_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	ssize_t length;
	int rc = 0;

	mutex_lock(&dad->sysfs_lock);

	length = cyttsp6_ic_parse_input(dev, buf, size, dad->ic_buf,
			CY_MAX_PRBUF_SIZE);
	if (length != 1) {
		dev_err(dev, "%s: Malformed input\n", __func__);
		rc = -EINVAL;
		goto exit_unlock;
	}

	dad->panel_scan_data_id = dad->ic_buf[0];

exit_unlock:
	mutex_unlock(&dad->sysfs_lock);

	if (rc)
		return rc;

	return size;
}

static CY_ATTR(panel_scan, S_IRUSR | S_IWUSR,
	cyttsp6_panel_scan_show, cyttsp6_panel_scan_store);

static ssize_t cyttsp6_get_idac_show(struct device *dev,
		struct cyttsp6_attribute *attr, char *buf)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int status = STATUS_FAIL;
	u8 cmd_status = 0;
	u8 data_format = 0;
	u16 act_length = 0;
	u16 read_length = 65535;
	int length;
	int size;
	int rc;

	mutex_lock(&dad->sysfs_lock);

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request exclusive r=%d\n",
				__func__, rc);
		goto put_pm_runtime;
	}

	rc = cmd->request_set_mode(dev, CY_MODE_CAT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request set mode to CAT r=%d\n",
				__func__, rc);
		goto release_exclusive;
	}

	rc = _cyttsp6_retrieve_data_structure_cmd(dev, 0, read_length,
			dad->get_idac_data_id, &cmd_status, &data_format,
			&act_length, &dad->ic_buf[5]);
	if (rc < 0) {
		dev_err(dev, "%s: Error on retrieve data structure r=%d\n",
				__func__, rc);
		goto set_mode_to_operational;
	}

	dad->ic_buf[0] = cmd_status;
	dad->ic_buf[1] = dad->get_idac_data_id;
	dad->ic_buf[2] = HI_BYTE(act_length);
	dad->ic_buf[3] = LO_BYTE(act_length);
	dad->ic_buf[4] = data_format;

	length = 5 + act_length;

	status = STATUS_SUCCESS;

set_mode_to_operational:
	cmd->request_set_mode(dev, CY_MODE_OPERATIONAL);

release_exclusive:
	cmd->release_exclusive(dev);

put_pm_runtime:
	pm_runtime_put(dev);

	if (status == STATUS_FAIL)
		length = 0;

	size = prepare_print_buffer(status, dad->ic_buf, length, buf);

	mutex_unlock(&dad->sysfs_lock);

	return size;
}

static ssize_t cyttsp6_get_idac_store(struct device *dev,
		struct cyttsp6_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	ssize_t length;
	int rc = 0;

	mutex_lock(&dad->sysfs_lock);

	length = cyttsp6_ic_parse_input(dev, buf, size, dad->ic_buf,
			CY_MAX_PRBUF_SIZE);
	if (length != 1) {
		dev_err(dev, "%s: Malformed input\n", __func__);
		rc = -EINVAL;
		goto exit_unlock;
	}

	/* Check device type */
	dad->get_idac_data_id = dad->ic_buf[0];

exit_unlock:
	mutex_unlock(&dad->sysfs_lock);

	if (rc)
		return rc;

	return size;
}

static CY_ATTR(get_idac, S_IRUSR | S_IWUSR,
	cyttsp6_get_idac_show, cyttsp6_get_idac_store);

static ssize_t cyttsp6_gest_show(struct device *dev,
                struct cyttsp6_attribute *attr, char *buf)
{
        struct cyttsp6_device_access_data *dad
                = cyttsp6_get_device_access_data(dev);
	int status = STATUS_FAIL;
        int length;
        int size;
        int rc = 0;
	u8 gest_ofs;

	mutex_lock(&dad->sysfs_lock);

        pm_runtime_get_sync(dev);

        rc = cmd->request_exclusive(dev, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
        if (rc < 0) {
                dev_err(dev, "%s: Error on request exclusive r=%d\n",
                                __func__, rc);
                goto put_pm_runtime;
        }
	/* read xy_data for all current touches & respective gesture */
	/* Enabled Stnadard / Enabled Extended */
	gest_ofs = dad->si->si_ofs.rep_ofs + dad->si->si_ofs.num_btn_regs + 2;
	dad->ic_buf[0] = dad->si->xy_mode[gest_ofs + 0];
	dad->ic_buf[1] = dad->si->xy_mode[gest_ofs + 1];
	dad->ic_buf[2] = dad->si->xy_mode[gest_ofs + 2];
	dad->ic_buf[3] = dad->si->xy_mode[gest_ofs + 3];
	dad->ic_buf[4] = dad->si->xy_mode[gest_ofs + 4];
	dad->ic_buf[5] = dad->si->xy_mode[gest_ofs + 5];
	dad->ic_buf[6] = dad->si->xy_mode[gest_ofs + 6];

	length = 7;

	status = STATUS_SUCCESS;

        cmd->release_exclusive(dev);

put_pm_runtime:
        pm_runtime_put(dev);

        if (status == STATUS_FAIL)
                length = 0;

        size = prepare_print_buffer(status, dad->ic_buf, length, buf);

        mutex_unlock(&dad->sysfs_lock);

        return size;
}

static CY_ATTR(gesture, S_IRUSR, cyttsp6_gest_show, NULL);

static ssize_t cyttsp6_noise_show(struct device *dev,
                struct cyttsp6_attribute *attr, char *buf)
{
        struct cyttsp6_device_access_data *dad
                = cyttsp6_get_device_access_data(dev);
        int status = STATUS_FAIL;
        int length;
        int size;
        int rc = 0;
	int noise_ofs;

        mutex_lock(&dad->sysfs_lock);

        pm_runtime_get_sync(dev);

        rc = cmd->request_exclusive(dev, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
        if (rc < 0) {
                dev_err(dev, "%s: Error on request exclusive r=%d\n",
                                __func__, rc);
                goto put_pm_runtime;
        }

	/* read noise wideband & noise level reg */
        noise_ofs = dad->si->si_ofs.rep_ofs + 2 +
			dad->si->si_ofs.num_btn_regs + 8;
	dad->ic_buf[0] = dad->si->xy_mode[noise_ofs + 0];
        dad->ic_buf[1] = dad->si->xy_mode[noise_ofs + 2];

	length = 2;

        status = STATUS_SUCCESS;

        cmd->release_exclusive(dev);

put_pm_runtime:
        pm_runtime_put(dev);

        if (status == STATUS_FAIL)
                length = 0;

        size = prepare_print_buffer(status, dad->ic_buf, length, buf);

        mutex_unlock(&dad->sysfs_lock);

        return size;
}

static CY_ATTR(noise, S_IRUSR, cyttsp6_noise_show, NULL);

static ssize_t cyttsp6_auto_shorts_show(struct device *dev,
		struct cyttsp6_attribute *attr, char *buf)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int status = STATUS_FAIL;
	u8 summary_result = 0;
	u8 cmd_status = 0;
	u16 act_length = 0;
	int length;
	int size;
	int rc;

	mutex_lock(&dad->sysfs_lock);

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request exclusive r=%d\n",
				__func__, rc);
		goto put_pm_runtime;
	}

	rc = cmd->request_set_mode(dev, CY_MODE_CAT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request set mode to CAT r=%d\n",
				__func__, rc);
		goto release_exclusive;
	}

	rc = _cyttsp6_run_autoshorts_self_test_cmd(dev, &cmd_status,
			&summary_result, NULL);
	if (rc < 0) {
		dev_err(dev, "%s: Error on run auto shorts self test r=%d\n",
				__func__, rc);
		goto set_mode_to_operational;
	}

	/* Form response buffer */
	dad->ic_buf[0] = cmd_status;
	dad->ic_buf[1] = summary_result;

	length = 2;

	/* Get data unless test result is success */
	if (cmd_status == CY_CMD_STATUS_SUCCESS
			&& summary_result == CY_ST_RESULT_PASS)
		goto status_success;

	/* Set length to 255 to read all */
	rc = _cyttsp6_get_autoshorts_self_test_results_cmd(dev, 0, 255,
			&cmd_status, &act_length, NULL, &dad->ic_buf[5]);
	if (rc < 0) {
		dev_err(dev, "%s: Error on get auto shorts self test results r=%d\n",
				__func__, rc);
		goto set_mode_to_operational;
	}

	dad->ic_buf[2] = cmd_status;
	dad->ic_buf[3] = HI_BYTE(act_length);
	dad->ic_buf[4] = LO_BYTE(act_length);

	length = 5 + act_length;

status_success:
	status = STATUS_SUCCESS;

set_mode_to_operational:
	cmd->request_set_mode(dev, CY_MODE_OPERATIONAL);

release_exclusive:
	cmd->release_exclusive(dev);

put_pm_runtime:
	pm_runtime_put(dev);

	if (status == STATUS_FAIL)
		length = 0;

	size = prepare_print_buffer(status, dad->ic_buf, length, buf);

	mutex_unlock(&dad->sysfs_lock);

	return size;
}

static CY_ATTR(auto_shorts, S_IRUSR,
	cyttsp6_auto_shorts_show, NULL);

static int _cyttsp6_run_panel_test(struct device *dev,
		u8 test_type, u8 *status, u8 *summary_result)
{
	u8 test_id = CY_ST_ID_CM_PANEL_TEST + test_type;
	return _cyttsp6_run_cap_self_test_cmd(dev,
			test_id, status, summary_result, NULL);
}

static int _cyttsp6_run_button_test(struct device *dev,
		u8 test_type, u8 *status, u8 *summary_result)
{
	u8 test_id = CY_ST_ID_CM_BUTTON_TEST + test_type;
	return _cyttsp6_run_cap_self_test_cmd(dev,
			test_id, status, summary_result, NULL);
}


static int _cyttsp6_get_panel_test_results(struct device *dev,
		u8 test_type, u8 *status, u16 *act_length, u8 *data)
{
	u8 test_id = CY_ST_ID_CM_PANEL_TEST + test_type;

	/* Set length to Max */
	return _cyttsp6_get_cap_self_test_results_cmd(dev, test_id,
			0, 65535 , status, act_length, data);
}

static int _cyttsp6_get_button_test_results(struct device *dev,
		u8 test_type, u8 *status, u16 *act_length, u8 *data)
{
	u8 test_id = CY_ST_ID_CM_BUTTON_TEST + test_type;
	/* Set length to Max */
	return _cyttsp6_get_cap_self_test_results_cmd(dev, test_id,
			0, 65535 , status, act_length, data);
}


static ssize_t cyttsp6_capacitance_show(struct device *dev,
		struct cyttsp6_attribute *attr, char *buf)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int status = STATUS_FAIL;
	u8 cmd_status = 0;
	u8 summary_result = 0;
	u16 act_length = 0;
	int length;
	int size;
	int rc;

	mutex_lock(&dad->sysfs_lock);

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request exclusive r=%d\n",
				__func__, rc);
		goto put_pm_runtime;
	}

	rc = cmd->request_set_mode(dev, CY_MODE_CAT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request set mode to CAT r=%d\n",
				__func__, rc);
		goto release_exclusive;
	}

	/* Run capacitance Self Test */
	if (dad->cap_target == CAP_TARGET_PANEL)
		rc = _cyttsp6_run_panel_test(dev, dad->cap_test_type,
				&cmd_status, &summary_result);
	else if (dad->cap_target == CAP_TARGET_BUTTON)
		rc = _cyttsp6_run_button_test(dev, dad->cap_test_type,
				&cmd_status, &summary_result);
	else
		rc = -EINVAL;

	if (rc < 0) {
		dev_err(dev, "%s: Error on run capacitance self test r=%d\n",
				__func__, rc);
		goto set_mode_to_operational;
	}

	/* Form response buffer */
	dad->ic_buf[0] = cmd_status;
	dad->ic_buf[1] = summary_result;

	length = 2;

	/* Get data unless test result is success */
	if (cmd_status == CY_CMD_STATUS_SUCCESS
			&& summary_result == CY_ST_RESULT_PASS)
		goto status_success;

	/* Get capacitance Self Test Results */
	if (dad->cap_target == CAP_TARGET_PANEL)
		rc = _cyttsp6_get_panel_test_results(dev, dad->cap_test_type,
				&cmd_status, &act_length, &dad->ic_buf[5]);
	else if (dad->cap_target == CAP_TARGET_BUTTON)
		rc = _cyttsp6_get_button_test_results(dev, dad->cap_test_type,
				&cmd_status, &act_length, &dad->ic_buf[5]);
	else
		rc = -EINVAL;

	if (rc < 0) {
		dev_err(dev, "%s: Error on get capacitance self test results r=%d\n",
				__func__, rc);
		goto set_mode_to_operational;
	}

	dad->ic_buf[2] = cmd_status;
	dad->ic_buf[3] = HI_BYTE(act_length);
	dad->ic_buf[4] = LO_BYTE(act_length);

	length = 5 + act_length;

status_success:
	status = STATUS_SUCCESS;

set_mode_to_operational:
	cmd->request_set_mode(dev, CY_MODE_OPERATIONAL);

release_exclusive:
	cmd->release_exclusive(dev);

put_pm_runtime:
	pm_runtime_put(dev);

	if (status == STATUS_FAIL)
		length = 0;

	size = prepare_print_buffer(status, dad->ic_buf, length, buf);

	mutex_unlock(&dad->sysfs_lock);

	return size;
}

static ssize_t cyttsp6_capacitance_store(struct device *dev,
		struct cyttsp6_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	ssize_t length;
	u8 target;
	u8 test_type;
	int rc = 0;

	mutex_lock(&dad->sysfs_lock);

	length = cyttsp6_ic_parse_input(dev, buf, size, dad->ic_buf,
			CY_MAX_PRBUF_SIZE);
	if (length != 2) {
		dev_err(dev, "%s: Malformed input\n", __func__);
		rc = -EINVAL;
		goto exit_unlock;
	}

	/* Check test type */
	test_type = dad->ic_buf[0];
	if ( test_type != CAP_TEST_TYPE_MUTUAL
			&& test_type != CAP_TEST_TYPE_SELF) {
		dev_err(dev, "%s: Invalid test type\n", __func__);
		rc = -EINVAL;
		goto exit_unlock;
	}

	/* Check target */
	target = dad->ic_buf[1];
	if ( target != CAP_TARGET_PANEL
			&& target != CAP_TARGET_BUTTON) {
		dev_err(dev, "%s: Invalid target type\n", __func__);
		rc = -EINVAL;
		goto exit_unlock;
	}

	dad->cap_test_type = test_type;
	dad->cap_target = target;

exit_unlock:
	mutex_unlock(&dad->sysfs_lock);

	if (rc)
		return rc;

	return size;
}

static CY_ATTR(capacitance, S_IRUSR | S_IWUSR,
	cyttsp6_capacitance_show, cyttsp6_capacitance_store);

static ssize_t cyttsp6_calibrate_show(struct device *dev,
		struct cyttsp6_attribute *attr, char *buf)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int status = STATUS_FAIL;
	int length;
	int size;
	int rc;

	mutex_lock(&dad->sysfs_lock);

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request exclusive r=%d\n",
				__func__, rc);
		goto put_pm_runtime;
	}

	rc = cmd->request_set_mode(dev, CY_MODE_CAT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request set mode to CAT r=%d\n",
				__func__, rc);
		goto release_exclusive;
	}

	rc = _cyttsp6_calibrate_idacs_cmd(dev, dad->calibrate_sensing_mode,
			&dad->ic_buf[0]);
	if (rc < 0) {
		dev_err(dev, "%s: Error on calibrate idacs r=%d\n",
				__func__, rc);
		goto set_mode_to_operational;
	}

	length = 1;

	/* Check if baseline initialization is requested */
	if (dad->calibrate_initialize_baselines) {
		/* Perform baseline initialization for all modes */
		rc = _cyttsp6_initialize_baselines_cmd(dev, CY_IB_SM_MUTCAP |
				CY_IB_SM_SELFCAP | CY_IB_SM_BUTTON,
				&dad->ic_buf[length]);
		if (rc < 0) {
			dev_err(dev, "%s: Error on initialize baselines r=%d\n",
					__func__, rc);
			goto set_mode_to_operational;
		}

		length++;
	}

	status = STATUS_SUCCESS;

set_mode_to_operational:
	cmd->request_set_mode(dev, CY_MODE_OPERATIONAL);

release_exclusive:
	cmd->release_exclusive(dev);

put_pm_runtime:
	pm_runtime_put(dev);

	if (status == STATUS_FAIL)
		length = 0;

	size = prepare_print_buffer(status, dad->ic_buf, length, buf);

	mutex_unlock(&dad->sysfs_lock);

	return size;
}

static ssize_t cyttsp6_calibrate_store(struct device *dev,
		struct cyttsp6_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	ssize_t length;
	int rc = 0;

	mutex_lock(&dad->sysfs_lock);

	length = cyttsp6_ic_parse_input(dev, buf, size, dad->ic_buf,
			CY_MAX_PRBUF_SIZE);
	if (length != 2) {
		dev_err(dev, "%s: Malformed input\n", __func__);
		rc = -EINVAL;
		goto exit_unlock;
	}

	dad->calibrate_sensing_mode = dad->ic_buf[0];
	dad->calibrate_initialize_baselines = dad->ic_buf[1];

exit_unlock:
	mutex_unlock(&dad->sysfs_lock);

	if (rc)
		return rc;

	return size;
}

static CY_ATTR(calibrate, S_IRUSR | S_IWUSR,
	cyttsp6_calibrate_show, cyttsp6_calibrate_store);

static ssize_t cyttsp6_baseline_show(struct device *dev,
		struct cyttsp6_attribute *attr, char *buf)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int status = STATUS_FAIL;
	int length;
	int size;
	int rc;

	mutex_lock(&dad->sysfs_lock);

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, CY_DA_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request exclusive r=%d\n",
				__func__, rc);
		goto put_pm_runtime;
	}

	rc = cmd->request_set_mode(dev, CY_MODE_CAT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request set mode to CAT r=%d\n",
				__func__, rc);
		goto release_exclusive;
	}

	rc = _cyttsp6_initialize_baselines_cmd(dev, dad->baseline_sensing_mode,
			&dad->ic_buf[0]);
	if (rc < 0) {
		dev_err(dev, "%s: Error on initialize baselines r=%d\n",
				__func__, rc);
		goto set_mode_to_operational;
	}

	length = 1;

	status = STATUS_SUCCESS;

set_mode_to_operational:
	cmd->request_set_mode(dev, CY_MODE_OPERATIONAL);

release_exclusive:
	cmd->release_exclusive(dev);

put_pm_runtime:
	pm_runtime_put(dev);

	if (status == STATUS_FAIL)
		length = 0;

	size = prepare_print_buffer(status, dad->ic_buf, length, buf);

	mutex_unlock(&dad->sysfs_lock);

	return size;
}

static ssize_t cyttsp6_baseline_store(struct device *dev,
		struct cyttsp6_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	ssize_t length;
	int rc = 0;

	mutex_lock(&dad->sysfs_lock);

	length = cyttsp6_ic_parse_input(dev, buf, size, dad->ic_buf,
			CY_MAX_PRBUF_SIZE);
	if (length != 1) {
		dev_err(dev, "%s: Malformed input\n", __func__);
		rc = -EINVAL;
		goto exit_unlock;
	}

	dad->baseline_sensing_mode = dad->ic_buf[0];

exit_unlock:
	mutex_unlock(&dad->sysfs_lock);

	if (rc)
		return rc;

	return size;
}

static CY_ATTR(baseline, S_IRUSR | S_IWUSR,
	cyttsp6_baseline_show, cyttsp6_baseline_store);

static int cyttsp6_setup_sysfs(struct device *dev)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int rc = 0;

	rc = device_create_file(dev, &dev_attr_ic_grpnum);
	if (rc) {
		dev_err(dev, "%s: Error, could not create ic_grpnum\n",
				__func__);
		goto exit;
	}

	rc = device_create_file(dev, &dev_attr_ic_grpoffset);
	if (rc) {
		dev_err(dev, "%s: Error, could not create ic_grpoffset\n",
				__func__);
		goto unregister_grpnum;
	}

	rc = device_create_file(dev, &dev_attr_ic_grpdata);
	if (rc) {
		dev_err(dev, "%s: Error, could not create ic_grpdata\n",
				__func__);
		goto unregister_grpoffset;
	}

	rc = device_create_file(dev, &dev_attr_get_panel_data);
	if (rc) {
		dev_err(dev, "%s: Error, could not create get_panel_data\n",
				__func__);
		goto unregister_grpdata;
	}

	rc = kobject_init_and_add(&dad->mfg_test, &cyttsp6_ktype, &dev->kobj,
			"mfg_test");
	if (rc) {
		dev_err(dev, "Unable to create mfg_test kobject\n");
		goto unregister_get_panel_data;
	}

	rc = cyttsp6_create_file(dev, &cy_attr_panel_scan);
	if (rc) {
		dev_err(dev, "%s: Error, could not create panel_scan\n",
				__func__);
		goto put_kobject;
	}

	rc = cyttsp6_create_file(dev, &cy_attr_auto_shorts);
	if (rc) {
		dev_err(dev, "%s: Error, could not create auto_shorts\n",
				__func__);
		goto unregister_panel_scan;
	}

	rc = cyttsp6_create_file(dev, &cy_attr_capacitance);
	if (rc) {
		dev_err(dev, "%s: Error, could not create capacitance\n",
				__func__);
		goto unregister_auto_shorts;
	}

	rc = cyttsp6_create_file(dev, &cy_attr_get_idac);
	if (rc) {
		dev_err(dev, "%s: Error, could not create get_idac\n",
				__func__);
		goto unregister_capacitance;
	}

	rc = cyttsp6_create_file(dev, &cy_attr_calibrate);
	if (rc) {
		dev_err(dev, "%s: Error, could not create calibrate\n",
				__func__);
		goto unregister_get_idac;
	}

	rc = cyttsp6_create_file(dev, &cy_attr_baseline);
	if (rc) {
		dev_err(dev, "%s: Error, could not create baseline\n",
				__func__);
		goto unregister_calibrate;
	}

	rc = cyttsp6_create_file(dev, &cy_attr_gesture);
        if (rc) {
                dev_err(dev, "%s: Error, could not create baseline\n",
                                __func__);
                goto unregister_baseline;
        }
	rc = cyttsp6_create_file(dev, &cy_attr_noise);
        if (rc) {
               dev_err(dev, "%s: Error, could not create baseline\n",
                                __func__);
                goto unregister_gesture;
        }
	dad->sysfs_nodes_created = true;

	return rc;

unregister_gesture:
        cyttsp6_remove_file(dev, &cy_attr_gesture);
unregister_baseline:
        cyttsp6_remove_file(dev, &cy_attr_baseline);
unregister_calibrate:
	cyttsp6_remove_file(dev, &cy_attr_calibrate);
unregister_get_idac:
	cyttsp6_remove_file(dev, &cy_attr_get_idac);
unregister_capacitance:
	cyttsp6_remove_file(dev, &cy_attr_capacitance);
unregister_auto_shorts:
	cyttsp6_remove_file(dev, &cy_attr_auto_shorts);
unregister_panel_scan:
	cyttsp6_remove_file(dev, &cy_attr_panel_scan);
put_kobject:
	kobject_put(&dad->mfg_test);
unregister_get_panel_data:
	device_remove_file(dev, &dev_attr_get_panel_data);
unregister_grpdata:
	device_remove_file(dev, &dev_attr_ic_grpdata);
unregister_grpoffset:
	device_remove_file(dev, &dev_attr_ic_grpoffset);
unregister_grpnum:
	device_remove_file(dev, &dev_attr_ic_grpnum);
exit:
	return rc;
}

static int cyttsp6_setup_sysfs_attention(struct device *dev)
{
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	int rc = 0;

	dad->si = cmd->request_sysinfo(dev);
	if (!dad->si)
		return -EINVAL;

	rc = cyttsp6_setup_sysfs(dev);

	cmd->unsubscribe_attention(dev, CY_ATTEN_STARTUP,
		CY_MODULE_DEVICE_ACCESS, cyttsp6_setup_sysfs_attention, 0);

	return rc;
}

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_DEVICE_ACCESS_API
int cyttsp6_device_access_read_command(const char *core_name, int ic_grpnum,
		int ic_grpoffset, u8 *buf, int buf_size)
{
	struct cyttsp6_core_data *cd;
	struct cyttsp6_device_access_data *dad;
	struct device *dev;
	int rc;

	might_sleep();

	/* Validate ic_grpnum */
	if (ic_grpnum >= CY_IC_GRPNUM_NUM) {
		pr_err("%s: Group %d does not exist.\n", __func__, ic_grpnum);
		return -EINVAL;
	}

	/* Validate ic_grpoffset */
	if (ic_grpoffset > 0xFFFF) {
		pr_err("%s: Offset %d invalid.\n", __func__, ic_grpoffset);
		return -EINVAL;
	}

	if (!core_name)
		core_name = CY_DEFAULT_CORE_ID;

	/* Find device */
	cd = cyttsp6_get_core_data((char *)core_name);
	if (!cd) {
		pr_err("%s: No device.\n", __func__);
		return -ENODEV;
	}

	dev = cd->dev;
	dad = cyttsp6_get_device_access_data(dev);

	/* Check sysinfo */
	if (!dad->si) {
		pr_err("%s: No sysinfo.\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&dad->sysfs_lock);
	/*
	 * Block grpnum change when own_exclusive flag is set
	 * which means the current grpnum implementation requires
	 * running exclusively on some consecutive grpdata operations
	 */
	if (dad->own_exclusive && dad->ic_grpnum != ic_grpnum) {
		dev_err(dev, "%s: own_exclusive\n", __func__);
		rc = -EBUSY;
		goto exit;
	}

	dad->ic_grpnum = ic_grpnum;
	dad->ic_grpoffset = ic_grpoffset;

	rc = cyttsp6_grpdata_show_functions[dad->ic_grpnum] (dev,
			buf, buf_size);

exit:
	mutex_unlock(&dad->sysfs_lock);

	return rc;
}
EXPORT_SYMBOL_GPL(cyttsp6_device_access_read_command);

int cyttsp6_device_access_write_command(const char *core_name, int ic_grpnum,
		int ic_grpoffset, u8 *buf, int length)
{
	struct cyttsp6_core_data *cd;
	struct cyttsp6_device_access_data *dad;
	struct device *dev;
	int rc;

	might_sleep();

	/* Validate ic_grpnum */
	if (ic_grpnum >= CY_IC_GRPNUM_NUM) {
		pr_err("%s: Group %d does not exist.\n", __func__, ic_grpnum);
		return -EINVAL;
	}

	/* Validate ic_grpoffset */
	if (ic_grpoffset > 0xFFFF) {
		pr_err("%s: Offset %d invalid.\n", __func__, ic_grpoffset);
		return -EINVAL;
	}

	if (!core_name)
		core_name = CY_DEFAULT_CORE_ID;

	/* Find device */
	cd = cyttsp6_get_core_data((char *)core_name);
	if (!cd) {
		pr_err("%s: No device.\n", __func__);
		return -ENODEV;
	}

	dev = cd->dev;
	dad = cyttsp6_get_device_access_data(dev);

	/* Check sysinfo */
	if (!dad->si) {
		pr_err("%s: No sysinfo.\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&dad->sysfs_lock);
	/*
	 * Block grpnum change when own_exclusive flag is set
	 * which means the current grpnum implementation requires
	 * running exclusively on some consecutive grpdata operations
	 */
	if (dad->own_exclusive && dad->ic_grpnum != ic_grpnum) {
		dev_err(dev, "%s: own_exclusive\n", __func__);
		rc = -EBUSY;
		goto exit;
	}

	dad->ic_grpnum = ic_grpnum;
	dad->ic_grpoffset = ic_grpoffset;

	/* write ic_buf to log */
	cyttsp6_pr_buf(dev, NULL, buf, length, "ic_buf");

	/* Call relevant store handler. */
	rc = cyttsp6_grpdata_store_functions[dad->ic_grpnum](dev, buf,
			length);
	if (rc < 0)
		dev_err(dev, "%s: Failed to store for grpnum=%d.\n",
				__func__, dad->ic_grpnum);

exit:
	mutex_unlock(&dad->sysfs_lock);

	return rc;
}
EXPORT_SYMBOL_GPL(cyttsp6_device_access_write_command);

struct command_work {
	struct work_struct work;
	const char *core_name;
	int ic_grpnum;
	int ic_grpoffset;
	u8 *buf;
	int length;
	void (*cont)(const char *core_name, int ic_grpnum,
		int ic_grpoffset, u8 *buf, int length, int rc);
	bool read;
};

static void cyttsp6_device_access_command_work_func(
		struct work_struct *work)
{
	struct command_work *cmd_work =
			container_of(work, struct command_work, work);
	int rc;

	if (cmd_work->read)
		rc = cyttsp6_device_access_read_command(cmd_work->core_name,
				cmd_work->ic_grpnum, cmd_work->ic_grpoffset,
				cmd_work->buf, cmd_work->length);
	else
		rc = cyttsp6_device_access_write_command(cmd_work->core_name,
				cmd_work->ic_grpnum, cmd_work->ic_grpoffset,
				cmd_work->buf, cmd_work->length);

	if (cmd_work->cont)
		cmd_work->cont(cmd_work->core_name, cmd_work->ic_grpnum,
				cmd_work->ic_grpoffset, cmd_work->buf,
				cmd_work->length, rc);

	kfree(cmd_work);
}

static int cyttsp6_device_access_command_async(const char *core_name,
		int ic_grpnum, int ic_grpoffset, u8 *buf, int length,
		void (*cont)(const char *core_name, int ic_grpnum,
			int ic_grpoffset, u8 *buf, int length, int rc),
		bool read)
{
	struct command_work *cmd_work;

	cmd_work = kzalloc(sizeof(*cmd_work), GFP_ATOMIC);
	if (!cmd_work)
		return -ENOMEM;

	cmd_work->core_name = core_name;
	cmd_work->ic_grpnum = ic_grpnum;
	cmd_work->ic_grpoffset = ic_grpoffset;
	cmd_work->buf = buf;
	cmd_work->length = length;
	cmd_work->cont = cont;
	cmd_work->read = read;

	INIT_WORK(&cmd_work->work,
			cyttsp6_device_access_command_work_func);
	schedule_work(&cmd_work->work);

	return 0;
}

int cyttsp6_device_access_read_command_async(const char *core_name,
		int ic_grpnum, int ic_grpoffset, u8 *buf, int length,
		void (*cont)(const char *core_name, int ic_grpnum,
			int ic_grpoffset, u8 *buf, int length, int rc))
{
	return cyttsp6_device_access_command_async(core_name, ic_grpnum,
			ic_grpoffset, buf, length, cont, true);
}
EXPORT_SYMBOL_GPL(cyttsp6_device_access_read_command_async);

int cyttsp6_device_access_write_command_async(const char *core_name,
		int ic_grpnum, int ic_grpoffset, u8 *buf, int length,
		void (*cont)(const char *core_name, int ic_grpnum,
			int ic_grpoffset, u8 *buf, int length, int rc))
{
	return cyttsp6_device_access_command_async(core_name, ic_grpnum,
			ic_grpoffset, buf, length, cont, false);
}
EXPORT_SYMBOL_GPL(cyttsp6_device_access_write_command_async);
#endif

static int cyttsp6_device_access_probe(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_device_access_data *dad;
	int rc = 0;

	dad = kzalloc(sizeof(*dad), GFP_KERNEL);
	if (!dad) {
		rc = -ENOMEM;
		goto cyttsp6_device_access_probe_data_failed;
	}

	mutex_init(&dad->sysfs_lock);
	init_waitqueue_head(&dad->wait_q);
	dad->dev = dev;
	dad->ic_grpnum = CY_IC_GRPNUM_TCH_REP;
	dad->test.cur_cmd = -1;
	dad->heatmap.num_element = 200;
	cd->cyttsp6_dynamic_data[CY_MODULE_DEVICE_ACCESS] = dad;

	/* get sysinfo */
	dad->si = cmd->request_sysinfo(dev);
	if (dad->si) {
		rc = cyttsp6_setup_sysfs(dev);
		if (rc)
			goto cyttsp6_device_access_setup_sysfs_failed;
	} else {
		dev_err(dev, "%s: Fail get sysinfo pointer from core p=%p\n",
				__func__, dad->si);
		cmd->subscribe_attention(dev, CY_ATTEN_STARTUP,
			CY_MODULE_DEVICE_ACCESS, cyttsp6_setup_sysfs_attention,
			0);
	}

	return 0;

 cyttsp6_device_access_setup_sysfs_failed:
	cd->cyttsp6_dynamic_data[CY_MODULE_DEVICE_ACCESS] = NULL;
	kfree(dad);
 cyttsp6_device_access_probe_data_failed:
	dev_err(dev, "%s failed.\n", __func__);

	return rc;
}

static int cyttsp6_device_access_release(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_device_access_data *dad
		= cyttsp6_get_device_access_data(dev);
	u8 ic_buf[CY_NULL_CMD_MODE_INDEX + 1];

	if (dad->own_exclusive) {
		dev_err(dev, "%s: Can't unload in CAT mode. First switch back to Operational mode\n"
				, __func__);
		ic_buf[CY_NULL_CMD_MODE_INDEX] = CY_HST_OPERATE;
		cyttsp6_test_cmd_mode(dad, ic_buf, CY_NULL_CMD_MODE_INDEX + 1);
	}

	if (dad->sysfs_nodes_created) {
		device_remove_file(dev, &dev_attr_ic_grpnum);
		device_remove_file(dev, &dev_attr_ic_grpoffset);
		device_remove_file(dev, &dev_attr_ic_grpdata);
		device_remove_file(dev, &dev_attr_get_panel_data);
		cyttsp6_remove_file(dev, &cy_attr_panel_scan);
		cyttsp6_remove_file(dev, &cy_attr_auto_shorts);
		cyttsp6_remove_file(dev, &cy_attr_capacitance);
		cyttsp6_remove_file(dev, &cy_attr_get_idac);
		cyttsp6_remove_file(dev, &cy_attr_calibrate);
		cyttsp6_remove_file(dev, &cy_attr_baseline);
		kobject_put(&dad->mfg_test);
	} else {
		cmd->unsubscribe_attention(dev, CY_ATTEN_STARTUP,
			CY_MODULE_DEVICE_ACCESS, cyttsp6_setup_sysfs_attention,
			0);
	}

	cd->cyttsp6_dynamic_data[CY_MODULE_DEVICE_ACCESS] = NULL;
	kfree(dad);

	return 0;
}

static char *core_ids[CY_MAX_NUM_CORE_DEVS] = {
	CY_DEFAULT_CORE_ID,
	NULL,
	NULL,
	NULL,
	NULL
};

static int num_core_ids = 1;

module_param_array(core_ids, charp, &num_core_ids, 0);
MODULE_PARM_DESC(core_ids,
	"Core id list of cyttsp6 core devices for device access module");

static int __init cyttsp6_device_access_init(void)
{
	struct cyttsp6_core_data *cd;
	int rc = 0;
	int i, j;

	/* Check for invalid or duplicate core_ids */
	for (i = 0; i < num_core_ids; i++) {
		if (!strlen(core_ids[i])) {
			pr_err("%s: core_id %d is empty\n",
				__func__, i+1);
			return -EINVAL;
		}
		for (j = i+1; j < num_core_ids; j++)
			if (!strcmp(core_ids[i], core_ids[j])) {
				pr_err("%s: core_ids %d and %d are same\n",
					__func__, i+1, j+1);
				return -EINVAL;
			}
	}

	cmd = cyttsp6_get_commands();
	if (!cmd)
		return -EINVAL;

	for (i = 0; i < num_core_ids; i++) {
		cd = cyttsp6_get_core_data(core_ids[i]);
		if (!cd)
			continue;
		pr_info("%s: Registering device access module for core_id: %s\n",
			__func__, core_ids[i]);
		rc = cyttsp6_device_access_probe(cd->dev);
		if (rc < 0) {
			pr_err("%s: Error, failed registering module\n",
				__func__);
			goto fail_unregister_devices;
		}
	}

	pr_info("%s: Cypress TTSP Device Access Driver (Built %s) rc=%d\n",
		 __func__, CY_DRIVER_VERSION, rc);

	return 0;

fail_unregister_devices:
	for (i--; i >= 0; i--) {
		cd = cyttsp6_get_core_data(core_ids[i]);
		if (!cd)
			continue;
		cyttsp6_device_access_release(cd->dev);
		pr_info("%s: Unregistering device access module for core_id: %s\n",
			__func__, core_ids[i]);
	}

	return rc;
}
module_init(cyttsp6_device_access_init);

static void __exit cyttsp6_device_access_exit(void)
{
	struct cyttsp6_core_data *cd;
	int i;

	for (i = 0; i < num_core_ids; i++) {
		cd = cyttsp6_get_core_data(core_ids[i]);
		if (!cd)
			continue;
		cyttsp6_device_access_release(cd->dev);
		pr_info("%s: Unregistering device access module for core_id: %s\n",
			__func__, core_ids[i]);
	}
}
module_exit(cyttsp6_device_access_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard Product Device Access Driver");
MODULE_AUTHOR("Cypress Semiconductor <ttdrivers@cypress.com>");
