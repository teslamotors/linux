/*
 * escore-slim.c  --  Slimbus interface for Audience earSmart chips
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "escore.h"
#include "escore-slim.h"
#include "escore-list.h"
/*
 * Delay for receiving response can be up to 20 ms.
 * To minimize waiting time, response is checking
 * up to 20 times with 1ms delay.
 */
#define MAX_SMB_TRIALS	3
#define SMB_DELAY	1000

static void escore_alloc_slim_rx_chan(struct escore_priv *escore);
static void escore_alloc_slim_tx_chan(struct escore_priv *escore);
static int escore_rx_ch_num_to_idx(struct escore_priv *escore, int ch_num);
static int escore_tx_ch_num_to_idx(struct escore_priv *escore, int ch_num);
static int populate_and_trigger_reset(struct device *dev);

#ifdef CONFIG_SLIMBUS_MSM_NGD
static struct mutex slimbus_fw_load_mutex;
#endif

static void escore_alloc_slim_rx_chan(struct escore_priv *escore_priv)
{
	struct slim_device *sbdev = escore_priv->gen0_client;
	struct escore_slim_ch *rx = escore_priv->slim_rx;
	int i;
	int port_id;

	dev_dbg(&sbdev->dev, "%s()\n", __func__);

	for (i = 0; i < escore_priv->slim_rx_ports; i++) {
		port_id = i;
		rx[i].ch_num = escore_priv->slim_rx_port_to_ch_map[i];
		slim_get_slaveport(sbdev->laddr, port_id, &rx[i].sph,
				   SLIM_SINK);
		slim_query_ch(sbdev, rx[i].ch_num, &rx[i].ch_h);
	}
}

static void escore_alloc_slim_tx_chan(struct escore_priv *escore_priv)
{
	struct slim_device *sbdev = escore_priv->gen0_client;
	struct escore_slim_ch *tx = escore_priv->slim_tx;
	int i;
	int port_id;

	dev_dbg(&sbdev->dev, "%s()\n", __func__);

	for (i = 0; i < escore_priv->slim_tx_ports; i++) {
		port_id = i + 10;
		tx[i].ch_num = escore_priv->slim_tx_port_to_ch_map[i];
		slim_get_slaveport(sbdev->laddr, port_id, &tx[i].sph,
				   SLIM_SRC);
		slim_query_ch(sbdev, tx[i].ch_num, &tx[i].ch_h);
	}
}

static int escore_rx_ch_num_to_idx(struct escore_priv *escore, int ch_num)
{
	int i;
	int idx = -1;

	pr_debug("%s(ch_num = %d)\n", __func__, ch_num);

	for (i = 0; i < escore->slim_rx_ports; i++) {
		if (ch_num == escore->slim_rx_port_to_ch_map[i]) {
			idx = i;
			break;
		}
	}

	return idx;
}

static int escore_tx_ch_num_to_idx(struct escore_priv *escore, int ch_num)
{
	int i;
	int idx = -1;

	pr_debug("%s(ch_num = %d)\n", __func__, ch_num);

	for (i = 0; i < escore->slim_tx_ports; i++) {
		if (ch_num == escore->slim_tx_port_to_ch_map[i]) {
			idx = i;
			break;
		}
	}

	return idx;
}

int escore_cfg_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
			     unsigned int ch_cnt, unsigned int rate)
{
	struct escore_priv *escore_priv = slim_get_devicedata(sbdev);
	struct escore_slim_ch *rx = escore_priv->slim_rx;
	u16 grph;
	u32 *sph;
	u16 *ch_h;
	struct slim_ch prop;
	int i;
	int idx;
	int rc = 0;

	dev_dbg(&sbdev->dev, "%s(ch_cnt = %d, rate = %d)\n", __func__,
		ch_cnt, rate);

	sph = kmalloc(sizeof(u32)*escore_priv->slim_rx_ports, GFP_KERNEL);
	if (!sph)
		return -ENOMEM;
	ch_h = kmalloc(sizeof(u32)*escore_priv->slim_rx_ports, GFP_KERNEL);
	if (!ch_h) {
		kfree(sph);
		return -ENOMEM;
	}

	for (i = 0; i < ch_cnt; i++) {
		idx = escore_rx_ch_num_to_idx(escore_priv, ch_num[i]);
		ch_h[i] = rx[idx].ch_h;
		sph[i] = rx[idx].sph;
	}

	prop.prot = SLIM_AUTO_ISO;
	prop.baser = SLIM_RATE_4000HZ;
	prop.dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop.auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop.ratem = (rate/4000);
	prop.sampleszbits = 16;

	rc = slim_define_ch(sbdev, &prop, ch_h, ch_cnt, true, &grph);
	if (rc < 0) {
		dev_err(&sbdev->dev, "%s(): slim_define_ch() failed: %d\n",
			__func__, rc);
		goto slim_define_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		rc = slim_connect_sink(sbdev, &sph[i], 1, ch_h[i]);
		if (rc < 0) {
			dev_err(&sbdev->dev,
				"%s(): slim_connect_sink() failed: %d\n",
				__func__, rc);
			goto slim_connect_sink_error;
		}
	}
	rc = slim_control_ch(sbdev, grph, SLIM_CH_ACTIVATE, true);
	if (rc < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_control_ch() failed: %d\n",
			__func__, rc);
		goto slim_control_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		idx = escore_rx_ch_num_to_idx(escore_priv, ch_num[i]);
		rx[idx].grph = grph;
	}
	kfree(sph);
	kfree(ch_h);
	return rc;
slim_control_ch_error:
slim_connect_sink_error:
	escore_close_slim_rx(sbdev, ch_num, ch_cnt);
slim_define_ch_error:
	kfree(sph);
	kfree(ch_h);
	return rc;
}

int escore_cfg_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
			     unsigned int ch_cnt, unsigned int rate)
{
	struct escore_priv *escore_priv = slim_get_devicedata(sbdev);
	struct escore_slim_ch *tx = escore_priv->slim_tx;
	u16 grph;
	u32 *sph;
	u16 *ch_h;
	struct slim_ch prop;
	int i;
	int idx;
	int rc;

	dev_dbg(&sbdev->dev, "%s(ch_cnt = %d, rate = %d)\n", __func__,
		ch_cnt, rate);

	sph = kmalloc(sizeof(u32)*escore_priv->slim_rx_ports, GFP_KERNEL);
	if (!sph)
		return -ENOMEM;
	ch_h = kmalloc(sizeof(u32)*escore_priv->slim_rx_ports, GFP_KERNEL);
	if (!ch_h) {
		kfree(sph);
		return -ENOMEM;
	}


	for (i = 0; i < ch_cnt; i++) {
		idx = escore_tx_ch_num_to_idx(escore_priv, ch_num[i]);
		ch_h[i] = tx[idx].ch_h;
		sph[i] = tx[idx].sph;
	}

	prop.prot = SLIM_AUTO_ISO;
	prop.baser = SLIM_RATE_4000HZ;
	prop.dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop.auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop.ratem = (rate/4000);
	prop.sampleszbits = 16;

	rc = slim_define_ch(sbdev, &prop, ch_h, ch_cnt, true, &grph);
	if (rc < 0) {
		dev_err(&sbdev->dev, "%s(): slim_define_ch() failed: %d\n",
			__func__, rc);
		goto slim_define_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		rc = slim_connect_src(sbdev, sph[i], ch_h[i]);
		if (rc < 0) {
			dev_err(&sbdev->dev,
				"%s(): slim_connect_src() failed: %d\n",
				__func__, rc);
			dev_err(&sbdev->dev,
				"%s(): ch_num[0] = %d\n",
				__func__, ch_num[0]);
			goto slim_connect_src_error;
		}
	}
	rc = slim_control_ch(sbdev, grph, SLIM_CH_ACTIVATE, true);
	if (rc < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_control_ch() failed: %d\n",
			__func__, rc);
		goto slim_control_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		idx = escore_tx_ch_num_to_idx(escore_priv, ch_num[i]);
		tx[idx].grph = grph;
	}

	kfree(sph);
	kfree(ch_h);
	return rc;
slim_control_ch_error:
slim_connect_src_error:
	escore_close_slim_tx(sbdev, ch_num, ch_cnt);
slim_define_ch_error:
	kfree(sph);
	kfree(ch_h);
	return rc;
}

int escore_close_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
			       unsigned int ch_cnt)
{
	struct escore_priv *escore_priv = slim_get_devicedata(sbdev);
	struct escore_slim_ch *rx = escore_priv->slim_rx;
	u16 grph = 0;
	u32 *sph;
	int i;
	int idx;
	int rc;

	sph = kmalloc(sizeof(u32)*escore_priv->slim_rx_ports, GFP_KERNEL);
	if (!sph)
		return -ENOMEM;

	dev_dbg(&sbdev->dev, "%s(ch_cnt = %d)\n", __func__, ch_cnt);

	for (i = 0; i < ch_cnt; i++) {
		idx = escore_rx_ch_num_to_idx(escore_priv, ch_num[i]);
		sph[i] = rx[idx].sph;
		grph = rx[idx].grph;
	}

	rc = slim_control_ch(sbdev, grph, SLIM_CH_REMOVE, true);
	if (rc < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_control_ch() failed: %d\n",
			__func__, rc);
		goto slim_control_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		idx = escore_rx_ch_num_to_idx(escore_priv, ch_num[i]);
		rx[idx].grph = 0;
	}
	rc = slim_disconnect_ports(sbdev, sph, ch_cnt);
	if (rc < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_disconnect_ports() failed: %d\n",
			__func__, rc);
	}
slim_control_ch_error:
	kfree(sph);
	return rc;
}

int escore_close_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
			       unsigned int ch_cnt)
{
	struct escore_priv *escore_priv = slim_get_devicedata(sbdev);
	struct escore_slim_ch *tx = escore_priv->slim_tx;
	u16 grph = 0;
	u32 *sph;
	int i;
	int idx;
	int rc;

	sph = kmalloc(sizeof(u32)*escore_priv->slim_tx_ports, GFP_KERNEL);
	if (!sph)
		return -ENOMEM;

	dev_dbg(&sbdev->dev, "%s(ch_cnt = %d)\n", __func__, ch_cnt);

	for (i = 0; i < ch_cnt; i++) {
		idx = escore_tx_ch_num_to_idx(escore_priv, ch_num[i]);
		sph[i] = tx[idx].sph;
		grph = tx[idx].grph;
	}

	rc = slim_control_ch(sbdev, grph, SLIM_CH_REMOVE, true);
	if (rc < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_control_ch() failed: %d\n",
			__func__, rc);
		goto slim_control_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		idx = escore_tx_ch_num_to_idx(escore_priv, ch_num[i]);
		tx[idx].grph = 0;
	}
	rc = slim_disconnect_ports(sbdev, sph, ch_cnt);
	if (rc < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_disconnect_ports() failed: %d\n",
			__func__, rc);
	}
slim_control_ch_error:
	kfree(sph);
	return rc;
}

/* esxxx -> codec - alsa playback function */
static int escore_codec_cfg_slim_tx(struct escore_priv *escore, int dai_id)
{
	struct slim_device *sbdev = escore->gen0_client;
	int rc;

	dev_dbg(&sbdev->dev, "%s(dai_id = %d)\n", __func__, dai_id);
	/* start slim channels associated with id */
	rc = escore_cfg_slim_tx(escore->gen0_client,
			       escore->slim_dai_data[DAI_INDEX(dai_id)].ch_num,
			       escore->slim_dai_data[DAI_INDEX(dai_id)].ch_tot,
			       escore->slim_dai_data[DAI_INDEX(dai_id)].rate);

	return rc;
}

/* esxxx <- codec - alsa capture function */
static int escore_codec_cfg_slim_rx(struct escore_priv *escore, int dai_id)
{
	struct slim_device *sbdev = escore->gen0_client;
	int rc;

	dev_dbg(&sbdev->dev, "%s(dai_id = %d)\n", __func__, dai_id);
	/* start slim channels associated with id */
	rc = escore_cfg_slim_rx(escore->gen0_client,
			       escore->slim_dai_data[DAI_INDEX(dai_id)].ch_num,
			       escore->slim_dai_data[DAI_INDEX(dai_id)].ch_tot,
			       escore->slim_dai_data[DAI_INDEX(dai_id)].rate);

	return rc;
}

/* esxxx -> codec - alsa playback function */
static int escore_codec_close_slim_tx(struct escore_priv *escore, int dai_id)
{
	struct slim_device *sbdev = escore->gen0_client;
	int rc;

	dev_dbg(&sbdev->dev, "%s(dai_id = %d)\n", __func__, dai_id);
	/* close slim channels associated with id */
	rc = escore_close_slim_tx(escore->gen0_client,
			 escore->slim_dai_data[DAI_INDEX(dai_id)].ch_num,
			 escore->slim_dai_data[DAI_INDEX(dai_id)].ch_tot);

	return rc;
}

/* esxxx <- codec - alsa capture function */
static int escore_codec_close_slim_rx(struct escore_priv *escore, int dai_id)
{
	struct slim_device *sbdev = escore->gen0_client;
	int rc;

	dev_dbg(&sbdev->dev, "%s(dai_id = %d)\n", __func__, dai_id);
	/* close slim channels associated with id */
	rc = escore_close_slim_rx(escore->gen0_client,
			 escore->slim_dai_data[DAI_INDEX(dai_id)].ch_num,
			 escore->slim_dai_data[DAI_INDEX(dai_id)].ch_tot);

	return rc;
}

int escore_remote_cfg_slim_rx(int dai_id)
{
	struct escore_priv *escore = &escore_priv;
	int be_id;
	int rc = 0;

	if (escore_priv.flag.local_slim_ch_cfg)
		return rc;

	dev_dbg(escore->dev, "%s(dai_id = %d)\n", __func__, dai_id);

	if (escore->slim_dai_data &&
		escore->slim_dai_data[DAI_INDEX(dai_id)].ch_tot != 0) {
		/* start slim channels associated with id */
		rc = escore_cfg_slim_rx(escore->gen0_client,
			       escore->slim_dai_data[DAI_INDEX(dai_id)].ch_num,
			       escore->slim_dai_data[DAI_INDEX(dai_id)].ch_tot,
			       escore->slim_dai_data[DAI_INDEX(dai_id)].rate);

		be_id = escore->slim_be_id[DAI_INDEX(dai_id)];
		escore->slim_dai_data[DAI_INDEX(be_id)].ch_tot =
			escore->slim_dai_data[DAI_INDEX(dai_id)].ch_tot;
		escore->slim_dai_data[DAI_INDEX(be_id)].rate =
			escore->slim_dai_data[DAI_INDEX(dai_id)].rate;

		rc = escore_codec_cfg_slim_tx(escore, be_id);
	}

	return rc;
}
EXPORT_SYMBOL_GPL(escore_remote_cfg_slim_rx);

int escore_remote_cfg_slim_tx(int dai_id)
{
	struct escore_priv *escore = &escore_priv;
	int be_id;
	int ch_cnt;
	int rc = 0;

	if (escore_priv.flag.local_slim_ch_cfg)
		return rc;

	dev_dbg(escore->dev, "%s(dai_id = %d)\n", __func__, dai_id);

	if (escore->slim_dai_data &&
		escore->slim_dai_data[DAI_INDEX(dai_id)].ch_tot != 0) {
		ch_cnt = escore->ap_tx1_ch_cnt;
		/* start slim channels associated with id */
		rc = escore_cfg_slim_tx(escore->gen0_client,
			       escore->slim_dai_data[DAI_INDEX(dai_id)].ch_num,
			       ch_cnt,
			       escore->slim_dai_data[DAI_INDEX(dai_id)].rate);

		be_id = escore->slim_be_id[DAI_INDEX(dai_id)];
		escore->slim_dai_data[DAI_INDEX(be_id)].ch_tot =
			escore->slim_dai_data[DAI_INDEX(dai_id)].ch_tot;
		escore->slim_dai_data[DAI_INDEX(be_id)].rate =
			escore->slim_dai_data[DAI_INDEX(dai_id)].rate;

		rc = escore_codec_cfg_slim_rx(escore, be_id);
	}

	return rc;
}
EXPORT_SYMBOL_GPL(escore_remote_cfg_slim_tx);

int escore_remote_close_slim_rx(int dai_id)
{
	struct escore_priv *escore = &escore_priv;
	int be_id;
	int rc = 0;

	if (escore_priv.flag.local_slim_ch_cfg)
		return rc;

	dev_dbg(escore->dev, "%s(dai_id = %d)\n", __func__, dai_id);

	if (escore->slim_dai_data &&
		escore->slim_dai_data[DAI_INDEX(dai_id)].ch_tot != 0) {
		rc = escore_close_slim_rx(escore->gen0_client,
			    escore->slim_dai_data[DAI_INDEX(dai_id)].ch_num,
			    escore->slim_dai_data[DAI_INDEX(dai_id)].ch_tot);

		be_id = escore->slim_be_id[DAI_INDEX(dai_id)];
		rc = escore_codec_close_slim_tx(escore, be_id);

		escore->slim_dai_data[DAI_INDEX(dai_id)].ch_tot = 0;
	}

	return rc;
}
EXPORT_SYMBOL_GPL(escore_remote_close_slim_rx);

int escore_remote_close_slim_tx(int dai_id)
{
	struct escore_priv *escore = &escore_priv;
	int be_id;
	int ch_cnt;
	int rc = 0;

	if (escore_priv.flag.local_slim_ch_cfg)
		return rc;

	dev_dbg(escore->dev, "%s(dai_id = %d)\n", __func__, dai_id);

	if (escore->slim_dai_data &&
		escore->slim_dai_data[DAI_INDEX(dai_id)].ch_tot != 0) {
		ch_cnt = escore->ap_tx1_ch_cnt;
		escore_close_slim_tx(escore->gen0_client,
			    escore->slim_dai_data[DAI_INDEX(dai_id)].ch_num,
			    ch_cnt);

		be_id = escore->slim_be_id[DAI_INDEX(dai_id)];
		rc = escore_codec_close_slim_rx(escore, be_id);

		escore->slim_dai_data[DAI_INDEX(dai_id)].ch_tot = 0;
	}

	return rc;
}
EXPORT_SYMBOL_GPL(escore_remote_close_slim_tx);

int escore_remote_route_enable(struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct escore_priv *escore = &escore_priv;
	int rc = 0;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
			dai->name, dai->id);

	if (escore->remote_route_enable)
		rc = escore->remote_route_enable(dai);

	return rc;
}

static void escore_init_slim_slave(struct escore_priv *escore)
{
	escore_alloc_slim_rx_chan(escore);
	escore_alloc_slim_tx_chan(escore);
}

static u32 escore_cpu_to_slim(struct escore_priv *escore, u32 resp)
{
	return cpu_to_le32(resp);
}

static u32 escore_slim_to_cpu(struct escore_priv *escore, u32 resp)
{
	return le32_to_cpu(resp);
}

int escore_slim_read(struct escore_priv *escore, void *buf, int len)
{
	int retry = MAX_SMB_TRIALS;
	int rc;
	struct slim_device *sbdev = escore->gen0_client;
	DECLARE_COMPLETION_ONSTACK(read_done);
	struct slim_ele_access msg = {
		.start_offset = ES_READ_VE_OFFSET,
		.num_bytes = ES_READ_VE_WIDTH,
		.comp = NULL,
	};


	rc = slim_get_logical_addr(sbdev, escore->gen0_client->e_addr,
			6, &(escore->gen0_client->laddr));
	if (rc)
		dev_err(&sbdev->dev, "%s(): get logical addr err %d\n",
							__func__, rc);

	do {
		rc = slim_request_val_element(sbdev, &msg, buf, len);
		if (rc != 0)
			dev_warn(&sbdev->dev,
				"%s: Warnning read failed rc=%d\n",
				__func__, rc);
		else
			break;
		usleep_range(SMB_DELAY, SMB_DELAY + 5);
	} while (--retry);

	if (rc != 0) {
		dev_err(&sbdev->dev,
			"%s: Error read failed rc=%d after %d retries\n",
			__func__, rc, MAX_SMB_TRIALS);

		ESCORE_FW_RECOVERY_FORCED_OFF(escore);
	}

	return rc;
}

int escore_slim_write(struct escore_priv *escore, const void *buf, int len)
{
	int retry = MAX_SMB_TRIALS;
	int rc;
	int wr;
	struct slim_device *sbdev = escore->gen0_client;
	struct slim_ele_access msg = {
		.start_offset = ES_WRITE_VE_OFFSET,
		.num_bytes = ES_WRITE_VE_WIDTH,
		.comp = NULL,
	};

	BUG_ON(len < 0);

	rc = wr = 0;
	while (wr < len) {
		int sz = min(len - wr, ES_WRITE_VE_WIDTH);

		/* As long as the caller expects the most significant
		 * bytes of the VE value written to be zero, this is
		 * valid.
		 */
		if (sz < ES_WRITE_VE_WIDTH)
			dev_dbg(&sbdev->dev,
				"write smaller than VE size %d < %d\n",
				sz, ES_WRITE_VE_WIDTH);
		rc = slim_get_logical_addr(sbdev,
				escore->gen0_client->e_addr,
				6, &(escore->gen0_client->laddr));
		if (rc)
			dev_err(&sbdev->dev,
				"%s(): get logical addr err %d\n",
				__func__, rc);

		do {
			rc = slim_change_val_element(sbdev, &msg, buf + wr, sz);
			if (rc != 0)
				dev_warn(&sbdev->dev,
					"%s: Warnning write failed rc=%d\n",
					__func__, rc);
			else
				break;
			usleep_range(SMB_DELAY, SMB_DELAY + 5);
		} while (--retry);

		if (rc != 0) {
			dev_err(&sbdev->dev,
			"%s: Error write failed rc=%d after %d retries\n",
			__func__, rc, MAX_SMB_TRIALS);
			break;
		}
		wr += sz;
	}

	if (rc)
		ESCORE_FW_RECOVERY_FORCED_OFF(escore);

	return rc;
}

int escore_slim_cmd(struct escore_priv *escore, u32 cmd, u32 *resp)
{
	int err = 0;
	int sr = cmd & BIT(28);
	u32 rv;
	int retry = ES_MAX_RETRIES + 1;
	struct slim_device *sbdev = escore->gen0_client;

	dev_dbg(&sbdev->dev,
			"%s: cmd=0x%08x  sr=0x%08x\n", __func__, cmd, sr);
	INC_DISABLE_FW_RECOVERY_USE_CNT(escore);

	if ((escore->cmd_compl_mode == ES_CMD_COMP_INTR) && !sr)
		escore_set_api_intr_wait(escore);

	*resp = 0;
	cmd = cpu_to_le32(cmd);
	err = escore_slim_write(escore, &cmd, sizeof(cmd));
	dev_dbg(&sbdev->dev, "%s: err=%d, cmd_cmpl_mode=%d\n",
		 __func__, err, escore->cmd_compl_mode);
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
			pr_debug("%s(): Polling method\n", __func__);
			usleep_range(ES_RESP_POLL_TOUT,
					ES_RESP_POLL_TOUT + 500);
		}
		err = escore_slim_read(escore, &rv, sizeof(rv));
		dev_dbg(&sbdev->dev, "%s: err=%d\n", __func__, err);
		*resp = le32_to_cpu(rv);
		dev_dbg(&sbdev->dev, "%s: *resp=0x%08x\n", __func__, *resp);
		if (err) {
			dev_dbg(&sbdev->dev,
				"%s: slim_read() fail %d\n", __func__, err);
		} else if ((*resp & ES_ILLEGAL_CMD) == ES_ILLEGAL_CMD) {
			dev_err(&sbdev->dev, "%s: illegal command 0x%08x\n",
				__func__, le32_to_cpu(cmd));
			err = -EINVAL;
			goto cmd_exit;
		} else if (*resp == ES_NOT_READY) {
			dev_dbg(&sbdev->dev,
				"%s: escore_slim_read() not ready\n", __func__);
			err = -EBUSY;
		} else {
			goto cmd_exit;
		}

		--retry;
	} while (retry != 0 && escore->cmd_compl_mode != ES_CMD_COMP_INTR);

cmd_exit:
	update_cmd_history(le32_to_cpu(cmd), *resp);
	DEC_DISABLE_FW_RECOVERY_USE_CNT(escore);
	if (err && ((*resp & ES_ILLEGAL_CMD) != ES_ILLEGAL_CMD))
		ESCORE_FW_RECOVERY_FORCED_OFF(escore);

	return err;
}

static int escore_slim_set_sysclk(struct snd_soc_dai *dai, int clk_id,
				unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	if (escore_priv.slim_dai_ops.set_sysclk)
		rc = escore_priv.slim_dai_ops.set_sysclk(dai, clk_id,
				freq, dir);

	return rc;
}

static int escore_slim_set_pll(struct snd_soc_dai *dai, int pll_id,
			     int source, unsigned int freq_in,
			     unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	if (escore_priv.slim_dai_ops.set_pll)
		rc = escore_priv.slim_dai_ops.set_pll(dai, pll_id,
				source, freq_in, freq_out);

	return rc;
}

static int escore_slim_set_clkdiv(struct snd_soc_dai *dai, int div_id,
				int div)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	if (escore_priv.slim_dai_ops.set_clkdiv)
		rc = escore_priv.slim_dai_ops.set_clkdiv(dai, div_id,
				div);

	return rc;
}

static int escore_slim_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	if (escore_priv.slim_dai_ops.set_fmt)
		rc = escore_priv.slim_dai_ops.set_fmt(dai, fmt);

	return rc;
}

static int escore_slim_set_tdm_slot(struct snd_soc_dai *dai,
				  unsigned int tx_mask, unsigned int rx_mask,
				  int slots, int slot_width)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	if (escore_priv.slim_dai_ops.set_tdm_slot)
		rc = escore_priv.slim_dai_ops.set_tdm_slot(dai, tx_mask,
				rx_mask, slots, slot_width);

	return rc;
}

int escore_slim_set_channel_map(struct snd_soc_dai *dai,
				     unsigned int tx_num, unsigned int *tx_slot,
				     unsigned int rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n",
		__func__, dai->name, dai->id);

	if (escore_priv.slim_dai_ops.set_channel_map)
		rc = escore_priv.slim_dai_ops.set_channel_map(dai, tx_num,
				tx_slot, rx_num, rx_slot);

	return rc;
}

static int escore_slim_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	if (escore_priv.slim_dai_ops.set_tristate)
		rc = escore_priv.slim_dai_ops.set_tristate(dai, tristate);

	return rc;
}

int escore_slim_get_channel_map(struct snd_soc_dai *dai,
				     unsigned int *tx_num,
				     unsigned int *tx_slot,
				     unsigned int *rx_num,
				     unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (escore_priv.slim_dai_ops.get_channel_map)
		rc = escore_priv.slim_dai_ops.get_channel_map(dai, tx_num,
				tx_slot, rx_num, rx_slot);

	return rc;
}

static int escore_slim_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s()\n", __func__);

	if (escore_priv.slim_dai_ops.digital_mute)
		rc = escore_priv.slim_dai_ops.digital_mute(dai, mute);

	return rc;
}

int escore_slim_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
			dai->name, dai->id);

	if (escore_priv.slim_dai_ops.startup)
		rc = escore_priv.slim_dai_ops.startup(substream, dai);

	if (codec && codec->dev && codec->dev->parent)
		pm_runtime_get_sync(codec->dev->parent);

	return rc;
}
EXPORT_SYMBOL_GPL(escore_slim_startup);

void escore_slim_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
			dai->name, dai->id);

	if (escore_priv.slim_dai_ops.shutdown)
		escore_priv.slim_dai_ops.shutdown(substream, dai);

	return;
}
EXPORT_SYMBOL_GPL(escore_slim_shutdown);

int escore_slim_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params,
			 struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct escore_priv *escore = &escore_priv;
	int id = dai->id;
	int channels;
	int rate;
	int rc = 0;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
			dai->name, dai->id);

	channels = params_channels(params);
	switch (channels) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
		escore->slim_dai_data[DAI_INDEX(id)].ch_tot = channels;
		break;
	default:
		dev_err(codec->dev,
			"%s(): unsupported number of channels, %d\n",
			__func__, channels);
		return -EINVAL;
	}
	rate = params_rate(params);
	switch (rate) {
	case 8000:
	case 16000:
	case 24000:
	case 32000:
	case 48000:
		escore->slim_dai_data[DAI_INDEX(id)].rate = rate;
		break;
	default:
		dev_err(codec->dev,
			"%s(): unsupported rate, %d\n",
			__func__, rate);
		return -EINVAL;
	}

	if (escore_priv.slim_dai_ops.hw_params)
		rc = escore_priv.slim_dai_ops.hw_params(substream, params,
				dai);

	return rc;
}
EXPORT_SYMBOL_GPL(escore_slim_hw_params);

static int escore_slim_hw_free(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	int rc = 0;

	if (escore_priv.slim_dai_ops.hw_free)
		rc = escore_priv.slim_dai_ops.hw_free(substream, dai);

	return rc;
}

static int escore_slim_prepare(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	int rc = 0;

	if (escore_priv.slim_dai_ops.prepare)
		rc = escore_priv.slim_dai_ops.prepare(substream, dai);

	return rc;
}

int escore_slim_trigger(struct snd_pcm_substream *substream,
		       int cmd, struct snd_soc_dai *dai)
{
	int rc = 0;

	if (escore_priv.slim_dai_ops.trigger)
		rc = escore_priv.slim_dai_ops.trigger(substream, cmd, dai);

	return rc;
}
EXPORT_SYMBOL_GPL(escore_slim_trigger);

struct snd_soc_dai_ops escore_slim_port_dai_ops = {
	.set_sysclk	= escore_slim_set_sysclk,
	.set_pll	= escore_slim_set_pll,
	.set_clkdiv	= escore_slim_set_clkdiv,
	.set_fmt	= escore_slim_set_fmt,
	.set_tdm_slot	= escore_slim_set_tdm_slot,
	.set_channel_map	= escore_slim_set_channel_map,
	.get_channel_map	= escore_slim_get_channel_map,
	.set_tristate	= escore_slim_set_tristate,
	.digital_mute	= escore_slim_digital_mute,
	.startup	= escore_slim_startup,
	.shutdown	= escore_slim_shutdown,
	.hw_params	= escore_slim_hw_params,
	.hw_free	= escore_slim_hw_free,
	.prepare	= escore_slim_prepare,
	.trigger	= escore_slim_trigger,
};

int escore_slim_boot_setup(struct escore_priv *escore)
{
	u32 boot_cmd = ES_SLIM_BOOT_CMD;
	u32 boot_ack;
	int rc;

	pr_debug("%s()\n", __func__);
	pr_debug("%s(): write ES_BOOT_CMD = 0x%08x\n", __func__, boot_cmd);

	rc = escore_slim_cmd(escore, boot_cmd, &boot_ack);
	if (rc < 0) {
		pr_err("%s(): firmware load failed boot slim cmd %d\n",
		       __func__, rc);
		goto escore_bootup_failed;
	}
	pr_debug("%s(): boot_ack = 0x%08x\n", __func__, boot_ack);
	if (boot_ack != ES_SLIM_BOOT_ACK) {
		pr_err("%s(): firmware load failed boot ack pattern (0x%x)",
		       __func__, boot_ack);
		rc = -EIO;
		goto escore_bootup_failed;
	}

escore_bootup_failed:
	return rc;
}

int escore_slim_boot_finish(struct escore_priv *escore)
{
	u32 sync_cmd;
	u32 sync_ack;
	int rc = 0;
	int sync_retry = ES_SYNC_MAX_RETRY;

	dev_dbg(escore->dev, "%s(): finish fw download\n", __func__);

	if (escore->mode == VOICESENSE) {
		sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_INTR_RISING_EDGE;
		dev_dbg(escore->dev, "%s(): FW type : VOICESENSE\n", __func__);
	} else {
		sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_POLLING;
		dev_dbg(escore->dev, "%s(): fw type : STANDARD\n", __func__);
	}
	sync_ack = sync_cmd;

	/* sometimes earSmart chip sends success in second sync command */
	do {
		pr_debug("%s(): write ES_SYNC_CMD = 0x%08x\n",
				__func__, sync_cmd);
		rc = escore_slim_cmd(escore, sync_cmd, &sync_ack);
		if (rc < 0) {
			pr_err("%s(): firmware load failed sync write %d\n",
			       __func__, rc);
			continue;
		}
		pr_debug("%s(): sync_ack = 0x%08x\n", __func__, sync_ack);
		if (sync_ack != sync_cmd) {
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

void escore_slim_get_logical_addr(struct escore_priv *escore)
{
	do {
		slim_get_logical_addr(escore->gen0_client,
				      escore->gen0_client->e_addr,
				      6, &(escore->gen0_client->laddr));
		usleep_range(1000, 2000);
	} while (escore->gen0_client->laddr == 0xf0);
	dev_dbg(&escore->gen0_client->dev,
		"%s(): gen0_client LA = %d\n",
		 __func__, escore->gen0_client->laddr);
}

static int escore_dt_parse_slim_dev_info(struct slim_device *slim_gen,
		struct slim_device *slim_ifd)
{
	struct property *prop;
	u8 *e_addr;
	struct device *dev = &slim_gen->dev;
	int rc;

	dev_dbg(dev, "%s()\n", __func__);
	prop = of_find_property(dev->of_node,
			escore_priv.device_name, NULL);
	if (!prop) {
		dev_err(dev, "Looking up %s property in node %s failed",
				"elemental-addr",
				dev->of_node->full_name);
		return -ENODEV;
	} else if (prop->length != 6) {
		dev_err(dev, "invalid codec slim addr. addr length = %d\n",
				prop->length);
		return -ENODEV;
	}
	memcpy(slim_gen->e_addr, prop->value, 6);

	e_addr = (u8 *)prop->value;
	dev_dbg(dev, "eaddr:%02x %02x %02x %02x %02x %02x\n",
			e_addr[0], e_addr[1], e_addr[2], e_addr[3], e_addr[4],
			e_addr[5]);

	/*Get default LA*/
	prop = of_find_property(dev->of_node,
			"adnc,laddr", NULL);
	e_addr = (u8 *)prop->value;
	if (!prop) {
		dev_err(dev, "Looking up %s property in node %s failed",
				"elemental-addr",
				dev->of_node->full_name);
		return -ENODEV;
	}

	slim_gen->laddr = e_addr[3];
	dev_dbg(dev, "defualt LA :  %2x\n", slim_gen->laddr);

	rc = of_property_read_string(dev->of_node,
				 escore_priv.interface_device_name,
				 &slim_ifd->name);
	if (rc < 0) {
		dev_err(dev, "%s(): %s fail (%d) for %s", __func__,
			dev->of_node->full_name, rc, slim_ifd->name);
		return rc;
	}

	prop = of_find_property(dev->of_node,
			       escore_priv.interface_device_elem_addr_name,
			       NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;
	if (prop->length < sizeof(slim_ifd->e_addr))
		return -EOVERFLOW;

	memcpy(slim_ifd->e_addr, prop->value, 6);
	e_addr = (u8 *)prop->value;
	dev_dbg(dev, "slim ifd eaddr:%02x %02x %02x %02x %02x %02x\n",
		e_addr[0], e_addr[1], e_addr[2], e_addr[3], e_addr[4],
		e_addr[5]);

	return 0;
}

static void escore_slim_setup_pri_intf(struct escore_priv *escore)
{
	escore->bus.ops.read = escore_slim_read;
	escore->bus.ops.write = escore_slim_write;
	escore->bus.ops.cmd = escore_slim_cmd;
	escore->bus.ops.cpu_to_bus = escore_cpu_to_slim;
	escore->bus.ops.bus_to_cpu = escore_slim_to_cpu;

#ifndef CONFIG_SLIMBUS_MSM_NGD
	/* Fix-Me: Board may not be ready by now. so this may fail */
	escore_slim_get_logical_addr(escore);
#endif

	escore->init_slim_slave = escore_init_slim_slave;
}

static int escore_slim_setup_high_bw_intf(struct escore_priv *escore)
{
	int rc;

	escore->bus.ops.high_bw_write = escore_slim_write;
	escore->bus.ops.high_bw_read = escore_slim_read;
	escore->bus.ops.high_bw_cmd = escore_slim_cmd;
	escore->boot_ops.setup = escore_slim_boot_setup;
	escore->boot_ops.finish = escore_slim_boot_finish;
	escore->bus.ops.cpu_to_high_bw_bus = escore_cpu_to_slim;
	escore->bus.ops.high_bw_bus_to_cpu = escore_slim_to_cpu;


	rc = escore->probe(&escore->gen0_client->dev);
	if (rc)
		goto out;

	mutex_lock(&escore->access_lock);
	rc = escore->boot_ops.bootup(escore);
	mutex_unlock(&escore->access_lock);

out:

	return rc;
}


/*
 * This is a duplicate of code from drivers/slimbus/slimbus.c.
 * However, this function is static there and cannot be shared.
 * Since existing slimbus stack is not supposed to be modified, this
 * code is duplicated here.
 */
static const struct slim_device_id *escore_slim_match(
				const struct slim_device_id *id,
				const struct slim_device *slim_dev)
{
	while (id->name[0]) {
		if (strncmp(slim_dev->name, id->name,
						SLIMBUS_NAME_SIZE) == 0)
			return id;
		id++;
	}
	return NULL;
}

static int populate_and_trigger_reset(struct device *dev)
{
	int reset_gpio;
	struct escore_priv *escore = &escore_priv;
	int rc = 0;

	if (escore->flag.reset_done) {
		dev_dbg(dev, "%s(): Reset done already\n", __func__);
		return rc;
	}
	reset_gpio = of_get_named_gpio(dev->of_node, "adnc,reset-gpio", 0);
	if (reset_gpio < 0) {
		dev_err(dev, "%s(): get reset_gpio failed\n", __func__);
		rc = -1;
		return rc;
	}
	dev_info(dev, "%s(): **** reset gpio %d\n", __func__, reset_gpio);

	rc = gpio_request(reset_gpio, "escore_reset");
	if (rc < 0) {
		dev_warn(dev, "%s(): escore_reset already requested",
				__func__);
	} else {
		rc = gpio_direction_output(reset_gpio, 0);
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): es705_reset direction fail %d\n",
				__func__, rc);
			return rc;
		}
		gpio_set_value(reset_gpio, 0);
		/* Wait 1 ms then pull Reset signal in High */
		usleep_range(1000, 1005);
		gpio_set_value(reset_gpio, 1);
		/* Wait 10 ms then */
		usleep_range(10000, 10050);
		gpio_free(reset_gpio);
		escore->flag.reset_done = 1;
	}

	return rc;
}

static int escore_slim_probe(struct slim_device *sbdev)
{
	struct escore_priv *escore = &escore_priv;
	const struct slim_driver *sdrv;
	const struct slim_device_id *id;
	static struct slim_device intf_dev;
	int rc = 0;

	dev_dbg(&sbdev->dev, "%s(): sbdev->name = %s\n", __func__, sbdev->name);

#ifdef CONFIG_SLIMBUS_MSM_NGD
	mutex_lock(&slimbus_fw_load_mutex);

	if (escore->device_up_called) {
		dev_dbg(&sbdev->dev, "Device up already called\n");
		goto out;
	}
#endif

	if (sbdev->dev.of_node) {
		rc = populate_and_trigger_reset(&sbdev->dev);
		if (rc)	{
			dev_err(&sbdev->dev, "%s(): chip reset failed : %d\n",
					__func__, rc);
			goto escore_core_probe_error;
		}
	}
	sdrv = to_slim_driver(sbdev->dev.driver);
	id = (sdrv ? escore_slim_match(sdrv->id_table, sbdev) : NULL);

	if (id->driver_data == ESCORE_GENERIC_DEVICE) {
		dev_dbg(&sbdev->dev, "%s(): generic device probe\n",
			__func__);
		escore_priv.gen0_client = sbdev;
		slim_set_clientdata(sbdev, &escore_priv);
	}

	if (!sbdev->dev.of_node) {
		if (id->driver_data == ESCORE_INTERFACE_DEVICE) {
			dev_dbg(&sbdev->dev, "%s(): interface device probe\n",
					__func__);
			escore_priv.intf_client = sbdev;
			slim_set_clientdata(sbdev, &escore_priv);
		}
	} else {
		escore_priv.intf_client = &intf_dev;
	}

	if (escore_priv.intf_client == NULL ||
	    escore_priv.gen0_client == NULL) {
		dev_dbg(&sbdev->dev, "%s() incomplete initialization\n",
			__func__);
		goto out;
	}

	if (escore_priv.gen0_client->dev.of_node) {
		rc = escore_dt_parse_slim_dev_info(escore_priv.gen0_client,
				escore_priv.intf_client);
		if (rc < 0)
			goto escore_core_probe_error;
	}



	if (escore->pri_intf == ES_SLIM_INTF)
		escore->bus.setup_prim_intf = escore_slim_setup_pri_intf;
	if (escore->high_bw_intf == ES_SLIM_INTF)
		escore->bus.setup_high_bw_intf = escore_slim_setup_high_bw_intf;

#ifndef CONFIG_SLIMBUS_MSM_NGD
	rc = escore_probe(&escore_priv, &escore_priv.gen0_client->dev,
			ES_SLIM_INTF, ES_CONTEXT_PROBE);
#endif

#ifdef CONFIG_SLIMBUS_MSM_NGD
	mutex_unlock(&slimbus_fw_load_mutex);
#endif
	return rc;

escore_core_probe_error:
	dev_err(&sbdev->dev, "%s(): exit with error, rc = %d\n", __func__, rc);
out:
#ifdef CONFIG_SLIMBUS_MSM_NGD
	mutex_unlock(&slimbus_fw_load_mutex);
#endif
	return rc;
}

static int escore_slim_remove(struct slim_device *sbdev)
{
	dev_dbg(&sbdev->dev, "%s(): sbdev->name = %s\n", __func__, sbdev->name);

	snd_soc_unregister_codec(&sbdev->dev);

	return 0;
}

#ifdef CONFIG_SLIMBUS_MSM_NGD
static int escore_slim_device_up(struct slim_device *sbdev)
{
	const struct slim_driver *sdrv;
	const struct slim_device_id *id;
	int rc = 0;
	dev_dbg(&sbdev->dev, "%s: name=%s laddr=%d\n",
			__func__, sbdev->name, sbdev->laddr);

	sdrv = to_slim_driver(sbdev->dev.driver);
	id = (sdrv ? escore_slim_match(sdrv->id_table, sbdev) : NULL);

	mutex_lock(&slimbus_fw_load_mutex);
	/*
	 * Allow device-up handling only for Generic device.
	 * This is because only Generic device node's contains the
	 * required DT properties.
	 */
	if (id && id->driver_data) {
		if (id->driver_data == ESCORE_GENERIC_DEVICE) {

			slim_set_clientdata(sbdev, &escore_priv);
			escore_priv.gen0_client = sbdev;
			escore_priv.device_up_called = 1;
			device_rename(&sbdev->dev, "earSmart-codec-gen0");
			rc = escore_probe(&escore_priv,
					&escore_priv.gen0_client->dev,
					ES_SLIM_INTF, ES_CONTEXT_THREAD);
			if (rc)
				dev_err(&sbdev->dev, "%s(): error, rc = %d\n",
						__func__, rc);
		}
	}
	mutex_unlock(&slimbus_fw_load_mutex);
	return rc;
}
#endif
struct slim_driver escore_slim_driver = {
	.driver = {
		.name = "earSmart-codec",
		.owner = THIS_MODULE,
		.pm = &escore_pm_ops,
	},
	.probe = escore_slim_probe,
	.remove = escore_slim_remove,
#ifdef CONFIG_SLIMBUS_MSM_NGD
	.device_up = escore_slim_device_up,
#endif
	.id_table = escore_slim_id,
};

int escore_slimbus_init()
{
	int rc;

	rc = slim_driver_register(&escore_slim_driver);
	if (!rc)
		pr_debug("%s() registered as SLIMBUS", __func__);

#ifdef CONFIG_SLIMBUS_MSM_NGD
	mutex_init(&slimbus_fw_load_mutex);
#endif

	return rc;
}

MODULE_DESCRIPTION("ASoC ES driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:escore-codec");
