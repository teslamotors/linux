/*
 * cyttsp6_loader.c
 * Cypress TrueTouch(TM) Standard Product V4 FW Loader Module.
 * For use with Cypress touchscreen controllers.
 * Supported parts include:
 * CY8CTMA46X
 * CY8CTMA1036/768
 * CYTMA445
 *
 * Copyright (C) 2009-2015 Cypress Semiconductor, Inc.
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
#include <linux/firmware.h>
#include <linux/version.h>

#define CYTTSP6_LOADER_NAME "cyttsp6_loader"
#define CY_FW_MANUAL_UPGRADE_FILE_NAME "cyttsp6_fw_manual_upgrade"


/* Enable UPGRADE_FW_AND_CONFIG_IN_PROBE definition
 * to perform FW and config upgrade during probe
 * instead of scheduling a work for it
 */
/* #define UPGRADE_FW_AND_CONFIG_IN_PROBE */


#define CYTTSP6_AUTO_LOAD_FOR_CORRUPTED_FW 1
#define CYTTSP6_LOADER_FW_UPGRADE_RETRY_COUNT 3

#define CYTTSP6_FW_UPGRADE \
	(defined(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_PLATFORM_FW_UPGRADE) \
	|| defined(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_BINARY_FW_UPGRADE))

#define CYTTSP6_TTCONFIG_UPGRADE \
	(defined(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_PLATFORM_TTCONFIG_UPGRADE) \
	|| defined(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_MANUAL_TTCONFIG_UPGRADE))

#define CY_CMD_BYTE					1
#define CY_STATUS_BYTE					1
#define CY_DATA_ROW_SIZE				128
#define CY_PACKET_DATA_LEN				96
#define CY_MAX_PACKET_LEN				512
#define CY_COMM_BUSY					0xFF
#define CY_CMD_BUSY					0xFE
#define CY_ARRAY_ID_OFFSET				0
#define CY_ROW_NUM_OFFSET				1
#define CY_ROW_SIZE_OFFSET				3
#define CY_ROW_DATA_OFFSET				5
#define CY_CMD_LDR_EXIT_CMD_SIZE			7
#define CY_CMD_LDR_EXIT_STAT_SIZE			7
#define CY_CMD_LDR_ENTER_CMD_SIZE			7
#define CY_CMD_LDR_INIT_CMD_SIZE			15
#define CY_CMD_LDR_INIT_STAT_SIZE			7
#define CY_CMD_LDR_PROG_ROW_STAT_SIZE			7
#define CY_CMD_LDR_VERIFY_ROW_STAT_SIZE			8
#define CY_CMD_LDR_VERIFY_ROW_CMD_SIZE			10
#define CY_CMD_LDR_VERIFY_CHKSUM_CMD_SIZE		7
#define CY_CMD_LDR_VERIFY_CHKSUM_STAT_SIZE		8
#define CY_COMPUTE_CRC(buf, len) \
	_cyttsp6_compute_crc(&buf[1], len - 1)

struct cyttsp6_loader_data {
	struct device *dev;
	struct cyttsp6_sysinfo *si;
	struct completion int_running;
	struct completion calibration_complete;
	struct work_struct fw_and_config_upgrade;
	struct work_struct calibration_work;
	struct cyttsp6_loader_platform_data *loader_pdata;
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_BINARY_FW_UPGRADE
	struct completion builtin_bin_fw_complete;
	int builtin_bin_fw_status;
#endif
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_MANUAL_TTCONFIG_UPGRADE
	struct mutex config_lock;
	u8 *config_data;
	int config_size;
	bool config_loading;
#endif
	u8 status_buf[CY_BL_MAX_STATUS_SIZE];
};

struct cyttsp6_dev_id {
	u32 silicon_id;
	u8 rev_id;
	u32 bl_ver;
};

enum ldr_status {
	ERROR_SUCCESS,
	ERROR_COMMAND,
	ERROR_FLASH_ARRAY,
	ERROR_PACKET_DATA,
	ERROR_PACKET_LEN,
	ERROR_PACKET_CHECKSUM,
	ERROR_FLASH_PROTECTION,
	ERROR_FLASH_CHECKSUM,
	ERROR_VERIFY_IMAGE,
	ERROR_UKNOWN1,
	ERROR_UKNOWN2,
	ERROR_UKNOWN3,
	ERROR_UKNOWN4,
	ERROR_UKNOWN5,
	ERROR_UKNOWN6,
	ERROR_INVALID_COMMAND,
	ERROR_INVALID
};

static struct cyttsp6_core_commands *cmd;

static inline struct cyttsp6_loader_data *cyttsp6_get_loader_data(
		struct device *dev)
{
	return cyttsp6_get_dynamic_data(dev, CY_MODULE_LOADER);
}

#if CYTTSP6_FW_UPGRADE || CYTTSP6_TTCONFIG_UPGRADE
/*
 * return code:
 * -1: Firmware version compared is older
 *  0: Firmware version compared is identical
 *  1: Firmware version compared is newer
 */
static int cyttsp6_check_firmware_version(struct device *dev, u32 fw_ver_new,
		u32 fw_revctrl_new_h, u32 fw_revctrl_new_l)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	u32 fw_ver_img;
	u32 fw_revctrl_img_h;
	u32 fw_revctrl_img_l;

	fw_ver_img = ld->si->si_ptrs.cydata->fw_ver_major << 8;
	fw_ver_img += ld->si->si_ptrs.cydata->fw_ver_minor;

	dev_dbg(dev, "%s: img vers:0x%04X new vers:0x%04X\n", __func__,
			fw_ver_img, fw_ver_new);

	if (fw_ver_new > fw_ver_img)
		return 1;

	if (fw_ver_new < fw_ver_img)
		return -1;

	fw_revctrl_img_h = be32_to_cpu(
		*(u32 *)(ld->si->si_ptrs.cydata->revctrl + 0));

	dev_dbg(dev, "%s: img revctrl_h:0x%04X new revctrl_h:0x%04X\n",
			__func__, fw_revctrl_img_h, fw_revctrl_new_h);

	if (fw_revctrl_new_h > fw_revctrl_img_h)
		return 1;

	if (fw_revctrl_new_h < fw_revctrl_img_h)
		return -1;

	fw_revctrl_img_l = be32_to_cpu(
		*(u32 *)(ld->si->si_ptrs.cydata->revctrl + 4));

	dev_dbg(dev, "%s: img revctrl_l:0x%04X new revctrl_l:0x%04X\n",
			__func__, fw_revctrl_img_l, fw_revctrl_new_l);

	if (fw_revctrl_new_l > fw_revctrl_img_l)
		return 1;

	if (fw_revctrl_new_l < fw_revctrl_img_l)
		return -1;

	return 0;
}

static void cyttsp6_calibrate_idacs(struct work_struct *calibration_work)
{
	struct cyttsp6_loader_data *ld = container_of(calibration_work,
			struct cyttsp6_loader_data, calibration_work);
	struct device *dev = ld->dev;
	u8 cmd_buf[CY_CMD_CAT_CALIBRATE_IDAC_CMD_SZ];
	u8 return_buf[CY_CMD_CAT_CALIBRATE_IDAC_RET_SZ];
	int rc;

	pm_runtime_get_sync(dev);

	dev_vdbg(dev, "%s: Requesting exclusive\n", __func__);
	rc = cmd->request_exclusive(dev, CY_LDR_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request exclusive r=%d\n",
				__func__, rc);
		goto exit;
	}

	dev_vdbg(dev, "%s: Requesting mode change to CAT\n", __func__);
	rc = cmd->request_set_mode(dev, CY_MODE_CAT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request set mode r=%d\n",
				__func__, rc);
		goto exit_release;
	}

	cmd_buf[0] = CY_CMD_CAT_CALIBRATE_IDACS;
	cmd_buf[1] = 0x00; /* Mutual Capacitance Screen */
	rc = cmd->request_exec_cmd(dev, CY_MODE_CAT,
			cmd_buf, CY_CMD_CAT_CALIBRATE_IDAC_CMD_SZ,
			return_buf, CY_CMD_CAT_CALIBRATE_IDAC_RET_SZ,
			CY_CALIBRATE_COMPLETE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Unable to execute calibrate command.\n",
			__func__);
		goto exit_setmode;
	}
	if (return_buf[0] != CY_CMD_STATUS_SUCCESS) {
		dev_err(dev, "%s: calibrate command unsuccessful\n", __func__);
		goto exit_setmode;
	}

	cmd_buf[1] = 0x01; /* Mutual Capacitance Button */
	rc = cmd->request_exec_cmd(dev, CY_MODE_CAT,
			cmd_buf, CY_CMD_CAT_CALIBRATE_IDAC_CMD_SZ,
			return_buf, CY_CMD_CAT_CALIBRATE_IDAC_RET_SZ,
			CY_CALIBRATE_COMPLETE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Unable to execute calibrate command.\n",
			__func__);
		goto exit_setmode;
	}
	if (return_buf[0] != CY_CMD_STATUS_SUCCESS) {
		dev_err(dev, "%s: calibrate command unsuccessful\n", __func__);
		goto exit_setmode;
	}

	cmd_buf[1] = 0x02; /* Self Capacitance */
	rc = cmd->request_exec_cmd(dev, CY_MODE_CAT,
			cmd_buf, CY_CMD_CAT_CALIBRATE_IDAC_CMD_SZ,
			return_buf, CY_CMD_CAT_CALIBRATE_IDAC_RET_SZ,
			CY_CALIBRATE_COMPLETE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Unable to execute calibrate command.\n",
			__func__);
		goto exit_setmode;
	}
	if (return_buf[0] != CY_CMD_STATUS_SUCCESS) {
		dev_err(dev, "%s: calibrate command unsuccessful\n", __func__);
		goto exit_setmode;
	}

exit_setmode:
	rc = cmd->request_set_mode(dev, CY_MODE_OPERATIONAL);
	if (rc < 0)
		dev_err(dev, "%s: Error on request set mode 2 r=%d\n",
				__func__, rc);

exit_release:
	rc = cmd->release_exclusive(dev);
	if (rc < 0)
		dev_err(dev, "%s: Error on release exclusive r=%d\n",
				__func__, rc);

exit:
	complete(&ld->calibration_complete);
	pm_runtime_put(dev);
}

static int cyttsp6_calibration_attention(struct device *dev)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);

	schedule_work(&ld->calibration_work);

	cmd->unsubscribe_attention(dev, CY_ATTEN_STARTUP, CY_MODULE_LOADER,
		cyttsp6_calibration_attention, 0);

	return 0;
}
#endif /* CYTTSP6_FW_UPGRADE || CYTTSP6_TTCONFIG_UPGRADE */


#if CYTTSP6_FW_UPGRADE
static u16 _cyttsp6_compute_crc(const u8 *buf, int size)
{
	u16 crc = 0xffff;
	u16 tmp;
	int i;

	if (size == 0)
		crc = ~crc;
	else {

		do {
			for (i = 0, tmp = 0x00ff & *buf++; i < 8;
				i++, tmp >>= 1) {
				if ((crc & 0x0001) ^ (tmp & 0x0001))
					crc = (crc >> 1) ^ 0x8408;
				else
					crc >>= 1;
			}
		} while (--size);

		crc = ~crc;
		tmp = crc;
		crc = (crc << 8) | (tmp >> 8 & 0xFF);
	}

	return crc;
}

static u16 _cyttsp6_get_short(u8 *buf)
{
	return ((u16)(*buf) << 8) + *(buf+1);
}

static u8 *_cyttsp6_get_row(struct device *dev, u8 *row_buf, u8 *image_buf,
		int size)
{
	memcpy(row_buf, image_buf, size);
	image_buf = image_buf + size;

	return image_buf;
}

static int _cyttsp6_get_status(struct device *dev, u8 *buf, int size,
		unsigned long timeout_ms)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	unsigned long uretval;
	int tries;
	int retval = 0;

	if (timeout_ms != 0) {
		/* wait until status ready interrupt or timeout occurs */
		uretval = wait_for_completion_timeout(
			&ld->int_running, msecs_to_jiffies(timeout_ms));

		/* TODO: Reconsider purpose of having retries here */
		for (tries = 0; tries < 2; tries++) {
			retval = cmd->read(dev, CY_MODE_BOOTLOADER, CY_REG_BASE,
					buf, size);
			/*
			 * retry if bus read error or
			 * status byte shows not ready
			 */
			if (buf[1] == CY_COMM_BUSY || buf[1] == CY_CMD_BUSY)
				msleep(20); /* TODO: Constant if code kept */
			else
				break;
		}
		dev_vdbg(dev, "%s: tries=%d ret=%d status=%02X\n",
			__func__, tries, retval, buf[1]);
	}

	return retval;
}

static int _cyttsp6_send_cmd(struct device *dev, const u8 *cmd_buf,
			     int cmd_size, u8 *stat_ret, size_t num_stat_byte,
			     size_t status_size, unsigned long timeout_ms)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	u8 *status_buf = ld->status_buf;
	int retval = 0;

	if (!cmd_buf)
		goto _cyttsp6_send_cmd_exit;

	if (!cmd_size)
		goto _cyttsp6_send_cmd_exit;

	if (timeout_ms > 0)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
		reinit_completion(&ld->int_running);
#else
		INIT_COMPLETION(ld->int_running);
#endif

	retval = cmd->write(dev, CY_MODE_BOOTLOADER, CY_REG_BASE, cmd_buf,
			cmd_size);
	if (retval < 0) {
		dev_err(dev, "%s: Fail writing command=%02X\n",
			__func__, cmd_buf[CY_CMD_BYTE]);
		goto _cyttsp6_send_cmd_exit;
	}

	if (timeout_ms > 0) {
		memset(status_buf, 0, sizeof(ld->status_buf));
		retval = _cyttsp6_get_status(dev, status_buf,
			status_size, timeout_ms);
		if (retval < 0 || status_buf[0] != CY_START_OF_PACKET) {
			dev_err(dev, "%s: Error getting status r=%d status_buf[0]=%02X\n",
				__func__, retval, status_buf[0]);
			if (!(retval < 0))
				retval = -EIO;
			goto _cyttsp6_send_cmd_exit;
		} else {
			if (status_buf[CY_STATUS_BYTE] != ERROR_SUCCESS) {
				dev_err(dev, "%s: Status=0x%02X error\n",
					__func__, status_buf[CY_STATUS_BYTE]);
				retval = -EIO;
			} else if (stat_ret != NULL) {
				if (num_stat_byte < status_size)
					*stat_ret = status_buf[num_stat_byte];
				else
					*stat_ret = 0;
			}
		}
	} else {
		if (stat_ret != NULL)
			*stat_ret = ERROR_SUCCESS;
	}

_cyttsp6_send_cmd_exit:
	return retval;
}

static int cyttsp6_add_host_sync(u8 *buf, int i)
{
	buf[i] = CY_CMD_LDR_HOST_SYNC;
	i++;

	return i;
}

static int _cyttsp6_ldr_enter(struct device *dev, struct cyttsp6_dev_id *dev_id)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	u16 crc = 0;
	int i = 0;
	u8 *status_buf = &ld->status_buf[0];
	u8 status = 0;
	int retval;
	/* +1 for TMA400 host sync byte */
	u8 ldr_enter_cmd[CY_CMD_LDR_ENTER_CMD_SIZE + 1];

	memset(status_buf, 0, sizeof(ld->status_buf));
	dev_id->bl_ver = 0;
	dev_id->rev_id = 0;
	dev_id->silicon_id = 0;

	i = cyttsp6_add_host_sync(ldr_enter_cmd, i);
	ldr_enter_cmd[i++] = CY_START_OF_PACKET;
	ldr_enter_cmd[i++] = CY_CMD_LDR_ENTER;
	ldr_enter_cmd[i++] = 0x00;	/* data len lsb */
	ldr_enter_cmd[i++] = 0x00;	/* data len msb */
	crc = CY_COMPUTE_CRC(ldr_enter_cmd, i);
	ldr_enter_cmd[i++] = (u8)crc;
	ldr_enter_cmd[i++] = (u8)(crc >> 8);
	ldr_enter_cmd[i++] = CY_END_OF_PACKET;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
	reinit_completion(&ld->int_running);
#else
	INIT_COMPLETION(ld->int_running);
#endif

	retval = cmd->write(dev, CY_MODE_BOOTLOADER, CY_REG_BASE,
			ldr_enter_cmd, i);
	if (retval < 0) {
		dev_err(dev, "%s: write block failed %d\n", __func__, retval);
		return retval;
	}
	retval = _cyttsp6_get_status(dev, status_buf,
		CY_CMD_LDR_ENTER_STAT_SIZE, CY_LDR_CMD_TIMEOUT);
	if (retval < 0) {
		dev_err(dev, "%s: Fail get status to Enter Loader command r=%d\n",
			__func__, retval);
		return retval;
	}
	status = status_buf[CY_STATUS_BYTE];
	if (status == ERROR_SUCCESS) {
		dev_id->bl_ver =
			status_buf[11] << 16 |
			status_buf[10] <<  8 |
			status_buf[9] <<  0;
		dev_id->rev_id =
			status_buf[8] <<  0;
		dev_id->silicon_id =
			status_buf[7] << 24 |
			status_buf[6] << 16 |
			status_buf[5] <<  8 |
			status_buf[4] <<  0;
		retval = 0;
	} else {
		retval = -EIO;
	}

	return retval;
}

static int _cyttsp6_ldr_init(struct device *dev)
{
	u16 crc;
	int i = 0;
	int retval = 0;
	const u8 *cyttsp6_security_key;
	int key_size;
	/* +1 for TMA400 host sync byte */
	u8 ldr_init_cmd[CY_CMD_LDR_INIT_CMD_SIZE + 1];

	cyttsp6_security_key = cmd->get_security_key(dev, &key_size);

	i = cyttsp6_add_host_sync(ldr_init_cmd, i);
	ldr_init_cmd[i++] = CY_START_OF_PACKET;
	ldr_init_cmd[i++] = CY_CMD_LDR_INIT;
	ldr_init_cmd[i++] = 0x08;	/* data len lsb */
	ldr_init_cmd[i++] = 0x00;	/* data len msb */
	memcpy(&ldr_init_cmd[i], cyttsp6_security_key, key_size);
	i += key_size;
	crc = _cyttsp6_compute_crc(&ldr_init_cmd[1], i - 1);
	ldr_init_cmd[i++] = (u8)crc;
	ldr_init_cmd[i++] = (u8)(crc >> 8);
	ldr_init_cmd[i++] = CY_END_OF_PACKET;

	retval = _cyttsp6_send_cmd(dev, ldr_init_cmd, i, NULL, 0,
				   CY_CMD_LDR_INIT_STAT_SIZE,
				   CY_LDR_CMD_INIT_TIMEOUT);
	if (retval < 0)
		dev_err(dev, "%s: Fail ldr init r=%d\n", __func__, retval);

	return retval;
}

struct cyttsp6_hex_image {
	u8 array_id;
	u16 row_num;
	u16 row_size;
	u8 row_data[CY_DATA_ROW_SIZE];
} __packed;

static int _cyttsp6_ldr_parse_row(struct device *dev, u8 *row_buf,
	struct cyttsp6_hex_image *row_image)
{
	int retval = 0;

	row_image->array_id = row_buf[CY_ARRAY_ID_OFFSET];
	row_image->row_num = _cyttsp6_get_short(&row_buf[CY_ROW_NUM_OFFSET]);
	row_image->row_size = _cyttsp6_get_short(&row_buf[CY_ROW_SIZE_OFFSET]);

	if (row_image->row_size > ARRAY_SIZE(row_image->row_data)) {
		dev_err(dev, "%s: row data buffer overflow\n", __func__);
		retval = -EOVERFLOW;
		goto cyttsp6_ldr_parse_row_exit;
	}

	memcpy(row_image->row_data, &row_buf[CY_ROW_DATA_OFFSET],
	       row_image->row_size);

cyttsp6_ldr_parse_row_exit:
	return retval;
}

static int _cyttsp6_ldr_prog_row(struct device *dev, int row_data_idx,
		struct cyttsp6_hex_image *row_image)
{
	u16 crc;
	int next = 0;
	int data;
	u16 row_sum = 0;
	size_t data_len;
	int retval = 0;
	u8 *cmd;

	cmd = kzalloc(CY_MAX_PACKET_LEN, GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	next = cyttsp6_add_host_sync(cmd, next);
	cmd[next++] = CY_START_OF_PACKET;
	cmd[next++] = CY_CMD_LDR_PROG_ROW;
	data_len = CY_DATA_ROW_SIZE;
	cmd[next++] = (u8)(data_len+3);
	cmd[next++] = (u8)((data_len+3) >> 8);
	cmd[next++] = row_image->array_id;
	cmd[next++] = (u8)row_image->row_num;
	cmd[next++] = (u8)(row_image->row_num >> 8);

	for (data = 0; data < data_len; data++) {
		cmd[next] = row_image->row_data[row_data_idx++];
		row_sum += cmd[next];
		next++;
	}

	crc = CY_COMPUTE_CRC(cmd, next);
	cmd[next++] = (u8)crc;
	cmd[next++] = (u8)(crc >> 8);
	cmd[next++] = CY_END_OF_PACKET;

	retval = _cyttsp6_send_cmd(dev, cmd, next, NULL, 0,
				   CY_CMD_LDR_PROG_ROW_STAT_SIZE,
				   CY_LDR_CMD_TIMEOUT);
	if (retval < 0)
		dev_err(dev, "%s: prog row=%d fail r=%d\n",
			__func__, row_image->row_num, retval);

	kfree(cmd);

	return retval;
}

static int _cyttsp6_ldr_verify_row(struct device *dev,
	struct cyttsp6_hex_image *row_image)
{
	u16 crc = 0;
	int i = 0;
	u8 verify_checksum;
	int retval = 0;
	/* +1 for TMA400 host sync byte */
	u8 ldr_verify_row_cmd[CY_CMD_LDR_VERIFY_ROW_CMD_SIZE + 1];

	i = cyttsp6_add_host_sync(ldr_verify_row_cmd, i);
	ldr_verify_row_cmd[i++] = CY_START_OF_PACKET;
	ldr_verify_row_cmd[i++] = CY_CMD_LDR_VERIFY_ROW;
	ldr_verify_row_cmd[i++] = 0x03;	/* data len lsb */
	ldr_verify_row_cmd[i++] = 0x00;	/* data len msb */
	ldr_verify_row_cmd[i++] = row_image->array_id;
	ldr_verify_row_cmd[i++] = (u8)row_image->row_num;
	ldr_verify_row_cmd[i++] = (u8)(row_image->row_num >> 8);
	crc = CY_COMPUTE_CRC(ldr_verify_row_cmd, i);
	ldr_verify_row_cmd[i++] = (u8)crc;
	ldr_verify_row_cmd[i++] = (u8)(crc >> 8);
	ldr_verify_row_cmd[i++] = CY_END_OF_PACKET;

	retval = _cyttsp6_send_cmd(dev, ldr_verify_row_cmd, i,
				   &verify_checksum, 4,
				   CY_CMD_LDR_VERIFY_ROW_STAT_SIZE,
				   CY_LDR_CMD_TIMEOUT);
	if (retval < 0)
		dev_err(dev, "%s: verify row=%d fail r=%d\n",
			__func__, row_image->row_num, retval);

	return retval;
}

static int _cyttsp6_ldr_verify_chksum(struct device *dev,
	u8 *app_chksum)
{
	u16 crc = 0;
	int i = 0;
	int retval = 0;
	/* +1 for TMA400 host sync byte */
	u8 ldr_verify_chksum_cmd[CY_CMD_LDR_VERIFY_CHKSUM_CMD_SIZE + 1];

	i = cyttsp6_add_host_sync(ldr_verify_chksum_cmd, i);
	ldr_verify_chksum_cmd[i++] = CY_START_OF_PACKET;
	ldr_verify_chksum_cmd[i++] = CY_CMD_LDR_VERIFY_CHKSUM;
	ldr_verify_chksum_cmd[i++] = 0x00;	/* data len lsb */
	ldr_verify_chksum_cmd[i++] = 0x00;	/* data len msb */
	crc = CY_COMPUTE_CRC(ldr_verify_chksum_cmd, i);
	ldr_verify_chksum_cmd[i++] = (u8)crc;
	ldr_verify_chksum_cmd[i++] = (u8)(crc >> 8);
	ldr_verify_chksum_cmd[i++] = CY_END_OF_PACKET;

	retval = _cyttsp6_send_cmd(dev, ldr_verify_chksum_cmd, i,
				   app_chksum, 4,
				   CY_CMD_LDR_VERIFY_CHKSUM_STAT_SIZE,
				   CY_LDR_CMD_TIMEOUT);

	if (retval < 0 || *app_chksum == 0) {
		dev_err(dev, "%s: verify checksum fail r=%d\n",
			__func__, retval);
		retval = retval < 0 ? retval : -EINVAL;
	}

	return retval;
}

/* Constructs loader exit command and sends via _cyttsp6_send_cmd() */
static int _cyttsp6_ldr_exit(struct device *dev)
{
	u16 crc = 0;
	int i = 0;
	int retval;
	/* +1 for TMA400 host sync byte */
	u8 ldr_exit_cmd[CY_CMD_LDR_EXIT_CMD_SIZE + 1];

	i = cyttsp6_add_host_sync(ldr_exit_cmd, i);
	ldr_exit_cmd[i++] = CY_START_OF_PACKET;
	ldr_exit_cmd[i++] = CY_CMD_LDR_EXIT;
	ldr_exit_cmd[i++] = 0x00;	/* data len lsb */
	ldr_exit_cmd[i++] = 0x00;	/* data len msb */
	crc = CY_COMPUTE_CRC(ldr_exit_cmd, i);
	ldr_exit_cmd[i++] = (u8)crc;
	ldr_exit_cmd[i++] = (u8)(crc >> 8);
	ldr_exit_cmd[i++] = CY_END_OF_PACKET;

	retval = _cyttsp6_send_cmd(dev, ldr_exit_cmd, i, NULL, 0,
				   CY_CMD_LDR_EXIT_STAT_SIZE, 0);
	if (retval < 0)
		dev_err(dev, "%s: BL Loader exit fail r=%d\n",
			__func__, retval);

	dev_vdbg(dev, "%s: Exit BL Loader r=%d\n", __func__, retval);

	return retval;
}

static int _cyttsp6_load_app(struct device *dev, const u8 *fw, int fw_size)
{
	u8 *p;
	int ret;
	int retval;	/* need separate return value at exit stage */
	int row_data_idx = 0;
	struct cyttsp6_dev_id *file_id = NULL;
	struct cyttsp6_dev_id *dev_id = NULL;
	struct cyttsp6_hex_image *row_image = NULL;
	u8 app_chksum;
	u8 *row_buf = NULL;
	size_t image_rec_size = sizeof(struct cyttsp6_hex_image);
	size_t row_buf_size = 1024 > CY_MAX_PRBUF_SIZE ?
		1024 : CY_MAX_PRBUF_SIZE;
	int row_count = 0;
	int count = 5;

	if (fw_size % image_rec_size != 0) {
		dev_err(dev,
			"%s: Firmware image is misaligned\n", __func__);
		retval = -EINVAL;
		goto _cyttsp6_load_app_exit;
	}

	/* Required for signal to the TTHE */
	dev_info(dev, "%s: start load app\n", __func__);

	row_buf = kzalloc(row_buf_size, GFP_KERNEL);
	row_image = kzalloc(sizeof(struct cyttsp6_hex_image), GFP_KERNEL);
	file_id = kzalloc(sizeof(struct cyttsp6_dev_id), GFP_KERNEL);
	dev_id = kzalloc(sizeof(struct cyttsp6_dev_id), GFP_KERNEL);
	if (!row_buf || !row_image || !file_id  || !dev_id) {
		retval = -ENOMEM;
		goto _cyttsp6_load_app_exit;
	}

	cmd->request_stop_wd(dev);

	p = (u8 *)fw;
	/* Enter Loader and return Silicon ID and Rev */

	retval = cmd->request_reset(dev);
	if (retval < 0) {
		dev_err(dev,
			"%s: Fail reset device r=%d\n", __func__, retval);
		goto _cyttsp6_load_app_exit;
	}

	/* Required for signal to the TTHE */
	dev_info(dev, "%s: Send BL Loader Enter\n", __func__);

	retval = _cyttsp6_ldr_enter(dev, dev_id);
	
	while (retval != 0 && count !=0){
		retval = _cyttsp6_ldr_enter(dev, dev_id);
		dev_info(dev, "%s: BL Loader retval=%d (attempt #: %d)\n", __func__, retval, count);
		count--;
	}

	if (retval < 0) {
		dev_err(dev, "%s: Error cannot start Loader (ret=%d)\n",
			__func__, retval);
		goto _cyttsp6_load_app_exit;
	}

	dev_vdbg(dev, "%s: dev: silicon id=%08X rev=%02X bl=%08X\n",
		__func__, dev_id->silicon_id,
		dev_id->rev_id, dev_id->bl_ver);

	udelay(1000);
	retval = _cyttsp6_ldr_init(dev);
	if (retval < 0) {
		dev_err(dev,
			"%s: Error cannot init Loader (ret=%d)\n",
			__func__, retval);
		goto _cyttsp6_load_app_exit;
	}

	/* Required for signal to the TTHE */
	dev_info(dev, "%s: Send BL Loader Blocks\n", __func__);

	while (p < (fw + fw_size)) {
		/* Get row */
		dev_dbg(dev, "%s: read row=%d\n", __func__, ++row_count);
		memset(row_buf, 0, row_buf_size);
		p = _cyttsp6_get_row(dev, row_buf, p, image_rec_size);

		/* Parse row */
		dev_vdbg(dev, "%s: p=%p buf=%p buf[0]=%02X\n", __func__,
			p, row_buf, row_buf[0]);
		retval = _cyttsp6_ldr_parse_row(dev, row_buf, row_image);
		dev_vdbg(dev, "%s: array_id=%02X row_num=%04X(%d) row_size=%04X(%d)\n",
			__func__, row_image->array_id,
			row_image->row_num, row_image->row_num,
			row_image->row_size, row_image->row_size);
		if (retval < 0) {
			dev_err(dev, "%s: Parse Row Error (a=%d r=%d ret=%d)\n",
				__func__, row_image->array_id,
				row_image->row_num, retval);
			goto bl_exit;
		} else {
			dev_vdbg(dev, "%s: Parse Row (a=%d r=%d ret=%d)\n",
				__func__, row_image->array_id,
				row_image->row_num, retval);
		}

		/* program row */
		retval = _cyttsp6_ldr_prog_row(dev, row_data_idx, row_image);
		if (retval < 0) {
			dev_err(dev, "%s: Program Row Error (array=%d row=%d ret=%d)\n",
				__func__, row_image->array_id,
				row_image->row_num, retval);
			goto _cyttsp6_load_app_exit;
		}

		/* verify row */
		retval = _cyttsp6_ldr_verify_row(dev, row_image);
		if (retval < 0) {
			dev_err(dev, "%s: Verify Row Error (array=%d row=%d ret=%d)\n",
				__func__, row_image->array_id,
				row_image->row_num, retval);
			goto _cyttsp6_load_app_exit;
		}

		dev_vdbg(dev, "%s: array=%d row_cnt=%d row_num=%04X\n",
			__func__, row_image->array_id, row_count,
			row_image->row_num);
	}

	/* verify app checksum */
	retval = _cyttsp6_ldr_verify_chksum(dev, &app_chksum);
	dev_dbg(dev, "%s: Application Checksum = %02X r=%d\n",
		__func__, app_chksum, retval);
	if (retval < 0)
		dev_err(dev, "%s: ldr_verify_chksum fail r=%d\n",
			__func__, retval);

	/* exit loader */
bl_exit:
	/* Required for signal to the TTHE */
	dev_info(dev, "%s: Send BL Loader Terminate\n", __func__);
	ret = _cyttsp6_ldr_exit(dev);
	if (ret) {
		dev_err(dev, "%s: Error on exit Loader (ret=%d)\n",
			__func__, ret);
		retval = ret;
	}

_cyttsp6_load_app_exit:
	kfree(row_buf);
	kfree(row_image);
	kfree(file_id);
	kfree(dev_id);

	return retval;
}

static int cyttsp6_upgrade_firmware(struct device *dev,
		const u8 *fw_img, int fw_size)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	bool wait_for_calibration_complete = false;
	int retry = CYTTSP6_LOADER_FW_UPGRADE_RETRY_COUNT;
	int rc;

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, CY_LDR_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0)
		goto exit;

	while (retry--) {
		rc = _cyttsp6_load_app(dev, fw_img, fw_size);
		if (rc < 0)
			dev_err(dev, "%s: Firmware update failed rc=%d, retry:%d\n",
				__func__, rc, retry);
		else
			break;
		msleep(20);
	}
	if (rc < 0) {
		dev_err(dev, "%s: Firmware update failed with error code %d\n",
			__func__, rc);
	} else if (ld->loader_pdata &&
			(ld->loader_pdata->flags &
				CY_LOADER_FLAG_CALIBRATE_AFTER_FW_UPGRADE)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
		reinit_completion(&ld->calibration_complete);
#else
		INIT_COMPLETION(ld->calibration_complete);
#endif

		/* set up call back for startup */
		dev_vdbg(dev, "%s: Adding callback for calibration\n",
			__func__);
		rc = cmd->subscribe_attention(dev, CY_ATTEN_STARTUP,
				CY_MODULE_LOADER,
				cyttsp6_calibration_attention, 0);
		if (rc) {
			dev_err(dev, "%s: Failed adding callback for calibration\n",
				__func__);
			dev_err(dev, "%s: No calibration will be performed\n",
				__func__);
			rc = 0;
		} else {
			wait_for_calibration_complete = true;
		}
	}

	cmd->release_exclusive(dev);
	cmd->request_restart(dev, false);

exit:
	pm_runtime_put(dev);

	if (wait_for_calibration_complete)
		wait_for_completion(&ld->calibration_complete);

	return rc;
}

static int cyttsp6_loader_attention(struct device *dev)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);

	complete(&ld->int_running);

	return 0;
}
#endif /* CYTTSP6_FW_UPGRADE */

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_PLATFORM_FW_UPGRADE
static int cyttsp6_check_firmware_version_platform(struct device *dev,
		struct cyttsp6_touch_firmware *fw)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	u32 fw_ver_new;
	u32 fw_revctrl_new_h;
	u32 fw_revctrl_new_l;
	int upgrade;

	if (!ld->si) {
		dev_info(dev, "%s: No firmware information found, device FW may be corrupted\n",
			__func__);
		return CYTTSP6_AUTO_LOAD_FOR_CORRUPTED_FW;
	}

	fw_ver_new = get_unaligned_be16(fw->ver + 2);
	fw_revctrl_new_h = get_unaligned_be32(fw->ver + 4);
	fw_revctrl_new_l = get_unaligned_be32(fw->ver + 8);

	upgrade = cyttsp6_check_firmware_version(dev, fw_ver_new,
			fw_revctrl_new_h, fw_revctrl_new_l);

	if (upgrade > 0)
		return 1;

	return 0;
}

static int upgrade_firmware_from_platform(struct device *dev,
		bool forced)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	struct cyttsp6_touch_firmware *fw;
	int rc = -ENOSYS;
	int upgrade;

	if (!ld->loader_pdata) {
		dev_err(dev, "%s: No loader platform data\n", __func__);
		return rc;
	}

	fw = ld->loader_pdata->fw;
	if (!fw || !fw->img || !fw->size) {
		dev_err(dev, "%s: No platform firmware\n", __func__);
		return rc;
	}

	if (!fw->ver || !fw->vsize) {
		dev_err(dev, "%s: No platform firmware version\n",
			__func__);
		return rc;
	}

	if (forced)
		upgrade = forced;
	else
		upgrade = cyttsp6_check_firmware_version_platform(dev, fw);

	if (upgrade)
		return cyttsp6_upgrade_firmware(dev, fw->img, fw->size);

	return rc;
}
#endif /* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_PLATFORM_FW_UPGRADE */

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_BINARY_FW_UPGRADE
static void _cyttsp6_firmware_cont(const struct firmware *fw, void *context)
{
	struct device *dev = context;
	u8 header_size = 0;

	if (!fw)
		return;

	if (!fw->data || !fw->size) {
		dev_err(dev, "%s: No firmware received\n", __func__);
		goto cyttsp6_firmware_cont_release_exit;
	}

	header_size = fw->data[0];
	if (header_size >= (fw->size + 1)) {
		dev_err(dev, "%s: Firmware format is invalid\n", __func__);
		goto cyttsp6_firmware_cont_release_exit;
	}

	cyttsp6_upgrade_firmware(dev, &(fw->data[header_size + 1]),
		fw->size - (header_size + 1));

cyttsp6_firmware_cont_release_exit:
	release_firmware(fw);
}

static int cyttsp6_check_firmware_version_builtin(struct device *dev,
		const struct firmware *fw)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	u32 fw_ver_new;
	u32 fw_revctrl_new_h;
	u32 fw_revctrl_new_l;
	int upgrade;

	if (!ld->si) {
		dev_info(dev, "%s: No firmware information found, device FW may be corrupted\n",
			__func__);
		return CYTTSP6_AUTO_LOAD_FOR_CORRUPTED_FW;
	}

	fw_ver_new = get_unaligned_be16(fw->data + 3);
	fw_revctrl_new_h = get_unaligned_be32(fw->data + 5);
	fw_revctrl_new_l = get_unaligned_be32(fw->data + 9);

	upgrade = cyttsp6_check_firmware_version(dev, fw_ver_new,
			fw_revctrl_new_h, fw_revctrl_new_l);

	if (upgrade > 0)
		return 1;

	return 0;
}

static void _cyttsp6_firmware_cont_builtin(const struct firmware *fw,
		void *context)
{
	struct device *dev = context;
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	int upgrade;

	if (!fw) {
		dev_info(dev, "%s: No builtin firmware\n", __func__);
		goto _cyttsp6_firmware_cont_builtin_exit;
	}

	if (!fw->data || !fw->size) {
		dev_err(dev, "%s: Invalid builtin firmware\n", __func__);
		goto _cyttsp6_firmware_cont_builtin_exit;
	}

	dev_dbg(dev, "%s: Found firmware\n", __func__);

	upgrade = cyttsp6_check_firmware_version_builtin(dev, fw);
	if (upgrade) {
		_cyttsp6_firmware_cont(fw, dev);
		ld->builtin_bin_fw_status = 0;
		complete(&ld->builtin_bin_fw_complete);
		return;
	}

_cyttsp6_firmware_cont_builtin_exit:
	release_firmware(fw);

	ld->builtin_bin_fw_status = -EINVAL;
	complete(&ld->builtin_bin_fw_complete);
}

static int upgrade_firmware_from_class(struct device *dev)
{
	int retval;

	dev_vdbg(dev, "%s: Enabling firmware class loader\n", __func__);

	retval = request_firmware_nowait(THIS_MODULE, FW_ACTION_NOHOTPLUG,
			CY_FW_MANUAL_UPGRADE_FILE_NAME, dev, GFP_KERNEL, dev,
			_cyttsp6_firmware_cont);
	if (retval < 0) {
		dev_err(dev, "%s: Fail request firmware class file load\n",
			__func__);
		return retval;
	}

	return 0;
}

static int upgrade_firmware_from_builtin(struct device *dev)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	int retval;

	dev_vdbg(dev, "%s: Enabling firmware class loader built-in\n",
		__func__);

	retval = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
			CY_FW_FILE_NAME, dev, GFP_KERNEL, dev,
			_cyttsp6_firmware_cont_builtin);
	if (retval < 0) {
		dev_err(dev, "%s: Fail request firmware class file load\n",
			__func__);
		return retval;
	}

	/* wait until FW binary upgrade finishes */
	wait_for_completion(&ld->builtin_bin_fw_complete);

	return ld->builtin_bin_fw_status;
}
#endif /* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_BINARY_FW_UPGRADE */

#if CYTTSP6_TTCONFIG_UPGRADE
static int cyttsp6_upgrade_ttconfig(struct device *dev,
		const u8 *ttconfig_data, int ttconfig_size)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	bool wait_for_calibration_complete = false;
	int rc, rc2;

	pm_runtime_get_sync(dev);

	dev_vdbg(dev, "%s: Requesting exclusive\n", __func__);
	rc = cmd->request_exclusive(dev, CY_LDR_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request exclusive r=%d\n",
				__func__, rc);
		goto exit;
	}

	dev_vdbg(dev, "%s: Requesting mode change to CAT\n", __func__);
	rc = cmd->request_set_mode(dev, CY_MODE_CAT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request set mode r=%d\n",
				__func__, rc);
		goto exit_release;
	}

	rc = cmd->request_write_config(dev, CY_TCH_PARM_EBID,
			0, (u8 *)ttconfig_data, ttconfig_size);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request write config r=%d\n",
				__func__, rc);
		goto exit_setmode;
	}

	if (ld->loader_pdata &&
			(ld->loader_pdata->flags
			 & CY_LOADER_FLAG_CALIBRATE_AFTER_TTCONFIG_UPGRADE)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
		reinit_completion(&ld->calibration_complete);
#else
		INIT_COMPLETION(ld->calibration_complete);
#endif

		/* set up call back for startup */
		dev_vdbg(dev, "%s: Adding callback for calibration\n",
			__func__);
		rc = cmd->subscribe_attention(dev, CY_ATTEN_STARTUP,
			CY_MODULE_LOADER, cyttsp6_calibration_attention, 0);
		if (rc) {
			dev_err(dev, "%s: Failed adding callback for calibration\n",
				__func__);
			dev_err(dev, "%s: No calibration will be performed\n",
				__func__);
			rc = 0;
		} else {
			wait_for_calibration_complete = true;
		}
	}

exit_setmode:
	rc2 = cmd->request_set_mode(dev, CY_MODE_OPERATIONAL);
	if (rc2 < 0)
		dev_err(dev, "%s: Error on request set mode r=%d\n",
				__func__, rc2);

exit_release:
	rc2 = cmd->release_exclusive(dev);
	if (rc < 0)
		dev_err(dev, "%s: Error on release exclusive r=%d\n",
				__func__, rc2);

exit:
	if (!rc)
		cmd->request_restart(dev, true);

	pm_runtime_put(dev);

	if (wait_for_calibration_complete)
		wait_for_completion(&ld->calibration_complete);

	return rc;
}
#endif /* CYTTSP6_TTCONFIG_UPGRADE */

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_PLATFORM_TTCONFIG_UPGRADE
static int cyttsp6_get_ttconfig_crc(struct device *dev,
		const u8 *ttconfig_data, int ttconfig_size, u16 *crc)
{
	u16 crc_loc;

	crc_loc = get_unaligned_le16(&ttconfig_data[2]);
	if (ttconfig_size < crc_loc + 2)
		return -EINVAL;

	*crc = get_unaligned_le16(&ttconfig_data[crc_loc]);

	return 0;
}

static int cyttsp6_get_ttconfig_version(struct cyttsp6_loader_data *ld,
		const u8 *ttconfig_data, int ttconfig_size, u16 *version)
{
	u16 ttconfig_offset = CY_TTCONFIG_VERSION_OFFSET;

	if (ttconfig_size < ttconfig_offset
			+ CY_TTCONFIG_VERSION_SIZE)
		return -EINVAL;

	*version = GET_FIELD16(ld->si, &ttconfig_data[ttconfig_offset]);

	return 0;
}

static int cyttsp6_check_ttconfig_version(struct device *dev,
		const u8 *ttconfig_data, int ttconfig_size)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	int rc;

	if (!ld->si)
		return 0;

	if (is_crc_stat_failed(ld->si)) {
		dev_info(dev, "%s: Config CRC invalid, will upgrade\n",
			__func__);
		return 1;
	}

	/* Check for config version */
	if (ld->loader_pdata->flags & CY_LOADER_FLAG_CHECK_TTCONFIG_VERSION) {
		u16 cfg_ver_new;

		rc = cyttsp6_get_ttconfig_version(ld, ttconfig_data,
				ttconfig_size, &cfg_ver_new);
		if (rc)
			return 0;

		dev_dbg(dev, "%s: img_ver:0x%04X new_ver:0x%04X\n",
			__func__, ld->si->ttconfig.version, cfg_ver_new);

		/* Check if config version is newer */
		if (cfg_ver_new > ld->si->ttconfig.version) {
			dev_dbg(dev, "%s: Config version newer, will upgrade\n",
				__func__);
			return 1;
		}

		dev_dbg(dev, "%s: Config version is identical or older, will NOT upgrade\n",
			__func__);
	/* Check for config CRC */
	} else {
		u16 cfg_crc_new;

		rc = cyttsp6_get_ttconfig_crc(dev, ttconfig_data,
				ttconfig_size, &cfg_crc_new);
		if (rc)
			return 0;

		dev_dbg(dev, "%s: img_crc:0x%04X new_crc:0x%04X\n",
			__func__, ld->si->ttconfig.crc, cfg_crc_new);

		/* Check if config CRC different. */
		if (cfg_crc_new != ld->si->ttconfig.crc) {
			dev_dbg(dev, "%s: Config CRC different, will upgrade\n",
				__func__);
			return 1;
		}

		dev_dbg(dev, "%s: Config CRC equal, will NOT upgrade\n",
			__func__);
	}

	return 0;
}

static int cyttsp6_check_ttconfig_version_platform(struct device *dev,
		struct cyttsp6_touch_config *ttconfig)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	u32 fw_ver_config;
	u32 fw_revctrl_config_h;
	u32 fw_revctrl_config_l;

	if (!ld->si) {
		dev_info(dev, "%s: No firmware information found, device FW may be corrupted\n",
			__func__);
		return 0;
	}

	fw_ver_config = get_unaligned_be16(ttconfig->fw_ver + 2);
	fw_revctrl_config_h = get_unaligned_be32(ttconfig->fw_ver + 4);
	fw_revctrl_config_l = get_unaligned_be32(ttconfig->fw_ver + 8);

	/* FW versions should match */
	if (cyttsp6_check_firmware_version(dev, fw_ver_config,
			fw_revctrl_config_h, fw_revctrl_config_l)) {
		dev_err(dev, "%s: FW versions mismatch\n", __func__);
		return 0;
	}

	return cyttsp6_check_ttconfig_version(dev, ttconfig->param_regs->data,
			ttconfig->param_regs->size);
}

static int upgrade_ttconfig_from_platform(struct device *dev)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	struct cyttsp6_touch_config *ttconfig;
	struct touch_settings *param_regs;
	struct cyttsp6_touch_fw;
	int rc = -ENOSYS;
	int upgrade;

	if (!ld->loader_pdata) {
		dev_info(dev, "%s: No loader platform data\n", __func__);
		return rc;
	}

	ttconfig = ld->loader_pdata->ttconfig;
	if (!ttconfig) {
		dev_info(dev, "%s: No ttconfig data\n", __func__);
		return rc;
	}

	param_regs = ttconfig->param_regs;
	if (!param_regs) {
		dev_info(dev, "%s: No touch parameters\n", __func__);
		return rc;
	}

	if (!param_regs->data || !param_regs->size) {
		dev_info(dev, "%s: Invalid touch parameters\n", __func__);
		return rc;
	}

	if (!ttconfig->fw_ver || !ttconfig->fw_vsize) {
		dev_info(dev, "%s: Invalid FW version for touch parameters\n",
			__func__);
		return rc;
	}

	upgrade = cyttsp6_check_ttconfig_version_platform(dev, ttconfig);
	if (upgrade)
		return cyttsp6_upgrade_ttconfig(dev, param_regs->data,
				param_regs->size);

	return rc;
}
#endif /* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_PLATFORM_TTCONFIG_UPGRADE */

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_MANUAL_TTCONFIG_UPGRADE
static ssize_t cyttsp6_config_data_write(struct file *filp,
		struct kobject *kobj, struct bin_attribute *bin_attr,
		char *buf, loff_t offset, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	u8 *p;

	dev_vdbg(dev, "%s: offset:%lld count:%zu\n", __func__, offset, count);

	mutex_lock(&ld->config_lock);

	if (!ld->config_loading) {
		mutex_unlock(&ld->config_lock);
		return -ENODEV;
	}

	p = krealloc(ld->config_data, offset + count, GFP_KERNEL);
	if (!p) {
		kfree(ld->config_data);
		ld->config_data = NULL;
		ld->config_size = 0;
		ld->config_loading = false;
		mutex_unlock(&ld->config_lock);
		return -ENOMEM;
	}
	ld->config_data = p;

	memcpy(&ld->config_data[offset], buf, count);
	ld->config_size += count;

	mutex_unlock(&ld->config_lock);

	return count;
}

static struct bin_attribute bin_attr_config_data = {
	.attr = {
		.name = "config_data",
		.mode = S_IWUSR,
	},
	.size = 0,
	.write = cyttsp6_config_data_write,
};

static ssize_t cyttsp6_config_loading_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	bool config_loading;

	mutex_lock(&ld->config_lock);
	config_loading = ld->config_loading;
	mutex_unlock(&ld->config_lock);

	return sprintf(buf, "%d\n", config_loading);
}

static int cyttsp6_verify_ttconfig_binary(struct device *dev,
		u8 *bin_config_data, int bin_config_size, u8 **start, int *len)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	int header_size;
	u16 config_size;
	u16 max_config_size;
	u32 fw_ver_config;
	u32 fw_revctrl_config_h;
	u32 fw_revctrl_config_l;

	if (!ld->si) {
		dev_err(dev, "%s: No firmware information found, device FW may be corrupted\n",
			__func__);
		return -ENODEV;
	}

	/*
	 * We need 11 bytes for FW version control info and at
	 * least 6 bytes in config (Length + Max Length + CRC).
	 */
	header_size = bin_config_data[0] + 1;
	if (header_size < 11 || header_size >= bin_config_size - 6) {
		dev_err(dev, "%s: Invalid header size %d\n", __func__,
			header_size);
		return -EINVAL;
	}

	fw_ver_config = get_unaligned_be16(&bin_config_data[1]);
	fw_revctrl_config_h = get_unaligned_be32(&bin_config_data[3]);
	fw_revctrl_config_l = get_unaligned_be32(&bin_config_data[7]);

	/* FW versions should match */
	if (cyttsp6_check_firmware_version(dev, fw_ver_config,
			fw_revctrl_config_h, fw_revctrl_config_l)) {
		dev_err(dev, "%s: FW versions mismatch\n", __func__);
		return -EINVAL;
	}

	config_size = get_unaligned_le16(&bin_config_data[header_size]);
	max_config_size =
		get_unaligned_le16(&bin_config_data[header_size + 2]);
	/* Perform a simple size check (2 bytes for CRC) */
	if (config_size != bin_config_size - header_size - 2) {
		dev_err(dev, "%s: Config size invalid\n", __func__);
		return -EINVAL;
	}
	/* Perform a size check against device config length */
	if (config_size != ld->si->ttconfig.length
			|| max_config_size != ld->si->ttconfig.max_length) {
		dev_err(dev, "%s: Config size mismatch\n", __func__);
		return -EINVAL;
	}

	*start = &bin_config_data[header_size];
	*len = bin_config_size - header_size;

	return 0;
}

/*
 * 1: Start loading TT Config
 * 0: End loading TT Config and perform upgrade
 *-1: Exit loading
 */
static ssize_t cyttsp6_config_loading_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);
	long value;
	u8 *start;
	int length;
	int rc;

	rc = kstrtol(buf, 10, &value);
	if (rc < 0 || value < -1 || value > 1) {
		dev_err(dev, "%s: Invalid value\n", __func__);
		return size;
	}

	mutex_lock(&ld->config_lock);

	if (value == 1)
		ld->config_loading = true;
	else if (value == -1)
		ld->config_loading = false;
	else if (value == 0 && ld->config_loading) {
		ld->config_loading = false;
		if (ld->config_size == 0) {
			dev_err(dev, "%s: No config data\n", __func__);
			goto exit_free;
		}

		rc = cyttsp6_verify_ttconfig_binary(dev,
				ld->config_data, ld->config_size,
				&start, &length);
		if (rc)
			goto exit_free;

		rc = cyttsp6_upgrade_ttconfig(dev, start, length);
	}

exit_free:
	kfree(ld->config_data);
	ld->config_data = NULL;
	ld->config_size = 0;

	mutex_unlock(&ld->config_lock);

	if (rc)
		return rc;

	return size;
}

static DEVICE_ATTR(config_loading, S_IRUSR | S_IWUSR,
	cyttsp6_config_loading_show, cyttsp6_config_loading_store);
#endif /* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_MANUAL_TTCONFIG_UPGRADE */

static void cyttsp6_fw_and_config_upgrade(
		struct work_struct *fw_and_config_upgrade)
{
	struct cyttsp6_loader_data *ld = container_of(fw_and_config_upgrade,
			struct cyttsp6_loader_data, fw_and_config_upgrade);
	struct device *dev = ld->dev;

	ld->si = cmd->request_sysinfo(dev);
	if (!ld->si)
		dev_err(dev, "%s: Fail get sysinfo pointer from core\n",
			__func__);

#if !CYTTSP6_FW_UPGRADE
	dev_info(dev, "%s: No FW upgrade method selected!\n", __func__);
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_PLATFORM_FW_UPGRADE
	if (!upgrade_firmware_from_platform(dev, false))
		return;
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_BINARY_FW_UPGRADE
	if (!upgrade_firmware_from_builtin(dev))
		return;
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_PLATFORM_TTCONFIG_UPGRADE
	if (!upgrade_ttconfig_from_platform(dev))
		return;
#endif
}

#if CYTTSP6_FW_UPGRADE
static int cyttsp6_fw_upgrade_cb(struct device *dev)
{
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_PLATFORM_FW_UPGRADE
	if (!upgrade_firmware_from_platform(dev, false))
		return 1;
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_BINARY_FW_UPGRADE
	if (!upgrade_firmware_from_builtin(dev))
		return 1;
#endif
	return 0;
}
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_PLATFORM_FW_UPGRADE
static ssize_t cyttsp6_forced_upgrade_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int rc;

	rc = upgrade_firmware_from_platform(dev, true);
	if (rc)
		return rc;

	return size;
}

static DEVICE_ATTR(forced_upgrade, S_IWUSR,
	NULL, cyttsp6_forced_upgrade_store);
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_BINARY_FW_UPGRADE
static ssize_t cyttsp6_manual_upgrade_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	upgrade_firmware_from_class(dev);

	return size;
}

static DEVICE_ATTR(manual_upgrade, S_IWUSR,
	NULL, cyttsp6_manual_upgrade_store);
#endif

static int cyttsp6_loader_probe(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_loader_data *ld;
	struct cyttsp6_platform_data *pdata = dev_get_platdata(dev);
	int rc;

	if (!pdata || !pdata->loader_pdata) {
		dev_err(dev, "%s: Missing platform data\n", __func__);
		rc = -ENODEV;
		goto error_no_pdata;
	}

	ld = kzalloc(sizeof(*ld), GFP_KERNEL);
	if (!ld) {
		rc = -ENOMEM;
		goto error_alloc_data_failed;
	}

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_PLATFORM_FW_UPGRADE
	rc = device_create_file(dev, &dev_attr_forced_upgrade);
	if (rc) {
		dev_err(dev, "%s: Error, could not create forced_upgrade\n",
				__func__);
		goto error_create_forced_upgrade;
	}
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_BINARY_FW_UPGRADE
	rc = device_create_file(dev, &dev_attr_manual_upgrade);
	if (rc) {
		dev_err(dev, "%s: Error, could not create manual_upgrade\n",
				__func__);
		goto error_create_manual_upgrade;
	}
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_MANUAL_TTCONFIG_UPGRADE
	rc = device_create_file(dev, &dev_attr_config_loading);
	if (rc) {
		dev_err(dev, "%s: Error, could not create config_loading\n",
				__func__);
		goto error_create_config_loading;
	}

	rc = device_create_bin_file(dev, &bin_attr_config_data);
	if (rc) {
		dev_err(dev, "%s: Error, could not create config_data\n",
				__func__);
		goto error_create_config_data;
	}
#endif

	ld->loader_pdata = pdata->loader_pdata;
	ld->dev = dev;
	cd->cyttsp6_dynamic_data[CY_MODULE_LOADER] = ld;

#if CYTTSP6_FW_UPGRADE
	init_completion(&ld->int_running);
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_BINARY_FW_UPGRADE
	init_completion(&ld->builtin_bin_fw_complete);
#endif
	cmd->subscribe_attention(dev, CY_ATTEN_IRQ, CY_MODULE_LOADER,
		cyttsp6_loader_attention, CY_MODE_BOOTLOADER);

	cmd->subscribe_attention(dev, CY_ATTEN_LOADER, CY_MODULE_LOADER,
		cyttsp6_fw_upgrade_cb, CY_MODE_UNKNOWN);
#endif
#if CYTTSP6_FW_UPGRADE || CYTTSP6_TTCONFIG_UPGRADE
	init_completion(&ld->calibration_complete);
	INIT_WORK(&ld->calibration_work, cyttsp6_calibrate_idacs);
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_MANUAL_TTCONFIG_UPGRADE
	mutex_init(&ld->config_lock);
#endif
#ifdef UPGRADE_FW_AND_CONFIG_IN_PROBE
	/* Call FW and config upgrade directly in probe */
	cyttsp6_fw_and_config_upgrade(&ld->fw_and_config_upgrade);
#else
	INIT_WORK(&ld->fw_and_config_upgrade, cyttsp6_fw_and_config_upgrade);
	schedule_work(&ld->fw_and_config_upgrade);
#endif

	dev_info(dev, "%s: Successful probe %s\n", __func__, dev_name(dev));

	return 0;


#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_MANUAL_TTCONFIG_UPGRADE
error_create_config_data:
	device_remove_file(dev, &dev_attr_config_loading);
error_create_config_loading:
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_BINARY_FW_UPGRADE
	device_remove_file(dev, &dev_attr_manual_upgrade);
#endif
#endif
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_BINARY_FW_UPGRADE
error_create_manual_upgrade:
#endif
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_PLATFORM_FW_UPGRADE
	device_remove_file(dev, &dev_attr_forced_upgrade);
error_create_forced_upgrade:
#endif
	cd->cyttsp6_dynamic_data[CY_MODULE_LOADER] = NULL;
	kfree(ld);
error_alloc_data_failed:
error_no_pdata:
	dev_err(dev, "%s failed.\n", __func__);

	return rc;
}

static int cyttsp6_loader_release(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_loader_data *ld = cyttsp6_get_loader_data(dev);

#if CYTTSP6_FW_UPGRADE
	cmd->unsubscribe_attention(dev, CY_ATTEN_IRQ, CY_MODULE_LOADER,
		cyttsp6_loader_attention, CY_MODE_BOOTLOADER);

	cmd->unsubscribe_attention(dev, CY_ATTEN_LOADER, CY_MODULE_LOADER,
		cyttsp6_fw_upgrade_cb, CY_MODE_UNKNOWN);
#endif
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_MANUAL_TTCONFIG_UPGRADE
	device_remove_bin_file(dev, &bin_attr_config_data);
	device_remove_file(dev, &dev_attr_config_loading);
	kfree(ld->config_data);
#endif
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_BINARY_FW_UPGRADE
	device_remove_file(dev, &dev_attr_manual_upgrade);
#endif
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_PLATFORM_FW_UPGRADE
	device_remove_file(dev, &dev_attr_forced_upgrade);
#endif
	cd->cyttsp6_dynamic_data[CY_MODULE_LOADER] = NULL;
	kfree(ld);

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
	"Core id list of cyttsp6 core devices for loader module");

static int __init cyttsp6_loader_init(void)
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
		pr_info("%s: Registering loader module for core_id: %s\n",
			__func__, core_ids[i]);
		rc = cyttsp6_loader_probe(cd->dev);
		if (rc < 0) {
			pr_err("%s: Error, failed registering module\n",
				__func__);
			goto fail_unregister_devices;
		}
	}

	pr_info("%s: Cypress TTSP FW Loader Driver (Built %s) rc=%d\n",
		 __func__, CY_DRIVER_VERSION, rc);

	return 0;

fail_unregister_devices:
	for (i--; i >= 0; i--) {
		cd = cyttsp6_get_core_data(core_ids[i]);
		if (!cd)
			continue;
		cyttsp6_loader_release(cd->dev);
		pr_info("%s: Unregistering loader module for core_id: %s\n",
			__func__, core_ids[i]);
	}

	return rc;
}
module_init(cyttsp6_loader_init);

static void __exit cyttsp6_loader_exit(void)
{
	struct cyttsp6_core_data *cd;
	int i;

	for (i = 0; i < num_core_ids; i++) {
		cd = cyttsp6_get_core_data(core_ids[i]);
		if (!cd)
			continue;
		cyttsp6_loader_release(cd->dev);
		pr_info("%s: Unregistering loader module for core_id: %s\n",
			__func__, core_ids[i]);
	}
}
module_exit(cyttsp6_loader_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard Product FW Loader Driver");
MODULE_AUTHOR("Cypress Semiconductor <ttdrivers@cypress.com>");
