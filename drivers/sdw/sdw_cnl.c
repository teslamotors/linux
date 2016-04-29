/*
 *  sdw_cnl.c - Intel SoundWire master controller driver implementation.
 *
 *  Copyright (C) 2015-2016 Intel Corp
 *  Author:  Hardik T Shah <hardik.t.shah@intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/idr.h>
#include <linux/rtmutex.h>
#include <linux/pm_runtime.h>
#include <linux/pm.h>
#include <linux/mod_devicetable.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/sdw_bus.h>
#include <linux/sdw/sdw_registers.h>
#include <linux/sdw/sdw_cnl.h>
#include "sdw_cnl_priv.h"

static inline int cnl_sdw_reg_readl(void __iomem *base, int offset)
{
	int value;

	value = readl(base + offset);
	return value;
}

static inline void cnl_sdw_reg_writel(void __iomem *base, int offset, int value)
{
	writel(value, base + offset);
}

static inline u16 cnl_sdw_reg_readw(void __iomem *base, int offset)
{
	int value;

	value = readw(base + offset);
	return value;
}

static inline void cnl_sdw_reg_writew(void __iomem *base, int offset, u16 value)
{
	writew(value, base + offset);
}

static inline int cnl_sdw_port_reg_readl(void __iomem *base, int offset,
						int port_num)
{
	return cnl_sdw_reg_readl(base, offset + port_num * 128);
}

static inline void cnl_sdw_port_reg_writel(u32 __iomem *base, int offset,
						int port_num, int value)
{
	return cnl_sdw_reg_writel(base, offset + port_num * 128, value);
}

struct cnl_sdw_async_msg {
	struct completion *async_xfer_complete;
	struct sdw_msg *msg;
	int length;
};

struct cnl_sdw {
	struct cnl_sdw_data data;
	struct sdw_master *mstr;
	irqreturn_t (*thread)(int irq, void *context);
	void *thread_context;
	struct completion tx_complete;
	struct cnl_sdw_port port[CNL_SDW_MAX_PORTS];
	int num_pcm_streams;
	struct cnl_sdw_pdi_stream *pcm_streams;
	int num_in_pcm_streams;
	struct cnl_sdw_pdi_stream *in_pcm_streams;
	int num_out_pcm_streams;
	struct cnl_sdw_pdi_stream *out_pcm_streams;
	int num_pdm_streams;
	struct cnl_sdw_pdi_stream *pdm_streams;
	int num_in_pdm_streams;
	struct cnl_sdw_pdi_stream *in_pdm_streams;
	int num_out_pdm_streams;
	struct cnl_sdw_pdi_stream *out_pdm_streams;
	struct mutex	stream_lock;
	spinlock_t ctrl_lock;
	struct cnl_sdw_async_msg async_msg;
	u32 response_buf[0x80];
	bool sdw_link_status;

};

static int sdw_power_up_link(struct cnl_sdw *sdw)
{
	volatile int link_control;
	struct sdw_master *mstr = sdw->mstr;
	struct cnl_sdw_data *data = &sdw->data;
	/* Try 10 times before timing out */
	int timeout = 10;
	int spa_mask, cpa_mask;

	link_control = cnl_sdw_reg_readl(data->sdw_shim, SDW_CNL_LCTL);
	spa_mask = (CNL_LCTL_SPA_MASK << (data->inst_id + CNL_LCTL_SPA_SHIFT));
	cpa_mask = (CNL_LCTL_CPA_MASK << (data->inst_id + CNL_LCTL_CPA_SHIFT));
	link_control |=  spa_mask;
	cnl_sdw_reg_writel(data->sdw_shim, SDW_CNL_LCTL, link_control);
	do {
		link_control = cnl_sdw_reg_readl(data->sdw_shim, SDW_CNL_LCTL);
		if (link_control & cpa_mask)
			break;
		timeout--;
		/* Wait 20ms before each time */
		msleep(20);
	} while (timeout != 0);
	/* Read once again to confirm */
	link_control = cnl_sdw_reg_readl(data->sdw_shim, SDW_CNL_LCTL);
	if (link_control & cpa_mask) {
		dev_info(&mstr->dev, "SoundWire ctrl %d Powered Up\n",
						data->inst_id);
		sdw->sdw_link_status = 1;
		return 0;
	}
	dev_err(&mstr->dev, "Failed to Power Up the SDW ctrl %d\n",
								data->inst_id);
	return -EIO;
}

static void sdw_power_down_link(struct cnl_sdw *sdw)
{
	volatile int link_control;
	struct sdw_master *mstr = sdw->mstr;
	struct cnl_sdw_data *data = &sdw->data;
	/* Retry 10 times before giving up */
	int timeout = 10;
	int spa_mask, cpa_mask;

	link_control = cnl_sdw_reg_readl(data->sdw_shim, SDW_CNL_LCTL);
	spa_mask = ~(CNL_LCTL_SPA_MASK << (data->inst_id + CNL_LCTL_SPA_SHIFT));
	cpa_mask = (CNL_LCTL_CPA_MASK << (data->inst_id + CNL_LCTL_CPA_SHIFT));
	link_control &=  spa_mask;
	cnl_sdw_reg_writel(data->sdw_shim, SDW_CNL_LCTL, link_control);
	do {
		link_control = cnl_sdw_reg_readl(data->sdw_shim, SDW_CNL_LCTL);
		if (!(link_control & cpa_mask))
			break;
		timeout--;
		/* Wait for 20ms before each retry */
		msleep(20);
	} while (timeout != 0);
	/* Read once again to confirm */
	link_control = cnl_sdw_reg_readl(data->sdw_shim, SDW_CNL_LCTL);
	if (!(link_control & cpa_mask)) {
		dev_info(&mstr->dev, "SoundWire ctrl %d Powered Down\n",
						data->inst_id);
		sdw->sdw_link_status = 0;
		return;
	}
	dev_err(&mstr->dev, "Failed to Power Down the SDW ctrl %d\n",
								data->inst_id);
}

static void sdw_init_phyctrl(struct cnl_sdw *sdw)
{
	/* TODO: Initialize based on hardware requirement */

}

static void sdw_switch_to_mip(struct cnl_sdw *sdw)
{
	u16 ioctl;
	u16 act = 0;
	struct cnl_sdw_data *data = &sdw->data;
	int ioctl_offset = SDW_CNL_IOCTL + (data->inst_id *
					SDW_CNL_IOCTL_REG_OFFSET);
	int act_offset = SDW_CNL_CTMCTL + (data->inst_id *
					SDW_CNL_CTMCTL_REG_OFFSET);

	ioctl = cnl_sdw_reg_readw(data->sdw_shim,  ioctl_offset);

	ioctl &= ~(CNL_IOCTL_DOE_MASK << CNL_IOCTL_DOE_SHIFT);
	cnl_sdw_reg_writew(data->sdw_shim,  ioctl_offset, ioctl);

	ioctl &= ~(CNL_IOCTL_DO_MASK << CNL_IOCTL_DO_SHIFT);
	cnl_sdw_reg_writew(data->sdw_shim,  ioctl_offset, ioctl);

	ioctl |= CNL_IOCTL_MIF_MASK << CNL_IOCTL_MIF_SHIFT;
	cnl_sdw_reg_writew(data->sdw_shim,  ioctl_offset, ioctl);

	ioctl &= ~(CNL_IOCTL_BKE_MASK << CNL_IOCTL_BKE_SHIFT);
	ioctl &= ~(CNL_IOCTL_COE_MASK << CNL_IOCTL_COE_SHIFT);

	cnl_sdw_reg_writew(data->sdw_shim,  ioctl_offset, ioctl);

	act |= 0x1 << CNL_CTMCTL_DOAIS_SHIFT;
	act |= CNL_CTMCTL_DACTQE_MASK << CNL_CTMCTL_DACTQE_SHIFT;
	act |= CNL_CTMCTL_DODS_MASK << CNL_CTMCTL_DODS_SHIFT;
	cnl_sdw_reg_writew(data->sdw_shim,  act_offset, act);
}

static void sdw_switch_to_glue(struct cnl_sdw *sdw)
{
	u16 ioctl;
	struct cnl_sdw_data *data = &sdw->data;
	int ioctl_offset = SDW_CNL_IOCTL + (data->inst_id *
					SDW_CNL_IOCTL_REG_OFFSET);

	ioctl = cnl_sdw_reg_readw(data->sdw_shim,  ioctl_offset);
	ioctl |= CNL_IOCTL_BKE_MASK << CNL_IOCTL_BKE_SHIFT;
	ioctl |= CNL_IOCTL_COE_MASK << CNL_IOCTL_COE_SHIFT;
	cnl_sdw_reg_writew(data->sdw_shim,  ioctl_offset, ioctl);

	ioctl &= ~(CNL_IOCTL_MIF_MASK << CNL_IOCTL_MIF_SHIFT);
	cnl_sdw_reg_writew(data->sdw_shim,  ioctl_offset, ioctl);
}

static void sdw_init_shim(struct cnl_sdw *sdw)
{
	u16 ioctl = 0;
	struct cnl_sdw_data *data = &sdw->data;
	int ioctl_offset = SDW_CNL_IOCTL + (data->inst_id *
					SDW_CNL_IOCTL_REG_OFFSET);


	ioctl |= CNL_IOCTL_BKE_MASK << CNL_IOCTL_BKE_SHIFT;
	cnl_sdw_reg_writew(data->sdw_shim,  ioctl_offset, ioctl);

	ioctl |= CNL_IOCTL_WPDD_MASK << CNL_IOCTL_WPDD_SHIFT;
	cnl_sdw_reg_writew(data->sdw_shim,  ioctl_offset, ioctl);

	ioctl |= CNL_IOCTL_DO_MASK << CNL_IOCTL_DO_SHIFT;
	cnl_sdw_reg_writew(data->sdw_shim,  ioctl_offset, ioctl);

	ioctl |= CNL_IOCTL_DOE_MASK << CNL_IOCTL_DOE_SHIFT;
	cnl_sdw_reg_writew(data->sdw_shim,  ioctl_offset, ioctl);
}

static int sdw_config_update(struct cnl_sdw *sdw)
{
	struct cnl_sdw_data *data = &sdw->data;
	struct sdw_master *mstr = sdw->mstr;

	volatile int config_update = 0;
	/* Try 10 times before giving up on configuration update */
	int timeout = 10;
	int config_updated = 0;

	config_update |= MCP_CONFIGUPDATE_CONFIGUPDATE_MASK <<
				MCP_CONFIGUPDATE_CONFIGUPDATE_SHIFT;
	/* Bit is self-cleared when configuration gets updated. */
	cnl_sdw_reg_writel(data->sdw_regs,  SDW_CNL_MCP_CONFIGUPDATE,
			config_update);
	do {
		config_update = cnl_sdw_reg_readl(data->sdw_regs,
				SDW_CNL_MCP_CONFIGUPDATE);
		if ((config_update &
				MCP_CONFIGUPDATE_CONFIGUPDATE_MASK) == 0) {
			config_updated = 1;
			break;
		}
		timeout--;
		/* Wait for 20ms between each try */
		msleep(20);

	} while (timeout != 0);
	if (!config_updated) {
		dev_err(&mstr->dev, "SoundWire update failed\n");
		return -EIO;
	}
	return 0;
}

static void sdw_enable_interrupt(struct cnl_sdw *sdw)
{
	struct cnl_sdw_data *data = &sdw->data;
	int int_mask = 0;

	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_SLAVEINTMASK0,
						MCP_SLAVEINTMASK0_MASK);
	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_SLAVEINTMASK1,
						MCP_SLAVEINTMASK1_MASK);
	/* Enable slave interrupt mask */
	int_mask |= MCP_INTMASK_SLAVERESERVED_MASK <<
				MCP_INTMASK_SLAVERESERVED_SHIFT;
	int_mask |= MCP_INTMASK_SLAVEALERT_MASK <<
				MCP_INTMASK_SLAVEALERT_SHIFT;
	int_mask |= MCP_INTMASK_SLAVEATTACHED_MASK <<
				MCP_INTMASK_SLAVEATTACHED_SHIFT;
	int_mask |= MCP_INTMASK_SLAVENOTATTACHED_MASK <<
				MCP_INTMASK_SLAVENOTATTACHED_SHIFT;
	int_mask |= MCP_INTMASK_CONTROLBUSCLASH_MASK <<
				MCP_INTMASK_CONTROLBUSCLASH_SHIFT;
	int_mask |= MCP_INTMASK_DATABUSCLASH_MASK <<
				MCP_INTMASK_DATABUSCLASH_SHIFT;
	int_mask |= MCP_INTMASK_RXWL_MASK <<
				MCP_INTMASK_RXWL_SHIFT;
	int_mask |= MCP_INTMASK_IRQEN_MASK <<
				MCP_INTMASK_IRQEN_SHIFT;
	int_mask |= MCP_INTMASK_DPPDIINT_MASK <<
				MCP_INTMASK_DPPDIINT_SHIFT;
	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_INTMASK, int_mask);
}

static int sdw_pcm_pdi_init(struct cnl_sdw *sdw)
{
	struct sdw_master *mstr = sdw->mstr;
	struct cnl_sdw_data *data = &sdw->data;
	int pcm_cap;
	int pcm_cap_offset = SDW_CNL_PCMSCAP + (data->inst_id *
					SDW_CNL_PCMSCAP_REG_OFFSET);
	int ch_cnt_offset;
	int i;

	pcm_cap = cnl_sdw_reg_readw(data->sdw_shim, pcm_cap_offset);
	sdw->num_pcm_streams = (pcm_cap >> CNL_PCMSCAP_BSS_SHIFT) &
			CNL_PCMSCAP_BSS_MASK;
	dev_info(&mstr->dev, "Number of Bidirectional PCM stream = %d\n",
			sdw->num_pcm_streams);
	sdw->pcm_streams = devm_kzalloc(&mstr->dev,
		sdw->num_pcm_streams * sizeof(struct cnl_sdw_pdi_stream),
		GFP_KERNEL);
	if (!sdw->pcm_streams)
		return -ENOMEM;
	/* Two of the PCM streams are reserved for bulk transfers */
	sdw->pcm_streams -= SDW_CNL_PCM_PDI_NUM_OFFSET;
	for (i = SDW_CNL_PCM_PDI_NUM_OFFSET; i < sdw->num_pcm_streams; i++) {
		ch_cnt_offset = SDW_CNL_PCMSCHC +
			(data->inst_id * SDW_CNL_PCMSCHC_REG_OFFSET) +
			((i + SDW_CNL_PCM_PDI_NUM_OFFSET) * 0x2);

		sdw->pcm_streams[i].ch_cnt = cnl_sdw_reg_readw(data->sdw_shim,
						ch_cnt_offset);
		/* Zero based value in register */
		sdw->pcm_streams[i].ch_cnt++;
		sdw->pcm_streams[i].pdi_num = i;
		sdw->pcm_streams[i].allocated = false;
		dev_info(&mstr->dev, "CH Count for stream %d is %d\n",
			i, sdw->pcm_streams[i].ch_cnt);
	}
	return 0;
}

static int sdw_pdm_pdi_init(struct cnl_sdw *sdw)
{
	int i;
	struct sdw_master *mstr = sdw->mstr;
	struct cnl_sdw_data *data = &sdw->data;
	int pdm_cap, pdm_ch_count, total_pdm_streams;
	int pdm_cap_offset = SDW_CNL_PDMSCAP +
			(data->inst_id * SDW_CNL_PDMSCAP_REG_OFFSET);

	pdm_cap = cnl_sdw_reg_readw(data->sdw_regs, pdm_cap_offset);
	sdw->num_pdm_streams = (pdm_cap >> CNL_PDMSCAP_BSS_SHIFT) &
			CNL_PDMSCAP_BSS_MASK;
	/* Zero based value in register */
	sdw->num_pdm_streams++;
	sdw->pdm_streams = devm_kzalloc(&mstr->dev,
		sdw->num_pdm_streams * sizeof(struct cnl_sdw_pdi_stream),
		GFP_KERNEL);
	if (!sdw->pdm_streams)
		return -ENOMEM;

	sdw->num_in_pdm_streams = (pdm_cap >> CNL_PDMSCAP_ISS_SHIFT) &
			CNL_PDMSCAP_ISS_MASK;
	/* Zero based value in register */
	sdw->num_in_pdm_streams++;
	sdw->in_pdm_streams = devm_kzalloc(&mstr->dev,
		sdw->num_in_pdm_streams * sizeof(struct cnl_sdw_pdi_stream),
		GFP_KERNEL);

	if (!sdw->in_pdm_streams)
		return -ENOMEM;

	sdw->num_out_pdm_streams = (pdm_cap >> CNL_PDMSCAP_OSS_SHIFT) &
			CNL_PDMSCAP_OSS_MASK;
	/* Zero based value in register */
	sdw->num_out_pdm_streams++;
	sdw->out_pdm_streams = devm_kzalloc(&mstr->dev,
		sdw->num_out_pdm_streams * sizeof(struct cnl_sdw_pdi_stream),
		GFP_KERNEL);
	if (!sdw->out_pdm_streams)
		return -ENOMEM;

	total_pdm_streams = sdw->num_pdm_streams +
			sdw->num_in_pdm_streams +
			sdw->num_out_pdm_streams;

	pdm_ch_count = (pdm_cap >> CNL_PDMSCAP_CPSS_SHIFT) &
				CNL_PDMSCAP_CPSS_MASK;
	for (i = 0; i < sdw->num_pdm_streams; i++) {
		sdw->pdm_streams[i].ch_cnt = pdm_ch_count;
		sdw->pdm_streams[i].pdi_num = i + SDW_CNL_PDM_PDI_NUM_OFFSET;
		sdw->pdm_streams[i].allocated = false;
	}
	for (i = 0; i < sdw->num_in_pdm_streams; i++) {
		sdw->in_pdm_streams[i].ch_cnt = pdm_ch_count;
		sdw->in_pdm_streams[i].pdi_num = i + SDW_CNL_PDM_PDI_NUM_OFFSET;
		sdw->in_pdm_streams[i].allocated = false;
	}
	for (i = 0; i < sdw->num_out_pdm_streams; i++) {
		sdw->out_pdm_streams[i].ch_cnt = pdm_ch_count;
		sdw->out_pdm_streams[i].pdi_num =
					i + SDW_CNL_PDM_PDI_NUM_OFFSET;
		sdw->out_pdm_streams[i].allocated = false;
	}
	return 0;
}

static int sdw_port_pdi_init(struct cnl_sdw *sdw)
{
	int i, ret = 0;

	for (i = 0; i < CNL_SDW_MAX_PORTS; i++) {
		sdw->port[i].port_num = i;
		sdw->port[i].allocated = false;
	}
	ret = sdw_pcm_pdi_init(sdw);
	if (ret)
		return ret;
	ret = sdw_pdm_pdi_init(sdw);

	return ret;
}

static int sdw_init(struct cnl_sdw *sdw)
{
	struct sdw_master *mstr = sdw->mstr;
	struct cnl_sdw_data *data = &sdw->data;
	int mcp_config, mcp_control, sync_reg;

	volatile int sync_update = 0;
	/* Try 10 times before timing out */
	int timeout = 10;
	int ret = 0;

	/* Power up the link controller */
	ret = sdw_power_up_link(sdw);
	if (ret)
		return ret;

	/* Initialize the IO control registers */
	sdw_init_shim(sdw);

	/* Switch the ownership to Master IP from glue logic */
	sdw_switch_to_mip(sdw);

	/* Set the Sync period to default */
	sync_reg = cnl_sdw_reg_readl(data->sdw_shim,  SDW_CNL_SYNC);
	sync_reg |= (SDW_CNL_DEFAULT_SYNC_PERIOD << CNL_SYNC_SYNCPRD_SHIFT);
	sync_reg |= (0x1 << CNL_SYNC_SYNCCPU_SHIFT);
	cnl_sdw_reg_writel(data->sdw_shim, SDW_CNL_SYNC, sync_reg);

	do {
		sync_update = cnl_sdw_reg_readl(data->sdw_shim,  SDW_CNL_SYNC);
		if ((sync_update & CNL_SYNC_SYNCCPU_MASK) == 0)
			break;
		timeout--;
		/* Wait 20ms before each time */
		msleep(20);
	} while (timeout != 0);
	if ((sync_update & CNL_SYNC_SYNCCPU_MASK) != 0) {
		dev_err(&mstr->dev, "Fail to set sync period\n");
		return -EINVAL;
	}

	/* Set command acceptance mode. This is required because when
	 * Master broadcasts the clock_stop command to slaves, slaves
	 * might be already suspended, so this return NO ACK, in that
	 * case also master should go to clock stop mode.
	 */
	mcp_control = cnl_sdw_reg_readl(data->sdw_regs,
					SDW_CNL_MCP_CONTROL);
	mcp_control |= (MCP_CONTROL_CMDACCEPTMODE_MASK <<
			MCP_CONTROL_CMDACCEPTMODE_SHIFT);
	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_CONTROL, mcp_control);

	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_FRAMESHAPEINIT, 0x48);

	mcp_config = cnl_sdw_reg_readl(data->sdw_regs, SDW_CNL_MCP_CONFIG);
	/* Set Max cmd retry to 15 times */
	mcp_config |= (CNL_SDW_MAX_CMD_RETRIES <<
				MCP_CONFIG_MAXCMDRETRY_SHIFT);

	/* Set Ping request to ping delay to 15 frames.
	 * Spec supports 32 max frames
	 */
	mcp_config |= (CNL_SDW_MAX_PREQ_DELAY <<
					MCP_CONFIG_MAXPREQDELAY_SHIFT);

	/* If master is synchronized to some other master set Multimode */
	if (mstr->link_sync_mask) {
		mcp_config |= (MCP_CONFIG_MMMODEEN_MASK <<
						MCP_CONFIG_MMMODEEN_SHIFT);
		mcp_config |= (MCP_CONFIG_SSPMODE_MASK <<
						MCP_CONFIG_SSPMODE_SHIFT);
	} else {
		mcp_config &= ~(MCP_CONFIG_MMMODEEN_MASK <<
						MCP_CONFIG_MMMODEEN_SHIFT);
		mcp_config &= ~(MCP_CONFIG_SSPMODE_MASK <<
						MCP_CONFIG_SSPMODE_SHIFT);
	}

	/* Disable automatic bus release */
	mcp_config &= ~(MCP_CONFIG_BRELENABLE_MASK <<
				MCP_CONFIG_BRELENABLE_SHIFT);

	/* Disable sniffer mode now */
	mcp_config &= ~(MCP_CONFIG_SNIFFEREN_MASK <<
				MCP_CONFIG_SNIFFEREN_SHIFT);

	/* Set the command mode for Tx and Rx command */
	mcp_config &= ~(MCP_CONFIG_CMDMODE_MASK <<
				MCP_CONFIG_CMDMODE_SHIFT);

	/* Set operation mode to normal */
	mcp_config &= ~(MCP_CONFIG_OPERATIONMODE_MASK <<
				MCP_CONFIG_OPERATIONMODE_SHIFT);
	mcp_config |= ((MCP_CONFIG_OPERATIONMODE_NORMAL &
			MCP_CONFIG_OPERATIONMODE_MASK) <<
			MCP_CONFIG_OPERATIONMODE_SHIFT);

	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_CONFIG, mcp_config);
	/* Set the SSP interval to 32 for both banks */
	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_SSPCTRL0,
					SDW_CNL_DEFAULT_SSP_INTERVAL);
	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_SSPCTRL1,
					SDW_CNL_DEFAULT_SSP_INTERVAL);

	/* Initialize the phy control registers. */
	sdw_init_phyctrl(sdw);

	/* Initlaize the ports */
	ret = sdw_port_pdi_init(sdw);
	if (ret) {
		dev_err(&mstr->dev, "SoundWire controller init failed %d\n",
				data->inst_id);
		sdw_power_down_link(sdw);
		return ret;
	}

	/* Lastly enable interrupts */
	sdw_enable_interrupt(sdw);

	/* Update soundwire configuration */
	return sdw_config_update(sdw);
}

static int sdw_alloc_pcm_stream(struct cnl_sdw *sdw,
			struct cnl_sdw_port *port, int ch_cnt,
			enum sdw_data_direction direction)
{
	int num_pcm_streams, pdi_ch_map = 0, stream_id;
	struct cnl_sdw_pdi_stream *stream, *pdi_stream;
	unsigned int i;
	unsigned int ch_map_offset, port_ctrl_offset, pdi_config_offset;
	struct sdw_master *mstr = sdw->mstr;
	unsigned int port_ctrl = 0, pdi_config = 0, channel_mask;
	unsigned int stream_config;

	/* Currently PCM supports only bi-directional streams only */
	num_pcm_streams = sdw->num_pcm_streams;
	stream = sdw->pcm_streams;

	mutex_lock(&sdw->stream_lock);
	for (i = SDW_CNL_PCM_PDI_NUM_OFFSET; i < num_pcm_streams; i++) {
		if (stream[i].allocated == false) {
			stream[i].allocated = true;
			stream[i].port_num = port->port_num;
			port->pdi_stream = &stream[i];
			break;
		}
	}
	mutex_unlock(&sdw->stream_lock);
	if (!port->pdi_stream) {
		dev_err(&mstr->dev, "Unable to allocate stream for PCM\n");
		return -EINVAL;
	}
	pdi_stream = port->pdi_stream;
	/* We didnt get enough PDI streams, so free the allocated
	 * PDI streams. Free the port as well and return with error
	 */
	pdi_stream->l_ch_num = 0;
	pdi_stream->h_ch_num = ch_cnt - 1;
	ch_map_offset = SDW_CNL_PCMSCHM +
			(SDW_CNL_PCMSCHM_REG_OFFSET * mstr->nr) +
			(0x2 * pdi_stream->pdi_num);
	if (port->direction == SDW_DATA_DIR_IN)
		pdi_ch_map |= (CNL_PCMSYCM_DIR_MASK << CNL_PCMSYCM_DIR_SHIFT);
	else
		pdi_ch_map &= ~(CNL_PCMSYCM_DIR_MASK << CNL_PCMSYCM_DIR_SHIFT);
	/* TODO: Remove this hardcoding */
	stream_id = mstr->nr * 16 + pdi_stream->pdi_num + 5;
	pdi_stream->sdw_pdi_num = stream_id;
	pdi_ch_map |= (stream_id & CNL_PCMSYCM_STREAM_MASK) <<
					CNL_PCMSYCM_STREAM_SHIFT;
	pdi_ch_map |= (pdi_stream->l_ch_num &
			CNL_PCMSYCM_LCHAN_MASK) <<
					CNL_PCMSYCM_LCHAN_SHIFT;
	pdi_ch_map |= (0xF & CNL_PCMSYCM_HCHAN_MASK) <<
					CNL_PCMSYCM_HCHAN_SHIFT;
	cnl_sdw_reg_writew(sdw->data.sdw_shim, ch_map_offset,
				pdi_ch_map);
	/* If direction is input, port is sink port*/
	if (direction ==  SDW_DATA_DIR_IN)
		port_ctrl |= (PORTCTRL_PORT_DIRECTION_MASK <<
				PORTCTRL_PORT_DIRECTION_SHIFT);
	else
		port_ctrl &= ~(PORTCTRL_PORT_DIRECTION_MASK <<
				PORTCTRL_PORT_DIRECTION_SHIFT);

	port_ctrl_offset =  SDW_CNL_PORTCTRL + (port->port_num *
				SDW_CNL_PORT_REG_OFFSET);
	cnl_sdw_reg_writel(sdw->data.sdw_regs, port_ctrl_offset, port_ctrl);

	pdi_config |= ((port->port_num & PDINCONFIG_PORT_NUMBER_MASK) <<
			PDINCONFIG_PORT_NUMBER_SHIFT);

	channel_mask = (1 << ch_cnt) - 1;
	pdi_config |= (channel_mask << PDINCONFIG_CHANNEL_MASK_SHIFT);
	/* TODO: Remove below hardcodings */
	pdi_config_offset =  (SDW_CNL_PDINCONFIG0 +
				(pdi_stream->pdi_num * 16));
	cnl_sdw_reg_writel(sdw->data.sdw_regs, pdi_config_offset, pdi_config);

	stream_config = cnl_sdw_reg_readl(sdw->data.alh_base,
			(pdi_stream->sdw_pdi_num * ALH_CNL_STRMZCFG_OFFSET));
	stream_config |= (CNL_STRMZCFG_DMAT_VAL & CNL_STRMZCFG_DMAT_MASK) <<
				CNL_STRMZCFG_DMAT_SHIFT;
	stream_config |=  ((ch_cnt - 1) & CNL_STRMZCFG_CHAN_MASK) <<
			CNL_STRMZCFG_CHAN_SHIFT;
	cnl_sdw_reg_writel(sdw->data.alh_base,
			 (pdi_stream->sdw_pdi_num * ALH_CNL_STRMZCFG_OFFSET),
			stream_config);
	return 0;
}

static int sdw_alloc_pdm_stream(struct cnl_sdw *sdw,
			struct cnl_sdw_port *port, int ch_cnt, int direction)
{
	int num_pdm_streams;
	struct cnl_sdw_pdi_stream *stream;
	int i;
	unsigned int port_ctrl_offset, pdi_config_offset;
	unsigned int port_ctrl = 0, pdi_config = 0, channel_mask;

	/* Currently PDM supports either Input or Output Streams */
	if (direction == SDW_DATA_DIR_IN) {
		num_pdm_streams = sdw->num_in_pdm_streams;
		stream = sdw->in_pdm_streams;
	} else {
		num_pdm_streams = sdw->num_out_pdm_streams;
		stream = sdw->out_pdm_streams;
	}
	mutex_lock(&sdw->stream_lock);
	for (i = 0; i < num_pdm_streams; i++) {
		if (stream[i].allocated == false) {
			stream[i].allocated = true;
			stream[i].port_num = port->port_num;
			port->pdi_stream = &stream[i];
			break;
		}
	}
	mutex_unlock(&sdw->stream_lock);
	if (!port->pdi_stream)
		return -EINVAL;
	/* If direction is input, port is sink port*/
	if (direction ==  SDW_DATA_DIR_IN)
		port_ctrl |= (PORTCTRL_PORT_DIRECTION_MASK <<
				PORTCTRL_PORT_DIRECTION_SHIFT);
	else
		port_ctrl &= ~(PORTCTRL_PORT_DIRECTION_MASK <<
				PORTCTRL_PORT_DIRECTION_SHIFT);

	port_ctrl_offset =  SDW_CNL_PORTCTRL + (port->port_num *
				SDW_CNL_PORT_REG_OFFSET);
	cnl_sdw_reg_writel(sdw->data.sdw_regs, port_ctrl_offset, port_ctrl);

	pdi_config |= ((port->port_num & PDINCONFIG_PORT_NUMBER_MASK) <<
			PDINCONFIG_PORT_NUMBER_SHIFT);

	channel_mask = (1 << ch_cnt) - 1;
	pdi_config |= (channel_mask << PDINCONFIG_CHANNEL_MASK_SHIFT);
	/* TODO: Remove below hardcodings */
	pdi_config_offset =  (SDW_CNL_PDINCONFIG0 + (stream[i].pdi_num * 16));
	cnl_sdw_reg_writel(sdw->data.sdw_regs, pdi_config_offset, pdi_config);

	return 0;
}

struct cnl_sdw_port *cnl_sdw_alloc_port(struct sdw_master *mstr, int ch_count,
				enum sdw_data_direction direction,
				enum cnl_sdw_pdi_stream_type stream_type)
{
	struct cnl_sdw *sdw;
	struct cnl_sdw_port *port = NULL;
	int i, ret = 0;
	struct num_pdi_streams;

	sdw = sdw_master_get_drvdata(mstr);

	mutex_lock(&sdw->stream_lock);
	for (i = 1; i <= CNL_SDW_MAX_PORTS; i++) {
		if (sdw->port[i].allocated == false) {
			port = &sdw->port[i];
			port->allocated = true;
			port->direction = direction;
			port->ch_cnt = ch_count;
			break;
		}
	}
	mutex_unlock(&sdw->stream_lock);
	if (!port) {
		dev_err(&mstr->dev, "Unable to allocate port\n");
		return NULL;
	}
	port->pdi_stream = NULL;
	if (stream_type == CNL_SDW_PDI_TYPE_PDM)
		ret = sdw_alloc_pdm_stream(sdw, port, ch_count, direction);
	else
		ret = sdw_alloc_pcm_stream(sdw, port, ch_count, direction);
	if (!ret)
		return port;

	dev_err(&mstr->dev, "Unable to allocate stream\n");
	mutex_lock(&sdw->stream_lock);
	port->allocated = false;
	mutex_unlock(&sdw->stream_lock);
	return NULL;
}
EXPORT_SYMBOL_GPL(cnl_sdw_alloc_port);

void cnl_sdw_free_port(struct sdw_master *mstr, int port_num)
{
	int i;
	struct cnl_sdw *sdw;
	struct cnl_sdw_port *port = NULL;

	sdw = sdw_master_get_drvdata(mstr);
	for (i = 1; i < CNL_SDW_MAX_PORTS; i++) {
		if (sdw->port[i].port_num == port_num) {
			port = &sdw->port[i];
			break;
		}
	}
	if (!port)
		return;
	mutex_lock(&sdw->stream_lock);
	port->pdi_stream->allocated = false;
	port->pdi_stream = NULL;
	port->allocated = false;
	mutex_unlock(&sdw->stream_lock);
}
EXPORT_SYMBOL_GPL(cnl_sdw_free_port);

static int cnl_sdw_update_slave_status(struct cnl_sdw *sdw, int slave_intstat0,
			int slave_intstat1)
{
	int i;
	struct sdw_status slave_status;
	u64 slaves_stat, slave_stat;
	int ret = 0;

	memset(&slave_status, 0x0, sizeof(slave_status));
	slaves_stat = (u64) slave_intstat1 <<
			SDW_CNL_SLAVES_STAT_UPPER_DWORD_SHIFT;
	slaves_stat |= slave_intstat0;
	for (i = 0; i <= SOUNDWIRE_MAX_DEVICES; i++) {
		slave_stat = slaves_stat >> (i * SDW_CNL_SLAVE_STATUS_BITS);
		if (slave_stat &  MCP_SLAVEINTSTAT_NOT_PRESENT_MASK)
			slave_status.status[i] = SDW_SLAVE_STAT_NOT_PRESENT;
		else if (slave_stat &  MCP_SLAVEINTSTAT_ATTACHED_MASK)
			slave_status.status[i] = SDW_SLAVE_STAT_ATTACHED_OK;
		else if (slave_stat &  MCP_SLAVEINTSTAT_ALERT_MASK)
			slave_status.status[i] = SDW_SLAVE_STAT_ALERT;
		else if (slave_stat &  MCP_SLAVEINTSTAT_RESERVED_MASK)
			slave_status.status[i] = SDW_SLAVE_STAT_RESERVED;
	}
	ret = sdw_master_update_slv_status(sdw->mstr, &slave_status);
	return ret;
}

static void cnl_sdw_read_response(struct cnl_sdw *sdw)
{
	struct cnl_sdw_data *data = &sdw->data;
	int num_res = 0, i;
	u32 cmd_base = SDW_CNL_MCP_COMMAND_BASE;

	num_res = cnl_sdw_reg_readl(data->sdw_regs, SDW_CNL_MCP_FIFOSTAT);
	num_res &= MCP_RX_FIFO_AVAIL_MASK;
	for (i = 0; i < num_res; i++) {
		sdw->response_buf[i] = cnl_sdw_reg_readl(data->sdw_regs,
				cmd_base);
		cmd_base += SDW_CNL_CMD_WORD_LEN;
	}
}

static enum sdw_command_response sdw_fill_message_response(
			struct sdw_master *mstr,
			struct sdw_msg *msg,
			int count, int offset)
{
	int i, j;
	int no_ack = 0, nack = 0;
	struct cnl_sdw *sdw = sdw_master_get_drvdata(mstr);

	for (i = 0; i < count; i++) {
		if (!(MCP_RESPONSE_ACK_MASK & sdw->response_buf[i])) {
			no_ack = 1;
			dev_err(&mstr->dev, "Ack not recevied\n");
			if ((MCP_RESPONSE_NACK_MASK &
					sdw->response_buf[i])) {
				nack = 1;
				dev_err(&mstr->dev, "NACK recevied\n");
			}
		}
		break;
	}
	if (nack) {
		dev_err(&mstr->dev, "Nack detected for slave %d\n", msg->slave_addr);
		msg->len = 0;
		return -EREMOTEIO;
	} else if (no_ack) {
		dev_err(&mstr->dev, "Command ignored for slave %d\n", msg->slave_addr);
		msg->len = 0;
		return -EREMOTEIO;
	}
	if (msg->flag == SDW_MSG_FLAG_WRITE)
		return 0;
	/* Response and Command has same base address */
	for (j = 0; j < count; j++)
			msg->buf[j + offset] =
			(sdw->response_buf[j]  >> MCP_RESPONSE_RDATA_SHIFT);
	return 0;
}


irqreturn_t cnl_sdw_irq_handler(int irq, void *context)
{
	struct cnl_sdw *sdw = context;
	volatile int int_status, status, wake_sts;

	struct cnl_sdw_data *data = &sdw->data;
	volatile int slave_intstat0 = 0, slave_intstat1 = 0;
	struct sdw_master *mstr = sdw->mstr;

	/*
	 * Return if IP is in power down state. Interrupt can still come
	 * since  its shared irq.
	 */
	if (!sdw->sdw_link_status)
		return IRQ_NONE;

	int_status = cnl_sdw_reg_readl(data->sdw_regs, SDW_CNL_MCP_INTSTAT);
	status = cnl_sdw_reg_readl(data->sdw_regs, SDW_CNL_MCP_STAT);
	slave_intstat0 = cnl_sdw_reg_readl(data->sdw_regs,
					SDW_CNL_MCP_SLAVEINTSTAT0);
	slave_intstat1 = cnl_sdw_reg_readl(data->sdw_regs,
					SDW_CNL_MCP_SLAVEINTSTAT1);
	wake_sts = cnl_sdw_reg_readw(data->sdw_shim,
				SDW_CNL_SNDWWAKESTS_REG_OFFSET);
	cnl_sdw_reg_writew(data->sdw_shim, SDW_CNL_SNDWWAKESTS_REG_OFFSET,
				wake_sts);

	if (!(int_status & (MCP_INTSTAT_IRQ_MASK << MCP_INTSTAT_IRQ_SHIFT)))
		return IRQ_NONE;

	if (int_status & (MCP_INTSTAT_RXWL_MASK << MCP_INTSTAT_RXWL_SHIFT)) {
		cnl_sdw_read_response(sdw);
		if (sdw->async_msg.async_xfer_complete) {
			sdw_fill_message_response(mstr, sdw->async_msg.msg,
					sdw->async_msg.length, 0);
			complete(sdw->async_msg.async_xfer_complete);
			sdw->async_msg.async_xfer_complete = NULL;
			sdw->async_msg.msg = NULL;
		} else
			complete(&sdw->tx_complete);
	}
	if (int_status & (MCP_INTSTAT_CONTROLBUSCLASH_MASK <<
				MCP_INTSTAT_CONTROLBUSCLASH_SHIFT)) {
		/* Some slave is behaving badly, where its driving
		 * data line during control word bits.
		 */
		dev_err_ratelimited(&mstr->dev, "Bus clash detected for control word\n");
		WARN_ONCE(1, "Bus clash detected for control word\n");
	}
	if (int_status & (MCP_INTSTAT_DATABUSCLASH_MASK <<
				MCP_INTSTAT_DATABUSCLASH_SHIFT)) {
		/* More than 1 slave is trying to drive bus. There is
		 * some problem with ownership of bus data bits,
		 * or either of the
		 * slave is behaving badly.
		 */
		dev_err_ratelimited(&mstr->dev, "Bus clash detected for control word\n");
		WARN_ONCE(1, "Bus clash detected for data word\n");
	}

	if (int_status & (MCP_INTSTAT_SLAVE_STATUS_CHANGED_MASK <<
		MCP_INTSTAT_SLAVE_STATUS_CHANGED_SHIFT)) {
		dev_info(&mstr->dev, "Slave status change\n");
		cnl_sdw_update_slave_status(sdw, slave_intstat0,
							slave_intstat1);
	}
	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_SLAVEINTSTAT0,
								slave_intstat0);
	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_SLAVEINTSTAT1,
								slave_intstat1);
	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_INTSTAT, int_status);
	return IRQ_HANDLED;
}

static enum sdw_command_response cnl_program_scp_addr(struct sdw_master *mstr,
					struct sdw_msg *msg)
{
	struct cnl_sdw *sdw = sdw_master_get_drvdata(mstr);
	struct cnl_sdw_data *data = &sdw->data;
	u32 cmd_base = SDW_CNL_MCP_COMMAND_BASE;
	u32 cmd_data[2] = {0, 0};
	unsigned long time_left;
	int no_ack = 0, nack = 0;
	int i;

	/* Since we are programming 2 commands, program the
	 * RX watermark level at 2
	 */
	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_FIFOLEVEL, 2);
	/* Program device address */
	cmd_data[0] |= (msg->slave_addr & MCP_COMMAND_DEV_ADDR_MASK) <<
				MCP_COMMAND_DEV_ADDR_SHIFT;
	/* Write command to program the scp_addr1 register */
	cmd_data[0] |= (0x3 << MCP_COMMAND_COMMAND_SHIFT);
	cmd_data[1] = cmd_data[0];
	/* scp_addr1 register address */
	cmd_data[0] |= (SDW_SCP_ADDRPAGE1 << MCP_COMMAND_REG_ADDR_L_SHIFT);
	cmd_data[1] |= (SDW_SCP_ADDRPAGE2 << MCP_COMMAND_REG_ADDR_L_SHIFT);
	cmd_data[0] |= msg->addr_page1;
	cmd_data[1] |= msg->addr_page2;

	cnl_sdw_reg_writel(data->sdw_regs, cmd_base, cmd_data[0]);
	cmd_base += SDW_CNL_CMD_WORD_LEN;
	cnl_sdw_reg_writel(data->sdw_regs, cmd_base, cmd_data[1]);

	time_left = wait_for_completion_timeout(&sdw->tx_complete,
						3000);
	if (!time_left) {
		dev_err(&mstr->dev, "Controller Timed out\n");
		msg->len = 0;
		return -ETIMEDOUT;
	}

	for (i = 0; i < CNL_SDW_SCP_ADDR_REGS; i++) {
		if (!(MCP_RESPONSE_ACK_MASK & sdw->response_buf[i])) {
			no_ack = 1;
				dev_err(&mstr->dev, "Ack not recevied\n");
			if ((MCP_RESPONSE_NACK_MASK & sdw->response_buf[i])) {
				nack = 1;
				dev_err(&mstr->dev, "NACK recevied\n");
			}
		}
	}
	/* We dont return error if NACK or No ACK detected for broadcast addr
	 * because some slave might support SCP addr, while some slaves may not
	 * support it. This is not correct, since we wont be able to find out
	 * if NACK is detected because of slave not supporting SCP_addrpage or
	 * its a genuine NACK because of bus errors. We are not sure what slaves
	 * will report, NACK or No ACK for the scp_addrpage programming if they
	 * dont support it. Spec is not clear about this.
	 * This needs to be thought through
	 */
	if (nack & (msg->slave_addr != 15)) {
		dev_err(&mstr->dev, "SCP_addrpage write NACKed for slave %d\n", msg->slave_addr);
		return -EREMOTEIO;
	} else if (no_ack && (msg->slave_addr != 15)) {
		dev_err(&mstr->dev, "SCP_addrpage write ignored for slave %d\n", msg->slave_addr);
		return -EREMOTEIO;
	} else
		return 0;

}

static enum sdw_command_response sdw_xfer_msg(struct sdw_master *mstr,
		struct sdw_msg *msg, int cmd, int offset, int count, bool async)
{
	struct cnl_sdw *sdw = sdw_master_get_drvdata(mstr);
	struct cnl_sdw_data *data = &sdw->data;
	int j;
	u32 cmd_base =  SDW_CNL_MCP_COMMAND_BASE;
	u32 cmd_data = 0;
	unsigned long time_left;
	u16 addr = msg->addr;

	/* Program the watermark level upto number of count */
	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_FIFOLEVEL, count);

	cmd_base = SDW_CNL_MCP_COMMAND_BASE;
	for (j = 0; j < count; j++) {
		/* Program device address */
		cmd_data = 0;
		cmd_data |= (msg->slave_addr &
			MCP_COMMAND_DEV_ADDR_MASK) <<
			MCP_COMMAND_DEV_ADDR_SHIFT;
		/* Program read/write command */
		cmd_data |= (cmd << MCP_COMMAND_COMMAND_SHIFT);
		/* program incrementing address register */
		cmd_data |= (addr++ << MCP_COMMAND_REG_ADDR_L_SHIFT);
		/* Program the data if write command */
		if (msg->flag == SDW_MSG_FLAG_WRITE)
			cmd_data |=
				msg->buf[j + offset];

		cmd_data |= ((msg->ssp_tag &
				MCP_COMMAND_SSP_TAG_MASK) <<
				MCP_COMMAND_SSP_TAG_SHIFT);
		cnl_sdw_reg_writel(data->sdw_regs,
					cmd_base, cmd_data);
		cmd_base += SDW_CNL_CMD_WORD_LEN;
	}

	/* If Async dont wait for completion */
	if (async)
		return 0;
	/* Wait for 3 second for timeout */
	time_left = wait_for_completion_timeout(&sdw->tx_complete, 3 * HZ);
	if (!time_left) {
		dev_err(&mstr->dev, "Controller timedout\n");
		msg->len = 0;
		return -ETIMEDOUT;
	}
	return sdw_fill_message_response(mstr, msg, count, offset);
}

static enum sdw_command_response cnl_sdw_xfer_msg_async(struct sdw_master *mstr,
		struct sdw_msg *msg, bool program_scp_addr_page,
		struct sdw_async_xfer_data *data)
{
	int ret = 0, cmd;
	struct cnl_sdw *sdw = sdw_master_get_drvdata(mstr);

	/* Only 1 message can be handled in Async fashion. This is used
	 * only for Bank switching where during aggregation it is required
	 * to synchronously switch the bank on  more than 1 controller
	 */
	if (msg->len > 1) {
		ret = -EINVAL;
		goto error;
	}
	/* If scp addr programming fails goto error */
	if (program_scp_addr_page)
		ret = cnl_program_scp_addr(mstr, msg);
	if (ret)
		goto error;

	switch (msg->flag) {
	case SDW_MSG_FLAG_READ:
		cmd = 0x2;
		break;
	case SDW_MSG_FLAG_WRITE:
		cmd = 0x3;
		break;
	default:
		dev_err(&mstr->dev, "Command not supported\n");
		return -EINVAL;
	}
	sdw->async_msg.async_xfer_complete = &data->xfer_complete;
	sdw->async_msg.msg = msg;
	sdw->async_msg.length = msg->len;
	/* Dont wait for reply, calling function will wait for reply. */
	ret = sdw_xfer_msg(mstr, msg, cmd, 0, msg->len, true);
	return ret;
error:
	msg->len = 0;
	complete(&data->xfer_complete);
	return -EINVAL;

}

static enum sdw_command_response cnl_sdw_xfer_msg(struct sdw_master *mstr,
		struct sdw_msg *msg, bool program_scp_addr_page)
{
	int i, ret = 0, cmd;

	if (program_scp_addr_page)
		ret = cnl_program_scp_addr(mstr, msg);

	if (ret) {
		msg->len = 0;
		return ret;
	}

	switch (msg->flag) {
	case SDW_MSG_FLAG_READ:
		cmd = 0x2;
		break;
	case SDW_MSG_FLAG_WRITE:
		cmd = 0x3;
		break;
	default:
		dev_err(&mstr->dev, "Command not supported\n");
		return -EINVAL;
	}
	for (i = 0; i < msg->len / SDW_CNL_MCP_COMMAND_LENGTH; i++) {
		ret = sdw_xfer_msg(mstr, msg,
				cmd, i * SDW_CNL_MCP_COMMAND_LENGTH,
				SDW_CNL_MCP_COMMAND_LENGTH, false);
		if (ret < 0)
			break;
	}
	if (!(msg->len % SDW_CNL_MCP_COMMAND_LENGTH))
		return ret;
	ret = sdw_xfer_msg(mstr, msg, cmd, i * SDW_CNL_MCP_COMMAND_LENGTH,
			msg->len % SDW_CNL_MCP_COMMAND_LENGTH, false);
	if (ret < 0)
		return -EINVAL;
	return ret;
}

static int cnl_sdw_xfer_bulk(struct sdw_master *mstr,
	struct sdw_bra_block *block)
{
	return 0;
}

static int cnl_sdw_mon_handover(struct sdw_master *mstr,
			bool enable)
{
	int mcp_config;
	struct cnl_sdw *sdw = sdw_master_get_drvdata(mstr);
	struct cnl_sdw_data *data = &sdw->data;

	mcp_config = cnl_sdw_reg_readl(data->sdw_regs, SDW_CNL_MCP_CONFIG);
	if (enable)
		mcp_config |= MCP_CONFIG_BRELENABLE_MASK <<
				MCP_CONFIG_BRELENABLE_SHIFT;
	else
		mcp_config &= ~(MCP_CONFIG_BRELENABLE_MASK <<
				MCP_CONFIG_BRELENABLE_SHIFT);

	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_CONFIG, mcp_config);
	return 0;
}

static int cnl_sdw_set_ssp_interval(struct sdw_master *mstr,
			int ssp_interval, int bank)
{
	struct cnl_sdw *sdw = sdw_master_get_drvdata(mstr);
	struct cnl_sdw_data *data = &sdw->data;
	int sspctrl_offset, check;

	if (bank)
		sspctrl_offset = SDW_CNL_MCP_SSPCTRL1;
	else
		sspctrl_offset = SDW_CNL_MCP_SSPCTRL0;

	cnl_sdw_reg_writel(data->sdw_regs, sspctrl_offset, ssp_interval);

	check = cnl_sdw_reg_readl(data->sdw_regs, sspctrl_offset);

	return 0;
}

static int cnl_sdw_set_clock_freq(struct sdw_master *mstr,
			int cur_clk_freq, int bank)
{
	struct cnl_sdw *sdw = sdw_master_get_drvdata(mstr);
	struct cnl_sdw_data *data = &sdw->data;
	int mcp_clockctrl_offset, mcp_clockctrl;


	/* TODO: Retrieve divider value or get value directly from calling
	 * function
	 */
#ifdef CONFIG_SND_SOC_SVFPGA
	int divider = ((9600000 * 2/cur_clk_freq) - 1);
#else
	int divider = ((9600000/cur_clk_freq) - 1);
#endif

	if (bank) {
		mcp_clockctrl_offset = SDW_CNL_MCP_CLOCKCTRL1;
		mcp_clockctrl = cnl_sdw_reg_readl(data->sdw_regs,
				SDW_CNL_MCP_CLOCKCTRL1);

	} else {
		mcp_clockctrl_offset = SDW_CNL_MCP_CLOCKCTRL0;
		mcp_clockctrl = cnl_sdw_reg_readl(data->sdw_regs,
				SDW_CNL_MCP_CLOCKCTRL0);
	}

	mcp_clockctrl |= divider;

	/* Write value here */
	cnl_sdw_reg_writel(data->sdw_regs, mcp_clockctrl_offset,
				mcp_clockctrl);

	mcp_clockctrl = cnl_sdw_reg_readl(data->sdw_regs,
				mcp_clockctrl_offset);
	return 0;
}

static int cnl_sdw_set_port_params(struct sdw_master *mstr,
			struct sdw_port_params *params, int bank)
{
	struct cnl_sdw *sdw = sdw_master_get_drvdata(mstr);
	struct cnl_sdw_data *data = &sdw->data;
	int dpn_config = 0, dpn_config_offset;

	if (bank)
		dpn_config_offset = SDW_CNL_DPN_CONFIG1;
	else
		dpn_config_offset = SDW_CNL_DPN_CONFIG0;

	dpn_config = cnl_sdw_port_reg_readl(data->sdw_regs,
				dpn_config_offset, params->num);

	dpn_config |= (((params->word_length - 1) & DPN_CONFIG_WL_MASK) <<
				DPN_CONFIG_WL_SHIFT);
	dpn_config |= ((params->port_flow_mode & DPN_CONFIG_PF_MODE_MASK) <<
				DPN_CONFIG_PF_MODE_SHIFT);
	dpn_config |= ((params->port_data_mode & DPN_CONFIG_PD_MODE_MASK) <<
				DPN_CONFIG_PD_MODE_SHIFT);
	cnl_sdw_port_reg_writel(data->sdw_regs,
				dpn_config_offset, params->num, dpn_config);

	cnl_sdw_port_reg_readl(data->sdw_regs,
				dpn_config_offset, params->num);
	return 0;
}

static int cnl_sdw_set_port_transport_params(struct sdw_master *mstr,
			struct sdw_transport_params *params, int bank)
{
struct cnl_sdw *sdw = sdw_master_get_drvdata(mstr);
	struct cnl_sdw_data *data = &sdw->data;

	int dpn_config = 0, dpn_config_offset;
	int dpn_samplectrl_offset;
	int dpn_offsetctrl = 0, dpn_offsetctrl_offset;
	int dpn_hctrl = 0, dpn_hctrl_offset;

	if (bank) {
		dpn_config_offset = SDW_CNL_DPN_CONFIG1;
		dpn_samplectrl_offset = SDW_CNL_DPN_SAMPLECTRL1;
		dpn_hctrl_offset = SDW_CNL_DPN_HCTRL1;
		dpn_offsetctrl_offset = SDW_CNL_DPN_OFFSETCTRL1;
	} else {
		dpn_config_offset = SDW_CNL_DPN_CONFIG0;
		dpn_samplectrl_offset = SDW_CNL_DPN_SAMPLECTRL0;
		dpn_hctrl_offset = SDW_CNL_DPN_HCTRL0;
		dpn_offsetctrl_offset = SDW_CNL_DPN_OFFSETCTRL0;
	}
	dpn_config = cnl_sdw_port_reg_readl(data->sdw_regs,
		dpn_config_offset,  params->num);
	dpn_config |= ((params->blockgroupcontrol & DPN_CONFIG_BGC_MASK) <<
					DPN_CONFIG_BGC_SHIFT);
	dpn_config |= ((params->blockpackingmode & DPN_CONFIG_BPM_MASK) <<
					DPN_CONFIG_BPM_SHIFT);

	cnl_sdw_port_reg_writel(data->sdw_regs,
		dpn_config_offset, params->num, dpn_config);

	cnl_sdw_port_reg_readl(data->sdw_regs,
		dpn_config_offset,  params->num);

	dpn_offsetctrl |= ((params->offset1 & DPN_OFFSETCTRL0_OF1_MASK) <<
			DPN_OFFSETCTRL0_OF1_SHIFT);

	dpn_offsetctrl |= ((params->offset2 & DPN_OFFSETCTRL0_OF2_MASK) <<
			DPN_OFFSETCTRL0_OF2_SHIFT);

	cnl_sdw_port_reg_writel(data->sdw_regs,
		dpn_offsetctrl_offset, params->num, dpn_offsetctrl);


	dpn_hctrl |= ((params->hstart & DPN_HCTRL_HSTART_MASK) <<
				DPN_HCTRL_HSTART_SHIFT);
	dpn_hctrl |= ((params->hstop & DPN_HCTRL_HSTOP_MASK) <<
				DPN_HCTRL_HSTOP_SHIFT);
	dpn_hctrl |= ((params->lanecontrol & DPN_HCTRL_LCONTROL_MASK) <<
				DPN_HCTRL_LCONTROL_SHIFT);

	cnl_sdw_port_reg_writel(data->sdw_regs,
			dpn_hctrl_offset, params->num, dpn_hctrl);

	cnl_sdw_port_reg_writel(data->sdw_regs,
			dpn_samplectrl_offset, params->num,
			(params->sample_interval - 1));

	cnl_sdw_port_reg_readl(data->sdw_regs,
		dpn_hctrl_offset,  params->num);

	cnl_sdw_port_reg_readl(data->sdw_regs,
		dpn_samplectrl_offset,  params->num);

	return 0;
}

static int cnl_sdw_port_activate_ch(struct sdw_master *mstr,
			struct sdw_activate_ch *activate_ch, int bank)
{
	struct cnl_sdw *sdw = sdw_master_get_drvdata(mstr);
	struct cnl_sdw_data *data = &sdw->data;
	int dpn_channelen_offset;
	int ch_mask;

	if (bank)
		dpn_channelen_offset = SDW_CNL_DPN_CHANNELEN1;
	else
		dpn_channelen_offset = SDW_CNL_DPN_CHANNELEN0;

	if (activate_ch->activate)
		ch_mask = activate_ch->ch_mask;
	else
		ch_mask = 0;

	cnl_sdw_port_reg_writel(data->sdw_regs,
			dpn_channelen_offset, activate_ch->num,
			ch_mask);

	return 0;
}

static int cnl_sdw_port_activate_ch_pre(struct sdw_master *mstr,
			struct sdw_activate_ch *activate_ch, int bank)
{
	int sync_reg;
	struct cnl_sdw *sdw = sdw_master_get_drvdata(mstr);
	struct cnl_sdw_data *data = &sdw->data;

	if (mstr->link_sync_mask) {
		/* Check if this link is synchronized with some other link */
		sync_reg = cnl_sdw_reg_readl(data->sdw_shim,  SDW_CNL_SYNC);
		/* If link is synchronized with other link than
		 * Need to make sure that command doesnt go till
		 * ssync is applied
		 */
		sync_reg |= (1 << (data->inst_id + CNL_SYNC_CMDSYNC_SHIFT));
		cnl_sdw_reg_writel(data->sdw_shim, SDW_CNL_SYNC, sync_reg);
	}

	return 0;
}
static int cnl_sdw_port_activate_ch_post(struct sdw_master *mstr,
			struct sdw_activate_ch *activate_ch, int bank)
{
	int sync_reg;
	struct cnl_sdw *sdw = sdw_master_get_drvdata(mstr);
	struct cnl_sdw_data *data = &sdw->data;

	sync_reg = cnl_sdw_reg_readl(data->sdw_shim,  SDW_CNL_SYNC);
	sync_reg |= CNL_SYNC_SYNCGO_MASK << CNL_SYNC_SYNCGO_SHIFT;
	cnl_sdw_reg_writel(data->sdw_shim, SDW_CNL_SYNC, sync_reg);

	return 0;
}

static int cnl_sdw_probe(struct sdw_master *mstr,
				const struct sdw_master_id *sdw_id)
{
	struct cnl_sdw *sdw;
	int ret = 0;
	struct cnl_sdw_data *data = mstr->dev.platform_data;

	sdw = devm_kzalloc(&mstr->dev, sizeof(*sdw), GFP_KERNEL);
	if (!sdw) {
		ret = -ENOMEM;
		return ret;
	}
	dev_info(&mstr->dev,
		"Controller Resources ctrl_base = %p shim=%p irq=%d inst_id=%d\n",
		data->sdw_regs, data->sdw_shim, data->irq, data->inst_id);
	sdw->data.sdw_regs = data->sdw_regs;
	sdw->data.sdw_shim = data->sdw_shim;
	sdw->data.irq = data->irq;
	sdw->data.inst_id = data->inst_id;
	sdw->data.alh_base = data->alh_base;
	sdw->mstr = mstr;
	spin_lock_init(&sdw->ctrl_lock);
	sdw_master_set_drvdata(mstr, sdw);
	init_completion(&sdw->tx_complete);
	mutex_init(&sdw->stream_lock);
	ret = sdw_init(sdw);
	if (ret) {
		dev_err(&mstr->dev, "SoundWire controller init failed %d\n",
				data->inst_id);
		return ret;
	}
	ret = devm_request_irq(&mstr->dev,
		sdw->data.irq, cnl_sdw_irq_handler, IRQF_SHARED, "SDW", sdw);
	if (ret) {
		dev_err(&mstr->dev, "unable to grab IRQ %d, disabling device\n",
			       sdw->data.irq);
		sdw_power_down_link(sdw);
		return ret;
	}
	pm_runtime_set_autosuspend_delay(&mstr->dev, 3000);
	pm_runtime_use_autosuspend(&mstr->dev);
	pm_runtime_enable(&mstr->dev);
	pm_runtime_get_sync(&mstr->dev);
	/* Resuming the device, since its already ON, function will simply
	 * return doing nothing
	 */
	pm_runtime_mark_last_busy(&mstr->dev);
	/* Suspending the device after 3 secs, by the time
	 * all the slave would have enumerated. Initial
	 * clock freq is 9.6MHz and frame shape is 48X2, so
	 * there are 200000 frames in second, total there are
	 * minimum 600000 frames before device suspends. Soundwire
	 * spec says slave should get attached to bus in 4096
	 * error free frames after reset. So this should be
	 * enough to make sure device gets attached to bus.
	 */
	pm_runtime_put_sync_autosuspend(&mstr->dev);
	return ret;
}

static int cnl_sdw_remove(struct sdw_master *mstr)
{
	struct cnl_sdw *sdw = sdw_master_get_drvdata(mstr);

	sdw_power_down_link(sdw);

	return 0;
}

#ifdef CONFIG_PM
static int cnl_sdw_runtime_suspend(struct device *dev)
{
	enum  sdw_clk_stop_mode clock_stop_mode;

	int volatile mcp_stat;
	int mcp_control;
	int timeout = 0;
	int ret = 0;

	struct cnl_sdw *sdw = dev_get_drvdata(dev);
	struct cnl_sdw_data *data = &sdw->data;

	/* If its suspended return */
	mcp_stat = cnl_sdw_reg_readl(data->sdw_regs,
					SDW_CNL_MCP_STAT);
	if (mcp_stat & (MCP_STAT_CLOCKSTOPPED_MASK <<
				MCP_STAT_CLOCKSTOPPED_SHIFT)) {
		dev_info(dev, "Clock is already stopped\n");
		return 0;
	}

	/* Write the MCP Control register to prevent block wakeup */
	mcp_control = cnl_sdw_reg_readl(data->sdw_regs,
					SDW_CNL_MCP_CONTROL);
	mcp_control |= (MCP_CONTROL_BLOCKWAKEUP_MASK <<
				MCP_CONTROL_BLOCKWAKEUP_SHIFT);
	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_CONTROL, mcp_control);

	/* Prepare all the slaves for clock stop */
	ret = sdw_prepare_for_clock_change(sdw->mstr, 1, &clock_stop_mode);
	if (ret)
		return ret;

	/* Call bus function to broadcast the clock stop now */
	ret = sdw_stop_clock(sdw->mstr, clock_stop_mode);
	if (ret)
		return ret;
	/* Wait for clock to be stopped, we are waiting at max 1sec now */
	while (timeout != 10) {
		mcp_stat = cnl_sdw_reg_readl(data->sdw_regs,
					SDW_CNL_MCP_STAT);
		if (mcp_stat & (MCP_STAT_CLOCKSTOPPED_MASK <<
			MCP_STAT_CLOCKSTOPPED_SHIFT))
			break;
		msleep(100);
		timeout++;
	}
	mcp_stat = cnl_sdw_reg_readl(data->sdw_regs,
					SDW_CNL_MCP_STAT);
	if (!(mcp_stat & (MCP_STAT_CLOCKSTOPPED_MASK <<
				MCP_STAT_CLOCKSTOPPED_SHIFT))) {
		dev_err(dev, "Clock Stop failed\n");
		ret = -EBUSY;
		goto out;
	}
	/* Switch control from master IP to glue */
	sdw_switch_to_glue(sdw);

	sdw_power_down_link(sdw);

	/* Enable the wakeup */
	cnl_sdw_reg_writew(data->sdw_shim,
			SDW_CNL_SNDWWAKEEN_REG_OFFSET,
			(0x1 << data->inst_id));
out:
	return ret;
}

static int cnl_sdw_clock_stop_exit(struct cnl_sdw *sdw)
{
	u16 wake_en, wake_sts, ioctl;
	int volatile mcp_control;
	int timeout = 0;
	struct cnl_sdw_data *data = &sdw->data;
	int ioctl_offset = SDW_CNL_IOCTL + (data->inst_id *
					SDW_CNL_IOCTL_REG_OFFSET);

	/* Disable the wake up interrupt */
	wake_en = cnl_sdw_reg_readw(data->sdw_shim,
				SDW_CNL_SNDWWAKEEN_REG_OFFSET);
	wake_en &= ~(0x1 << data->inst_id);
	cnl_sdw_reg_writew(data->sdw_shim, SDW_CNL_SNDWWAKEEN_REG_OFFSET,
				wake_en);

	/* Clear wake status. This may be set if Slave requested wakeup has
	 * happened, or may not be if it master requested. But in any case
	 * this wont make any harm
	 */
	wake_sts = cnl_sdw_reg_readw(data->sdw_shim,
				SDW_CNL_SNDWWAKESTS_REG_OFFSET);
	wake_sts |= (0x1 << data->inst_id);
	cnl_sdw_reg_writew(data->sdw_shim, SDW_CNL_SNDWWAKESTS_REG_OFFSET,
				wake_sts);

	ioctl = cnl_sdw_reg_readw(data->sdw_shim, ioctl_offset);
	ioctl |= CNL_IOCTL_DO_MASK << CNL_IOCTL_DO_SHIFT;
	cnl_sdw_reg_writew(data->sdw_shim,  ioctl_offset, ioctl);
	ioctl |= CNL_IOCTL_DOE_MASK << CNL_IOCTL_DOE_SHIFT;
	cnl_sdw_reg_writew(data->sdw_shim,  ioctl_offset, ioctl);
	/* Switch control back to master */
	sdw_switch_to_mip(sdw);

	mcp_control = cnl_sdw_reg_readl(data->sdw_regs,
					SDW_CNL_MCP_CONTROL);
	mcp_control &= ~(MCP_CONTROL_BLOCKWAKEUP_MASK <<
				MCP_CONTROL_BLOCKWAKEUP_SHIFT);
	mcp_control |= (MCP_CONTROL_CLOCKSTOPCLEAR_MASK <<
				MCP_CONTROL_CLOCKSTOPCLEAR_SHIFT);
	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_CONTROL, mcp_control);
	/*
	 * Wait for timeout to be clear to successful enabling of the clock
	 * We will wait for 1sec before giving up
	 */
	while (timeout != 10) {
		mcp_control = cnl_sdw_reg_readl(data->sdw_regs,
					SDW_CNL_MCP_CONTROL);
		if ((mcp_control & (MCP_CONTROL_CLOCKSTOPCLEAR_MASK <<
				MCP_CONTROL_CLOCKSTOPCLEAR_SHIFT)) == 0)
			break;
		msleep(1000);
		timeout++;
	}
	mcp_control = cnl_sdw_reg_readl(data->sdw_regs,
					SDW_CNL_MCP_CONTROL);
	if ((mcp_control & (MCP_CONTROL_CLOCKSTOPCLEAR_MASK <<
			MCP_CONTROL_CLOCKSTOPCLEAR_SHIFT)) != 0) {
		dev_err(&sdw->mstr->dev, "Clop Stop Exit failed\n");
		return -EBUSY;
	}

	dev_info(&sdw->mstr->dev, "Exit from clock stop successful\n");
	return 0;

}

static int cnl_sdw_runtime_resume(struct device *dev)
{
	struct cnl_sdw *sdw = dev_get_drvdata(dev);
	struct cnl_sdw_data *data = &sdw->data;
	int volatile mcp_stat;
	struct sdw_master *mstr;
	int ret = 0;

	mstr = sdw->mstr;
	/*
	 * If already resumed, do nothing. This can happen because of
	 * wakeup enable.
	 */
	mcp_stat = cnl_sdw_reg_readl(data->sdw_regs,
					SDW_CNL_MCP_STAT);
	if (!(mcp_stat & (MCP_STAT_CLOCKSTOPPED_MASK <<
				MCP_STAT_CLOCKSTOPPED_SHIFT))) {
		dev_info(dev, "Clock is already running\n");
		return 0;
	}
	dev_info(dev, "%s %d Clock is stopped\n", __func__, __LINE__);

	ret = cnl_sdw_clock_stop_exit(sdw);
	if (ret)
		return ret;
	dev_info(&mstr->dev, "Exit from clock stop successful\n");

	/* Prepare all the slaves to comeout of clock stop */
	ret = sdw_prepare_for_clock_change(sdw->mstr, 0, NULL);
	if (ret)
		return ret;

	return 0;
}

static int cnl_sdw_sleep_resume(struct device *dev)
{
	return cnl_sdw_runtime_resume(dev);
}
static int cnl_sdw_sleep_suspend(struct device *dev)
{
	return cnl_sdw_runtime_suspend(dev);
}
#endif

static const struct dev_pm_ops cnl_sdw_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(cnl_sdw_sleep_suspend, cnl_sdw_sleep_resume)
	SET_RUNTIME_PM_OPS(cnl_sdw_runtime_suspend,
				cnl_sdw_runtime_resume, NULL)
};

static struct sdw_master_ops cnl_sdw_master_ops  = {
	.xfer_msg_async = cnl_sdw_xfer_msg_async,
	.xfer_msg = cnl_sdw_xfer_msg,
	.xfer_bulk = cnl_sdw_xfer_bulk,
	.monitor_handover = cnl_sdw_mon_handover,
	.set_ssp_interval = cnl_sdw_set_ssp_interval,
	.set_clock_freq = cnl_sdw_set_clock_freq,
	.set_frame_shape = NULL,
};

static struct sdw_master_port_ops cnl_sdw_master_port_ops = {
	.dpn_set_port_params = cnl_sdw_set_port_params,
	.dpn_set_port_transport_params = cnl_sdw_set_port_transport_params,
	.dpn_port_activate_ch = cnl_sdw_port_activate_ch,
	.dpn_port_activate_ch_pre = cnl_sdw_port_activate_ch_pre,
	.dpn_port_activate_ch_post = cnl_sdw_port_activate_ch_post,
	.dpn_port_prepare_ch = NULL,
	.dpn_port_prepare_ch_pre = NULL,
	.dpn_port_prepare_ch_post = NULL,

};

static struct sdw_mstr_driver cnl_sdw_mstr_driver = {
	.driver_type = SDW_DRIVER_TYPE_MASTER,
	.driver = {
		.name   = "cnl_sdw_mstr",
		.pm	= &cnl_sdw_pm_ops,
	},
	.probe          = cnl_sdw_probe,
	.remove         = cnl_sdw_remove,
	.mstr_ops	= &cnl_sdw_master_ops,
	.mstr_port_ops = &cnl_sdw_master_port_ops,
};

static int __init cnl_sdw_init(void)
{
	return sdw_mstr_driver_register(&cnl_sdw_mstr_driver);
}
module_init(cnl_sdw_init);

static void cnl_sdw_exit(void)
{
	sdw_mstr_driver_unregister(&cnl_sdw_mstr_driver);
}
module_exit(cnl_sdw_exit);

MODULE_DESCRIPTION("Intel SoundWire Master Controller Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Hardik Shah <hardik.t.shah@intel.com>");
