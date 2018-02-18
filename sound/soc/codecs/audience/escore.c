/*
 * escore.c  --  Audience earSmart Soc Audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "escore.h"
#include "escore-i2c.h"
#include "escore-slim.h"
#include "escore-spi.h"
#include "escore-uart.h"
#include "escore-i2s.h"
#include <linux/time.h>

struct escore_macro cmd_hist[ES_MAX_ROUTE_MACRO_CMD] = { {0} };

#ifdef ES_WDB_PROFILING
#define es_profiling(x) getnstimeofday(x)
#else
#define es_profiling(x)
#endif

int cmd_hist_index;

/* is_forced allows to force recover irrespective of the value of
 * disable_fw_recovery_cnt. Helps in asynchronous cases (e.g. interrupt) */
int escore_fw_recovery(struct escore_priv *escore, int is_forced)
{
	int ret = 0;
	char *event[] = { "ACTION=ADNC_FW_RECOVERY", NULL };

	if (escore->disable_fw_recovery_cnt && !is_forced)
		return 0;

	/* stop streaming thread if running */
	if (escore->stream_thread &&
			(atomic_read(&escore->stream_thread->usage) > 0))
		kthread_stop(escore->stream_thread);

	dev_info(escore->dev, "%s(): is_forced = %d\n", __func__, is_forced);

	escore_gpio_reset(escore);
	ret = escore->boot_ops.bootup(escore);
	if (ret < 0) {
		dev_err(escore->dev, "%s(): Bootup failed %d\n", __func__, ret);
		return ret;
	}

	escore->escore_power_state = ES_SET_POWER_STATE_NORMAL;

	INC_DISABLE_FW_RECOVERY_USE_CNT(escore);
	ret = escore_reconfig_intr(escore);
	if (ret < 0)
		dev_err(escore->dev, "%s(): Interrupt setup failed %d\n",
				__func__, ret);

	DEC_DISABLE_FW_RECOVERY_USE_CNT(escore);
	pm_runtime_mark_last_busy(escore->dev);

	if (!ret) {
		int rc;
		if (escore->debug_buff == NULL) {
			escore->debug_buff = kmalloc(DBG_BLK_SIZE, GFP_KERNEL);
			if (escore->debug_buff == NULL) {
				dev_err(escore->dev,
				"%s():Not enough memory to read fw debug data\n"
				, __func__);
				return ret;
			}
		}

		rc = read_debug_data(escore->debug_buff);
		if (rc < 0) {
			dev_err(escore->dev, "%s():Read Debug data failed %d\n",
				__func__, rc);
			kfree(escore->debug_buff);
			escore->debug_buff = NULL;
		}

		/* send uevent irrespective of read success
		 * 1. It's highly unlikely that debug read would fail
		 * 2. If it does, this uevent would cause manual debug read by
		 *    the application (via sys)
		 */
		kobject_uevent_env(&escore->dev->kobj, KOBJ_CHANGE, event);
	}

	return ret;
}

/* History struture, log route commands to debug */
/* Send a single command to the chip.
 *
 * If the SR (suppress response bit) is NOT set, will read the
 * response and cache it the driver object retrieve with escore_resp().
 *
 * Returns:
 * 0 - on success.
 * EITIMEDOUT - if the chip did not respond in within the expected time.
 * E* - any value that can be returned by the underlying HAL.
 */

int escore_cmd_nopm(struct escore_priv *escore, u32 cmd, u32 *resp)
{
	int sr;
	int err;

	*resp = 0;
	sr = cmd & BIT(28);
	err = escore->bus.ops.cmd(escore, cmd, resp);
	if (err || sr)
		goto exit;

	if ((*resp) == 0) {
		err = -ETIMEDOUT;
		dev_err(escore->dev, "no response to command 0x%08x\n", cmd);
	} else {
		escore->bus.last_response = *resp;
		get_monotonic_boottime(&escore->last_resp_time);
	}

exit:
	return err;
}

int escore_cmd_locked(struct escore_priv *escore, u32 cmd, u32 *resp)
{
	int ret;

	mutex_lock(&escore->access_lock);
	ret = escore_pm_get_sync();
	if (ret > -1) {
		ret = escore_cmd_nopm(escore, cmd, resp);
		escore_pm_put_autosuspend();
	}
	mutex_unlock(&escore->access_lock);
	return ret;
}

int escore_cmd(struct escore_priv *escore, u32 cmd, u32 *resp)
{
	int ret;
	ret = escore_pm_get_sync();
	if (ret > -1) {
		ret = escore_cmd_nopm(escore, cmd, resp);
		escore_pm_put_autosuspend();
	}
	return ret;
}
int escore_write_block(struct escore_priv *escore, const u32 *cmd_block)
{
	int ret = 0;
	u32 resp;
	mutex_lock(&escore->access_lock);
	ret = escore_pm_get_sync();
	if (ret > -1) {
		while (*cmd_block != 0xffffffff) {
			ret = escore_cmd_nopm(escore, *cmd_block, &resp);
			if (ret)
				break;

			usleep_range(1000, 1005);
			cmd_block++;
		}
		escore_pm_put_autosuspend();
	}
	mutex_unlock(&escore->access_lock);
	return ret;
}

int escore_prepare_msg(struct escore_priv *escore, unsigned int reg,
		       unsigned int value, char *msg, int *len, int msg_type)
{
	struct escore_api_access *api_access;
	u32 api_word[2] = {0};
	unsigned int val_mask;
	int msg_len;

	if (reg > escore->api_addr_max) {
		pr_err("%s(): invalid address = 0x%04x\n", __func__, reg);
		return -EINVAL;
	}

	pr_debug("%s(): reg=%08x val=%d\n", __func__, reg, value);

	api_access = &escore->api_access[reg];
	val_mask = (1 << get_bitmask_order(api_access->val_max)) - 1;

	if (msg_type == ES_MSG_WRITE) {
		msg_len = api_access->write_msg_len;
		memcpy((char *)api_word, (char *)api_access->write_msg,
				msg_len);

		switch (msg_len) {
		case 8:
			api_word[1] |= ((val_mask & value) <<
						api_access->val_shift);
			break;
		case 4:
			api_word[0] |= ((val_mask & value) <<
						api_access->val_shift);
			break;
		}
	} else {
		msg_len = api_access->read_msg_len;
		memcpy((char *)api_word, (char *)api_access->read_msg,
				msg_len);
	}

	*len = msg_len;
	memcpy(msg, (char *)api_word, *len);

	return 0;

}

static int _escore_read(struct snd_soc_codec *codec, unsigned int reg)
{
	struct escore_priv *escore = &escore_priv;
	u32 api_word[2] = {0};
	unsigned int msg_len;
	unsigned int value = 0;
	u32 resp;
	int rc;

	rc = escore_prepare_msg(escore, reg, value, (char *) api_word,
			&msg_len, ES_MSG_READ);
	if (rc) {
		pr_err("%s(): Prepare read message fail %d\n",
		       __func__, rc);
		goto out;
	}

	rc = escore_cmd_nopm(escore, api_word[0], &resp);
	if (rc < 0) {
		pr_err("%s(): _escore_cmd failed, rc = %d", __func__, rc);
		return rc;
	}
	api_word[0] = escore->bus.last_response;

	value = api_word[0] & 0xffff;
out:
	return value;
}

/* Locked variant of escore_read():
 * Exclusive firmware access is guaranteed when this variant is called.
 */
int escore_read_locked(struct snd_soc_codec *codec, unsigned int reg)
{
	unsigned int ret = 0;
	int rc;
	struct escore_priv *escore = &escore_priv;

	mutex_lock(&escore->access_lock);
	rc = escore_pm_get_sync();
	if (rc > -1) {
		ret = _escore_read(codec, reg);
		escore_pm_put_autosuspend();
	}
	mutex_unlock(&escore->access_lock);
	return ret;
}

/* READ API to firmware:
 * This API may be interrupted. If there is a series of READs  being issued to
 * firmware, there must be a fw_access lock acquired in order to ensure the
 * atomicity of entire operation.
 */
int escore_read(struct snd_soc_codec *codec, unsigned int reg)
{
	unsigned int ret = 0;
	int rc;
	rc = escore_pm_get_sync();
	if (rc > -1) {
		ret = _escore_read(codec, reg);
		escore_pm_put_autosuspend();
	}
	return ret;
}

static int _escore_write(struct snd_soc_codec *codec, unsigned int reg,
		       unsigned int value)
{
	struct escore_priv *escore = &escore_priv;
	u32 api_word[2] = {0};
	int msg_len;
	u32 resp;
	int rc;
	int i;

	rc = escore_prepare_msg(escore, reg, value, (char *) api_word,
			&msg_len, ES_MSG_WRITE);
	if (rc) {
		pr_err("%s(): Failed to prepare write message %d\n",
		       __func__, rc);
		goto out;
	}

	for (i = 0; i < msg_len / 4; i++) {
		rc = escore_cmd_nopm(escore, api_word[i], &resp);
		if (rc < 0) {
			pr_err("%s(): escore_cmd()", __func__);
			return rc;
		}
	}
	pr_debug("%s(): mutex unlock\n", __func__);
out:
	return rc;
}

/* This function must be called with access_lock acquired */
int escore_reconfig_intr(struct escore_priv *escore)
{
	int rc = 0;
	u32 cmd, resp;

	cmd = ((ES_SYNC_CMD | ES_SUPRESS_RESPONSE) << 16);
	if (escore->pdata->gpioa_gpio != -1) {
		/* Set Interrupt Mode */
		escore->cmd_compl_mode = ES_CMD_COMP_INTR;
		cmd |= escore->pdata->gpio_a_irq_type;
	}

	rc = escore_cmd_nopm(escore, cmd, &resp);
	if (rc < 0) {
		dev_err(escore->dev,
				"%s() - failed sync cmd resume rc = %d\n",
				__func__, rc);
		if (escore->pdata->gpioa_gpio != -1)
			escore->cmd_compl_mode = ES_CMD_COMP_POLL;
		goto out;
	}

	if (escore->config_jack) {
		rc = escore->config_jack(escore);
		if (rc < 0) {
			dev_err(escore->dev, "%s() - jack config failed : %d\n",
					__func__, rc);
			goto out;
		}
	} else {
		/* Setup the Event response */
		cmd = (ES_SET_EVENT_RESP << 16) |
						escore->pdata->gpio_b_irq_type;
		rc = escore_cmd_nopm(escore, cmd, &resp);
		if (rc < 0) {
			dev_err(escore->dev,
				"%s(): Error %d in setting event response\n",
				__func__, rc);
			goto out;
		}
	}
out:
	return rc;
}

int escore_datablock_open(struct escore_priv *escore)
{
	int rc = 0;
	if (escore->bus.ops.high_bw_open)
		rc = escore->bus.ops.high_bw_open(escore);
	return rc;
}

int escore_datablock_close(struct escore_priv *escore)
{
	int rc = 0;
	if (escore->bus.ops.high_bw_close)
		rc = escore->bus.ops.high_bw_close(escore);
	return rc;
}

int escore_datablock_wait(struct escore_priv *escore)
{
	int rc = 0;
	if (escore->bus.ops.high_bw_wait)
		rc = escore->bus.ops.high_bw_wait(escore);
	return rc;
}

/* Sends RDB ID to fw and gets data block size from the response */
int escore_get_rdb_size(struct escore_priv *escore, int id)
{
	int rc;
	int size;
	u32 resp;
	u32 cmd = (ES_READ_DATA_BLOCK << 16) | (id & 0xFFFF);

	rc = escore->bus.ops.high_bw_cmd(escore, cmd, &resp);
	if (rc < 0) {
		pr_err("%s(): escore_cmd() failed rc = %d\n", __func__, rc);
		return rc;
	}

	if ((resp >> 16) != ES_READ_DATA_BLOCK) {
		pr_err("%s(): Invalid response received: 0x%08x\n",
				__func__, resp);
		return -EINVAL;
	}

	size = resp & 0xFFFF;
	pr_debug("%s(): RDB size = %d\n", __func__, size);
	if (size == 0 || size % 4 != 0) {
		pr_err("%s(): Read Data Block with invalid size:%d\n",
				__func__, size);
		return -EINVAL;
	}

	return size;
}

/* Call escore_get_rdb_size before calling this as that's the one that actually
 * sends RDB ID to fw. Attempt to read before sending ID would cause error */
int escore_datablock_read(struct escore_priv *escore, void *buf,
		size_t len)
{
	int rc;
	int rdcnt = 0;

	if (escore->bus.ops.rdb) {
		/* returns 0 on success */
		rc = escore->bus.ops.rdb(escore, buf, len);
		if (rc)
			return rc;
		return len;
	}

	for (rdcnt = 0; rdcnt < len;) {
		rc = escore->bus.ops.high_bw_read(escore, buf, 4);
		if (rc < 0) {
			pr_err("%s(): Read Data Block error %d\n",
					__func__, rc);
			return rc;
		}
		rdcnt += 4;
		buf += 4;
	}

	return len;
}

int escore_datablock_write(struct escore_priv *escore, const void *buf,
		size_t len)
{
	int rc, ret;
	int count;
	u32 resp;
	u32 cmd = ES_WRITE_DATA_BLOCK << 16;
	size_t size = 0;
	size_t remaining = len;
	u8 *dptr = (u8 *) buf;
#if defined(CONFIG_SND_SOC_ES_SPI) || \
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SPI)
	u32 resp32;
#endif

#ifdef ES_WDB_PROFILING
	struct timespec tstart;
	struct timespec tend;
	struct timespec tstart_cmd;
	struct timespec tend_cmd;
	struct timespec tstart_wdb;
	struct timespec tend_wdb;
	struct timespec tstart_resp;
	struct timespec tend_resp;
#endif
	pr_debug("%s() len = %zd\n", __func__, len);
	es_profiling(&tstart);
	es_profiling(&tstart_cmd);

	if (escore->datablock_dev.wdb_config) {
		rc = escore->datablock_dev.wdb_config(escore);
		if (rc < 0) {
			pr_err("%s(): WDB speed config error : %d\n",
					__func__, rc);
			goto out;
		}
	}

	while (remaining) {

		rc = escore_is_sleep_aborted(escore);
		if (rc == -EABORT) {
			/* BAS-3232: On CVQ abort while WDB is running,
			 * provide 5ms delay before performing abort
			 * sequence. */
			usleep_range(5000, 5050);
			goto out;
		}

		/* If multiple WDB blocks are written, some delay is required
		 * before starting next WDB. This delay is not documented but
		 * if this delay is not added, extra zeros are obsevred in
		 * escore_uart_read() causing WDB failure.
		 */
		if (len > escore->es_wdb_max_size)
			usleep_range(2000, 2050);

		size = remaining > escore->es_wdb_max_size ?
		       escore->es_wdb_max_size : remaining;

		cmd = ES_WRITE_DATA_BLOCK << 16;
		cmd = cmd | (size & 0xFFFF);
		pr_debug("%s(): cmd = 0x%08x\n", __func__, cmd);
		rc = escore->bus.ops.high_bw_cmd(escore, cmd, &resp);
		if (rc < 0) {
			pr_err("%s(): escore_cmd() failed rc = %d\n",
					__func__, rc);
			goto out;
		}
		if ((resp >> 16) != ES_WRITE_DATA_BLOCK) {
			pr_err("%s(): Invalid response received: 0x%08x\n",
					__func__, resp);
			rc = -EIO;
			goto out;
		}
		es_profiling(&tend_cmd);
		es_profiling(&tstart_wdb);

		rc = escore->bus.ops.high_bw_write(escore, dptr, size);
		if (rc < 0) {
			pr_err("%s(): WDB error:%d\n", __func__, rc);
			goto out;
		}
		es_profiling(&tend_wdb);
		/* After completing wdb response should be 0x802f0000,
		   retry until we receive response*/
		es_profiling(&tstart_resp);
#if defined(CONFIG_SND_SOC_ES_SPI) || \
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SPI)
		count = ES_SPI_MAX_RETRIES; /* retries for SPI only */
#else
		count = ES_MAX_RETRIES + 5;
#endif
		while (count-- > 0) {
			resp = 0;
#if (defined(CONFIG_SND_SOC_ES_SPI)) || \
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SPI)
			resp32 = 0;
			rc = escore->bus.ops.high_bw_read(escore, &resp32,
					sizeof(resp32));
			if (rc < 0) {
				pr_err("%s(): WDB last ACK read error:%d\n",
					__func__, rc);
				goto out;
			}
			resp = (cpu_to_be32(resp32));

			if (resp == 0) {
				pr_debug("%s(): Invalid response 0x%0x\n",
						__func__, resp);
				rc = -EIO;
				continue;
			} else if ((resp >> 16) == 0) {
				/* Fw SPI interface is 16bit. So in the 32bit
				 * read of command's response, sometimes fw
				 * sends first 16bit in first 32bit read and
				 * second 16bit in second 32bit. So this is
				 * special condition handling to make response.
				 */
				rc = escore->bus.ops.high_bw_read(escore,
						&resp32, sizeof(resp32));
				if (rc < 0) {
					pr_err("%s(): WDB last ACK read error:%d\n",
						__func__, rc);
					goto out;
				} else {
					resp = resp << 16;
					resp |= (cpu_to_be32(resp32) >> 16);
				}
			}
			if (resp != (ES_WRITE_DATA_BLOCK << 16)) {
				pr_debug("%s(): response not ready 0x%0x\n",
						__func__, resp);
				rc = -EIO;
			} else {
				break;
			}
			if (count % ES_SPI_CONT_RETRY == 0) {
				usleep_range(ES_SPI_RETRY_DELAY,
				ES_SPI_RETRY_DELAY + 200);
			}
#else
			rc = escore->bus.ops.high_bw_read(escore, &resp,
					sizeof(resp));
			if (rc < 0) {
				pr_err("%s(): WDB last ACK read error:%d\n",
					__func__, rc);
				goto out;
			}

			resp = escore->bus.ops.high_bw_bus_to_cpu(escore, resp);
			if (resp != (ES_WRITE_DATA_BLOCK << 16)) {
				pr_debug("%s(): response not ready 0x%0x\n",
						__func__, resp);
				rc = -EIO;
			} else {
				break;
			}
			usleep_range(1000, 1005);
#endif
		}
		if (rc == -EIO) {
			pr_err("%s(): write data block error 0x%0x\n",
					__func__, resp);
			goto out;
		}
		pr_debug("%s(): resp = 0x%08x\n", __func__, resp);

		dptr += size;
		remaining -= size;
	}

	if (escore->datablock_dev.wdb_reset_config) {
		rc = escore->datablock_dev.wdb_reset_config(escore);
		if (rc < 0) {
			pr_err("%s(): WDB speed reset config error : %d\n",
					__func__, rc);
			goto out;
		}
	}

	es_profiling(&tend_resp);
	es_profiling(&tend);
#ifdef ES_WDB_PROFILING
	tstart = (timespec_sub(tstart, tend));
	tstart_cmd = (timespec_sub(tstart_cmd, tend_cmd));
	tstart_wdb = (timespec_sub(tstart_wdb, tend_wdb));
	tstart_resp = (timespec_sub(tstart_resp, tend_resp));

	dev_info(escore->dev, "tend-tstart = %lu,\n"
			"cmd = %lu,\n"
			"wdb = %lu,\n"
			"resp = %lu,\n",
			(tstart.tv_nsec)/1000000,
			(tstart_cmd.tv_nsec)/1000000,
			(tstart_wdb.tv_nsec)/1000000,
			(tstart_resp.tv_nsec)/1000000);
#endif
	return len;

out:
	if (escore->datablock_dev.wdb_reset_config) {
		ret = escore->datablock_dev.wdb_reset_config(escore);
		if (ret < 0) {
			rc = ret;
			pr_err("%s(): WDB speed reset config error : %d\n",
					__func__, rc);
		}
	}

	return rc;
}

/*
 * This function assumes chip isn't in sleep mode; which is when you typically
 * need debug data
 */
int read_debug_data(void *buf)
{
	int ret = escore_datablock_open(&escore_priv);
	if (ret) {
		dev_err(escore_priv.dev,
			"%s(): can't open datablock device = %d\n", __func__,
			ret);
		return ret;
	}

	/* ignore size as it's fix; we are calling this to send RDB ID */
	ret = escore_get_rdb_size(&escore_priv, DBG_ID);
	if (ret < 0)
		goto datablock_close;

	ret = escore_datablock_read(&escore_priv, buf, DBG_BLK_SIZE);
	if (ret < 0)
		dev_err(escore_priv.dev,
			"%s(): failed to read debug data; err: %d\n", __func__,
			ret);

datablock_close:
	escore_datablock_close(&escore_priv);
	return ret;
}

/* Locked variant of escore_write():
 * Exclusive firmware access is guaranteed when this variant is called.
 */
int escore_write_locked(struct snd_soc_codec *codec, unsigned int reg,
		       unsigned int value)
{
	int ret;
	struct escore_priv *escore = &escore_priv;

	mutex_lock(&escore->access_lock);
	ret = escore_pm_get_sync();
	if (ret > -1) {
		ret = _escore_write(codec, reg, value);
		escore_pm_put_autosuspend();
	}
	mutex_unlock(&escore->access_lock);
	return ret;

}

/* WRITE API to firmware:
 * This API may be interrupted. If there is a series of WRITEs or READs  being
 * issued to firmware, there must be a fw_access lock acquired in order to
 * ensure the atomicity of entire operation.
 */
int escore_write(struct snd_soc_codec *codec, unsigned int reg,
		       unsigned int value)
{
	int ret;
	ret = escore_pm_get_sync();
	if (ret > -1) {
		ret = _escore_write(codec, reg, value);
		escore_pm_put_autosuspend();
	}
	return ret;

}

int escore_start_int_osc(struct escore_priv *escore)
{
	int rc = 0;
	int retry = MAX_RETRY_TO_SWITCH_TO_LOW_POWER_MODE;
	u32 cmd, rsp;

	dev_info(escore->dev, "%s()\n", __func__);

	/* Start internal Osc. */
	cmd = ES_INT_OSC_MEASURE_START << 16;
	rc = escore_cmd_nopm(escore, cmd, &rsp);
	if (rc) {
		dev_err(escore->dev, "%s(): Int Osc Msr Start cmd fail %d\n",
			__func__, rc);
		goto escore_int_osc_exit;
	}

	/* Poll internal Osc. status */
	do {
		/*
		 * Wait 20ms each time before reading
		 * up to 100ms
		 */
		msleep(20);
		cmd = ES_INT_OSC_MEASURE_STATUS << 16;
		rc = escore_cmd_nopm(escore, cmd, &rsp);
		if (rc) {
			dev_err(escore->dev,
				"%s(): Int Osc Msr Sts cmd fail %d\n",
				__func__, rc);
			goto escore_int_osc_exit;
		}
		rsp &= 0xFFFF;
		dev_dbg(escore->dev,
			"%s(): OSC Measure Status = 0x%04x\n", __func__, rsp);
	} while (rsp && --retry);

	if (rsp > 0) {
		dev_err(escore->dev,
			"%s(): Unexpected OSC Measure Status = 0x%04x\n",
			__func__, rc);
		dev_err(escore->dev,
			"%s(): Can't switch to Low Power Mode\n", __func__);
	}

escore_int_osc_exit:
	return rc;
}

/* Reconfig API Interrupt */
int escore_reconfig_api_intr(struct escore_priv *escore)
{
	int rc = 0;
	u32 cmd = ES_SYNC_CMD << 16;
	u32 rsp;

	/*  GPIO_A is not configured, reconfigure is not required */
	if (escore->pdata->gpioa_gpio == -1)
		return rc;

	pr_debug("%s(): Reconfig API Interrupt with mode %d\n",
		__func__, escore->pdata->gpio_a_irq_type);

	cmd |= escore->pdata->gpio_a_irq_type;
	escore->cmd_compl_mode = ES_CMD_COMP_INTR;
	rc = escore_cmd_nopm(escore,
			(cmd | (ES_SUPRESS_RESPONSE << 16)),
			&rsp);
	if (rc) {
		dev_err(escore->dev,
		    "%s() - API interrupt reconfig failed, rc = %d\n",
		    __func__, rc);
		escore->cmd_compl_mode = ES_CMD_COMP_POLL;
	}

	return rc;
}

/* API Interrupt completion handler */
int escore_api_intr_wait_completion(struct escore_priv *escore)
{
	int rc = 0;

	pr_debug("%s(): Waiting for API interrupt", __func__);

	rc = wait_for_completion_timeout(&escore->cmd_compl,
			msecs_to_jiffies(ES_API_INTR_TOUT_MSEC));
	if (!rc) {
		rc = -ETIMEDOUT;
		dev_err(escore->dev,
			"%s(): API Interrupt wait timeout %d\n", __func__, rc);
		escore->wait_api_intr = 0;
	} else {
		rc = 0;
	}

	return rc;
}

int escore_wakeup(struct escore_priv *escore)
{
	u32 cmd = ES_SYNC_CMD << 16;
	u32 rsp;
	int rc = 0;
	int retry = 20;
	u32 p_cmd = ES_GET_POWER_STATE << 16;

	escore->cmd_compl_mode = ES_CMD_COMP_POLL;
	/* Enable the clocks */
	if (escore_priv.pdata->esxxx_clk_cb) {
		escore_priv.pdata->esxxx_clk_cb(1);
		/* Setup for clock stablization delay */
		msleep(ES_PM_CLOCK_STABILIZATION);
	}

	do {
		/* Set flag to Wait for API Interrupt */
		escore_set_api_intr_wait(escore);

		/* Toggle the wakeup pin H->L the L->H */
		if (escore->wakeup_intf == ES_UART_INTF &&
				escore->escore_uart_wakeup) {
			rc = escore->escore_uart_wakeup(escore);
			if (rc) {
				dev_err(escore->dev,
						"%s() Wakeup failed rc = %d\n",
						__func__, rc);
				goto escore_wakeup_exit;
			}
		} else if (escore->pdata->wakeup_gpio != -1) {
			gpio_set_value(escore->pdata->wakeup_gpio, 1);
			usleep_range(1000, 1005);
			gpio_set_value(escore->pdata->wakeup_gpio, 0);
			usleep_range(1000, 1005);
			gpio_set_value(escore->pdata->wakeup_gpio, 1);
			usleep_range(1000, 1005);
			gpio_set_value(escore->pdata->wakeup_gpio, 0);
		}

		if (escore->pdata->gpioa_gpio == -1) {
			/* Give the device time to "wakeup" */
			if (escore->escore_power_state ==
					ES_SET_POWER_STATE_VS_LOWPWR)
				msleep(escore->delay.wakeup_to_vs);
			else if (escore->escore_power_state ==
					ES_SET_POWER_STATE_MP_SLEEP)
				msleep(escore->delay.mpsleep_to_normal);
			else
				msleep(escore->delay.wakeup_to_normal);

			/* Send sync command to verify device is active */
			rc = escore_cmd_nopm(escore, cmd, &rsp);
			if (rc < 0) {
				dev_err(escore->dev,
					"%s(): failed sync cmd resume %d\n",
					__func__, rc);
			}
			if (cmd != rsp) {
				dev_err(escore->dev,
					"%s(): failed sync rsp resume %d\n",
					__func__, rc);
				rc = -EIO;
			}
		} else {
			/* Wait for API Interrupt to confirm
			 * that device is active */
			rc = escore_api_intr_wait_completion(escore);
			if (rc)
				goto escore_wakeup_exit;

			/* Reconfig API Interrupt mode after wakeup */
			rc = escore_reconfig_api_intr(escore);
			if (rc)
				goto escore_wakeup_exit;
		}
	} while (rc && --retry);

	/* Set the Smooth Mute rate to Zero */
	cmd = ES_SET_SMOOTH_MUTE << 16 | ES_SMOOTH_MUTE_ZERO;
	rc = escore_cmd_nopm(escore, cmd, &rsp);
	if (rc) {
		dev_err(escore->dev, "%s(): Set Smooth Mute cmd fail %d\n",
			__func__, rc);
		goto escore_wakeup_exit;
	}

escore_wakeup_exit:
	return rc;
}

/* Update pm_status value to ES_PM_OFF / ES_PM_ON */
int escore_pm_status_update(bool value)
{
	struct escore_priv *escore = &escore_priv;
	int rc;

	if (!escore->pm_enable) {
		dev_info(escore->dev, "%s() RPM for device is not enabled\n",
							__func__);
		return 0;
	}

	if (value == escore->pm_status) {
		dev_info(escore->dev, "%s() Ignoring same value %d\n",
						__func__, escore->pm_status);
		return 0;
	}

	/* PM_STATUS OFF request */
	if (value == ES_PM_OFF) {
		rc = escore_pm_get_sync();
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): pm_get_sync failed :%d\n",
					__func__, rc);
			return rc;
		}
	} else /* PM_STATUS ON request */
		escore_pm_put_autosuspend();

	escore->pm_status = value;

	return 0;
}

int escore_get_runtime_pm_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = escore_priv.pm_status;

	return 0;
}

int escore_put_runtime_pm_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	return escore_pm_status_update(ucontrol->value.enumerated.item[0]);
}


int escore_put_control_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	int rc = 0;
	struct escore_priv *escore = &escore_priv;

	mutex_lock(&escore->access_lock);
	rc = escore_pm_get_sync();
	if (rc > -1) {
		value = ucontrol->value.enumerated.item[0];
		rc = _escore_write(NULL, reg, value);
		escore_pm_put_autosuspend();
	}
	mutex_unlock(&escore->access_lock);
	return 0;
}

int escore_get_control_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	struct escore_priv *escore = &escore_priv;

	mutex_lock(&escore->access_lock);
	ret = escore_pm_get_sync();
	if (ret > -1) {
		value = _escore_read(NULL, reg);
		ucontrol->value.enumerated.item[0] = value;
		escore_pm_put_autosuspend();
	}
	mutex_unlock(&escore->access_lock);
	return 0;
}

int escore_put_control_value(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int ret = 0;
	struct escore_priv *escore = &escore_priv;

	mutex_lock(&escore->access_lock);
	ret = escore_pm_get_sync();
	if (ret  > -1) {
		value = ucontrol->value.integer.value[0];
		ret = _escore_write(NULL, reg, value);
		escore_pm_put_autosuspend();
	}
	mutex_unlock(&escore->access_lock);
	return ret;
}

int escore_get_control_value(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	struct escore_priv *escore = &escore_priv;

	mutex_lock(&escore->access_lock);
	ret = escore_pm_get_sync();
	if (ret  > -1) {
		value = _escore_read(NULL, reg);
		ucontrol->value.integer.value[0] = value;
		escore_pm_put_autosuspend();
		ret = 0;
	}
	mutex_unlock(&escore->access_lock);
	return ret;
}

int escore_put_streaming_mode(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	escore_priv.es_streaming_mode = ucontrol->value.enumerated.item[0];
	return 0;
}

int escore_get_streaming_mode(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = escore_priv.es_streaming_mode;

	return 0;
}

int escore_switch_ext_osc(struct escore_priv *escore)
{
	u32 cmd_resp;
	int rc = 0;

	dev_dbg(escore->dev, "%s(): Switch ext oscillator", __func__);

	/* Sending preset command to switch to external oscillator */
	rc = escore_cmd(escore, (ES_SET_PRESET << 16 | ES_ASR_PRESET),
			&cmd_resp);
	if (rc) {
		dev_err(escore_priv.dev, "%s(): Set Preset fail %d\n",
			__func__, rc);
		goto cmd_error;
	}
	usleep_range(2000, 2005);

	rc = escore_cmd(escore, (ES_GET_POWER_LEVEL << 16), &cmd_resp);
	if (rc) {
		dev_err(escore->dev, "%s(): Error getting power level %d\n",
			__func__, rc);
		goto cmd_error;
	} else if (cmd_resp != ((ES_GET_POWER_LEVEL << 16)
				| ES_POWER_LEVEL_6)) {
		dev_err(escore->dev, "%s(): Invalid power level 0x%04x\n",
				__func__, cmd_resp);
		rc = -EINVAL;
	}
	usleep_range(2000, 2005);

cmd_error:
	return rc;
}

static int _es_validate_fw_switch(struct escore_priv *escore)
{
	int ret = 0;
	u32 cmd, resp = 0;

	cmd = ES_SYNC_CMD << 16;
	cmd |= escore->pdata->gpio_a_irq_type;

	/* Reconfig API Interrupt mode */
	ret = escore_reconfig_api_intr(escore);
	if (ret)
		goto exit;

	ret = escore_cmd_nopm(escore, cmd, &resp);
	if (ret < 0) {
		pr_err("%s() Sync command failed %d\n", __func__, ret);
		goto exit;
	}

	if (cmd != resp) {
		pr_err("%s() Invalid sync response  %x\n", __func__, resp);
		ret = -EIO;
	}
exit:
	return ret;
}

/* This function must be called with access_lock acquired */
int escore_af_to_ns(struct escore_priv *escore)
{
	int ret = 0;
	u32 cmd, resp = 0;

	cmd = (ES_SET_POWER_STATE << 16) | ES_SET_POWER_STATE_NORMAL;
	ret = escore_cmd(escore, cmd, &resp);
	if (ret) {
		pr_err("%s(): Set Power State to Normal failed %d\n",
				__func__, ret);
		goto exit;
	}

	msleep(35);

	ret = _es_validate_fw_switch(escore);
	if (ret) {
		pr_err("%s(): Switching to NS mode failed %d\n", __func__, ret);
		goto exit;
	}

	escore->mode = STANDARD;
exit:
	return ret;
}


/* This function must be called with access_lock acquired */
int escore_ns_to_af(struct escore_priv *escore)
{
	int ret = 0;
	u32 cmd, resp = 0;

	if (!escore->boot_ops.setup || !escore->boot_ops.finish ||
			!escore->bus.ops.high_bw_write) {
		pr_err("%s(): Required bus operations not implemented\n",
				__func__);
		ret = -ENOSYS;
		goto exit;
	}

	/*
	 * Need to call escore-pm handler explicitly to check whether chip
	 * needs a wakeup or it is already awake. If chip is brought to NS mode
	 * then we need to wait for 35ms before entering into AF Overlay mode.
	 *
	 * State diagram reference: Jira-2653
	 */
	ret = escore_pm_get_sync();
	if (ret < 0) {
		pr_err("%s(): Failed to wakeup the chip %d\n", __func__, ret);
		goto exit;
	}

	if (ret == 0) {
		/*
		 * Chip is just brought to Normal mode (i.e. NS firmware).
		 * Wait for 35ms as before entering into AF Overlay mod
		 */
		msleep(35);
	}

	cmd = (ES_SET_POWER_STATE << 16) | ES_POWER_STATE_AF_OVERLAY;
	ret = escore_cmd_nopm(escore, cmd, &resp);
	if (ret) {
		pr_err("%s(): Failed to enter into AF overlay mode %d\n",
				__func__, ret);
		goto overlay_fail;
	}

	/* Need to wait until chip is moved to Overlay mode. */
	msleep(23);

	if (escore->bus.ops.high_bw_open) {
		ret = escore->bus.ops.high_bw_open(escore);
		if (ret) {
			pr_err("%s(): high_bw_open failed %d\n", __func__, ret);
			goto highbw_dev_fail;
		}
	}

	ret = escore->boot_ops.setup(escore);
	if (ret) {
		pr_err("%s(): AF download start error %d\n", __func__, ret);
		goto fw_load_fail;
	}


	ret = escore->bus.ops.high_bw_write(escore, (char *)escore->af->data,
			escore->af->size);
	if (ret < 0) {
		pr_err("%s(): AF firmware download failed %d\n", __func__, ret);
		goto fw_load_fail;
	}

	msleep(35);

	ret = escore->boot_ops.finish(escore);
	if (ret) {
		pr_err("%s() AF download finish error %d\n", __func__, ret);
		goto fw_load_fail;
	}

	pr_debug("%s(): AF Firmware download successful\n", __func__);

	/* Setup the Event response */
	cmd = (ES_SET_EVENT_RESP<<16) | escore->pdata->gpio_b_irq_type;
	ret = escore_cmd_nopm(escore, cmd, &resp);
	if (ret < 0) {
		pr_err("%s(): Error %d in setting event response\n",
			__func__, ret);
		goto fw_load_fail;
	}

	escore->mode = AF;

fw_load_fail:
	if (escore->bus.ops.high_bw_close) {
		int rc = 0;
		rc = escore->bus.ops.high_bw_close(escore);
		if (rc) {
			pr_err("%s(): high_bw_close failed %d\n", __func__, rc);
			ret = (ret) ? : rc;
		}
	}

overlay_fail:
highbw_dev_fail:
	escore_pm_put_autosuspend();
exit:
	return ret;
}

int escore_put_af_status(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *escore = &escore_priv;
	int ret = 0;
	int value;

	value = ucontrol->value.enumerated.item[0];

	if (!escore->af && value) {
		ret = request_firmware((const struct firmware **)&escore->af,
				escore->fw_af_filename, escore->dev);
		if (ret) {
			pr_err("%s(): request_firmware(%s) for AF failed %d\n",
					__func__, escore->fw_af_filename, ret);
			escore_priv.flag.af = 0;
			goto exit;
		}
	}

	escore->flag.af = 1;

	if (value) {
		if (escore->mode == AF) {
			pr_debug("%s(): Already in AF state\n", __func__);
			goto exit;
		}

		mutex_lock(&escore->access_lock);
		ret = escore_ns_to_af(escore);
		mutex_unlock(&escore->access_lock);
	} else {
		if (escore->mode != AF) {
			pr_debug("%s(): AF state already disabled\n", __func__);
			goto exit;
		}

		if (atomic_read(&escore->active_streams)) {
			pr_debug("%s(): Active stream detected\n", __func__);
			goto exit;
		}

		mutex_lock(&escore->access_lock);
		ret = escore_af_to_ns(escore);
		mutex_unlock(&escore->access_lock);
	}

exit:
	return ret;
}

int escore_get_af_status(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *escore = &escore_priv;

	ucontrol->value.enumerated.item[0] = (escore->mode == AF);
	return 0;
}


void escore_register_notify(struct blocking_notifier_head *list,
		struct notifier_block *nb)
{
	blocking_notifier_chain_register(list, nb);
}

void escore_gpio_reset(struct escore_priv *escore)
{
	if (escore->pdata->reset_gpio == -1) {
		pr_warn("%s(): Reset GPIO not initialized\n", __func__);
		return;
	}

	gpio_set_value(escore->pdata->reset_gpio, 0);
	/* Wait 1 ms then pull Reset signal in High */
	usleep_range(1000, 1005);
	gpio_set_value(escore->pdata->reset_gpio, 1);
	/* Wait 10 ms then */
	usleep_range(10000, 10050);
	/* eSxxx is READY */
	escore->flag.reset_done = 1;
	escore->mode = SBL;
}

int escore_probe(struct escore_priv *escore, struct device *dev, int curr_intf,
		int context)
{
	int rc = 0;

	mutex_lock(&escore->intf_probed_mutex);

	/* Update intf_probed only when a valid interface is probed */

	if (curr_intf != ES_INVAL_INTF)
		escore->intf_probed |= curr_intf;

	if (curr_intf == escore->pri_intf)
		escore->dev = dev;

	if (escore->intf_probed != (escore->pri_intf | escore->high_bw_intf)) {
		pr_debug("%s(): Both interfaces are not probed %d\n",
				__func__, escore->intf_probed);
		mutex_unlock(&escore->intf_probed_mutex);
		return 0;
	}
	mutex_unlock(&escore->intf_probed_mutex);

	escore->bus.setup_prim_intf(escore);

	rc = escore->bus.setup_high_bw_intf(escore);
	if (rc) {
		pr_err("%s(): Error while setting up high bw interface %d\n",
				__func__, rc);
		escore->is_probe_error = 1;
		goto out;
	}

	if (escore->flag.is_codec) {
		rc = snd_soc_register_codec(escore->dev,
				escore->soc_codec_dev_escore,
				escore->dai,
				escore->dai_nr);

		if (rc) {
			pr_err("%s(): Codec registration failed %d\n",
					__func__, rc);
			escore->is_probe_error = 1;
			goto out;
		}
		if (context == ES_CONTEXT_THREAD) {
			rc = escore_retrigger_probe();
			if (rc)
				pr_err("%s(): Adding UART dummy dev fail %d\n",
				       __func__, rc);
		}
	}

	/* Enable the gpiob IRQ */
	if (escore_priv.pdata->gpiob_gpio != -1)
		enable_irq(gpio_to_irq(escore_priv.pdata->gpiob_gpio));

#ifdef CONFIG_SLIMBUS_MSM_NGD
	if (escore_priv.high_bw_intf != ES_SLIM_INTF)
		complete(&escore->fw_download);
#else
	complete(&escore->fw_download);
#endif
	/* Don't call following function if Runtime PM support
	 * is required to be disabled */
	escore->is_probe_error = 0;
	escore_pm_enable();

out:
	return rc;
}

static struct platform_device *escore_dummy_device;

/*
 * Helper routine to retrigger the probe context when some probe() routines
 * have returned pre-maturely with -EPROBE_DEFER
 */
int escore_retrigger_probe(void)
{
	int rc = 0;

	/* Release previously alloc'ed device */
	if (escore_dummy_device)
		platform_device_put(escore_dummy_device);

	escore_dummy_device = platform_device_alloc("escore-codec.dummy", -1);
	if (!escore_dummy_device) {
		pr_err("%s(): dummy platform device allocation failed\n",
				__func__);
		rc = -ENOMEM;
		goto out;
	}

	rc = platform_device_add(escore_dummy_device);
	if (rc) {
		pr_err("%s(): Error while adding dummy device %d\n",
		       __func__, rc);
		platform_device_put(escore_dummy_device);
	}
out:
	return rc;
}

static int escore_plat_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct escore_pdata *pdata;

	pr_debug("%s()\n", __func__);
	pdata = pdev->dev.platform_data;

	if (pdata && pdata->probe)
		rc = pdata->probe(pdev);

	return rc;
}

static int escore_plat_remove(struct platform_device *pdev)
{
	int rc = 0;
	struct escore_pdata *pdata;

	pr_debug("%s()\n", __func__);
	pdata = pdev->dev.platform_data;

	if (pdata && pdata->remove)
		rc = pdata->remove(pdev);

	return rc;
}

static struct platform_device_id escore_id_table[] = {
	{
		/* For UART device */
		.name = "escore-codec.uart",
	}, {
		/* For Dummy device to re-trigger probe context */
		.name = "escore-codec.dummy",
	}, {
		/* sentinel */
	}
};

struct platform_driver escore_plat_driver = {
	.driver = {
		.name = "escore-codec",
		.owner = THIS_MODULE,
	},
	.probe = escore_plat_probe,
	.remove = escore_plat_remove,
	.id_table = escore_id_table,
};

int escore_platform_init(void)
{
	int rc;

	rc = platform_driver_register(&escore_plat_driver);
	if (rc)
		return rc;

	pr_debug("%s(): Registered escore platform driver", __func__);

	return rc;
}
