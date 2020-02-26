/*
 * cyttsp6_core.c
 * Cypress TrueTouch(TM) Standard Product V4 Core Module.
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

#define CY_CORE_STARTUP_RETRY_COUNT		3
/*
 * If platform init (cd->cpdata->init) function resets or powers on the device,
 * then set this flag as true.
 */
#define CY_CORE_DEVICE_RESET_BY_PLATFORM_INIT	false

static const u8 security_key[] = {
	0xA5, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFD, 0x5A
};

static const u8 ldr_exit[] = {
	CY_CMD_LDR_HOST_SYNC, CY_START_OF_PACKET, CY_CMD_LDR_EXIT, 0x00, 0x00,
	0x4F, 0x6D, CY_END_OF_PACKET
};

static const u8 ldr_fast_exit[] = {
	CY_CMD_LDR_HOST_SYNC, CY_START_OF_PACKET, CY_CMD_LDR_FAST_EXIT,
	0x00, 0x00, 0xC3, 0x68, CY_END_OF_PACKET
};

static const u8 ldr_err_app[] = {
	CY_START_OF_PACKET, 0x02, 0x00, 0x00, 0x55, 0xDD, CY_END_OF_PACKET
};

MODULE_FIRMWARE(CY_FW_FILE_NAME);

struct atten_node {
	struct list_head node;
	char id;
	int (*func)(struct device *);
	struct device *dev;
	int mode;
};

static inline size_t merge_bytes(u8 high, u8 low)
{
	return (high << 8) + low;
}

#ifdef VERBOSE_DEBUG
void cyttsp6_pr_buf(struct device *dev, u8 *pr_buf, u8 *dptr, int size,
		const char *data_name)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	int i, k;
	const char fmt[] = "%02X ";
	int max;

	if (!size)
		return;

	if (!pr_buf) {
		if (cd->pr_buf)
			pr_buf = cd->pr_buf;
		else
			return;
	}

	max = (CY_MAX_PRBUF_SIZE - 1) - sizeof(CY_PR_TRUNCATED);

	pr_buf[0] = 0;
	for (i = k = 0; i < size && k < max; i++, k += 3)
		scnprintf(pr_buf + k, CY_MAX_PRBUF_SIZE, fmt, dptr[i]);

	dev_vdbg(dev, "%s:  %s[0..%d]=%s%s\n", __func__, data_name, size - 1,
			pr_buf, size <= max ? "" : CY_PR_TRUNCATED);
}
EXPORT_SYMBOL_GPL(cyttsp6_pr_buf);
#endif

static inline int cyttsp6_adap_read(struct cyttsp6_core_data *cd, u16 addr,
		void *buf, int size)
{
	return cd->bus_ops->read(cd->dev, addr, buf, size, cd->max_xfer);
}

static inline int cyttsp6_adap_write(struct cyttsp6_core_data *cd, u16 addr,
		const void *buf, int size)
{
	return cd->bus_ops->write(cd->dev, addr, cd->wr_buf, buf, size,
			cd->max_xfer);
}

/* cyttsp6_platform_detect_read()
 *
 * This function is passed to platform detect
 * function to perform a read operation
 */
static int cyttsp6_platform_detect_read(struct device *dev, u16 addr,
		void *buf, int size)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	return cd->bus_ops->read(cd->dev, addr, buf, size, cd->max_xfer);
}

static const u8 *cyttsp6_get_security_key_(struct device *dev,
		int *size)
{
	if (size)
		*size = sizeof(security_key);

	return security_key;
}

static u16 cyttsp6_calc_app_crc(const u8 *data, int size)
{
	int i, j;
	u16 crc = 0xFFFF;

	for (i = 0; i < size; i++) {
		crc ^= ((u16)data[i] << 8);
		for (j = 8; j > 0; j--)
			if (crc & 0x8000)
				crc = (crc << 1) ^ 0x1021;
			else
				crc <<= 1;
	}

	return crc;
}

static inline void cyttsp6_get_touch_axis(struct cyttsp6_core_data *cd,
		int *axis, int size, int max, u8 *xy_data, int bofs)
{
	int nbyte;
	int next = 0;

	for (nbyte = 0, *axis = 0; nbyte < size; nbyte++) {
		dev_vdbg(cd->dev,
			"%s: *axis=%02X(%d) size=%d max=%08X xy_data=%p xy_data[%d]=%02X(%d) bofs=%d\n",
			__func__, *axis, *axis, size, max, xy_data, next,
			xy_data[next], xy_data[next], bofs);
		*axis = (*axis << 8) + (xy_data[next] >> bofs);
		next++;
	}

	*axis &= max - 1;

	dev_vdbg(cd->dev,
		"%s: *axis=%02X(%d) size=%d max=%08X xy_data=%p xy_data[%d]=%02X(%d)\n",
		__func__, *axis, *axis, size, max, xy_data, next,
		xy_data[next], xy_data[next]);
}

/*
 * cyttsp6_get_touch_record_()
 *
 * Fills touch info for a touch record specified by rec_no
 * Should only be called in Operational mode IRQ attention and
 * rec_no should be less than the number of current touch records
 */
void cyttsp6_get_touch_record_(struct device *dev, int rec_no, int *rec_abs)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_sysinfo *si = &cd->sysinfo;
	u8 *xy_data = si->xy_data + (rec_no * si->si_ofs.tch_rec_size);
	enum cyttsp6_tch_abs abs;

	memset(rec_abs, 0, CY_TCH_NUM_ABS * sizeof(int));
	for (abs = CY_TCH_X; abs < CY_TCH_NUM_ABS; abs++) {
		cyttsp6_get_touch_axis(cd, &rec_abs[abs],
			si->si_ofs.tch_abs[abs].size,
			si->si_ofs.tch_abs[abs].max,
			xy_data + si->si_ofs.tch_abs[abs].ofs,
			si->si_ofs.tch_abs[abs].bofs);
		dev_vdbg(dev, "%s: get %s=%04X(%d)\n", __func__,
			cyttsp6_tch_abs_string[abs],
			rec_abs[abs], rec_abs[abs]);
	}
}

static int cyttsp6_load_status_and_touch_regs_(struct cyttsp6_core_data *cd,
		bool optimize)
{
	struct cyttsp6_sysinfo *si = &cd->sysinfo;
	struct device *dev = cd->dev;
	int first_read_len;
	int second_read_off;
	int num_read_rec;
	u8 num_cur_rec;
	u8 hst_mode;
	u8 rep_len;
	u8 rep_stat;
	u8 tt_stat;
	int rc;
#ifdef VERBOSE_DEBUG
	u8 rep_stat_counter;
#endif

	first_read_len = si->si_ofs.rep_hdr_size;
	/* Read one touch record additionally */
	if (optimize)
		first_read_len += si->si_ofs.tch_rec_size;

	rc = cyttsp6_adap_read(cd, si->si_ofs.rep_ofs,
			&si->xy_mode[si->si_ofs.rep_ofs], first_read_len);
	if (rc < 0) {
		dev_err(dev, "%s: reading touch record failed r=%d\n",
			__func__, rc);
		return rc;
	}

	/* print xy data */
	cyttsp6_pr_buf(dev, cd->pr_buf, si->xy_mode,
		si->si_ofs.mode_size, "xy_mode");

	hst_mode = si->xy_mode[CY_REG_BASE];
	rep_len = si->xy_mode[si->si_ofs.rep_ofs];
	rep_stat = si->xy_mode[si->si_ofs.rep_ofs + 1];
	tt_stat = si->xy_mode[si->si_ofs.tt_stat_ofs];
	dev_vdbg(dev, "%s: %s%02X %s%d %s%02X %s%02X\n", __func__,
		"hst_mode=", hst_mode, "rep_len=", rep_len,
		"rep_stat=", rep_stat, "tt_stat=", tt_stat);

#ifdef VERBOSE_DEBUG
	cd->rep_stat_counter = ((cd->rep_stat_counter + 1) & 0x3);
	rep_stat_counter = rep_stat >> 6;
	if (cd->rep_stat_counter != rep_stat_counter) {
		dev_vdbg(dev, "Possibly missed a touch record expected:%d device:%d\n",
				cd->rep_stat_counter, rep_stat_counter);
		cd->rep_stat_counter = rep_stat_counter;
	}
#endif

	num_cur_rec = GET_NUM_TOUCH_RECORDS(tt_stat);
	dev_vdbg(dev, "%s: num_cur_rec=%d\n", __func__, num_cur_rec);

	if (rep_len == 0 && num_cur_rec > 0) {
		dev_err(dev, "%s: report length error rep_len=%d num_rec=%d\n",
			__func__, rep_len, num_cur_rec);
		return -EIO;
	}

	if (num_cur_rec > si->si_ofs.max_tchs) {
		dev_err(dev, "%s: %s (n=%d c=%zu)\n", __func__,
			"too many tch; set to max tch",
			num_cur_rec, si->si_ofs.max_tchs);
		num_cur_rec = si->si_ofs.max_tchs;
	}

	num_read_rec = num_cur_rec;
	second_read_off = si->si_ofs.tt_stat_ofs + 1;
	if (optimize) {
		num_read_rec--;
		second_read_off += si->si_ofs.tch_rec_size;
	}

	if (num_read_rec <= 0)
		goto exit_print;

	rc = cyttsp6_adap_read(cd, second_read_off,
			&si->xy_mode[second_read_off],
			num_read_rec * si->si_ofs.tch_rec_size);
	if (rc < 0) {
		dev_err(dev, "%s: read fail on touch regs r=%d\n",
			__func__, rc);
		return rc;
	}

exit_print:
	/* print xy data */
	cyttsp6_pr_buf(dev, cd->pr_buf, si->xy_data,
		num_cur_rec * si->si_ofs.tch_rec_size, "xy_data");

	return 0;
}

static int cyttsp6_handshake_(struct cyttsp6_core_data *cd, u8 mode)
{
	u8 cmd = mode ^ CY_HST_TOGGLE;
	int rc;

	if (mode & CY_HST_MODE_CHANGE) {
		dev_err(cd->dev, "%s: Host mode change bit set, NO handshake\n",
				__func__);
		return 0;
	}

	rc = cyttsp6_adap_write(cd, CY_REG_BASE, &cmd, sizeof(cmd));
	if (rc < 0)
		dev_err(cd->dev, "%s: bus write fail on handshake (ret=%d)\n",
				__func__, rc);

	return rc;
}

static int cyttsp6_toggle_low_power_(struct cyttsp6_core_data *cd, u8 mode)
{
	u8 cmd = mode ^ CY_HST_LOWPOW;
	int rc;

	rc = cyttsp6_adap_write(cd, CY_REG_BASE, &cmd, sizeof(cmd));
	if (rc < 0)
		dev_err(cd->dev, "%s: bus write fail on toggle low power (ret=%d)\n",
			__func__, rc);

	return rc;
}

static int cyttsp6_hw_soft_reset_(struct cyttsp6_core_data *cd)
{
	u8 cmd = CY_HST_RESET;
	int rc;

	rc = cyttsp6_adap_write(cd, CY_REG_BASE, &cmd, sizeof(cmd));
	if (rc < 0) {
		dev_err(cd->dev, "%s: FAILED to execute SOFT reset\n",
				__func__);
		return rc;
	}
	dev_dbg(cd->dev, "%s: execute SOFT reset\n", __func__);

	return rc;
}

static int cyttsp6_hw_hard_reset(struct cyttsp6_core_data *cd)
{
	if (cd->cpdata->xres) {
		cd->cpdata->xres(cd->cpdata, cd->dev);
		dev_dbg(cd->dev, "%s: execute HARD reset\n", __func__);
		return 0;
	}
	dev_err(cd->dev, "%s: FAILED to execute HARD reset\n", __func__);

	return -ENOSYS;
}

static int cyttsp6_hw_reset_(struct cyttsp6_core_data *cd)
{
	int rc = cyttsp6_hw_hard_reset(cd);

	if (rc == -ENOSYS)
		rc = cyttsp6_hw_soft_reset_(cd);

	return rc;
}

static inline int cyttsp6_bits_2_bytes(int nbits, size_t *max)
{
	*max = 1 << nbits;

	return (nbits + 7) / 8;
}

static int cyttsp6_si_data_offsets_(struct cyttsp6_core_data *cd)
{
	struct cyttsp6_sysinfo *si = &cd->sysinfo;
	int rc;

	rc = cyttsp6_adap_read(cd, CY_REG_BASE, &si->si_data,
		sizeof(si->si_data));
	if (rc < 0) {
		dev_err(cd->dev, "%s: fail read sysinfo data offsets r=%d\n",
				__func__, rc);
		return rc;
	}

	/* Print sysinfo data offsets */
	cyttsp6_pr_buf(cd->dev, cd->pr_buf, (u8 *)&si->si_data,
		       sizeof(si->si_data), "sysinfo_data_offsets");

	/* convert sysinfo data offset bytes into integers */

	si->si_ofs.map_sz = merge_bytes(si->si_data.map_szh,
			si->si_data.map_szl);
	si->si_ofs.map_sz = merge_bytes(si->si_data.map_szh,
			si->si_data.map_szl);
	si->si_ofs.cydata_ofs = merge_bytes(si->si_data.cydata_ofsh,
			si->si_data.cydata_ofsl);
	si->si_ofs.test_ofs = merge_bytes(si->si_data.test_ofsh,
			si->si_data.test_ofsl);
	si->si_ofs.pcfg_ofs = merge_bytes(si->si_data.pcfg_ofsh,
			si->si_data.pcfg_ofsl);
	si->si_ofs.opcfg_ofs = merge_bytes(si->si_data.opcfg_ofsh,
			si->si_data.opcfg_ofsl);
	si->si_ofs.ddata_ofs = merge_bytes(si->si_data.ddata_ofsh,
			si->si_data.ddata_ofsl);
	si->si_ofs.mdata_ofs = merge_bytes(si->si_data.mdata_ofsh,
			si->si_data.mdata_ofsl);

	return rc;
}

static int cyttsp6_si_get_cydata_(struct cyttsp6_core_data *cd)
{
	struct cyttsp6_sysinfo *si = &cd->sysinfo;
	void *p;
	int rc;
	int read_offset;
	int mfgid_sz, calc_mfgid_sz;

	si->si_ofs.cydata_size = si->si_ofs.test_ofs - si->si_ofs.cydata_ofs;
	dev_dbg(cd->dev, "%s: cydata size: %zu\n", __func__,
			si->si_ofs.cydata_size);

	if (si->si_ofs.cydata_size <= 0)
		return -EINVAL;

	p = krealloc(si->si_ptrs.cydata, si->si_ofs.cydata_size, GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	si->si_ptrs.cydata = p;

	read_offset = si->si_ofs.cydata_ofs;

	/* Read the CYDA registers up to MFGID field */
	rc = cyttsp6_adap_read(cd, read_offset, si->si_ptrs.cydata,
			offsetof(struct cyttsp6_cydata, mfgid_sz)
			+ sizeof(si->si_ptrs.cydata->mfgid_sz));
	if (rc < 0) {
		dev_err(cd->dev, "%s: fail read cydata r=%d\n", __func__, rc);
		return rc;
	}

	/* Check MFGID size */
	mfgid_sz = si->si_ptrs.cydata->mfgid_sz;
	calc_mfgid_sz = si->si_ofs.cydata_size - sizeof(struct cyttsp6_cydata);
	if (mfgid_sz != calc_mfgid_sz) {
		dev_err(cd->dev, "%s: mismatch in MFGID size, reported:%d calculated:%d\n",
			__func__, mfgid_sz, calc_mfgid_sz);
		return -EINVAL;
	}

	read_offset += offsetof(struct cyttsp6_cydata, mfgid_sz)
			+ sizeof(si->si_ptrs.cydata->mfgid_sz);

	/* Read the CYDA registers for MFGID field */
	rc = cyttsp6_adap_read(cd, read_offset, si->si_ptrs.cydata->mfg_id,
			si->si_ptrs.cydata->mfgid_sz);
	if (rc < 0) {
		dev_err(cd->dev, "%s: fail read cydata r=%d\n", __func__, rc);
		return rc;
	}

	read_offset += si->si_ptrs.cydata->mfgid_sz;

	/* Read the rest of the CYDA registers */
	rc = cyttsp6_adap_read(cd, read_offset, &si->si_ptrs.cydata->cyito_idh,
			sizeof(struct cyttsp6_cydata)
			- offsetof(struct cyttsp6_cydata, cyito_idh));
	if (rc < 0) {
		dev_err(cd->dev, "%s: fail read cydata r=%d\n", __func__, rc);
		return rc;
	}

	cyttsp6_pr_buf(cd->dev, cd->pr_buf, (u8 *)si->si_ptrs.cydata,
		si->si_ofs.cydata_size - mfgid_sz, "sysinfo_cydata");
	cyttsp6_pr_buf(cd->dev, cd->pr_buf, si->si_ptrs.cydata->mfg_id,
		mfgid_sz, "sysinfo_cydata_mfgid");
	return rc;
}

static int cyttsp6_si_get_test_data_(struct cyttsp6_core_data *cd)
{
	struct cyttsp6_sysinfo *si = &cd->sysinfo;
	void *p;
	int rc;

	si->si_ofs.test_size = si->si_ofs.pcfg_ofs - si->si_ofs.test_ofs;

	if (si->si_ofs.test_size <= 0)
		return -EINVAL;

	p = krealloc(si->si_ptrs.test, si->si_ofs.test_size, GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	si->si_ptrs.test = p;

	rc = cyttsp6_adap_read(cd, si->si_ofs.test_ofs, si->si_ptrs.test,
			si->si_ofs.test_size);
	if (rc < 0) {
		dev_err(cd->dev, "%s: fail read test data r=%d\n",
			__func__, rc);
		return rc;
	}

	cyttsp6_pr_buf(cd->dev, cd->pr_buf,
		       (u8 *)si->si_ptrs.test, si->si_ofs.test_size,
		       "sysinfo_test_data");

	if (si->si_ptrs.test->post_codel & CY_POST_CODEL_WDG_RST)
		dev_info(cd->dev, "%s: Reset was a WATCHDOG RESET codel=%02X\n",
			 __func__, si->si_ptrs.test->post_codel);

	if (is_crc_stat_failed(si))
		dev_err(cd->dev, "%s: Config Data CRC FAIL codel=%02X\n",
				__func__, si->si_ptrs.test->post_codel);

	if (!(si->si_ptrs.test->post_codel & CY_POST_CODEL_PANEL_TEST_FAIL))
		dev_info(cd->dev, "%s: PANEL TEST Status=0 codel=%02X\n",
			 __func__, si->si_ptrs.test->post_codel);

	dev_info(cd->dev, "%s: SCANNING is %s codel=%02X\n", __func__,
		si->si_ptrs.test->post_codel & CY_POST_CODEL_SCAN_STATUS ?
		"ENABLED" : "DISABLED", si->si_ptrs.test->post_codel);

	return rc;
}

static int cyttsp6_si_get_pcfg_data_(struct cyttsp6_core_data *cd)
{
	struct cyttsp6_sysinfo *si = &cd->sysinfo;
	void *p;
	int rc;

	dev_vdbg(cd->dev, "%s: get pcfg data\n", __func__);
	si->si_ofs.pcfg_size = si->si_ofs.opcfg_ofs - si->si_ofs.pcfg_ofs;

	if (si->si_ofs.pcfg_size <= 0)
		return -EINVAL;

	p = krealloc(si->si_ptrs.pcfg, si->si_ofs.pcfg_size, GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	si->si_ptrs.pcfg = p;

	rc = cyttsp6_adap_read(cd, si->si_ofs.pcfg_ofs, si->si_ptrs.pcfg,
			si->si_ofs.pcfg_size);
	if (rc < 0) {
		dev_err(cd->dev, "%s: fail read pcfg data r=%d\n",
			__func__, rc);
		return rc;
	}

	si->si_ofs.max_x = merge_bytes((si->si_ptrs.pcfg->res_xh
			& CY_PCFG_RESOLUTION_X_MASK), si->si_ptrs.pcfg->res_xl);
	si->si_ofs.x_origin = !!(si->si_ptrs.pcfg->res_xh
			& CY_PCFG_ORIGIN_X_MASK);
	si->si_ofs.max_y = merge_bytes((si->si_ptrs.pcfg->res_yh
			& CY_PCFG_RESOLUTION_Y_MASK), si->si_ptrs.pcfg->res_yl);
	si->si_ofs.y_origin = !!(si->si_ptrs.pcfg->res_yh
			& CY_PCFG_ORIGIN_Y_MASK);
	si->si_ofs.max_p = merge_bytes(si->si_ptrs.pcfg->max_zh,
			si->si_ptrs.pcfg->max_zl);

	si->si_ofs.len_x = merge_bytes((si->si_ptrs.pcfg->len_xh
			& CY_PCFG_RESOLUTION_X_MASK), si->si_ptrs.pcfg->len_xl);

	cyttsp6_pr_buf(cd->dev, cd->pr_buf,
		       (u8 *)si->si_ptrs.pcfg,
		       si->si_ofs.pcfg_size, "sysinfo_pcfg_data");

	return rc;
}

static int cyttsp6_si_get_opcfg_data_(struct cyttsp6_core_data *cd)
{
	struct cyttsp6_sysinfo *si = &cd->sysinfo;
	int i;
	enum cyttsp6_tch_abs abs;
	void *p;
	int rc;

	dev_vdbg(cd->dev, "%s: get opcfg data\n", __func__);
	si->si_ofs.opcfg_size = si->si_ofs.ddata_ofs - si->si_ofs.opcfg_ofs;

	if (si->si_ofs.opcfg_size <= 0)
		return -EINVAL;

	p = krealloc(si->si_ptrs.opcfg, si->si_ofs.opcfg_size, GFP_KERNEL);
	if (!p) {
		rc = -ENOMEM;
		goto cyttsp6_si_get_opcfg_data_exit;
	}
	si->si_ptrs.opcfg = p;

	rc = cyttsp6_adap_read(cd, si->si_ofs.opcfg_ofs, si->si_ptrs.opcfg,
			si->si_ofs.opcfg_size);
	if (rc < 0) {
		dev_err(cd->dev, "%s: fail read opcfg data r=%d\n",
			__func__, rc);
		goto cyttsp6_si_get_opcfg_data_exit;
	}
	si->si_ofs.cmd_ofs = si->si_ptrs.opcfg->cmd_ofs;
	si->si_ofs.rep_ofs = si->si_ptrs.opcfg->rep_ofs;
	si->si_ofs.rep_sz = (si->si_ptrs.opcfg->rep_szh * 256) +
		si->si_ptrs.opcfg->rep_szl;
	si->si_ofs.num_btns = si->si_ptrs.opcfg->num_btns;
	si->si_ofs.num_btn_regs = (si->si_ofs.num_btns +
		CY_NUM_BTN_PER_REG - 1) / CY_NUM_BTN_PER_REG;
	si->si_ofs.tt_stat_ofs = si->si_ptrs.opcfg->tt_stat_ofs;
	si->si_ofs.obj_cfg0 = si->si_ptrs.opcfg->obj_cfg0;
	si->si_ofs.max_tchs = si->si_ptrs.opcfg->max_tchs &
		CY_BYTE_OFS_MASK;
	si->si_ofs.tch_rec_size = si->si_ptrs.opcfg->tch_rec_size &
		CY_BYTE_OFS_MASK;

	/* Get the old touch fields */
	for (abs = CY_TCH_X; abs < CY_NUM_TCH_FIELDS; abs++) {
		si->si_ofs.tch_abs[abs].ofs =
			si->si_ptrs.opcfg->tch_rec_old[abs].loc &
			CY_BYTE_OFS_MASK;
		si->si_ofs.tch_abs[abs].size =
			cyttsp6_bits_2_bytes
			(si->si_ptrs.opcfg->tch_rec_old[abs].size,
			&si->si_ofs.tch_abs[abs].max);
		si->si_ofs.tch_abs[abs].bofs =
			(si->si_ptrs.opcfg->tch_rec_old[abs].loc &
			CY_BOFS_MASK) >> CY_BOFS_SHIFT;
	}

	/* button fields */
	si->si_ofs.btn_rec_size = si->si_ptrs.opcfg->btn_rec_size;
	si->si_ofs.btn_diff_ofs = si->si_ptrs.opcfg->btn_diff_ofs;
	si->si_ofs.btn_diff_size = si->si_ptrs.opcfg->btn_diff_size;

	if (IS_TTSP_VER_GE(si, 2, 3)) {
		/* Get the extended touch fields */
		for (i = 0; i < CY_NUM_EXT_TCH_FIELDS; abs++, i++) {
			si->si_ofs.tch_abs[abs].ofs =
				si->si_ptrs.opcfg->tch_rec_new[i].loc &
				CY_BYTE_OFS_MASK;
			si->si_ofs.tch_abs[abs].size =
				cyttsp6_bits_2_bytes
				(si->si_ptrs.opcfg->tch_rec_new[i].size,
				&si->si_ofs.tch_abs[abs].max);
			si->si_ofs.tch_abs[abs].bofs =
				(si->si_ptrs.opcfg->tch_rec_new[i].loc
				& CY_BOFS_MASK) >> CY_BOFS_SHIFT;
		}
	}

	if (IS_TTSP_VER_GE(si, 2, 4)) {
		si->si_ofs.noise_data_ofs = si->si_ptrs.opcfg->noise_data_ofs;
		si->si_ofs.noise_data_sz = si->si_ptrs.opcfg->noise_data_sz;
	}

	for (abs = 0; abs < CY_TCH_NUM_ABS; abs++) {
		dev_dbg(cd->dev, "%s: tch_rec_%s\n", __func__,
			cyttsp6_tch_abs_string[abs]);
		dev_dbg(cd->dev, "%s:     ofs =%2zu\n", __func__,
			si->si_ofs.tch_abs[abs].ofs);
		dev_dbg(cd->dev, "%s:     siz =%2zu\n", __func__,
			si->si_ofs.tch_abs[abs].size);
		dev_dbg(cd->dev, "%s:     max =%2zu\n", __func__,
			si->si_ofs.tch_abs[abs].max);
		dev_dbg(cd->dev, "%s:     bofs=%2zu\n", __func__,
			si->si_ofs.tch_abs[abs].bofs);
	}

	si->si_ofs.mode_size = si->si_ofs.tt_stat_ofs + 1;
	si->si_ofs.data_size = si->si_ofs.max_tchs *
		si->si_ptrs.opcfg->tch_rec_size;
	si->si_ofs.rep_hdr_size = si->si_ofs.mode_size - si->si_ofs.rep_ofs;

	cyttsp6_pr_buf(cd->dev, cd->pr_buf, (u8 *)si->si_ptrs.opcfg,
		si->si_ofs.opcfg_size, "sysinfo_opcfg_data");

cyttsp6_si_get_opcfg_data_exit:
	return rc;
}

static int cyttsp6_si_get_ddata_(struct cyttsp6_core_data *cd)
{
	struct cyttsp6_sysinfo *si = &cd->sysinfo;
	void *p;
	int rc;

	dev_vdbg(cd->dev, "%s: get ddata data\n", __func__);
	si->si_ofs.ddata_size = si->si_ofs.mdata_ofs - si->si_ofs.ddata_ofs;

	if (si->si_ofs.ddata_size <= 0)
		return -EINVAL;

	p = krealloc(si->si_ptrs.ddata, si->si_ofs.ddata_size, GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	si->si_ptrs.ddata = p;

	rc = cyttsp6_adap_read(cd, si->si_ofs.ddata_ofs, si->si_ptrs.ddata,
			si->si_ofs.ddata_size);
	if (rc < 0)
		dev_err(cd->dev, "%s: fail read ddata data r=%d\n",
			__func__, rc);
	else
		cyttsp6_pr_buf(cd->dev, cd->pr_buf,
			       (u8 *)si->si_ptrs.ddata,
			       si->si_ofs.ddata_size, "sysinfo_ddata");

	return rc;
}

static int cyttsp6_si_get_mdata_(struct cyttsp6_core_data *cd)
{
	struct cyttsp6_sysinfo *si = &cd->sysinfo;
	void *p;
	int rc;

	dev_vdbg(cd->dev, "%s: get mdata data\n", __func__);
	si->si_ofs.mdata_size = si->si_ofs.map_sz - si->si_ofs.mdata_ofs;

	if (si->si_ofs.mdata_size <= 0)
		return -EINVAL;

	p = krealloc(si->si_ptrs.mdata, si->si_ofs.mdata_size, GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	si->si_ptrs.mdata = p;

	rc = cyttsp6_adap_read(cd, si->si_ofs.mdata_ofs, si->si_ptrs.mdata,
			si->si_ofs.mdata_size);
	if (rc < 0)
		dev_err(cd->dev, "%s: fail read mdata data r=%d\n",
			__func__, rc);
	else
		cyttsp6_pr_buf(cd->dev, cd->pr_buf,
			       (u8 *)si->si_ptrs.mdata,
			       si->si_ofs.mdata_size, "sysinfo_mdata");

	return rc;
}

static int cyttsp6_si_get_btn_data(struct cyttsp6_core_data *cd)
{
	struct cyttsp6_sysinfo *si = &cd->sysinfo;
	int btn;
	int num_defined_keys;
	u16 *key_table;
	void *p;
	int rc = 0;

	dev_vdbg(cd->dev, "%s: get btn data\n", __func__);

	if (!si->si_ofs.num_btns) {
		si->si_ofs.btn_keys_size = 0;
		kfree(si->btn);
		si->btn = NULL;
		return rc;
	}

	si->si_ofs.btn_keys_size = si->si_ofs.num_btns *
		sizeof(struct cyttsp6_btn);

	if (si->si_ofs.btn_keys_size <= 0)
		return -EINVAL;

	p = krealloc(si->btn, si->si_ofs.btn_keys_size, GFP_KERNEL|__GFP_ZERO);
	if (!p)
		return -ENOMEM;
	si->btn = p;

	if (cd->cpdata->sett[CY_IC_GRPNUM_BTN_KEYS] == NULL)
		num_defined_keys = 0;
	else if (cd->cpdata->sett[CY_IC_GRPNUM_BTN_KEYS]->data == NULL)
		num_defined_keys = 0;
	else
		num_defined_keys =
			cd->cpdata->sett[CY_IC_GRPNUM_BTN_KEYS]->size;

	for (btn = 0; btn < si->si_ofs.num_btns
			&& btn < num_defined_keys; btn++) {
		key_table =
			(u16 *)cd->cpdata->sett[CY_IC_GRPNUM_BTN_KEYS]->data;
		si->btn[btn].key_code = key_table[btn];
		si->btn[btn].state = CY_BTN_RELEASED;
		si->btn[btn].enabled = true;
	}
	for (; btn < si->si_ofs.num_btns; btn++) {
		si->btn[btn].key_code = KEY_RESERVED;
		si->btn[btn].state = CY_BTN_RELEASED;
		si->btn[btn].enabled = true;
	}

	return rc;
}

static int cyttsp6_si_get_op_data_ptrs(struct cyttsp6_core_data *cd)
{
	struct cyttsp6_sysinfo *si = &cd->sysinfo;
	void *p;
	int size;

	p = krealloc(si->xy_mode, si->si_ofs.mode_size +
			si->si_ofs.data_size, GFP_KERNEL|__GFP_ZERO);
	if (!p)
		return -ENOMEM;
	si->xy_mode = p;
	si->xy_data = &si->xy_mode[si->si_ofs.tt_stat_ofs + 1];

	size = si->si_ofs.btn_rec_size * si->si_ofs.num_btns;
	if (!size)
		return 0;

	p = krealloc(si->btn_rec_data, size, GFP_KERNEL|__GFP_ZERO);
	if (!p)
		return -ENOMEM;
	si->btn_rec_data = p;

	return 0;
}

static void cyttsp6_si_put_log_data(struct cyttsp6_core_data *cd)
{
	struct cyttsp6_sysinfo *si = &cd->sysinfo;

	dev_dbg(cd->dev, "%s: cydata_ofs =%4zu siz=%4zu\n", __func__,
		si->si_ofs.cydata_ofs, si->si_ofs.cydata_size);
	dev_dbg(cd->dev, "%s: test_ofs   =%4zu siz=%4zu\n", __func__,
		si->si_ofs.test_ofs, si->si_ofs.test_size);
	dev_dbg(cd->dev, "%s: pcfg_ofs   =%4zu siz=%4zu\n", __func__,
		si->si_ofs.pcfg_ofs, si->si_ofs.pcfg_size);
	dev_dbg(cd->dev, "%s: opcfg_ofs  =%4zu siz=%4zu\n", __func__,
		si->si_ofs.opcfg_ofs, si->si_ofs.opcfg_size);
	dev_dbg(cd->dev, "%s: ddata_ofs  =%4zu siz=%4zu\n", __func__,
		si->si_ofs.ddata_ofs, si->si_ofs.ddata_size);
	dev_dbg(cd->dev, "%s: mdata_ofs  =%4zu siz=%4zu\n", __func__,
		si->si_ofs.mdata_ofs, si->si_ofs.mdata_size);

	dev_dbg(cd->dev, "%s: cmd_ofs       =%4zu\n", __func__,
		si->si_ofs.cmd_ofs);
	dev_dbg(cd->dev, "%s: rep_ofs       =%4zu\n", __func__,
		si->si_ofs.rep_ofs);
	dev_dbg(cd->dev, "%s: rep_sz        =%4zu\n", __func__,
		si->si_ofs.rep_sz);
	dev_dbg(cd->dev, "%s: num_btns      =%4zu\n", __func__,
		si->si_ofs.num_btns);
	dev_dbg(cd->dev, "%s: num_btn_regs  =%4zu\n", __func__,
		si->si_ofs.num_btn_regs);
	dev_dbg(cd->dev, "%s: tt_stat_ofs   =%4zu\n", __func__,
		si->si_ofs.tt_stat_ofs);
	dev_dbg(cd->dev, "%s: tch_rec_size   =%4zu\n", __func__,
		si->si_ofs.tch_rec_size);
	dev_dbg(cd->dev, "%s: max_tchs      =%4zu\n", __func__,
		si->si_ofs.max_tchs);
	dev_dbg(cd->dev, "%s: mode_size     =%4zu\n", __func__,
		si->si_ofs.mode_size);
	dev_dbg(cd->dev, "%s: data_size     =%4zu\n", __func__,
		si->si_ofs.data_size);
	dev_dbg(cd->dev, "%s: rep_hdr_size  =%4zu\n", __func__,
		si->si_ofs.rep_hdr_size);
	dev_dbg(cd->dev, "%s: map_sz        =%4zu\n", __func__,
		si->si_ofs.map_sz);

	dev_dbg(cd->dev, "%s: btn_rec_size   =%2zu\n", __func__,
		si->si_ofs.btn_rec_size);
	dev_dbg(cd->dev, "%s: btn_diff_ofs  =%2zu\n", __func__,
		si->si_ofs.btn_diff_ofs);
	dev_dbg(cd->dev, "%s: btn_diff_size  =%2zu\n", __func__,
		si->si_ofs.btn_diff_size);

	dev_dbg(cd->dev, "%s: max_x    = 0x%0zX (%zu)\n", __func__,
		si->si_ofs.max_x, si->si_ofs.max_x);
	dev_dbg(cd->dev, "%s: x_origin = %zu (%s)\n", __func__,
		si->si_ofs.x_origin,
		si->si_ofs.x_origin == CY_NORMAL_ORIGIN ?
		"left corner" : "right corner");
	dev_dbg(cd->dev, "%s: max_y    = 0x%04zX (%zu)\n", __func__,
		si->si_ofs.max_y, si->si_ofs.max_y);
	dev_dbg(cd->dev, "%s: y_origin = %zu (%s)\n", __func__,
		si->si_ofs.y_origin,
		si->si_ofs.y_origin == CY_NORMAL_ORIGIN ?
		"upper corner" : "lower corner");
	dev_dbg(cd->dev, "%s: max_p    = 0x%04zX (%zu)\n", __func__,
		si->si_ofs.max_p, si->si_ofs.max_p);

	dev_dbg(cd->dev, "%s: xy_mode=%p xy_data=%p\n", __func__,
		si->xy_mode, si->xy_data);
}

static int cyttsp6_get_sysinfo_regs_(struct cyttsp6_core_data *cd)
{
	struct cyttsp6_sysinfo *si = &cd->sysinfo;
	int rc;

	rc = cyttsp6_si_data_offsets_(cd);
	if (rc < 0)
		return rc;

	rc = cyttsp6_si_get_cydata_(cd);
	if (rc < 0)
		return rc;

	rc = cyttsp6_si_get_test_data_(cd);
	if (rc < 0)
		return rc;

	rc = cyttsp6_si_get_pcfg_data_(cd);
	if (rc < 0)
		return rc;

	rc = cyttsp6_si_get_opcfg_data_(cd);
	if (rc < 0)
		return rc;

	rc = cyttsp6_si_get_ddata_(cd);
	if (rc < 0)
		return rc;

	rc = cyttsp6_si_get_mdata_(cd);
	if (rc < 0)
		return rc;

	rc = cyttsp6_si_get_btn_data(cd);
	if (rc < 0)
		return rc;

	rc = cyttsp6_si_get_op_data_ptrs(cd);
	if (rc < 0) {
		dev_err(cd->dev, "%s: failed to get_op_data\n", __func__);
		return rc;
	}

	cyttsp6_si_put_log_data(cd);

	/* provide flow control handshake */
	rc = cyttsp6_handshake_(cd, si->si_data.hst_mode);
	if (rc < 0)
		dev_err(cd->dev, "%s: handshake fail on sysinfo reg\n",
			__func__);

	mutex_lock(&cd->system_lock);
	si->ready = true;
	mutex_unlock(&cd->system_lock);

	return rc;
}

static void _cyttsp6_queue_startup(struct cyttsp6_core_data *cd)
{
	if (cd->startup_state == STARTUP_NONE) {
		cd->startup_state = STARTUP_QUEUED;
		schedule_work(&cd->startup_work);
		/* Required for signal to the TTHE */
		dev_info(cd->dev, "%s: cyttsp6_startup queued\n", __func__);
	} else {
		dev_dbg(cd->dev, "%s: startup_state = %d\n", __func__,
			cd->startup_state);
	}
}

static void cyttsp6_queue_startup(struct cyttsp6_core_data *cd)
{
	dev_vdbg(cd->dev, "%s: enter\n", __func__);
	mutex_lock(&cd->system_lock);
	_cyttsp6_queue_startup(cd);
	mutex_unlock(&cd->system_lock);
}

static void call_atten_cb(struct cyttsp6_core_data *cd,
		enum cyttsp6_atten_type type, int mode)
{
	struct atten_node *atten, *atten_n;

	dev_vdbg(cd->dev, "%s: check list type=%d mode=%d\n",
		__func__, type, mode);
	mutex_lock(&cd->atten_lock);
	list_for_each_entry_safe(atten, atten_n,
			&cd->atten_list[type], node) {
		if (!mode || atten->mode & mode) {
			mutex_unlock(&cd->atten_lock);
			dev_vdbg(cd->dev, "%s: attention for '%s'", __func__,
				dev_name(atten->dev));
			atten->func(atten->dev);
			mutex_lock(&cd->atten_lock);
		}
	}
	mutex_unlock(&cd->atten_lock);
}

static void cyttsp6_start_wd_timer(struct cyttsp6_core_data *cd)
{
	if (!CY_WATCHDOG_TIMEOUT)
		return;

	mod_timer(&cd->watchdog_timer, jiffies +
			msecs_to_jiffies(CY_WATCHDOG_TIMEOUT));
}

static void cyttsp6_postpone_wd_timer(struct cyttsp6_core_data *cd)
{
	if (!CY_WATCHDOG_TIMEOUT)
		return;

	mod_timer_pending(&cd->watchdog_timer, jiffies +
			msecs_to_jiffies(CY_WATCHDOG_TIMEOUT));
}

static void cyttsp6_stop_wd_timer(struct cyttsp6_core_data *cd)
{
	if (!CY_WATCHDOG_TIMEOUT)
		return;

	/*
	 * Ensure we wait until the watchdog timer
	 * running on a different CPU finishes
	 */
	del_timer_sync(&cd->watchdog_timer);
	cancel_work_sync(&cd->watchdog_work);
	del_timer_sync(&cd->watchdog_timer);
}

static void cyttsp6_watchdog_timer(unsigned long handle)
{
	struct cyttsp6_core_data *cd = (struct cyttsp6_core_data *)handle;

	dev_vdbg(cd->dev, "%s: Timer triggered\n", __func__);

	if (!work_pending(&cd->watchdog_work))
		schedule_work(&cd->watchdog_work);
}

static int cyttsp6_request_stop_wd_(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	cyttsp6_stop_wd_timer(cd);

	return 0;
}

static int _cyttsp6_put_device_into_deep_sleep(struct cyttsp6_core_data *cd,
		u8 hst_mode_reg)
{
	int rc;

	hst_mode_reg |= CY_HST_SLEEP;

	dev_vdbg(cd->dev, "%s: write DEEP SLEEP...\n", __func__);
	rc = cyttsp6_adap_write(cd, CY_REG_BASE, &hst_mode_reg,
			sizeof(hst_mode_reg));
	if (rc) {
		dev_err(cd->dev, "%s: Fail write adapter r=%d\n", __func__, rc);
		return -EINVAL;
	}
	dev_vdbg(cd->dev, "%s: write DEEP SLEEP succeeded\n", __func__);

	if (cd->cpdata->power) {
		dev_dbg(cd->dev, "%s: Power down HW\n", __func__);
		rc = cd->cpdata->power(cd->cpdata, 0, cd->dev, &cd->ignore_irq);
	} else {
		dev_dbg(cd->dev, "%s: No power function\n", __func__);
		rc = 0;
	}
	if (rc < 0) {
		dev_err(cd->dev, "%s: HW Power down fails r=%d\n",
			__func__, rc);
		return -EINVAL;
	}

	return 0;
}

static irqreturn_t cyttsp6_hard_irq(int irq, void *handle)
{
	struct cyttsp6_core_data *cd = handle;

	/*
	 * Check whether this IRQ should be ignored (external)
	 * This should be the very first thing to check since
	 * ignore_irq may be set for a very short period of time
	 */
	if (atomic_read(&cd->ignore_irq)) {
		dev_vdbg(cd->dev, "%s: Ignoring IRQ\n", __func__);
		return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

static char *int_status2str(int int_status)
{
	switch (int_status) {
	case CY_INT_NONE:
		return "regular";
	case CY_INT_IGNORE:
		return "ignore";
	case CY_INT_MODE_CHANGE:
		return "mode_change";
	case CY_INT_EXEC_CMD:
		return "exec_cmd";
	case CY_INT_AWAKE:
		return "awake";
	default:
		return "int_failure";
	}
}

static char *mode2str(int mode)
{
	switch (mode) {
	case CY_MODE_UNKNOWN:
		return "unknown";
	case CY_MODE_BOOTLOADER:
		return "bootloader";
	case CY_MODE_OPERATIONAL:
		return "operational";
	case CY_MODE_SYSINFO:
		return "sysinfo";
	case CY_MODE_CAT:
		return "cat";
	case CY_MODE_STARTUP:
		return "startup";
	case CY_MODE_LOADER:
		return "loader";
	case CY_MODE_CHANGE_MODE:
		return "change_mode";
	case CY_MODE_CHANGED:
		return "changed";
	case CY_MODE_CMD_COMPLETE:
		return "cmd_complete";
	default:
		return "mode_failure";
	}
}

static void cyttsp6_parse_mode(struct device *dev, u8 mode,
		enum cyttsp6_mode *cur_mode)
{
	switch (mode & CY_HST_DEVICE_MODE) {
	case CY_HST_OPERATE:
		*cur_mode = CY_MODE_OPERATIONAL;
		dev_vdbg(dev, "%s: operational\n", __func__);
		break;
	case CY_HST_CAT:
		*cur_mode = CY_MODE_CAT;
		dev_vdbg(dev, "%s: CaT\n", __func__);
		break;
	case CY_HST_SYSINFO:
		*cur_mode = CY_MODE_SYSINFO;
		dev_vdbg(dev, "%s: sysinfo\n", __func__);
		break;
	default:
		*cur_mode = CY_MODE_UNKNOWN;
		dev_err(dev, "%s: unknown HST mode 0x%02X\n", __func__, mode);
		break;
	}
}

static irqreturn_t cyttsp6_irq(int irq, void *handle)
{
	struct cyttsp6_core_data *cd = handle;
	struct device *dev = cd->dev;
	enum cyttsp6_mode cur_mode;
	u8 cmd_ofs = cd->sysinfo.si_ofs.cmd_ofs;
	bool command_complete = false;
	u8 mode[3];
	int rc;

	dev_dbg(dev, "%s int:%s\n", __func__, int_status2str(cd->int_status));

	mutex_lock(&cd->system_lock);

	rc = cyttsp6_adap_read(cd, CY_REG_BASE, mode, sizeof(mode));
	if (rc < 0) {
		dev_err(cd->dev, "%s: Fail read adapter r=%d\n", __func__, rc);
		goto cyttsp6_irq_exit;
	}
	dev_vdbg(dev, "%s mode[0-2]:0x%X 0x%X 0x%X\n", __func__,
			mode[0], mode[1], mode[2]);

	if (IS_BOOTLOADER(mode[0], mode[1])) {
		cur_mode = CY_MODE_BOOTLOADER;
		dev_vdbg(dev, "%s: bl running\n", __func__);
		call_atten_cb(cd, CY_ATTEN_IRQ, cur_mode);

		/* switch to bootloader */
		if (cd->mode != CY_MODE_BOOTLOADER)
			dev_dbg(dev, "%s: restart switch to bl m=%s -> m=%s\n",
				__func__, mode2str(cd->mode),
				mode2str(cur_mode));

		/* catch operation->bl glitch */
		if (cd->mode != CY_MODE_BOOTLOADER
				&& cd->mode != CY_MODE_UNKNOWN) {
			/* Incase startup_state, do not let startup_() */
			cd->mode = CY_MODE_UNKNOWN;
			_cyttsp6_queue_startup(cd);
			goto cyttsp6_irq_exit;
		}

		/* Recover if stuck in bootloader idle mode */
		if (cd->mode == CY_MODE_BOOTLOADER) {
			if (IS_BOOTLOADER_IDLE(mode[0], mode[1])) {
				if (cd->heartbeat_count > 3) {
					cd->heartbeat_count = 0;
					_cyttsp6_queue_startup(cd);
					goto cyttsp6_irq_exit;
				}
				cd->heartbeat_count++;
			}
		}

		cd->mode = cur_mode;
		/* Signal bootloader heartbeat heard */
		wake_up(&cd->wait_q);
		goto cyttsp6_irq_exit;
	}

	cyttsp6_parse_mode(dev, mode[0], &cur_mode);

	/* Check whether this IRQ should be ignored (internal) */
	if (cd->int_status & CY_INT_IGNORE) {
		if (IS_DEEP_SLEEP_CONFIGURED(cd->easy_wakeup_gesture)) {
			/* Put device back to sleep on premature wakeup */
			dev_dbg(dev, "%s: Put device back to sleep\n",
				__func__);
			_cyttsp6_put_device_into_deep_sleep(cd, mode[0]);
			goto cyttsp6_irq_exit;
		}
		/* Check for Wait for Event command */
		if (IS_TTSP_VER_GE(&cd->sysinfo, 2, 5)) {
			u8 cmd_code = mode[cmd_ofs] & CY_CMD_MASK;

			if (cmd_code == CY_CMD_OP_WAIT_FOR_EVENT
					&& mode[cmd_ofs] & CY_CMD_COMPLETE) {
				cd->wake_initiated_by_device = 1;
				dev_dbg(dev, "%s: Wake by device\n", __func__);
				call_atten_cb(cd, CY_ATTEN_WAKE, 0);
				goto cyttsp6_irq_handshake;
			}
		}
	}

	/* Check for wake up interrupt */
	if (cd->int_status & CY_INT_AWAKE) {
		cd->int_status &= ~CY_INT_AWAKE;
		wake_up(&cd->wait_q);
		dev_vdbg(dev, "%s: Received wake up interrupt\n", __func__);
		goto cyttsp6_irq_handshake;
	}

	/* Expecting mode change interrupt */
	if ((cd->int_status & CY_INT_MODE_CHANGE)
			&& (mode[0] & CY_HST_MODE_CHANGE) == 0) {
		cd->int_status &= ~CY_INT_MODE_CHANGE;
		dev_dbg(dev, "%s: finish mode switch m=%s -> m=%s\n",
			__func__, mode2str(cd->mode), mode2str(cur_mode));
		cd->mode = cur_mode;
		wake_up(&cd->wait_q);
		goto cyttsp6_irq_handshake;
	}

	/* compare current core mode to current device mode */
	dev_vdbg(dev, "%s: cd->mode=%s cur_mode=%s\n",
			__func__, mode2str(cd->mode), mode2str(cur_mode));
	if ((mode[0] & CY_HST_MODE_CHANGE) == 0 && cd->mode != cur_mode) {
		/* Unexpected mode change occurred */
		dev_err(dev, "%s %s->%s int:%s\n", __func__, mode2str(cd->mode),
			mode2str(cur_mode), int_status2str(cd->int_status));
		dev_vdbg(dev, "%s: Unexpected mode change, startup\n",
				__func__);
		_cyttsp6_queue_startup(cd);
		goto cyttsp6_irq_exit;
	}

	/* Expecting command complete interrupt */
	dev_vdbg(dev, "%s: command byte:0x%x, toggle:0x%x\n",
			__func__, mode[cmd_ofs], cd->cmd_toggle);
	if ((cd->int_status & CY_INT_EXEC_CMD)
			&& mode[cmd_ofs] & CY_CMD_COMPLETE) {
		command_complete = true;
		cd->int_status &= ~CY_INT_EXEC_CMD;
		dev_vdbg(dev, "%s: Received command complete interrupt\n",
				__func__);
		wake_up(&cd->wait_q);
		/*
		 * It is possible to receive a single interrupt for
		 * command complete and touch/button status report.
		 * Continue processing for a possible status report.
		 */
	}

	if (!cd->sysinfo.xy_mode)
		goto cyttsp6_irq_handshake;

	/* Copy the mode registers */
	memcpy(cd->sysinfo.xy_mode, mode, sizeof(mode));

	/* This should be status report, read status and touch regs */
	if (cd->mode == CY_MODE_OPERATIONAL) {
		dev_vdbg(dev, "%s: Read status and touch registers\n",
			__func__);
		rc = cyttsp6_load_status_and_touch_regs_(cd, !command_complete);
		if (rc < 0)
			dev_err(dev, "%s: fail read mode/touch regs r=%d\n",
				__func__, rc);
	}

	/* attention IRQ */
	call_atten_cb(cd, CY_ATTEN_IRQ, cd->mode);

cyttsp6_irq_handshake:
	/* Update watchdog expire timer */
	cyttsp6_postpone_wd_timer(cd);

	/* handshake the event */
	dev_vdbg(dev, "%s: Handshake mode=0x%02X r=%d\n",
			__func__, mode[0], rc);
	rc = cyttsp6_handshake_(cd, mode[0]);
	if (rc < 0)
		dev_err(dev, "%s: Fail handshake mode=0x%02X r=%d\n",
				__func__, mode[0], rc);

	/*
	 * a non-zero udelay period is required for using
	 * IRQF_TRIGGER_LOW in order to delay until the
	 * device completes isr deassert
	 */
	udelay(cd->cpdata->level_irq_udelay);

cyttsp6_irq_exit:
	mutex_unlock(&cd->system_lock);
	dev_vdbg(dev, "%s: irq done\n", __func__);

	return IRQ_HANDLED;
}

int cyttsp6_write_(struct device *dev, int mode, u16 addr, const void *buf,
	int size)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	if (mode != cd->mode) {
		dev_dbg(dev, "%s: attempt to write in missing mode (having %x while %x requested)\n",
			__func__, cd->mode, mode);
		return -EACCES;
	}

	return cyttsp6_adap_write(cd, addr, buf, size);
}

int cyttsp6_read_(struct device *dev, int mode, u16 addr, void *buf, int size)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	if (mode != cd->mode) {
		dev_dbg(dev, "%s: attempt to read in missing mode (having %x while %x requested)\n",
			__func__, cd->mode, mode);
		return -EACCES;
	}

	return cyttsp6_adap_read(cd, addr, buf, size);
}

int cyttsp6_subscribe_attention_(struct device *dev,
	enum cyttsp6_atten_type type, char id, int (*func)(struct device *),
	int mode)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct atten_node *atten, *atten_new;

	atten_new = kzalloc(sizeof(*atten_new), GFP_KERNEL);
	if (!atten_new)
		return -ENOMEM;

	dev_dbg(cd->dev, "%s from '%s'\n", __func__, dev_name(cd->dev));

	mutex_lock(&cd->atten_lock);
	list_for_each_entry(atten, &cd->atten_list[type], node) {
		if (atten->id == id && atten->mode == mode) {
			mutex_unlock(&cd->atten_lock);
			kfree(atten_new);
			dev_vdbg(cd->dev, "%s: already subscribed attention=%p mode=%d\n",
				 __func__, dev, mode);
			return 0;
		}
	}

	atten_new->id = id;
	atten_new->dev = dev;
	atten_new->mode = mode;
	atten_new->func = func;

	list_add(&atten_new->node, &cd->atten_list[type]);
	mutex_unlock(&cd->atten_lock);

	return 0;
}

int cyttsp6_unsubscribe_attention_(struct device *dev,
	enum cyttsp6_atten_type type, char id, int (*func)(struct device *),
	int mode)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct atten_node *atten, *atten_n;

	mutex_lock(&cd->atten_lock);
	list_for_each_entry_safe(atten, atten_n, &cd->atten_list[type], node) {
		if (atten->id == id && atten->mode == mode) {
			list_del(&atten->node);
			mutex_unlock(&cd->atten_lock);
			kfree(atten);
			dev_vdbg(cd->dev, "%s: unsub for atten->dev=%p atten->mode=%d\n",
				__func__, atten->dev, atten->mode);
			return 0;
		}
	}
	mutex_unlock(&cd->atten_lock);

	return -ENODEV;
}

static int request_exclusive(struct cyttsp6_core_data *cd, void *ownptr,
		int timeout_ms)
{
	int t = msecs_to_jiffies(timeout_ms);
	bool with_timeout = (timeout_ms != 0);

	mutex_lock(&cd->system_lock);
	if (!cd->exclusive_dev && cd->exclusive_waits == 0) {
		cd->exclusive_dev = ownptr;
		goto exit;
	}

	cd->exclusive_waits++;
wait:
	mutex_unlock(&cd->system_lock);
	if (with_timeout) {
		t = wait_event_timeout(cd->wait_q, !cd->exclusive_dev, t);
		if (IS_TMO(t)) {
			dev_err(cd->dev, "%s: tmo waiting exclusive access\n",
				__func__);
			mutex_lock(&cd->system_lock);
			cd->exclusive_waits--;
			mutex_unlock(&cd->system_lock);
			return -ETIME;
		}
	} else {
		wait_event(cd->wait_q, !cd->exclusive_dev);
	}
	mutex_lock(&cd->system_lock);
	if (cd->exclusive_dev)
		goto wait;
	cd->exclusive_dev = ownptr;
	cd->exclusive_waits--;
exit:
	mutex_unlock(&cd->system_lock);
	dev_vdbg(cd->dev, "%s: request_exclusive ok=%p\n", __func__, ownptr);

	return 0;
}

int cyttsp6_request_exclusive(struct device *dev, int timeout_ms)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	return request_exclusive(cd, dev, timeout_ms);
}

static int cyttsp6_request_exclusive_(struct device *dev,
		int timeout_ms)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	return request_exclusive(cd, (void *)dev, timeout_ms);
}

/*
 * returns error if was not owned
 */
static int release_exclusive(struct cyttsp6_core_data *cd, void *ownptr)
{
	mutex_lock(&cd->system_lock);
	if (cd->exclusive_dev != ownptr) {
		mutex_unlock(&cd->system_lock);
		return -EINVAL;
	}

	dev_vdbg(cd->dev, "%s: exclusive_dev %p freed\n",
		__func__, cd->exclusive_dev);
	cd->exclusive_dev = NULL;
	wake_up(&cd->wait_q);
	mutex_unlock(&cd->system_lock);

	return 0;
}

int cyttsp6_release_exclusive(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	return release_exclusive(cd, dev);
}

static int cyttsp6_release_exclusive_(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	return release_exclusive(cd, (void *)dev);
}

static int cyttsp6_wait_bl_heartbeat(struct cyttsp6_core_data *cd)
{
	long t;
	int rc = 0;

	/* wait heartbeat */
	dev_vdbg(cd->dev, "%s: wait heartbeat...\n", __func__);
	t = wait_event_timeout(cd->wait_q, cd->mode == CY_MODE_BOOTLOADER,
			msecs_to_jiffies(CY_CORE_RESET_AND_WAIT_TIMEOUT));
	if (IS_TMO(t)) {
		dev_err(cd->dev, "%s: tmo waiting bl heartbeat cd->mode=%d\n",
			__func__, cd->mode);
		rc = -ETIME;
	}

	return rc;
}

static int cyttsp6_wait_sysinfo_mode(struct cyttsp6_core_data *cd)
{
	long t;

	dev_vdbg(cd->dev, "%s: wait sysinfo...\n", __func__);

	t = wait_event_timeout(cd->wait_q, cd->mode == CY_MODE_SYSINFO,
			msecs_to_jiffies(CY_CORE_WAIT_SYSINFO_MODE_TIMEOUT));
	if (IS_TMO(t)) {
		dev_err(cd->dev, "%s: tmo waiting exit bl cd->mode=%d\n",
			__func__, cd->mode);
		mutex_lock(&cd->system_lock);
		cd->int_status &= ~CY_INT_MODE_CHANGE;
		mutex_unlock(&cd->system_lock);
		return -ETIME;
	}

	return 0;
}

static int cyttsp6_reset_and_wait(struct cyttsp6_core_data *cd)
{
	int rc;

	/* reset hardware */
	mutex_lock(&cd->system_lock);
	dev_dbg(cd->dev, "%s: reset hw...\n", __func__);
	rc = cyttsp6_hw_reset_(cd);
	cd->mode = CY_MODE_UNKNOWN;
	mutex_unlock(&cd->system_lock);
	if (rc < 0) {
		dev_err(cd->dev, "%s: Fail hw reset dev='%s' r=%d\n",
			__func__, dev_name(cd->dev), rc);
		return rc;
	}

	return cyttsp6_wait_bl_heartbeat(cd);
}

/*
 * returns err if refused or timeout; block until mode change complete
 * bit is set (mode change interrupt)
 */
static int set_mode_(struct cyttsp6_core_data *cd, int new_mode)
{
	u8 new_dev_mode;
	u8 mode;
	long t;
	int rc;

	switch (new_mode) {
	case CY_MODE_OPERATIONAL:
		new_dev_mode = CY_HST_OPERATE;
		break;
	case CY_MODE_SYSINFO:
		new_dev_mode = CY_HST_SYSINFO;
		break;
	case CY_MODE_CAT:
		new_dev_mode = CY_HST_CAT;
		break;
	default:
		dev_err(cd->dev, "%s: invalid mode: %02X(%d)\n",
			__func__, new_mode, new_mode);
		return -EINVAL;
	}

	/* change mode */
	dev_dbg(cd->dev, "%s: have exclusive=%p new_dev_mode=%02X new_mode=%s\n",
			__func__, cd->exclusive_dev,
			new_dev_mode, mode2str(new_mode));

	mutex_lock(&cd->system_lock);
	rc = cyttsp6_adap_read(cd, CY_REG_BASE, &mode, sizeof(mode));
	if (rc < 0) {
		mutex_unlock(&cd->system_lock);
		dev_err(cd->dev, "%s: Fail read mode r=%d\n", __func__, rc);
		goto exit;
	}

	/* Clear device mode bits and set to new mode */
	mode &= ~CY_HST_DEVICE_MODE;
	mode |= new_dev_mode | CY_HST_MODE_CHANGE;

	cd->int_status |= CY_INT_MODE_CHANGE;
	rc = cyttsp6_adap_write(cd, CY_REG_BASE, &mode, sizeof(mode));
	mutex_unlock(&cd->system_lock);
	if (rc < 0) {
		dev_err(cd->dev, "%s: Fail write mode change r=%d\n",
			__func__, rc);
		goto exit;
	}

	/* wait for mode change done interrupt */
	t = wait_event_timeout(cd->wait_q,
			(cd->int_status & CY_INT_MODE_CHANGE) == 0,
			msecs_to_jiffies(CY_CORE_MODE_CHANGE_TIMEOUT));
	dev_dbg(cd->dev, "%s: back from wait t=%ld cd->mode=%s\n",
			__func__, t, mode2str(cd->mode));

	if (IS_TMO(t)) {
		dev_err(cd->dev, "%s: tmo waiting mode change\n", __func__);
		mutex_lock(&cd->system_lock);
		cd->int_status &= ~CY_INT_MODE_CHANGE;
		mutex_unlock(&cd->system_lock);
		rc = -EINVAL;
	}

exit:
	return rc;
}

/*
 * returns err if refused or timeout(core uses fixed timeout period) occurs;
 * blocks until ISR occurs
 */
static int cyttsp6_request_reset_(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	int rc;

	mutex_lock(&cd->system_lock);
	cd->sysinfo.ready = false;
	mutex_unlock(&cd->system_lock);

	rc = cyttsp6_reset_and_wait(cd);
	if (rc < 0)
		dev_err(dev, "%s: Error on h/w reset r=%d\n", __func__, rc);

	return rc;
}

/*
 * returns err if refused ; if no error then restart has completed
 * and system is in normal operating mode
 */
static int cyttsp6_request_restart_(struct device *dev, bool wait)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	mutex_lock(&cd->system_lock);
	cd->sysinfo.ready = false;
	cd->bl_fast_exit = false;
	mutex_unlock(&cd->system_lock);

	cyttsp6_queue_startup(cd);

	if (wait)
		wait_event(cd->wait_q, cd->startup_state == STARTUP_NONE);

	return 0;
}

static int cyttsp6_request_set_mode_(struct device *dev, int mode)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	int rc;

	rc = set_mode_(cd, mode);
	if (rc < 0)
		dev_err(dev, "%s: fail set_mode=%s(%02X)\n",
			__func__, mode2str(cd->mode), cd->mode);

	return rc;
}

/*
 * returns NULL if sysinfo has not been acquired from the device yet
 */
struct cyttsp6_sysinfo *cyttsp6_request_sysinfo_(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	bool ready;

	mutex_lock(&cd->system_lock);
	ready = cd->sysinfo.ready;
	mutex_unlock(&cd->system_lock);
	if (ready)
		return &cd->sysinfo;

	return NULL;
}

static struct cyttsp6_loader_platform_data *cyttsp6_request_loader_pdata_(
		struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	return cd->pdata->loader_pdata;
}

static int cyttsp6_request_handshake_(struct device *dev, u8 mode)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	int rc;

	rc = cyttsp6_handshake_(cd, mode);
	if (rc < 0)
		dev_err(dev, "%s: Fail handshake r=%d\n", __func__, rc);

	return rc;
}

static int cyttsp6_request_toggle_lowpower_(struct device *dev,
		u8 mode)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	int rc;

	rc = cyttsp6_toggle_low_power_(cd, mode);
	if (rc < 0)
		dev_err(dev, "%s: Fail toggle low power r=%d\n", __func__, rc);

	return rc;
}

static int cyttsp6_wait_cmd_exec(struct cyttsp6_core_data *cd, int timeout_ms)
{
	struct device *dev = cd->dev;
	int rc;

	rc = wait_event_timeout(cd->wait_q,
			(cd->int_status & CY_INT_EXEC_CMD) == 0,
			msecs_to_jiffies(timeout_ms));
	if (IS_TMO(rc)) {
		dev_err(dev, "%s: Command execution timed out\n", __func__);
		cd->int_status &= ~CY_INT_EXEC_CMD;
		return -ETIME;
	}

	return 0;
}

static int get_cmd_offs_and_cmd_data_len(struct cyttsp6_core_data *cd,
		u8 mode, int *cmd_ofs, int *cmd_data_len)
{
	struct cyttsp6_sysinfo *si = &cd->sysinfo;
	struct device *dev = cd->dev;

	switch (mode) {
	case CY_MODE_CAT:
		*cmd_ofs = CY_REG_CAT_CMD;
		*cmd_data_len = CY_REG_CAT_CMD_DATA_MAX - CY_REG_CAT_CMD;
		break;
	case CY_MODE_OPERATIONAL:
		*cmd_ofs = si->si_ofs.cmd_ofs;
		*cmd_data_len = si->si_ofs.rep_ofs - si->si_ofs.cmd_ofs - 1;
		break;
	default:
		dev_err(dev, "%s: Unsupported mode %x for exec cmd\n",
				__func__, mode);
		return -EACCES;
	}

	return 0;
}

/*
 * Send command to device for CAT and OP modes
 * return negative value on error, 0 on success
 */
static int _cyttsp6_exec_cmd(struct cyttsp6_core_data *cd, u8 mode,
		u8 *cmd_buf, size_t cmd_size)
{
	struct device *dev = cd->dev;
	int cmd_ofs;
	int cmd_data_len;
	int cmd_param_ofs;
	u8 command;
	u8 *cmd_param_buf;
	size_t cmd_param_size;
	int rc;

	if (mode != cd->mode) {
		dev_err(dev, "%s: attempt to exec cmd in missing mode (having %x while %x requested)\n",
			__func__, cd->mode, mode);
		return -EACCES;
	}

	rc = get_cmd_offs_and_cmd_data_len(cd, mode, &cmd_ofs, &cmd_data_len);
	if (rc < 0)
		return rc;

	cmd_param_ofs = cmd_ofs + 1;
	cmd_param_buf = cmd_buf + 1;
	cmd_param_size = cmd_size - 1;

	/* Check for parameter size overflow */
	if (cmd_param_size > cmd_data_len) {
		dev_err(dev, "%s: Parameter size overflow:%zu max:%d\n",
			__func__, cmd_param_size, cmd_data_len);
		return -EINVAL;
	}

	/* Check if complete is set, so write new command */
	rc = cyttsp6_adap_read(cd, cmd_ofs, &command, 1);
	if (rc < 0) {
		dev_err(dev, "%s: Error on read r=%d\n", __func__, rc);
		return rc;
	}

	cd->cmd_toggle = GET_TOGGLE(command);
	cd->int_status |= CY_INT_EXEC_CMD;

	if ((command & CY_CMD_COMPLETE_MASK) == 0)
		return -EBUSY;

	/*
	 * Write new command
	 * Only update command bits 0:5
	 * Clear command complete bit & toggle bit
	 */
	cmd_buf[0] = cmd_buf[0] & CY_CMD_MASK;
	/* Write command parameters first */
	if (cmd_size > 1) {
		rc = cyttsp6_adap_write(cd, cmd_param_ofs, cmd_param_buf,
				cmd_param_size);
		if (rc < 0) {
			dev_err(dev, "%s: Error on write command parameters r=%d\n",
				__func__, rc);
			return rc;
		}
	}
	/* Write the command */
	rc = cyttsp6_adap_write(cd, cmd_ofs, cmd_buf, 1);
	if (rc < 0) {
		dev_err(dev, "%s: Error on write command r=%d\n", __func__, rc);
		return rc;
	}

	return 0;
}

static int cyttsp6_exec_cmd(struct cyttsp6_core_data *cd, u8 mode,
		u8 *cmd_buf, size_t cmd_size, u8 *return_buf,
		size_t param_ret_size, int timeout_ms)
{
	struct device *dev = cd->dev;
	int cmd_ofs;
	int cmd_data_len;
	int cmd_return_ofs;
	int rc;

	mutex_lock(&cd->system_lock);
	rc = _cyttsp6_exec_cmd(cd, mode, cmd_buf, cmd_size);
	mutex_unlock(&cd->system_lock);

	if (rc == -EBUSY) {
		rc = cyttsp6_wait_cmd_exec(cd, CY_COMMAND_COMPLETE_TIMEOUT);
		if (rc)
			return rc;
		mutex_lock(&cd->system_lock);
		rc = _cyttsp6_exec_cmd(cd, mode, cmd_buf, cmd_size);
		mutex_unlock(&cd->system_lock);
	}

	if (rc < 0)
		return rc;

	if (timeout_ms == 0)
		return 0;

	/*
	 * Wait command to be completed
	 */
	rc = cyttsp6_wait_cmd_exec(cd, timeout_ms);
	if (rc < 0)
		return rc;

	if (param_ret_size == 0 || return_buf == NULL)
		return 0;

	rc = get_cmd_offs_and_cmd_data_len(cd, mode, &cmd_ofs, &cmd_data_len);
	if (rc < 0)
		return rc;

	/* Check for return buffer size overflow */
	if (param_ret_size > cmd_data_len) {
		dev_err(dev, "%s: Return data size overflow:%zu max:%d\n",
			__func__, param_ret_size, cmd_data_len);
		return -EINVAL;
	}

	cmd_return_ofs = cmd_ofs + 1;
	
	/* A mutex lock (provided by Cypress) during an i2c read was added to handle two bugs: 
	 * a) During CRC reads prior to an upgrade, an interrupt would cause the device pointer to get mangled
	 *    which caused incorrect data to be read back instead of the CRC. 
	 * b) During a ttconfig upgrade, the kernel was returning execution to the main thread before the 
	 *    threaded irq operation was completed. This also caused the device pointer to get mangled
	 *    which in turn stopped the flashing process mid-way. 
	 */
	mutex_lock(&cd->system_lock);
	rc = cyttsp6_adap_read(cd, cmd_return_ofs, return_buf, param_ret_size);
	mutex_unlock(&cd->system_lock);

	if (rc < 0) {
		dev_err(dev, "%s: Error on read 3 r=%d\n", __func__, rc);
		return rc;
	}

	return 0;
}

static int cyttsp6_request_exec_cmd_(struct device *dev, u8 mode,
		u8 *cmd_buf, size_t cmd_size, u8 *return_buf,
		size_t param_ret_size, int timeout_ms)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	return cyttsp6_exec_cmd(cd, mode, cmd_buf, cmd_size,
			return_buf, param_ret_size, timeout_ms);
}

static int cyttsp6_get_parameter(struct cyttsp6_core_data *cd, u8 param_id,
		u32 *param_value)
{
	u8 command_buf[CY_CMD_OP_GET_PARAM_CMD_SZ];
	u8 return_buf[CY_CMD_OP_GET_PARAM_RET_SZ];
	u8 param_size;
	u8 *value_buf;
	int rc;

	command_buf[0] = CY_CMD_OP_GET_PARAM;
	command_buf[1] = param_id;
	rc = cyttsp6_exec_cmd(cd, CY_MODE_OPERATIONAL,
			command_buf, CY_CMD_OP_GET_PARAM_CMD_SZ,
			return_buf, CY_CMD_OP_GET_PARAM_RET_SZ,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc < 0) {
		dev_err(cd->dev, "%s: Unable to execute get parameter command.\n",
			__func__);
		return rc;
	}

	if (return_buf[0] != param_id) {
		dev_err(cd->dev, "%s: Fail to execute get parameter command.\n",
			__func__);
		return -EIO;
	}

	param_size = return_buf[1];
	value_buf = &return_buf[2];

	*param_value = 0;
	while (param_size--)
		*param_value += *(value_buf++) << (8 * param_size);

	return 0;
}

static int cyttsp6_set_parameter(struct cyttsp6_core_data *cd, u8 param_id,
		u8 param_size, u32 param_value)
{
	u8 command_buf[CY_CMD_OP_SET_PARAM_CMD_SZ];
	u8 return_buf[CY_CMD_OP_SET_PARAM_RET_SZ];
	int rc;

	command_buf[0] = CY_CMD_OP_SET_PARAM;
	command_buf[1] = param_id;
	command_buf[2] = param_size;

	if (param_size == 1) {
		command_buf[3] = (u8)param_value;
	} else if (param_size == 2) {
		command_buf[3] = (u8)(param_value >> 8);
		command_buf[4] = (u8)param_value;
	} else if (param_size == 4) {
		command_buf[3] = (u8)(param_value >> 24);
		command_buf[4] = (u8)(param_value >> 16);
		command_buf[5] = (u8)(param_value >> 8);
		command_buf[6] = (u8)param_value;
	} else {
		dev_err(cd->dev, "%s: Invalid parameter size %d\n",
			__func__, param_size);
		return -EINVAL;
	}

	rc = cyttsp6_exec_cmd(cd, CY_MODE_OPERATIONAL,
			command_buf, 3 + param_size,
			return_buf, CY_CMD_OP_SET_PARAM_RET_SZ,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc < 0) {
		dev_err(cd->dev, "%s: Unable to execute set parameter command.\n",
			__func__);
		return rc;
	}

	if (return_buf[0] != param_id || return_buf[1] != param_size) {
		dev_err(cd->dev, "%s: Fail to execute set parameter command.\n",
			__func__);
		return -EIO;
	}

	return 0;
}

static int cyttsp6_get_scantype(struct cyttsp6_core_data *cd, u8 *scantype)
{
	int rc;
	u32 value;

	rc = cyttsp6_get_parameter(cd, CY_RAM_ID_SCAN_TYPE, &value);
	if (!rc)
		*scantype = (u8)value;

	return rc;
}

static int cyttsp6_set_scantype(struct cyttsp6_core_data *cd, u8 scantype)
{
	int rc;

	rc = cyttsp6_set_parameter(cd, CY_RAM_ID_SCAN_TYPE, 1, scantype);

	return rc;
}

static u8 _cyttsp6_generate_new_scantype(struct cyttsp6_core_data *cd)
{
	u8 new_scantype = cd->default_scantype;

	if (cd->apa_mc_en)
		new_scantype |= CY_SCAN_TYPE_APA_MC;
	if (cd->glove_en)
		new_scantype |= CY_SCAN_TYPE_GLOVE;
	if (cd->stylus_en)
		new_scantype |= CY_SCAN_TYPE_STYLUS;
	if (cd->proximity_en)
		new_scantype |= CY_SCAN_TYPE_PROXIMITY;

	return new_scantype;
}

static int cyttsp6_set_new_scan_type(struct cyttsp6_core_data *cd,
		u8 scan_type, bool enable)
{
	int inc = enable ? 1 : -1;
	int *en;
	int rc;
	u8 new_scantype;

	switch (scan_type) {
	case CY_ST_GLOVE:
		en = &cd->glove_en;
		break;
	case CY_ST_STYLUS:
		en = &cd->stylus_en;
		break;
	case CY_ST_PROXIMITY:
		en = &cd->proximity_en;
		break;
	case CY_ST_APA_MC:
		en = &cd->apa_mc_en;
		break;
	default:
		return -EINVAL;
	}

	*en += inc;

	new_scantype = _cyttsp6_generate_new_scantype(cd);

	rc = cyttsp6_set_scantype(cd, new_scantype);
	if (rc)
		*en -= inc;

	return rc;
}

static int cyttsp6_set_proximity(struct cyttsp6_core_data *cd, bool enable)
{
	int touchmode, touchmode_orig;
	int rc;

	rc = cyttsp6_get_parameter(cd, CY_RAM_ID_TOUCHMODE_ENABLED, &touchmode);
	if (rc)
		return rc;
	touchmode_orig = touchmode;

	if (enable)
		touchmode |= 0x80;
	else
		touchmode &= 0x7F;

	if (touchmode_orig == touchmode)
		return rc;

	rc = cyttsp6_set_parameter(cd, CY_RAM_ID_TOUCHMODE_ENABLED, 1,
			touchmode);

	return rc;
}

int cyttsp6_request_enable_scan_type_(struct device *dev, u8 scan_type)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	if (cd->cpdata->flags & CY_CORE_FLAG_SCAN_MODE_USES_RAM_ID_SCAN_TYPE)
		return cyttsp6_set_new_scan_type(cd, scan_type, true);
	if (scan_type == CY_ST_PROXIMITY)
		return cyttsp6_set_proximity(cd, true);

	return -EINVAL;
}

int cyttsp6_request_disable_scan_type_(struct device *dev, u8 scan_type)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	if (cd->cpdata->flags & CY_CORE_FLAG_SCAN_MODE_USES_RAM_ID_SCAN_TYPE)
		return cyttsp6_set_new_scan_type(cd, scan_type, false);
	if (scan_type == CY_ST_PROXIMITY)
		return cyttsp6_set_proximity(cd, false);

	return -EINVAL;
}

static int cyttsp6_read_config_block(struct cyttsp6_core_data *cd, u8 ebid,
		u16 row, u8 *data, u16 length)
{
	u8 command_buf[CY_CMD_CAT_READ_CFG_BLK_CMD_SZ];
	u8 *return_buf;
	int return_buf_sz;
	u16 crc;
	int rc;

	/* Allocate buffer for read config block command response
	 * Header(5) + Data(length) + CRC(2)
	 */
	return_buf_sz = CY_CMD_CAT_READ_CFG_BLK_RET_SZ + length;
	return_buf = kmalloc(return_buf_sz, GFP_KERNEL);
	if (!return_buf)
		return -ENOMEM;

	command_buf[0] = CY_CMD_CAT_READ_CFG_BLK;
	command_buf[1] = HI_BYTE(row);
	command_buf[2] = LO_BYTE(row);
	command_buf[3] = HI_BYTE(length);
	command_buf[4] = LO_BYTE(length);
	command_buf[5] = ebid;

	rc = cyttsp6_exec_cmd(cd, CY_MODE_CAT,
			command_buf, CY_CMD_CAT_READ_CFG_BLK_CMD_SZ,
			return_buf, return_buf_sz,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc) {
		dev_err(cd->dev, "%s: Error executing command r=%d\n",
			__func__, rc);
		goto free_buffer;
	}

	crc = cyttsp6_calc_app_crc(
		&return_buf[CY_CMD_CAT_READ_CFG_BLK_RET_HDR_SZ], length);

	/* Validate response */
	if (return_buf[0] != CY_CMD_STATUS_SUCCESS
			|| return_buf[1] != ebid
			|| return_buf[2] != HI_BYTE(length)
			|| return_buf[3] != LO_BYTE(length)
			|| return_buf[CY_CMD_CAT_READ_CFG_BLK_RET_HDR_SZ
				+ length] != HI_BYTE(crc)
			|| return_buf[CY_CMD_CAT_READ_CFG_BLK_RET_HDR_SZ
				+ length + 1] != LO_BYTE(crc)) {
		dev_err(cd->dev, "%s: Fail executing command\n", __func__);
		rc = -EINVAL;
		goto free_buffer;
	}

	memcpy(data, &return_buf[CY_CMD_CAT_READ_CFG_BLK_RET_HDR_SZ], length);

	cyttsp6_pr_buf(cd->dev, cd->pr_buf, data, length, "read_config_block");

free_buffer:
	kfree(return_buf);

	return rc;
}

static int cyttsp6_write_config_block(struct cyttsp6_core_data *cd, u8 ebid,
		u16 row, const u8 *data, u16 length)
{
	u8 return_buf[CY_CMD_CAT_WRITE_CFG_BLK_RET_SZ];
	u8 *command_buf;
	int command_buf_sz;
	u16 crc;
	int rc;

	/* Allocate buffer for write config block command
	 * Header(6) + Data(length) + Security Key(8) + CRC(2)
	 */
	command_buf_sz = CY_CMD_CAT_WRITE_CFG_BLK_CMD_SZ + length
		+ sizeof(security_key);
	command_buf = kmalloc(command_buf_sz, GFP_KERNEL);
	if (!command_buf)
		return -ENOMEM;

	crc = cyttsp6_calc_app_crc(data, length);

	command_buf[0] = CY_CMD_CAT_WRITE_CFG_BLK;
	command_buf[1] = HI_BYTE(row);
	command_buf[2] = LO_BYTE(row);
	command_buf[3] = HI_BYTE(length);
	command_buf[4] = LO_BYTE(length);
	command_buf[5] = ebid;

	command_buf[CY_CMD_CAT_WRITE_CFG_BLK_CMD_HDR_SZ + length
		+ sizeof(security_key)] = HI_BYTE(crc);
	command_buf[CY_CMD_CAT_WRITE_CFG_BLK_CMD_HDR_SZ + length + 1
		+ sizeof(security_key)] = LO_BYTE(crc);

	memcpy(&command_buf[CY_CMD_CAT_WRITE_CFG_BLK_CMD_HDR_SZ], data, length);
	memcpy(&command_buf[CY_CMD_CAT_WRITE_CFG_BLK_CMD_HDR_SZ + length],
		security_key, sizeof(security_key));

	cyttsp6_pr_buf(cd->dev, cd->pr_buf, command_buf, command_buf_sz,
		"write_config_block");

	rc = cyttsp6_exec_cmd(cd, CY_MODE_CAT,
			command_buf, command_buf_sz,
			return_buf, CY_CMD_CAT_WRITE_CFG_BLK_RET_SZ,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc) {
		dev_err(cd->dev, "%s: Error executing command r=%d\n",
			__func__, rc);
		goto free_buffer;
	}

	/* Validate response */
	if (return_buf[0] != CY_CMD_STATUS_SUCCESS
			|| return_buf[1] != ebid
			|| return_buf[2] != HI_BYTE(length)
			|| return_buf[3] != LO_BYTE(length)) {
		dev_err(cd->dev, "%s: Fail executing command\n", __func__);
		rc = -EINVAL;
		goto free_buffer;
	}

free_buffer:
	kfree(command_buf);

	return rc;
}

static int cyttsp6_get_config_row_size(struct cyttsp6_core_data *cd,
		u16 *config_row_size)
{
	u8 command_buf[CY_CMD_CAT_GET_CFG_ROW_SIZE_CMD_SZ];
	u8 return_buf[CY_CMD_CAT_GET_CFG_ROW_SIZE_RET_SZ];
	int rc;

	command_buf[0] = CY_CMD_CAT_GET_CFG_ROW_SZ;

	rc = cyttsp6_exec_cmd(cd, CY_MODE_CAT,
			command_buf, CY_CMD_CAT_GET_CFG_ROW_SIZE_CMD_SZ,
			return_buf, CY_CMD_CAT_GET_CFG_ROW_SIZE_RET_SZ,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc) {
		dev_err(cd->dev, "%s: Error executing command r=%d\n",
			__func__, rc);
		goto exit;
	}

	*config_row_size = get_unaligned_be16(&return_buf[0]);

exit:
	return rc;
}

static int cyttsp6_request_config_row_size_(struct device *dev,
		u16 *config_row_size)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	return cyttsp6_get_config_row_size(cd, config_row_size);
}

static int cyttsp6_verify_config_block_crc(struct cyttsp6_core_data *cd,
		u8 ebid, u16 *calc_crc, u16 *stored_crc, bool *match)
{
	u8 command_buf[CY_CMD_CAT_VERIFY_CFG_BLK_CRC_CMD_SZ];
	u8 return_buf[CY_CMD_CAT_VERIFY_CFG_BLK_CRC_RET_SZ];
	int rc;

	command_buf[0] = CY_CMD_CAT_VERIFY_CFG_BLK_CRC;
	command_buf[1] = ebid;
	rc = cyttsp6_exec_cmd(cd, CY_MODE_CAT,
			command_buf, CY_CMD_CAT_VERIFY_CFG_BLK_CRC_CMD_SZ,
			return_buf, CY_CMD_CAT_VERIFY_CFG_BLK_CRC_RET_SZ,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc) {
		dev_err(cd->dev, "%s: Error executing command r=%d\n",
			__func__, rc);
		goto exit;
	}

	*calc_crc = get_unaligned_be16(&return_buf[1]);
	*stored_crc = get_unaligned_be16(&return_buf[3]);
	if (match)
		*match = !return_buf[0];
exit:
	return rc;
}

static int cyttsp6_get_config_block_crc(struct cyttsp6_core_data *cd,
		u8 ebid, u16 *crc)
{
	u8 command_buf[CY_CMD_OP_GET_CFG_BLK_CRC_CMD_SZ];
	u8 return_buf[CY_CMD_OP_GET_CFG_BLK_CRC_RET_SZ];
	int rc;

	command_buf[0] = CY_CMD_OP_GET_CRC;
	command_buf[1] = ebid;

	rc = cyttsp6_exec_cmd(cd, CY_MODE_OPERATIONAL,
			command_buf, CY_CMD_OP_GET_CFG_BLK_CRC_CMD_SZ,
			return_buf, CY_CMD_OP_GET_CFG_BLK_CRC_RET_SZ,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc) {
		dev_err(cd->dev, "%s: Error executing command r=%d\n",
			__func__, rc);
		goto exit;
	}

	/* Validate response */
	if (return_buf[0] != CY_CMD_STATUS_SUCCESS) {
		dev_err(cd->dev, "%s: Fail executing command\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	*crc = get_unaligned_be16(&return_buf[1]);

exit:
	return rc;
}

static int cyttsp6_get_config_length(struct cyttsp6_core_data *cd, u8 ebid,
		u16 *length, u16 *max_length)
{
	struct cyttsp6_sysinfo *si = &cd->sysinfo;
	u8 data[CY_CONFIG_LENGTH_INFO_SIZE];
	int rc;

	rc = cyttsp6_read_config_block(cd, ebid, CY_CONFIG_LENGTH_INFO_OFFSET,
			data, sizeof(data));
	if (rc) {
		dev_err(cd->dev, "%s: Error on read config block\n", __func__);
		goto exit;
	}

	*length = GET_FIELD16(si, &data[CY_CONFIG_LENGTH_OFFSET]);
	*max_length = GET_FIELD16(si, &data[CY_CONFIG_MAXLENGTH_OFFSET]);

exit:
	return rc;
}

static int cyttsp6_write_config_common(struct cyttsp6_core_data *cd, u8 ebid,
		u16 offset, u8 *data, u16 length)
{
	u16 cur_block, cur_off, end_block, end_off;
	int copy_len;
	u16 config_row_size = 0;
	u8 *row_data = NULL;
	int rc;

	rc = cyttsp6_get_config_row_size(cd, &config_row_size);
	if (rc) {
		dev_err(cd->dev, "%s: Cannot get config row size\n", __func__);
		return rc;
	}

	cur_block = offset / config_row_size;
	cur_off = offset % config_row_size;

	/*
	 * length should be greater than or equal to 1
	 */
	end_block = (offset + length - 1) / config_row_size;
	end_off = (offset + length - 1) % config_row_size;

	/* Check whether we need to fetch the whole block first */
	if (cur_off == 0)
		goto no_offset;

	row_data = kmalloc(config_row_size, GFP_KERNEL);
	if (!row_data)
		return -ENOMEM;

	copy_len = (cur_block == end_block) ?
		length : config_row_size - cur_off;

	/* Read up to current offset, append the new data and write it back */
	rc = cyttsp6_read_config_block(cd, ebid, cur_block, row_data, cur_off);
	if (rc) {
		dev_err(cd->dev, "%s: Error on read config block\n", __func__);
		goto free_row_data;
	}

	memcpy(&row_data[cur_off], data, copy_len);

	rc = cyttsp6_write_config_block(cd, ebid, cur_block, row_data,
			cur_off + copy_len);
	if (rc) {
		dev_err(cd->dev, "%s: Error on initial write config block\n",
			__func__);
		goto free_row_data;
	}

	data += copy_len;
	cur_off = 0;
	cur_block++;

no_offset:
	while (cur_block < end_block) {
		rc = cyttsp6_write_config_block(cd, ebid, cur_block, data,
				config_row_size);
		if (rc) {
			dev_err(cd->dev, "%s: Error on write config block\n",
				__func__);
			goto free_row_data;
		}

		data += config_row_size;
		cur_block++;
	}

	/* Last block */
	if (cur_block == end_block) {
		rc = cyttsp6_write_config_block(cd, ebid, end_block, data,
				end_off);
		if (rc) {
			dev_err(cd->dev, "%s: Error on last write config block\n",
				__func__);
			goto free_row_data;
		}
	}

free_row_data:
	kfree(row_data);

	return rc;
}

static int cyttsp6_write_config(struct cyttsp6_core_data *cd, u8 ebid,
		u16 offset, u8 *data, u16 length)
{
	int rc;
	struct cyttsp6_sysinfo *si = &cd->sysinfo;
	u16 crc_new, crc_old;
	u8 crc_data[2];
	u16 crc_offset;
	u16 conf_len;

	/* CRC is stored at config max length offset */
	rc = cyttsp6_get_config_length(cd, ebid, &conf_len, &crc_offset);
	if (rc) {
		dev_err(cd->dev, "%s: Error on get config length\n", __func__);
		return rc;
	}

	/* Allow CRC update also */
	if (offset + length > crc_offset + 2) {
		dev_err(cd->dev, "%s: offset + length exceeds max length(%d)\n",
			__func__, crc_offset + 2);
		return -EINVAL;
	}

	rc = cyttsp6_write_config_common(cd, ebid, offset, data, length);
	if (rc) {
		dev_err(cd->dev, "%s: Error on write config\n", __func__);
		return rc;
	}

	/* Verify config block CRC */
	rc = cyttsp6_verify_config_block_crc(cd, ebid,
			&crc_new, &crc_old, NULL);
	if (rc) {
		dev_err(cd->dev, "%s: Error on verify config block crc\n",
			__func__);
		return rc;
	}

	dev_vdbg(cd->dev, "%s: crc_new:%04X crc_old:%04X\n",
		__func__, crc_new, crc_old);

	if (crc_new == crc_old) {
		dev_vdbg(cd->dev, "%s: Calculated crc matches stored crc\n",
			__func__);
		return rc;
	}

	PUT_FIELD16(si, crc_new, crc_data);

	rc = cyttsp6_write_config_common(cd, ebid, crc_offset, crc_data, 2);
	if (rc)
		dev_err(cd->dev, "%s: Error on write config crc\n", __func__);

	return rc;
}

static int cyttsp6_request_write_config_(struct device *dev, u8 ebid,
		u16 offset, u8 *data, u16 length)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	return cyttsp6_write_config(cd, ebid, offset, data, length);
}

#ifdef CYTTSP6_WATCHDOG_NULL_CMD
static int cyttsp6_watchdog_work_(struct cyttsp6_core_data *cd)
{
	u8 cmd_buf[CY_CMD_OP_NULL_CMD_SZ];
	int rc;

	cmd_buf[0] = CY_CMD_OP_NULL;
	rc = cyttsp6_exec_cmd(cd, cd->mode,
			cmd_buf, CY_CMD_OP_NULL_CMD_SZ,
			NULL, CY_CMD_OP_NULL_RET_SZ,
			CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc < 0)
		dev_err(cd->dev, "%s: Watchdog NULL cmd failed.\n", __func__);

	return rc;
}

static void cyttsp6_watchdog_work(struct work_struct *work)
{
	struct cyttsp6_core_data *cd =
		container_of(work, struct cyttsp6_core_data, watchdog_work);
	bool restart = false;
	int rc;

	if (request_exclusive(cd, cd->dev, 1))
		goto exit;

	rc = cyttsp6_watchdog_work_(cd);
	if (rc < 0)
		restart = true;

	release_exclusive(cd, cd->dev);
exit:
	if (restart)
		cyttsp6_queue_startup(cd);
	else
		cyttsp6_start_wd_timer(cd);
}
#else
static int _cyttsp6_watchdog_work(struct cyttsp6_core_data *cd)
{
	u8 mode[2];
	int rc;

	rc = cyttsp6_adap_read(cd, CY_REG_BASE, &mode, sizeof(mode));
	if (rc) {
		dev_err(cd->dev, "%s: failed to access device r=%d\n",
			__func__, rc);
		return rc;
	}

	if (IS_BOOTLOADER(mode[0], mode[1])) {
		dev_err(cd->dev, "%s: device found in bootloader mode\n",
			__func__);
		return -EINVAL;
	}

	return rc;
}

static void cyttsp6_watchdog_work(struct work_struct *work)
{
	struct cyttsp6_core_data *cd =
		container_of(work, struct cyttsp6_core_data, watchdog_work);
	bool restart = false;
	int rc;

	mutex_lock(&cd->system_lock);

	rc = _cyttsp6_watchdog_work(cd);
	if (rc < 0)
		restart = true;

	mutex_unlock(&cd->system_lock);

	if (restart)
		cyttsp6_queue_startup(cd);
	else
		cyttsp6_start_wd_timer(cd);
}
#endif

static int _cyttsp6_put_device_into_easy_wakeup(struct cyttsp6_core_data *cd)
{
	u8 command_buf[CY_CMD_OP_WAIT_FOR_EVENT_CMD_SZ];
	int rc;

	dev_dbg(cd->dev, "%s\n", __func__);
	if (!IS_TTSP_VER_GE(&cd->sysinfo, 2, 5))
		return -EINVAL;

	command_buf[0] = CY_CMD_OP_WAIT_FOR_EVENT;
	command_buf[1] = cd->easy_wakeup_gesture;

	rc = _cyttsp6_exec_cmd(cd, CY_MODE_OPERATIONAL, command_buf,
			CY_CMD_OP_WAIT_FOR_EVENT_CMD_SZ);
	if (rc) {
		dev_err(cd->dev, "%s: Error executing command r=%d\n",
			__func__, rc);
		return rc;
	}

	/* Getting interrupt means error */
	mutex_unlock(&cd->system_lock);
	rc = wait_event_timeout(cd->wait_q,
			(cd->int_status & CY_INT_EXEC_CMD) == 0,
			msecs_to_jiffies(CY_CORE_CMD_WAIT_FOR_EVENT_TIMEOUT));
	mutex_lock(&cd->system_lock);
	if (IS_TMO(rc)) {
		dev_err(cd->dev, "%s: TMO waiting rc %d\n",
			__func__, rc);
		cd->int_status &= ~CY_INT_EXEC_CMD;
		return rc;
	}

	return -EINVAL;
}

static int cyttsp6_wait_for_refresh_cycle(struct cyttsp6_core_data *cd,
		int cycle)
{
	int active_refresh_cycle_ms;

	if (cd->active_refresh_cycle_ms)
		active_refresh_cycle_ms = cd->active_refresh_cycle_ms;
	else
		active_refresh_cycle_ms = 20;

	msleep(cycle * active_refresh_cycle_ms);

	return 0;
}

static int _cyttsp6_put_device_into_sleep(struct cyttsp6_core_data *cd,
		u8 hst_mode_reg)
{
	int rc;

	dev_dbg(cd->dev, "%s\n", __func__);
	if (IS_DEEP_SLEEP_CONFIGURED(cd->easy_wakeup_gesture))
		rc = _cyttsp6_put_device_into_deep_sleep(cd, hst_mode_reg);
	else
		rc = _cyttsp6_put_device_into_easy_wakeup(cd);

	dev_dbg(cd->dev, "%s ret %d\n", __func__,rc);
	return rc;
}

static int _cyttsp6_core_sleep_device(struct cyttsp6_core_data *cd)
{
	u8 mode[2];
	int rc = 0;

	dev_dbg(cd->dev, "%s\n", __func__);
	rc = cyttsp6_adap_read(cd, CY_REG_BASE, &mode, sizeof(mode));
	if (rc) {
		dev_err(cd->dev, "%s: Fail read adapter r=%d\n", __func__, rc);
		goto exit;
	}

	if (IS_BOOTLOADER(mode[0], mode[1])) {
		dev_err(cd->dev, "%s: Device in BOOTLADER mode.\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	/* Deep sleep is only allowed in Operating mode */
	if (GET_HSTMODE(mode[0]) != CY_HST_OPERATE) {
		dev_err(cd->dev, "%s: Device is not in Operating mode (%02X)\n",
			__func__, GET_HSTMODE(mode[0]));
		mutex_unlock(&cd->system_lock);
		/* Try switching to Operating mode */
		rc = set_mode_(cd, CY_MODE_OPERATIONAL);
		mutex_lock(&cd->system_lock);
		if (rc < 0) {
			dev_err(cd->dev, "%s: failed to set mode to Operational rc=%d\n",
				__func__, rc);
			_cyttsp6_queue_startup(cd);
			rc = 0;
			goto exit;
		}

		/* Get the new host mode register value */
		rc = cyttsp6_adap_read(cd, CY_REG_BASE, &mode, sizeof(mode));
		if (rc) {
			dev_err(cd->dev, "%s: Fail read adapter r=%d\n",
				__func__, rc);
			goto exit;
		}
	}

	rc = _cyttsp6_put_device_into_sleep(cd, mode[0]);

exit:
	return rc;
}

static int _cyttsp6_core_poweroff_device(struct cyttsp6_core_data *cd)
{
	int rc;

	if (cd->irq_enabled) {
		cd->irq_enabled = false;
		disable_irq_nosync(cd->irq);
	}

	/* No need for cd->pdata->power check since we did it in probe */
	rc = cd->cpdata->power(cd->cpdata, 0, cd->dev, 0);
	if (rc < 0)
		dev_err(cd->dev, "%s: HW Power down fails r=%d\n",
			__func__, rc);

	return rc;
}

static int _cyttsp6_core_sleep_(struct cyttsp6_core_data *cd)
{
	int rc;

	if (cd->sleep_state == SS_SLEEP_OFF)
		cd->sleep_state = SS_SLEEPING;
	else
		return 1;

	if (cd->cpdata->flags & CY_CORE_FLAG_POWEROFF_ON_SLEEP)
		rc = _cyttsp6_core_poweroff_device(cd);
	else
		rc = _cyttsp6_core_sleep_device(cd);

	if (rc) {
		dev_info(cd->dev, "%s: sleep state rc %d\n", __func__,rc);
		cd->sleep_state = SS_SLEEP_OFF;
		cyttsp6_start_wd_timer(cd);
	} else {
		dev_dbg(cd->dev, "%s: sleep state rc %d\n", __func__,rc);
		cd->sleep_state = SS_SLEEP_ON;
		cd->int_status |= CY_INT_IGNORE;
	}

	return rc;
}

static int cyttsp6_core_sleep_(struct cyttsp6_core_data *cd)
{
	int rc;

	cyttsp6_stop_wd_timer(cd);
	cancel_work_sync(&cd->startup_work);
	cyttsp6_stop_wd_timer(cd);

	mutex_lock(&cd->system_lock);
	rc = _cyttsp6_core_sleep_(cd);
	mutex_unlock(&cd->system_lock);

	return rc;
}

static int cyttsp6_core_sleep(struct cyttsp6_core_data *cd)
{
	int rc;

	rc = request_exclusive(cd, cd->dev,
			CY_CORE_SLEEP_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0)
		return 0;

	rc = cyttsp6_core_sleep_(cd);

	release_exclusive(cd, cd->dev);

	/* Give time to FW to sleep */
	cyttsp6_wait_for_refresh_cycle(cd, 2);

	return rc;
}

static int _cyttsp6_awake_device_from_deep_sleep(struct cyttsp6_core_data *cd,
		int timeout_ms)
{
	struct device *dev = cd->dev;
	u8 mode;
	int t;
	int rc = 0;

	cd->int_status |= CY_INT_AWAKE;

	/* Wake up using platform power function */
	if (cd->cpdata->power) {
		dev_dbg(dev, "%s: Power up HW\n", __func__);
		cd->cpdata->power(cd->cpdata, 1, dev, &cd->ignore_irq);
	}

	/* Initiate a read transaction to wake up */
	cyttsp6_adap_read(cd, CY_REG_BASE, &mode, sizeof(mode));

	mutex_unlock(&cd->system_lock);

	t = wait_event_timeout(cd->wait_q,
			(cd->int_status & CY_INT_AWAKE) == 0,
			msecs_to_jiffies(timeout_ms));
	mutex_lock(&cd->system_lock);
	if (IS_TMO(t)) {
		dev_dbg(dev, "%s: TMO waiting for wakeup\n", __func__);
		cd->int_status &= ~CY_INT_AWAKE;
		/* Perform a read transaction to check if device is awake */
		rc = cyttsp6_adap_read(cd, CY_REG_BASE, &mode, sizeof(mode));
		if (rc < 0 || GET_HSTMODE(mode) != CY_HST_OPERATE) {
			dev_err(dev, "%s: Queueing startup\n", __func__);
			/* Try starting up */
			_cyttsp6_queue_startup(cd);
		}
	}

	return rc;
}

static int _cyttsp6_awake_device(struct cyttsp6_core_data *cd)
{
	int timeout_ms;

	if (cd->wake_initiated_by_device) {
		cd->wake_initiated_by_device = 0;
		/* To prevent sequential wake/sleep caused by ttsp modules */
		msleep(20);
		return 0;
	}

	if (IS_DEEP_SLEEP_CONFIGURED(cd->easy_wakeup_gesture))
		timeout_ms = CY_CORE_WAKEUP_TIMEOUT;
	else
		timeout_ms = CY_CORE_WAKEUP_TIMEOUT * 4;

	return _cyttsp6_awake_device_from_deep_sleep(cd, timeout_ms);
}

static int _exit_bootloader(struct cyttsp6_core_data *cd)
{
	const u8 *exit_cmd;
	int exit_cmd_size;
	int rc;

	cd->int_status &= ~CY_INT_IGNORE;
	cd->int_status |= CY_INT_MODE_CHANGE;

	if ((cd->cpdata->flags & CY_CORE_FLAG_BOOTLOADER_FAST_EXIT)
			&& cd->bl_fast_exit) {
		dev_vdbg(cd->dev, "%s: Issuing ldr_fast_exit\n", __func__);
		exit_cmd = ldr_fast_exit;
		exit_cmd_size = sizeof(ldr_fast_exit);
	} else {
		dev_vdbg(cd->dev, "%s: Issuing ldr_exit\n", __func__);
		exit_cmd = ldr_exit;
		exit_cmd_size = sizeof(ldr_exit);
	}

	rc = cyttsp6_adap_write(cd, CY_REG_BASE, exit_cmd, exit_cmd_size);
	if (rc < 0)
		dev_err(cd->dev, "%s: Fail to write rc=%d\n", __func__, rc);

	return rc;
}

static int _fast_startup(struct cyttsp6_core_data *cd)
{
	struct device *dev = cd->dev;
	int rc;

	mutex_unlock(&cd->system_lock);
	rc = cyttsp6_wait_bl_heartbeat(cd);
	mutex_lock(&cd->system_lock);
	if (rc) {
		dev_err(dev, "%s: Error on waiting bl heartbeat r=%d\n",
			__func__, rc);
		return rc;
	}

	/* exit bl into sysinfo mode */
	rc = _exit_bootloader(cd);
	if (rc < 0) {
		dev_err(dev, "%s: Fail exit bootloader rc=%d\n",
			__func__, rc);
		return rc;
	}

	mutex_unlock(&cd->system_lock);
	rc = cyttsp6_wait_sysinfo_mode(cd);
	if (rc) {
		dev_err(dev, "%s: Fail switch to sysinfo mode, r=%d\n",
			__func__, rc);
		goto exit_lock;
	}

	rc = set_mode_(cd, CY_MODE_OPERATIONAL);
	if (rc) {
		dev_err(dev, "%s: Fail set mode to Operational mode, r=%d\n",
			__func__, rc);
		goto exit_lock;
	}

exit_lock:
	mutex_lock(&cd->system_lock);

	return rc;
}

static int _cyttsp6_core_poweron_device(struct cyttsp6_core_data *cd)
{
	struct device *dev = cd->dev;
	int rc;

	cd->mode = CY_MODE_UNKNOWN;

	/* No need for cd->pdata->power check since we did it in probe */
	rc = cd->cpdata->power(cd->cpdata, 1, dev, 0);
	if (rc < 0) {
		dev_err(dev, "%s: HW Power up fails r=%d\n", __func__, rc);
		goto exit;
	}

	if (!cd->irq_enabled) {
		cd->irq_enabled = true;
		enable_irq(cd->irq);
	}

	rc = _fast_startup(cd);

exit:
	return rc;
}

static int cyttsp6_core_wake_(struct cyttsp6_core_data *cd)
{
	int rc;

	/* Already woken? */
	mutex_lock(&cd->system_lock);
	if (cd->sleep_state == SS_SLEEP_OFF) {
		mutex_unlock(&cd->system_lock);
		return 0;
	}

	cd->int_status &= ~CY_INT_IGNORE;
	cd->sleep_state = SS_WAKING;

	if (cd->cpdata->flags & CY_CORE_FLAG_POWEROFF_ON_SLEEP)
		rc = _cyttsp6_core_poweron_device(cd);
	else
		rc = _cyttsp6_awake_device(cd);

	if (rc)
		_cyttsp6_queue_startup(cd);

	cd->sleep_state = SS_SLEEP_OFF;
	mutex_unlock(&cd->system_lock);

	cyttsp6_start_wd_timer(cd);

	return 0;
}

static int cyttsp6_core_wake(struct cyttsp6_core_data *cd)
{
	int rc;

	rc = request_exclusive(cd, cd->dev,
			CY_CORE_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0)
		return 0;

	rc = cyttsp6_core_wake_(cd);

	release_exclusive(cd, cd->dev);

	/* If a startup queued in wake, wait it to finish */
	wait_event_timeout(cd->wait_q, cd->startup_state == STARTUP_NONE,
			msecs_to_jiffies(CY_CORE_RESET_AND_WAIT_TIMEOUT));

	return rc;
}
static int cyttsp6_read_gesture_enable(struct cyttsp6_core_data *cd,
		u8 *gesture_enable){
	int rc, rowid,offset;
	u8 *row_data;
	u16 config_row_size;

	rc = cyttsp6_get_config_row_size(cd, &config_row_size);
	if (rc) {
		dev_err(cd->dev, "%s: Cannot get config row size\n", __func__);
		return rc;
	}

	rowid = CY_CONFIG_GESTURE_ENABLE_ADDR / config_row_size;
	offset = CY_CONFIG_GESTURE_ENABLE_ADDR % config_row_size;

	row_data = kmalloc(offset,GFP_KERNEL);
	if(!row_data){
		dev_err(cd->dev, "%s: failed to allocate memory\n",__func__);
		return -ENOMEM;
	}

	rc = cyttsp6_read_config_block(cd, CY_TCH_PARM_EBID,rowid ,
		       row_data, offset);
	if (rc < 0){
		dev_err(cd->dev, "%s: failed to read gesture enable rc=%d\n",
			__func__, rc);
		goto free_data;
	}
	*gesture_enable = row_data[offset - 1];
free_data:
	kfree(row_data);
	return 0;
}

static int cyttsp6_get_ttconfig_info(struct cyttsp6_core_data *cd)
{
	struct cyttsp6_sysinfo *si = &cd->sysinfo;
	u16 length, max_length;
	u16 version = 0;
	u16 crc = 0;
	int rc, rc2;
	u8 gesture_enable;

	rc = set_mode_(cd, CY_MODE_CAT);
	if (rc < 0) {
		dev_err(cd->dev, "%s: failed to set mode to CAT rc=%d\n",
			__func__, rc);
		return rc;
	}

	/* CY_ITO regs already has TTConfig version. */
	version = si->si_ptrs.cydata->cyito_verh << 8;
	version |= si->si_ptrs.cydata->cyito_verl;

	rc = cyttsp6_get_config_length(cd, CY_TCH_PARM_EBID,
			&length, &max_length);
	if (rc < 0) {
		dev_err(cd->dev, "%s: failed to get ttconfig properties rc=%d\n",
			__func__, rc);
		goto exit;
	}

	rc = cyttsp6_read_gesture_enable(cd, &gesture_enable);
	if (rc < 0) {
		dev_err(cd->dev, "%s: failed to get ttconfig crc rc=%d\n",
			__func__, rc);
		goto exit;
	}

	rc = set_mode_(cd, CY_MODE_OPERATIONAL);
	if (rc < 0) {
		dev_err(cd->dev, "%s: failed to set mode to Operational rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = cyttsp6_get_config_block_crc(cd, CY_TCH_PARM_EBID, &crc);
	if (rc < 0) {
		dev_err(cd->dev, "%s: failed to get ttconfig crc rc=%d\n",
			__func__, rc);
		return rc;
	}

	mutex_lock(&cd->system_lock);
	si->ttconfig.version = version;
	si->ttconfig.length = length;
	si->ttconfig.max_length = max_length;
	si->ttconfig.crc = crc;
	si->ttconfig.gesture_enable = gesture_enable;

	dev_info(cd->dev, "%s: TT Config Version:%04X Length:%d Max Length:%d CRC:%04X\n",
		__func__, si->ttconfig.version, si->ttconfig.length,
		si->ttconfig.max_length, si->ttconfig.crc);
	mutex_unlock(&cd->system_lock);

	return 0;
exit:
	rc2 = set_mode_(cd, CY_MODE_OPERATIONAL);
	if (rc2 < 0)
		dev_err(cd->dev, "%s: failed to set mode to Operational rc=%d\n",
			__func__, rc2);

	return rc;
}

static int cyttsp6_get_active_refresh_cycle(struct cyttsp6_core_data *cd)
{
	int rc;
	u32 value;

	rc = cyttsp6_get_parameter(cd, CY_RAM_ID_REFRESH_INTERVAL, &value);
	if (!rc)
		cd->active_refresh_cycle_ms = (u8)value;

	return rc;
}

static int cyttsp6_set_initial_scantype(struct cyttsp6_core_data *cd)
{
	u8 new_scantype;
	int rc;

	rc = cyttsp6_get_scantype(cd, &cd->default_scantype);
	if (rc < 0) {
		dev_err(cd->dev, "%s: failed to get scantype rc=%d\n",
			__func__, rc);
		goto exit;
	}

	/* Disable proximity sensing by default */
	cd->default_scantype &= ~CY_SCAN_TYPE_PROXIMITY;

	new_scantype = _cyttsp6_generate_new_scantype(cd);

	rc = cyttsp6_set_scantype(cd, new_scantype);
	if (rc < 0) {
		dev_err(cd->dev, "%s: failed to set scantype rc=%d\n",
			__func__, rc);
		goto exit;
	}
exit:
	return rc;
}
static int cyttsp6_startup_(struct cyttsp6_core_data *cd, bool reset_needed)
{
	int retry = CY_CORE_STARTUP_RETRY_COUNT;
	int rc;
	bool detected = false;

	dev_dbg(cd->dev, "%s: enter...\n", __func__);

	cyttsp6_stop_wd_timer(cd);

reset:
	if (retry != CY_CORE_STARTUP_RETRY_COUNT)
		dev_dbg(cd->dev, "%s: Retry %d\n", __func__,
			CY_CORE_STARTUP_RETRY_COUNT - retry);

	if (reset_needed || retry != CY_CORE_STARTUP_RETRY_COUNT) {
		/* reset hardware and wait for heartbeat */
		rc = cyttsp6_reset_and_wait(cd);
		if (rc < 0) {
			dev_err(cd->dev, "%s: Error on h/w reset r=%d\n",
					__func__, rc);
			if (retry--)
				goto reset;
			goto exit;
		}
	}

	detected = true;

	/*
	 * reset_needed flag should be set as FALSE only by cyttsp6_probe().
	 */
	if (!reset_needed) {
		rc = cyttsp6_wait_bl_heartbeat(cd);
		if (rc < 0) {
			/* force reset since bl heartbeat is not get. */
			if (retry--)
				goto reset;
			goto exit;
		}
	}

	/* exit bl into sysinfo mode */
	mutex_lock(&cd->system_lock);
	rc = _exit_bootloader(cd);
	mutex_unlock(&cd->system_lock);
	if (rc < 0) {
		dev_err(cd->dev, "%s: Fail exit bootloader rc=%d\n",
			__func__, rc);
		if (retry--)
			goto reset;
		goto exit;
	}

	rc = cyttsp6_wait_sysinfo_mode(cd);
	if (rc < 0) {
		u8 buf[sizeof(ldr_err_app)];
		int rc1;

		/* Check for invalid/corrupted touch application */
		rc1 = cyttsp6_adap_read(cd, CY_REG_BASE, buf,
			sizeof(ldr_err_app));
		if (rc1) {
			dev_err(cd->dev, "%s: Fail to read rc=%d\n",
				__func__, rc1);
		} else if (!memcmp(buf, ldr_err_app, sizeof(ldr_err_app))) {
			dev_err(cd->dev, "%s: Error launching touch application\n",
				__func__);
			mutex_lock(&cd->system_lock);
			cd->invalid_touch_app = true;
			mutex_unlock(&cd->system_lock);
			goto exit_no_wd;
		}

		if (retry--)
			goto reset;
		goto exit;
	}

	mutex_lock(&cd->system_lock);
	cd->invalid_touch_app = false;
	cd->bl_fast_exit = true;
	mutex_unlock(&cd->system_lock);

	/* read sysinfo data */
	dev_vdbg(cd->dev, "%s: get sysinfo regs..\n", __func__);
	rc = cyttsp6_get_sysinfo_regs_(cd);
	if (rc < 0) {
		dev_err(cd->dev, "%s: failed to get sysinfo regs rc=%d\n",
			__func__, rc);
		if (retry--)
			goto reset;
		goto exit;
	}

	dev_info(cd->dev, "TTSP Protocol Version is %u.%u\n",
			cd->sysinfo.si_ptrs.cydata->ttsp_ver_major,
			cd->sysinfo.si_ptrs.cydata->ttsp_ver_minor);

	rc = set_mode_(cd, CY_MODE_OPERATIONAL);
	if (rc < 0) {
		dev_err(cd->dev, "%s: failed to set mode to operational rc=%d\n",
			__func__, rc);
		if (retry--)
			goto reset;
		goto exit;
	}

	if (cd->cpdata->flags & CY_CORE_FLAG_SCAN_MODE_USES_RAM_ID_SCAN_TYPE)
		rc = cyttsp6_set_initial_scantype(cd);
	else
		rc = cyttsp6_set_proximity(cd, false);
	if (rc < 0) {
		dev_err(cd->dev, "%s: failed to set scantype rc=%d\n",
			__func__, rc);
		if (retry--)
			goto reset;
		goto exit;
	}

	rc = cyttsp6_get_ttconfig_info(cd);
	if (rc < 0) {
		dev_err(cd->dev, "%s: failed to get ttconfig info rc=%d\n",
			__func__, rc);
		if (retry--)
			goto reset;
		goto exit;
	}

	rc = cyttsp6_get_active_refresh_cycle(cd);
	if (rc < 0)
		dev_err(cd->dev, "%s: failed to get refresh cycle time rc=%d\n",
			__func__, rc);

	/* attention startup */
	call_atten_cb(cd, CY_ATTEN_STARTUP, 0);

	/* restore to sleep if was suspended */
	mutex_lock(&cd->system_lock);
	if (cd->sleep_state == SS_SLEEP_ON) {
		cd->sleep_state = SS_SLEEP_OFF;
		_cyttsp6_core_sleep_(cd);
		mutex_unlock(&cd->system_lock);
		/* watchdog is restarted by cyttsp6_core_sleep_() on error */
		goto exit_no_wd;
	}
	mutex_unlock(&cd->system_lock);

exit:
	cyttsp6_start_wd_timer(cd);

exit_no_wd:
	if (!detected)
		rc = -ENODEV;

	/* Required for signal to the TTHE */
	dev_info(cd->dev, "%s: cyttsp6_exit startup rc=%d...\n", __func__, rc);

	return rc;
}

static int cyttsp6_startup(struct cyttsp6_core_data *cd, bool reset_needed)
{
	int rc;

	mutex_lock(&cd->system_lock);
	cd->startup_state = STARTUP_RUNNING;
	mutex_unlock(&cd->system_lock);

	rc = request_exclusive(cd, cd->dev, CY_CORE_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0)
		goto exit;

	rc = cyttsp6_startup_(cd, reset_needed);

	release_exclusive(cd, cd->dev);

exit:
	mutex_lock(&cd->system_lock);
	cd->startup_state = STARTUP_NONE;
	mutex_unlock(&cd->system_lock);

	/* Wake the waiters for end of startup */
	wake_up(&cd->wait_q);

	return rc;
}

static void cyttsp6_startup_work_function(struct work_struct *work)
{
	struct cyttsp6_core_data *cd =  container_of(work,
		struct cyttsp6_core_data, startup_work);
	int rc;

	rc = cyttsp6_startup(cd, true);
	if (rc < 0)
		dev_err(cd->dev, "%s: Fail queued startup r=%d\n",
			__func__, rc);
}

static void cyttsp6_free_si_ptrs(struct cyttsp6_core_data *cd)
{
	struct cyttsp6_sysinfo *si = &cd->sysinfo;

	if (!si)
		return;

	kfree(si->si_ptrs.cydata);
	kfree(si->si_ptrs.test);
	kfree(si->si_ptrs.pcfg);
	kfree(si->si_ptrs.opcfg);
	kfree(si->si_ptrs.ddata);
	kfree(si->si_ptrs.mdata);
	kfree(si->btn);
	kfree(si->xy_mode);
	kfree(si->btn_rec_data);
}

/*
 * CONFIG_PM_RUNTIME option is removed in 3.19.0.
 */
#if defined(CONFIG_PM_RUNTIME) || \
		(LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
static int cyttsp6_core_rt_suspend(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	int rc;

	dev_dbg(dev, "%s\n", __func__);
	rc = cyttsp6_core_sleep(cd);
	if (rc < 0) {
		dev_err(dev, "%s: Error on sleep\n", __func__);
		return -EAGAIN;
	}

	return 0;
}

static int cyttsp6_core_rt_resume(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	int rc;

	rc = cyttsp6_core_wake(cd);
	if (rc < 0) {
		dev_err(dev, "%s: Error on wake\n", __func__);
		return -EAGAIN;
	}

	return 0;
}
#endif

#if defined(CONFIG_PM_SLEEP)
static int cyttsp6_core_suspend(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	cyttsp6_core_sleep(cd);

	/*
	 * In case of invalid touch application,
	 * return success to not inhibit Linux from suspend
	 */
	if (cd->invalid_touch_app) {
		cyttsp6_stop_wd_timer(cd);
		return 0;
	}

	if (!(cd->cpdata->flags & CY_CORE_FLAG_WAKE_ON_GESTURE))
		return 0;

	/*
	 * This will not prevent resume
	 * Required to prevent interrupts before i2c awake
	 */
	disable_irq(cd->irq);

	if (device_may_wakeup(dev)) {
		dev_vdbg(dev, "%s Device MAY wakeup\n", __func__);
		if (!enable_irq_wake(cd->irq))
			cd->irq_wake = 1;
	} else {
		dev_dbg(dev, "%s Device may NOT wakeup\n", __func__);
	}

	return 0;
}

static int cyttsp6_core_resume(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	if (cd->invalid_touch_app)
		goto exit;

	if (!(cd->cpdata->flags & CY_CORE_FLAG_WAKE_ON_GESTURE))
		goto exit;

	enable_irq(cd->irq);

	dev_info(dev, "%s\n", __func__);
	if (device_may_wakeup(dev)) {
		dev_dbg(dev, "%s Device MAY wakeup\n", __func__);
		if (cd->irq_wake) {
			disable_irq_wake(cd->irq);
			cd->irq_wake = 0;
		}
	} else {
		dev_dbg(dev, "%s Device may NOT wakeup\n", __func__);
	}

exit:
	cyttsp6_core_wake(cd);

	return 0;
}
#endif

#if NEED_SUSPEND_NOTIFIER
static int cyttsp6_pm_notifier(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct cyttsp6_core_data *cd = container_of(nb,
			struct cyttsp6_core_data, pm_notifier);

	if (action == PM_SUSPEND_PREPARE) {
		dev_dbg(cd->dev, "%s: Suspend prepare\n", __func__);

		/*
		 * If not runtime PM suspended, either call runtime
		 * PM suspend callback or wait until it finishes
		 */
		if (!pm_runtime_suspended(cd->dev))
			pm_runtime_suspend(cd->dev);

		(void) cyttsp6_core_suspend(cd->dev);
	}

	return NOTIFY_DONE;
}
#endif

const struct dev_pm_ops cyttsp6_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(cyttsp6_core_suspend, cyttsp6_core_resume)
	SET_RUNTIME_PM_OPS(cyttsp6_core_rt_suspend, cyttsp6_core_rt_resume,
			NULL)
};
EXPORT_SYMBOL_GPL(cyttsp6_pm_ops);

/*
 * Show Firmware version via sysfs
 */
static ssize_t cyttsp6_ic_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_cydata *cydata;
	ssize_t ret;

	mutex_lock(&cd->system_lock);
	if (!cd->sysinfo.ready) {
		if (cd->invalid_touch_app) {
			mutex_unlock(&cd->system_lock);
			return snprintf(buf, CY_MAX_PRBUF_SIZE,
					"Corrupted Touch application!\n");
		} else {
			mutex_unlock(&cd->system_lock);
			return snprintf(buf, CY_MAX_PRBUF_SIZE,
					"System Information not ready!\n");
		}
	}

	cydata = cd->sysinfo.si_ptrs.cydata;

	ret = snprintf(buf, CY_MAX_PRBUF_SIZE,
		"%s: 0x%02X 0x%02X\n"
		"%s: 0x%02X\n"
		"%s: 0x%02X\n"
		"%s: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n"
		"%s: 0x%04X\n"
		"%s: 0x%02X\n"
		"%s: 0x%02X\n",
		"TrueTouch Product ID", cydata->ttpidh, cydata->ttpidl,
		"Firmware Major Version", cydata->fw_ver_major,
		"Firmware Minor Version", cydata->fw_ver_minor,
		"Revision Control Number", cydata->revctrl[0],
		cydata->revctrl[1], cydata->revctrl[2], cydata->revctrl[3],
		cydata->revctrl[4], cydata->revctrl[5], cydata->revctrl[6],
		cydata->revctrl[7],
		"TrueTouch Config Version", cd->sysinfo.ttconfig.version,
		"Bootloader Major Version", cydata->blver_major,
		"Bootloader Minor Version", cydata->blver_minor);
	mutex_unlock(&cd->system_lock);

	return ret;
}

/*
 * Show TT Config version via sysfs
 */
static ssize_t cyttsp6_ttconfig_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&cd->system_lock);
	ret =  snprintf(buf, CY_MAX_PRBUF_SIZE, "0x%04X\n",
			cd->sysinfo.ttconfig.version);
	mutex_unlock(&cd->system_lock);

	return ret;
}

static ssize_t cyttsp6_ttconfig_crc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_sysinfo *si = &cd->sysinfo;
	ssize_t ret;
	int stat;

	stat = is_crc_stat_failed(si);
	if (stat) {
		dev_err(cd->dev, "%s: Config Data CRC FAIL codel=%02X\n",
			__func__, si->si_ptrs.test->post_codel);
	}

	ret = snprintf(buf, CY_MAX_PRBUF_SIZE, "%d\n", stat ? 0 : 1);
	return ret;
}


/*
 * Show Driver version via sysfs
 */
static ssize_t cyttsp6_drv_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, CY_MAX_PRBUF_SIZE,
		"Driver: %s\nVersion: %s\nDate: %s\n",
		CYTTSP6_CORE_NAME, CY_DRIVER_VERSION, CY_DRIVER_DATE);
}

/*
 * HW reset via sysfs
 */
static ssize_t cyttsp6_hw_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	int rc = 0;

	rc = cyttsp6_startup(cd, true);
	if (rc < 0)
		return rc;

	return size;
}

/*
 * Show IRQ status via sysfs
 */
static ssize_t cyttsp6_hw_irq_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	int retval;

	if (cd->cpdata->irq_stat) {
		retval = cd->cpdata->irq_stat(cd->cpdata, dev);
		switch (retval) {
		case 0:
			return snprintf(buf, CY_MAX_PRBUF_SIZE,
				"Interrupt line is LOW.\n");
		case 1:
			return snprintf(buf, CY_MAX_PRBUF_SIZE,
				"Interrupt line is HIGH.\n");
		default:
			return snprintf(buf, CY_MAX_PRBUF_SIZE,
				"Function irq_stat() returned %d.\n", retval);
		}
	}

	return snprintf(buf, CY_MAX_PRBUF_SIZE,
		"Function irq_stat() undefined.\n");
}

/*
 * Show ERROR status via sysfs
 */
static ssize_t cyttsp6_hw_error_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	int retval;
	int rc;
	u8 error[2];
	u16 err_code=0;

	if (cd->cpdata->error_stat) {
		retval = cd->cpdata->error_stat(cd->cpdata, dev);
		switch (retval) {
		case 0:
			return snprintf(buf, CY_MAX_PRBUF_SIZE,
				"Error line is LOW.\n");
		case 1:
			rc = cyttsp6_adap_read(cd, CY_ERR_OFFSET, error, 2);
			if (rc < 0) {
				dev_err(dev,"%s: Failed to read rc=%d\n",
						__func__,rc);
			}
			err_code = (error[1] << 8) | error[0];
			return snprintf(buf, CY_MAX_PRBUF_SIZE,
				"Error line is HIGH (0x%02X)\n",err_code);
		default:
			return snprintf(buf, CY_MAX_PRBUF_SIZE,
				"Function error_stat() returned %d.\n", retval);
		}
	}

	return snprintf(buf, CY_MAX_PRBUF_SIZE,
		"Function error_stat() undefined.\n");
}


/*
 * Show IRQ enable/disable status via sysfs
 */
static ssize_t cyttsp6_drv_irq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&cd->system_lock);
	if (cd->irq_enabled)
		ret = snprintf(buf, CY_MAX_PRBUF_SIZE,
			"Driver interrupt is ENABLED\n");
	else
		ret = snprintf(buf, CY_MAX_PRBUF_SIZE,
			"Driver interrupt is DISABLED\n");
	mutex_unlock(&cd->system_lock);

	return ret;
}

/*
 * Enable/disable IRQ via sysfs
 */
static ssize_t cyttsp6_drv_irq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	unsigned long value;
	int retval;

	retval = kstrtoul(buf, 10, &value);
	if (retval < 0)
		return retval;

	mutex_lock(&cd->system_lock);
	switch (value) {
	case 0:
		if (cd->irq_enabled) {
			cd->irq_enabled = false;
			disable_irq_nosync(cd->irq);
		}
		break;

	case 1:
		if (cd->irq_enabled == false) {
			cd->irq_enabled = true;
			enable_irq(cd->irq);
		}
		break;

	default:
		mutex_unlock(&(cd->system_lock));
		return -EINVAL;
	}
	mutex_unlock(&(cd->system_lock));

	return size;
}

/*
 * Debugging options via sysfs
 */
static ssize_t cyttsp6_drv_debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	unsigned long value = 0;
	int rc = 0;

	rc = kstrtoul(buf, 10, &value);
	if (rc < 0)
		return rc;

	switch (value) {
	case CY_DBG_SUSPEND:
		dev_info(dev, "%s: SUSPEND (cd=%p)\n", __func__, cd);
		rc = cyttsp6_core_sleep(cd);
		break;
	case CY_DBG_RESUME:
		dev_info(dev, "%s: RESUME (cd=%p)\n", __func__, cd);
		rc = cyttsp6_core_wake(cd);
		break;
	case CY_DBG_SOFT_RESET:
		dev_info(dev, "%s: SOFT RESET (cd=%p)\n", __func__, cd);
		rc = cyttsp6_hw_soft_reset_(cd);
		break;
	case CY_DBG_RESET:
		dev_info(dev, "%s: HARD RESET (cd=%p)\n", __func__, cd);
		rc = cyttsp6_hw_hard_reset(cd);
		break;
	default:
		return -EINVAL;
	}

	if (rc)
		return rc;

	return size;
}

/*
 * Show system status on deep sleep status via sysfs
 */
static ssize_t cyttsp6_sleep_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&cd->system_lock);
	if (cd->sleep_state == SS_SLEEP_ON)
		ret = snprintf(buf, CY_MAX_PRBUF_SIZE, "Sleep ON\n");
	else
		ret = snprintf(buf, CY_MAX_PRBUF_SIZE, "Sleep OFF\n");
	mutex_unlock(&cd->system_lock);

	return ret;
}

static ssize_t cyttsp6_easy_wakeup_gesture_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&cd->system_lock);
	ret = snprintf(buf, CY_MAX_PRBUF_SIZE, "0x%02X\n",
			cd->easy_wakeup_gesture);
	mutex_unlock(&cd->system_lock);
	return ret;
}

static ssize_t cyttsp6_easy_wakeup_gesture_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	unsigned long value;
	int ret;

	if (!(cd->cpdata->flags & CY_CORE_FLAG_WAKE_ON_GESTURE))
		return -EINVAL;

	ret = kstrtoul(buf, 10, &value);
	if (ret < 0 || value > 0xFF)
		return -EINVAL;

	pm_runtime_get_sync(dev);

	mutex_lock(&cd->system_lock);
	if (cd->sysinfo.ready && IS_TTSP_VER_GE(&cd->sysinfo, 2, 5))
		cd->easy_wakeup_gesture = (u8)value;
	else
		ret = -ENODEV;
	mutex_unlock(&cd->system_lock);

	pm_runtime_put(dev);

	if (ret)
		return ret;

	return size;
}

static struct device_attribute attributes[] = {
	__ATTR(ic_ver, S_IRUGO, cyttsp6_ic_ver_show, NULL),
	__ATTR(ttconfig_ver, S_IRUGO, cyttsp6_ttconfig_ver_show, NULL),
	__ATTR(drv_ver, S_IRUGO, cyttsp6_drv_ver_show, NULL),
	__ATTR(hw_reset, S_IWUSR, NULL, cyttsp6_hw_reset_store),
	__ATTR(hw_irq_stat, S_IRUSR, cyttsp6_hw_irq_stat_show, NULL),
	__ATTR(hw_error_stat, S_IRUSR, cyttsp6_hw_error_stat_show, NULL),
	__ATTR(drv_irq, S_IRUSR | S_IWUSR, cyttsp6_drv_irq_show,
		cyttsp6_drv_irq_store),
	__ATTR(drv_debug, S_IWUSR, NULL, cyttsp6_drv_debug_store),
	__ATTR(sleep_status, S_IRUSR, cyttsp6_sleep_status_show, NULL),
	__ATTR(easy_wakeup_gesture, S_IRUSR | S_IWUSR,
		cyttsp6_easy_wakeup_gesture_show,
		cyttsp6_easy_wakeup_gesture_store),
	__ATTR(ttconfig_crc_ok, S_IRUGO, cyttsp6_ttconfig_crc_show, NULL),
};

static int add_sysfs_interfaces(struct cyttsp6_core_data *cd,
		struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(dev, attributes + i))
			goto undo;

	return 0;
undo:
	for (i--; i >= 0 ; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "%s: failed to create sysfs interface\n", __func__);
	return -ENODEV;
}

static void remove_sysfs_interfaces(struct cyttsp6_core_data *cd,
		struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
}

static struct cyttsp6_core_commands _cyttsp6_core_commands = {
	.subscribe_attention = cyttsp6_subscribe_attention_,
	.unsubscribe_attention = cyttsp6_unsubscribe_attention_,
	.request_exclusive = cyttsp6_request_exclusive_,
	.release_exclusive = cyttsp6_release_exclusive_,
	.request_reset = cyttsp6_request_reset_,
	.request_restart = cyttsp6_request_restart_,
	.request_set_mode = cyttsp6_request_set_mode_,
	.request_sysinfo = cyttsp6_request_sysinfo_,
	.request_loader_pdata = cyttsp6_request_loader_pdata_,
	.request_handshake = cyttsp6_request_handshake_,
	.request_exec_cmd = cyttsp6_request_exec_cmd_,
	.request_stop_wd = cyttsp6_request_stop_wd_,
	.request_toggle_lowpower = cyttsp6_request_toggle_lowpower_,
	.request_config_row_size = cyttsp6_request_config_row_size_,
	.request_write_config = cyttsp6_request_write_config_,
	.request_enable_scan_type = cyttsp6_request_enable_scan_type_,
	.request_disable_scan_type = cyttsp6_request_disable_scan_type_,
	.get_security_key = cyttsp6_get_security_key_,
	.get_touch_record = cyttsp6_get_touch_record_,
	.write = cyttsp6_write_,
	.read = cyttsp6_read_,
	.calc_app_crc = cyttsp6_calc_app_crc,
};

struct cyttsp6_core_commands *cyttsp6_get_commands(void)
{
	return &_cyttsp6_core_commands;
}
EXPORT_SYMBOL_GPL(cyttsp6_get_commands);

static LIST_HEAD(core_list);
static int core_number;
static DEFINE_MUTEX(core_list_lock);
struct cyttsp6_core_data *cyttsp6_get_core_data(char *id)
{
	struct cyttsp6_core_data *d;

	mutex_lock(&core_list_lock);
	list_for_each_entry(d, &core_list, node) {
		if (!strncmp(d->core_id, id, 20)) {
			mutex_unlock(&core_list_lock);
			return d;
		}
	}
	mutex_unlock(&core_list_lock);

	return NULL;
}
EXPORT_SYMBOL_GPL(cyttsp6_get_core_data);

static void cyttsp6_add_core(struct device *dev)
{
	struct cyttsp6_core_data *d;
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	mutex_lock(&core_list_lock);
	list_for_each_entry(d, &core_list, node) {
		if (d->dev == dev) {
			mutex_unlock(&core_list_lock);
			return;
		}
	}

	list_add(&cd->node, &core_list);

	mutex_unlock(&core_list_lock);
}

static void cyttsp6_del_core(struct device *dev)
{
	struct cyttsp6_core_data *d, *d_n;

	mutex_lock(&core_list_lock);
	list_for_each_entry_safe(d, d_n, &core_list, node) {
		if (d->dev == dev) {
			list_del(&d->node);
			mutex_unlock(&core_list_lock);
			return;
		}
	}

	mutex_unlock(&core_list_lock);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cyttsp6_early_suspend(struct early_suspend *h)
{
	struct cyttsp6_core_data *cd =
		container_of(h, struct cyttsp6_core_data, es);

	call_atten_cb(cd, CY_ATTEN_SUSPEND, 0);
}

static void cyttsp6_late_resume(struct early_suspend *h)
{
	struct cyttsp6_core_data *cd =
		container_of(h, struct cyttsp6_core_data, es);

	call_atten_cb(cd, CY_ATTEN_RESUME, 0);
}

static void cyttsp6_setup_early_suspend(struct cyttsp6_core_data *cd)
{
	cd->es.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	cd->es.suspend = cyttsp6_early_suspend;
	cd->es.resume = cyttsp6_late_resume;

	register_early_suspend(&cd->es);
}
#elif defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct cyttsp6_core_data *cd =
		container_of(self, struct cyttsp6_core_data, fb_notifier);
	struct fb_event *evdata = data;
	int *blank;

	if (event != FB_EVENT_BLANK || !evdata)
		goto exit;

	blank = evdata->data;
	if (*blank == FB_BLANK_UNBLANK) {
		dev_info(cd->dev, "%s: UNBLANK!\n", __func__);
		if (cd->fb_state != FB_ON) {
			call_atten_cb(cd, CY_ATTEN_RESUME, 0);
			cd->fb_state = FB_ON;
		}
	} else if (*blank == FB_BLANK_POWERDOWN) {
		dev_info(cd->dev, "%s: POWERDOWN!\n", __func__);
		if (cd->fb_state != FB_OFF) {
			call_atten_cb(cd, CY_ATTEN_SUSPEND, 0);
			cd->fb_state = FB_OFF;
		}
	}

exit:
	return 0;
}

static void cyttsp6_setup_fb_notifier(struct cyttsp6_core_data *cd)
{
	int rc;

	cd->fb_state = FB_ON;

	cd->fb_notifier.notifier_call = fb_notifier_callback;

	rc = fb_register_client(&cd->fb_notifier);
	if (rc)
		dev_err(cd->dev, "Unable to register fb_notifier: %d\n", rc);
}
#endif

static int cyttsp6_setup_irq_gpio(struct cyttsp6_core_data *cd)
{
	struct device *dev = cd->dev;
	unsigned long irq_flags;
	int rc;

	/* Initialize IRQ */
	cd->irq = gpio_to_irq(cd->cpdata->irq_gpio);
	if (cd->irq < 0)
		return -EINVAL;

	cd->irq_enabled = true;

	dev_dbg(dev, "%s: initialize threaded irq=%d\n", __func__, cd->irq);
	if (cd->cpdata->level_irq_udelay > 0)
		/* use level triggered interrupts */
		irq_flags = IRQF_TRIGGER_LOW | IRQF_ONESHOT;
	else
		/* use edge triggered interrupts */
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;

	rc = request_threaded_irq(cd->irq, cyttsp6_hard_irq, cyttsp6_irq,
			irq_flags, dev_name(dev), cd);
	if (rc < 0)
		dev_err(dev, "%s: Error, could not request irq\n", __func__);

	return rc;
}

static int cyttsp6_set_max_xfer(struct cyttsp6_core_data *cd)
{
	cd->max_xfer = CY_DEFAULT_ADAP_MAX_XFER;
	if (cd->cpdata->max_xfer_len) {
		if (cd->cpdata->max_xfer_len < CY_ADAP_MIN_XFER) {
			dev_err(cd->dev, "%s: max_xfer_len invalid (min=%d)\n",
				__func__, CY_ADAP_MIN_XFER);
			return -EINVAL;
		}
		cd->max_xfer = cd->cpdata->max_xfer_len;
		dev_dbg(cd->dev, "%s: max_xfer set to %d\n", __func__,
				cd->max_xfer);
	}

	return 0;
}

int cyttsp6_probe(const struct cyttsp6_bus_ops *ops, struct device *dev,
		u16 irq, size_t xfer_buf_size)
{
	struct cyttsp6_core_data *cd;
	struct cyttsp6_platform_data *pdata = dev_get_platdata(dev);
	enum cyttsp6_atten_type type;
	int rc = 0;

	if (!pdata || !pdata->core_pdata || !pdata->mt_pdata) {
		dev_err(dev, "%s: Missing platform data\n", __func__);
		rc = -ENODEV;
		goto error_no_pdata;
	}

	/* get context and debug print buffers */
	cd = kzalloc(sizeof(*cd), GFP_KERNEL);
	if (!cd) {
		rc = -ENOMEM;
		goto error_alloc_data;
	}

	/* Initialize device info */
	cd->dev = dev;
	cd->pdata = pdata;
	cd->cpdata = pdata->core_pdata;
	cd->bus_ops = ops;
	if (cyttsp6_set_max_xfer(cd))
		goto error_max_xfer;
	scnprintf(cd->core_id, 20, "%s%d", CYTTSP6_CORE_NAME, core_number++);

	/* Check POWEROFF_ON_SLEEP flag and power function */
	if ((cd->cpdata->flags & CY_CORE_FLAG_POWEROFF_ON_SLEEP)
			&& (cd->cpdata->power == NULL)) {
		dev_err(dev, "%s: No power function with POWEROFF_ON_SLEEP flag\n",
			__func__);
		rc = -EINVAL;
		goto error_power;
	}

	/* Initialize mutexes */
	mutex_init(&cd->system_lock);
	mutex_init(&cd->atten_lock);

	/* Initialize attention lists */
	for (type = 0; type < CY_ATTEN_NUM_ATTEN; type++)
		INIT_LIST_HEAD(&cd->atten_list[type]);

	/* Initialize wait queue */
	init_waitqueue_head(&cd->wait_q);

	/* Initialize works */
	INIT_WORK(&cd->startup_work, cyttsp6_startup_work_function);
	INIT_WORK(&cd->watchdog_work, cyttsp6_watchdog_work);

	dev_set_drvdata(dev, cd);
	cyttsp6_add_core(dev);

	/* Call platform init function */
	if (cd->cpdata->init) {
		dev_dbg(cd->dev, "%s: Init HW\n", __func__);
		rc = cd->cpdata->init(cd->cpdata, 1, cd->dev);
	} else {
		dev_info(cd->dev, "%s: No HW INIT function\n", __func__);
		rc = 0;
	}
	if (rc < 0)
		dev_err(cd->dev, "%s: HW Init fail r=%d\n", __func__, rc);

	/* Call platform detect function */
	if (cd->cpdata->detect) {
		dev_info(cd->dev, "%s: Detect HW\n", __func__);
		rc = cd->cpdata->detect(cd->cpdata, cd->dev,
				cyttsp6_platform_detect_read);
		if (rc) {
			dev_info(cd->dev, "%s: No HW detected\n", __func__);
			rc = -ENODEV;
			goto error_detect;
		}
	}

	/* Setup watchdog timer */
	setup_timer(&cd->watchdog_timer, cyttsp6_watchdog_timer,
		(unsigned long)cd);

	rc = cyttsp6_setup_irq_gpio(cd);
	if (rc < 0) {
		dev_err(dev, "%s: Error, could not setup IRQ\n", __func__);
		goto error_setup_irq;
	}

	pm_runtime_get_noresume(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	/*
	 * call startup directly to ensure that the device
	 * is tested before leaving the probe
	 */
	dev_dbg(dev, "%s: call startup\n", __func__);
	rc = cyttsp6_startup(cd, !CY_CORE_DEVICE_RESET_BY_PLATFORM_INIT);

	pm_runtime_put_sync(dev);

	/* Do not fail probe if startup fails but the device is detected */
	if (rc == -ENODEV) {
		dev_err(cd->dev, "%s: Fail initial startup r=%d\n",
			__func__, rc);
		goto error_startup;
	}

	/* easywake feature added after ttsp protocol >= 2.5 */
	if (IS_TTSP_VER_GE(&cd->sysinfo, 2, 5))
		cd->easy_wakeup_gesture = cd->cpdata->easy_wakeup_gesture;
	else
		cd->easy_wakeup_gesture = 0xFF;

	dev_dbg(dev, "%s: add sysfs interfaces\n", __func__);
	rc = add_sysfs_interfaces(cd, dev);
	if (rc < 0) {
		dev_err(dev, "%s: Error, fail sysfs init\n", __func__);
		goto error_startup;
	}

	device_init_wakeup(dev, 1);

	rc = cyttsp6_mt_probe(dev);
	if (rc < 0) {
		dev_err(dev, "%s: Error, fail mt probe\n", __func__);
		goto error_sysfs_interfaces;
	}

	rc = cyttsp6_btn_probe(dev);
	if (rc < 0) {
		dev_err(dev, "%s: Error, fail btn probe\n", __func__);
		goto error_startup_mt;
	}

	rc = cyttsp6_proximity_probe(dev);
	if (rc < 0) {
		dev_err(dev, "%s: Error, fail proximity probe\n", __func__);
		goto error_startup_btn;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	cyttsp6_setup_early_suspend(cd);
#elif defined(CONFIG_FB)
	cyttsp6_setup_fb_notifier(cd);
#endif

#if NEED_SUSPEND_NOTIFIER
	cd->pm_notifier.notifier_call = cyttsp6_pm_notifier;
	register_pm_notifier(&cd->pm_notifier);
#endif

	return 0;

error_startup_btn:
	cyttsp6_btn_release(dev);
error_startup_mt:
	cyttsp6_mt_release(dev);
error_sysfs_interfaces:
	device_init_wakeup(dev, 1);
	remove_sysfs_interfaces(cd, dev);
error_startup:
	pm_runtime_disable(dev);
	cancel_work_sync(&cd->startup_work);
	cyttsp6_stop_wd_timer(cd);
	cyttsp6_free_si_ptrs(cd);
	del_timer(&cd->watchdog_timer);
	free_irq(cd->irq, cd);
error_setup_irq:
error_detect:
	if (cd->cpdata->init)
		cd->cpdata->init(cd->cpdata, 0, dev);
	cyttsp6_del_core(dev);
	dev_set_drvdata(dev, NULL);
error_power:
error_max_xfer:
	kfree(cd);
error_alloc_data:
error_no_pdata:
	dev_err(dev, "%s failed.\n", __func__);

	return rc;
}
EXPORT_SYMBOL_GPL(cyttsp6_probe);

int cyttsp6_release(struct cyttsp6_core_data *cd)
{
	struct device *dev = cd->dev;

	cyttsp6_proximity_release(dev);
	cyttsp6_btn_release(dev);
	cyttsp6_mt_release(dev);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&cd->es);
#elif defined(CONFIG_FB)
	fb_unregister_client(&cd->fb_notifier);
#endif

#if NEED_SUSPEND_NOTIFIER
	unregister_pm_notifier(&cd->pm_notifier);
#endif

	/*
	 * Suspend the device before freeing the startup_work and stopping
	 * the watchdog since sleep function restarts watchdog on failure
	 */
	pm_runtime_suspend(dev);
	pm_runtime_disable(dev);

	cancel_work_sync(&cd->startup_work);

	cyttsp6_stop_wd_timer(cd);

	device_init_wakeup(dev, 0);

	remove_sysfs_interfaces(cd, dev);
	free_irq(cd->irq, cd);
	if (cd->cpdata->init)
		cd->cpdata->init(cd->cpdata, 0, dev);
	dev_set_drvdata(dev, NULL);
	cyttsp6_del_core(dev);
	cyttsp6_free_si_ptrs(cd);
	kfree(cd);

	return 0;
}
EXPORT_SYMBOL_GPL(cyttsp6_release);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard Product Core Driver");
MODULE_AUTHOR("Cypress Semiconductor <ttdrivers@cypress.com>");
