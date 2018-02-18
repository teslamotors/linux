/*
 * escore-i2c.c  --  I2C interface for Audience earSmart chips
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "escore.h"
#include "escore-i2c.h"

static const struct i2c_client *escore_i2c;

static u32 escore_cpu_to_i2c(struct escore_priv *escore, u32 resp)
{
	return cpu_to_be32(resp);
}

static u32 escore_i2c_to_cpu(struct escore_priv *escore, u32 resp)
{
	return be32_to_cpu(resp);
}

int escore_i2c_read(struct escore_priv *escore, void *buf, int len)
{
	struct i2c_msg msg[] = {
		{
			.addr = escore_i2c->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};
	int rc = 0;

	rc = i2c_transfer(escore_i2c->adapter, msg, 1);

	/*
	 * i2c_transfer returns number of messages executed. Since we
	 * are always sending only 1 msg, return value should be 1 for
	 * success case
	 */
	if (rc != 1) {
		pr_err("%s(): i2c_transfer() failed, rc = %d, msg_len = %d\n",
			__func__, rc, len);

		ESCORE_FW_RECOVERY_FORCED_OFF(escore);
		return rc;
	}

	return 0;
}

int escore_i2c_write(struct escore_priv *escore, const void *buf, int len)
{
	struct i2c_msg msg;
	int max_xfer_len = ES_MAX_I2C_XFER_LEN;
	int rc = 0, written = 0, xfer_len;
	int retry = ES_MAX_RETRIES;

	msg.addr = escore_i2c->addr;
	msg.flags = 0;

	while (written < len) {
		xfer_len = min(len - written, max_xfer_len);

		msg.len = xfer_len;
		msg.buf = (void *)(buf + written);
		do {
			rc = i2c_transfer(escore_i2c->adapter, &msg, 1);
			if (rc == 1)
				break;
			else
				pr_err("%s(): failed, retrying, rc:%d\n",
						__func__, rc);
			usleep_range(10000, 10050);
			--retry;
		} while (retry != 0);

		/*
		 * i2c_transfer returns number of messages executed. Since we
		 * are always sending only 1 msg, return value should be 1 for
		 * success case
		 */
		if (rc != 1) {
			pr_err("%s:i2c_transfer() failed, rc=%d, msg_len=%d\n",
					 __func__, rc, xfer_len);

			ESCORE_FW_RECOVERY_FORCED_OFF(escore);
			return rc;
		}

		retry = ES_MAX_RETRIES;
		written += xfer_len;
	}

	return 0;
}

int escore_i2c_cmd(struct escore_priv *escore, u32 cmd, u32 *resp)
{
	int err = 0;
	int sr = cmd & BIT(28);
	u32 rv;
	int retry = ES_MAX_RETRIES + 1;

	dev_dbg(escore->dev,
			"%s: cmd=0x%08x  sr=0x%08x\n", __func__, cmd, sr);
	INC_DISABLE_FW_RECOVERY_USE_CNT(escore);

	if ((escore->cmd_compl_mode == ES_CMD_COMP_INTR) && !sr)
		escore_set_api_intr_wait(escore);

	*resp = 0;
	cmd = cpu_to_be32(cmd);
	err = escore_i2c_write(escore, &cmd, sizeof(cmd));
	if (err || sr)
		goto cmd_exit;

	do {
		if (escore->cmd_compl_mode == ES_CMD_COMP_INTR) {
			pr_debug("%s(): Waiting for API interrupt. Jiffies:%lu",
					__func__, jiffies);
			err = escore_api_intr_wait_completion(escore);
			if (err)
				break;
		} else {
			usleep_range(ES_RESP_POLL_TOUT,
					ES_RESP_POLL_TOUT + 500);
		}
		err = escore_i2c_read(escore, &rv, sizeof(rv));
		dev_dbg(escore->dev, "%s: err=%d\n", __func__, err);
		*resp = be32_to_cpu(rv);
		dev_dbg(escore->dev, "%s: *resp=0x%08x\n", __func__, *resp);
		if (err) {
			dev_dbg(escore->dev,
				"%s: I2C Read() failed %d\n", __func__, err);
		} else if ((*resp & ES_ILLEGAL_CMD) == ES_ILLEGAL_CMD) {
			dev_err(escore->dev, "%s: illegal command 0x%08x\n",
				__func__, be32_to_cpu(cmd));
			err = -EINVAL;
			goto cmd_exit;
		} else if (*resp == ES_NOT_READY) {
			dev_dbg(escore->dev,
				"%s: escore_i2c_read() not ready\n", __func__);
			err = -EBUSY;
		} else {
			goto cmd_exit;
		}

		--retry;
	} while (retry != 0 && escore->cmd_compl_mode != ES_CMD_COMP_INTR);

cmd_exit:
	update_cmd_history(be32_to_cpu(cmd), *resp);
	DEC_DISABLE_FW_RECOVERY_USE_CNT(escore);
	if (err && ((*resp & ES_ILLEGAL_CMD) != ES_ILLEGAL_CMD))
		ESCORE_FW_RECOVERY_FORCED_OFF(escore);

	return err;
}

int escore_i2c_boot_setup(struct escore_priv *escore)
{
	u16 boot_cmd = ES_I2C_BOOT_CMD;
	u16 boot_ack = 0;
	char msg[2];
	int rc;

	pr_info("%s()\n", __func__);
	pr_info("%s(): write ES_BOOT_CMD = 0x%04x\n", __func__, boot_cmd);
	cpu_to_be16s(&boot_cmd);
	memcpy(msg, (char *)&boot_cmd, 2);
	rc = escore_i2c_write(escore, msg, 2);
	if (rc < 0) {
		pr_err("%s(): firmware load failed boot write %d\n",
		       __func__, rc);
		goto escore_bootup_failed;
	}
	usleep_range(1000, 1005);
	memset(msg, 0, 2);
	rc = escore_i2c_read(escore, msg, 2);
	if (rc < 0) {
		pr_err("%s(): firmware load failed boot ack %d\n",
		       __func__, rc);
		goto escore_bootup_failed;
	}
	memcpy((char *)&boot_ack, msg, 2);
	pr_info("%s(): boot_ack = 0x%04x\n", __func__, boot_ack);
	if (boot_ack != ES_I2C_BOOT_ACK) {
		pr_err("%s(): firmware load failed boot ack pattern (0x%x)",
		       __func__, boot_ack);
		rc = -EIO;
		goto escore_bootup_failed;
	}

escore_bootup_failed:
	return rc;
}

int escore_i2c_boot_finish(struct escore_priv *escore)
{
	u32 sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_POLLING;
	u32 sync_ack;
	int rc = 0;
	int sync_retry = ES_SYNC_MAX_RETRY;

	/* sometimes earSmart chip sends success in second sync command */
	do {
		pr_debug("%s(): write ES_SYNC_CMD = 0x%08x\n",
				__func__, sync_cmd);
		rc = escore_i2c_cmd(escore, sync_cmd, &sync_ack);
		if (rc < 0) {
			pr_err("%s(): firmware load failed sync write %d\n",
			       __func__, rc);
			continue;
		}
		pr_debug("%s(): sync_ack = 0x%08x\n", __func__, sync_ack);
		if (sync_ack != ES_SYNC_ACK) {
			pr_err("%s(): firmware load failed sync ack pattern",
					__func__);
			rc = -EIO;
		} else {
			pr_info("%s(): firmware load success", __func__);
			break;
		}
	} while (sync_retry--);

	return rc;
}

static void escore_i2c_setup_pri_intf(struct escore_priv *escore)
{
	escore->bus.ops.read = escore_i2c_read;
	escore->bus.ops.write = escore_i2c_write;
	escore->bus.ops.cmd = escore_i2c_cmd;
	escore->streamdev = es_i2c_streamdev;
	escore->bus.ops.cpu_to_bus = escore_cpu_to_i2c;
	escore->bus.ops.bus_to_cpu = escore_i2c_to_cpu;
}

static int escore_i2c_setup_high_bw_intf(struct escore_priv *escore)
{
	int rc;

	escore->bus.ops.high_bw_write = escore_i2c_write;
	escore->bus.ops.high_bw_read = escore_i2c_read;
	escore->bus.ops.high_bw_cmd = escore_i2c_cmd;
	escore->boot_ops.setup = escore_i2c_boot_setup;
	escore->boot_ops.finish = escore_i2c_boot_finish;
	escore->bus.ops.cpu_to_high_bw_bus = escore_cpu_to_i2c;
	escore->bus.ops.high_bw_bus_to_cpu = escore_i2c_to_cpu;
	rc = escore->probe(escore->dev);
	if (rc)
		goto out;

	mutex_lock(&escore->access_lock);
	rc = escore->boot_ops.bootup(escore);
	mutex_unlock(&escore->access_lock);

out:
	return rc;
}

#ifdef CONFIG_ARCH_MSM8974
static int reg_set_optimum_mode_check(struct regulator *reg, int load_ua)
{
	return (regulator_count_voltages(reg) > 0) ?
		regulator_set_optimum_mode(reg, load_ua) : 0;
}

static int escore_i2c_power_on(struct escore_priv *escore)
{
	int rc;

	if (escore->i2c_pull_up) {
		rc = reg_set_optimum_mode_check(escore->vcc_i2c,
				ESCORE_I2C_LOAD_UA);
		if (rc < 0) {
			dev_err(escore->dev,
				"Regulator vcc_i2c set_opt failed rc=%d\n", rc);
			goto error_reg_opt_i2c;
		}

		rc = regulator_enable(escore->vcc_i2c);
		if (rc) {
			dev_err(escore->dev,
				"Regulator vcc_i2c enable failed rc=%d\n", rc);
			goto error_reg_en_vcc_i2c;
		}
	}
	msleep(130);
	return 0;

error_reg_en_vcc_i2c:
	if (escore->i2c_pull_up)
		reg_set_optimum_mode_check(escore->vcc_i2c, 0);
error_reg_opt_i2c:
	return rc;

}

static int escore_i2c_regulator_configure(struct escore_priv *escore)
{
	int rc;
	pr_debug("%s:%d Configuring i2c_pull_up\n", __func__, __LINE__);

	if (escore->i2c_pull_up) {
		escore->vcc_i2c = regulator_get(escore->dev, "vcc_i2c");
		if (IS_ERR(escore->vcc_i2c)) {
			rc = PTR_ERR(escore->vcc_i2c);
			dev_err(escore->dev,
				"Regulator get failed rc=%d\n", rc);
			goto error_get_vtg_i2c;
		}
		pr_debug("%s:%d regulator_count_voltages %d",
				__func__, __LINE__,
				regulator_count_voltages(escore->vcc_i2c));

		if (regulator_count_voltages(escore->vcc_i2c) > 0) {
			rc = regulator_set_voltage(escore->vcc_i2c,
				ESCORE_I2C_VTG_MIN_UV, ESCORE_I2C_VTG_MAX_UV);
			if (rc) {
				dev_err(escore->dev,
					"regulator set_vtg failed rc=%d\n", rc);
				goto error_set_vtg_i2c;
			}
		}
	}
	return 0;

error_set_vtg_i2c:
	regulator_put(escore->vcc_i2c);
error_get_vtg_i2c:
	return rc;
}

static void escore_i2c_regulator_disable(struct escore_priv *escore)
{
	if (escore->i2c_pull_up) {
		reg_set_optimum_mode_check(escore->vcc_i2c, 0);
		regulator_disable(escore->vcc_i2c);
		msleep(50);
	}
}

static int escore_i2c_power_off(struct escore_priv *escore)
{
	if (escore->i2c_pull_up) {
		if (regulator_count_voltages(escore->vcc_i2c) > 0)
			regulator_set_voltage(escore->vcc_i2c, 0,
						ESCORE_I2C_VTG_MAX_UV);
		regulator_put(escore->vcc_i2c);
	}
	return 0;
}
#endif

static int escore_i2c_probe(struct i2c_client *i2c,
			   const struct i2c_device_id *id)
{
	struct escore_priv *escore = &escore_priv;
	int rc;
	struct device_node *np = NULL;

	pr_debug("%s:%d", __func__, __LINE__);

	escore_i2c = i2c;

	if (escore->pri_intf == ES_I2C_INTF) {
		escore->bus.setup_prim_intf = escore_i2c_setup_pri_intf;
		escore->dev = &i2c->dev;
	}
	if (escore->high_bw_intf == ES_I2C_INTF)
		escore->bus.setup_high_bw_intf = escore_i2c_setup_high_bw_intf;

	i2c_set_clientdata(i2c, escore);

#ifdef CONFIG_ARCH_MSM8974
	if (escore->dev)
		np = escore->dev->of_node;

	if (np) {
		/* regulator info */
		escore->i2c_pull_up =
			of_property_read_bool(np, "adnc,i2c-pull-up");
		pr_debug("%s:%d i2c_pull_up %d\n",
			__func__, __LINE__, escore->i2c_pull_up);

		rc = escore_i2c_regulator_configure(escore);
		if (rc) {
			dev_err(escore->dev, "%s: initialize hw fail %d\n",
				__func__, rc);
			goto continue_init;
		}
		rc = escore_i2c_power_on(escore);
		if (rc)
			dev_err(escore->dev, "%s: Power On hardware Fail %d\n",
				__func__, rc);
	}

continue_init:
#endif
	rc = escore_probe(escore, &i2c->dev, ES_I2C_INTF, ES_CONTEXT_PROBE);
	return rc;
}

static int escore_i2c_remove(struct i2c_client *i2c)
{
#ifdef CONFIG_ARCH_MSM8974
	escore_i2c_power_off(&escore_priv);
	escore_i2c_regulator_disable(&escore_priv);
#endif
	snd_soc_unregister_codec(&i2c->dev);
	kfree(i2c_get_clientdata(i2c));
	return 0;
}



/* streamdev read function should return length.
 * In case of I2C as streaming device,I2C read returns
 * 0 in case of success  otherwise error.
 * So implement this wrapper function to support
 * streaming over I2C.
 * This function return length on success and 0 in case of failure
 * so that streaming thread exit gracefully.
 */
static int escore_i2c_stream_read(struct escore_priv *escore, void *buf,
		int len)
{
	int rc = escore_i2c_read(escore, buf, len);

	if (rc != 0) {
		pr_err("%s: i2c stream read fail\n", __func__);
		return 0;
	}
	return len;
}

struct es_stream_device es_i2c_streamdev = {
	.read = escore_i2c_stream_read,
	.intf = ES_I2C_INTF,
};

int escore_i2c_init(void)
{
	int rc;
	pr_debug("%s: adding i2c driver\n", __func__);

	rc = i2c_add_driver(&escore_i2c_driver);
	if (!rc)
		pr_info("%s() registered as I2C", __func__);

	else
		pr_err("%s(): i2c_add_driver failed, rc = %d", __func__, rc);

	return rc;
}

static const struct i2c_device_id escore_i2c_id[] = {
	{ "earSmart", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, escore_i2c_id);

struct i2c_driver escore_i2c_driver = {
	.driver = {
		.name = "earSmart-codec",
		.owner = THIS_MODULE,
		.pm = &escore_pm_ops,
	},
	.probe = escore_i2c_probe,
	.remove = escore_i2c_remove,
	.id_table = escore_i2c_id,
};

MODULE_DESCRIPTION("Audience earSmart I2C core driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:earSmart-codec");
