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
	int sync_reg, syncgo_mask;
	volatile int config_update = 0;
	volatile int sync_update = 0;
	/* Try 10 times before giving up on configuration update */
	int timeout = 10;
	int config_updated = 0;

	config_update |= MCP_CONFIGUPDATE_CONFIGUPDATE_MASK <<
				MCP_CONFIGUPDATE_CONFIGUPDATE_SHIFT;
	/* Bit is self-cleared when configuration gets updated. */
	cnl_sdw_reg_writel(data->sdw_regs,  SDW_CNL_MCP_CONFIGUPDATE,
			config_update);

	/*
	 * Set SYNCGO bit for Master(s) running in aggregated mode
	 * (MMModeEN = 1). This action causes all gSyncs of all Master IPs
	 * to be unmasked and asserted at the currently active gSync rate.
	 * The initialization-pending Master IP SoundWire bus clock will
	 * start up synchronizing to gSync, leading to bus reset entry,
	 * subsequent exit, and 1st Frame generation aligning to gSync.
	 * Note that this is done in order to overcome hardware bug related
	 * to mis-alignment of gSync and frame.
	 */
	if (mstr->link_sync_mask) {
		sync_reg = cnl_sdw_reg_readl(data->sdw_shim,  SDW_CNL_SYNC);
		sync_reg |= (CNL_SYNC_SYNCGO_MASK << CNL_SYNC_SYNCGO_SHIFT);
		cnl_sdw_reg_writel(data->sdw_shim, SDW_CNL_SYNC, sync_reg);
		syncgo_mask = (CNL_SYNC_SYNCGO_MASK << CNL_SYNC_SYNCGO_SHIFT);

		do {
			sync_update = cnl_sdw_reg_readl(data->sdw_shim,
								SDW_CNL_SYNC);
			if ((sync_update & syncgo_mask) == 0)
				break;

			msleep(20);
			timeout--;

		}  while (timeout);

		if ((sync_update & syncgo_mask) != 0) {
			dev_err(&mstr->dev, "Failed to set sync go\n");
			return -EIO;
		}

		/* Reset timeout */
		timeout = 10;
	}

	/* Wait for config update bit to be self cleared */
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
	pdm_cap = cnl_sdw_reg_readw(data->sdw_shim, pdm_cap_offset);
	sdw->num_pdm_streams = (pdm_cap >> CNL_PDMSCAP_BSS_SHIFT) &
			CNL_PDMSCAP_BSS_MASK;

	sdw->pdm_streams = devm_kzalloc(&mstr->dev,
		sdw->num_pdm_streams * sizeof(struct cnl_sdw_pdi_stream),
		GFP_KERNEL);
	if (!sdw->pdm_streams)
		return -ENOMEM;

	sdw->num_in_pdm_streams = (pdm_cap >> CNL_PDMSCAP_ISS_SHIFT) &
			CNL_PDMSCAP_ISS_MASK;

	sdw->in_pdm_streams = devm_kzalloc(&mstr->dev,
		sdw->num_in_pdm_streams * sizeof(struct cnl_sdw_pdi_stream),
		GFP_KERNEL);

	if (!sdw->in_pdm_streams)
		return -ENOMEM;

	sdw->num_out_pdm_streams = (pdm_cap >> CNL_PDMSCAP_OSS_SHIFT) &
			CNL_PDMSCAP_OSS_MASK;
	/* Zero based value in register */
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

static int sdw_init(struct cnl_sdw *sdw, bool is_first_init)
{
	struct sdw_master *mstr = sdw->mstr;
	struct cnl_sdw_data *data = &sdw->data;
	int mcp_config, mcp_control, sync_reg, mcp_clockctrl;
	volatile int sync_update = 0;
	int timeout = 10; /* Try 10 times before timing out */
	int ret = 0;

	/* Power up the link controller */
	ret = sdw_power_up_link(sdw);
	if (ret)
		return ret;

	/* Initialize the IO control registers */
	sdw_init_shim(sdw);

	/* Switch the ownership to Master IP from glue logic */
	sdw_switch_to_mip(sdw);

	/* Set SyncPRD period */
	sync_reg = cnl_sdw_reg_readl(data->sdw_shim,  SDW_CNL_SYNC);
	sync_reg |= (SDW_CNL_DEFAULT_SYNC_PERIOD << CNL_SYNC_SYNCPRD_SHIFT);

	/* Set SyncPU bit */
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

	/*
	 * Set CMDSYNC bit based on Master ID
	 * Note that this bit is set only for the Master which will be
	 * running in aggregated mode (MMModeEN = 1). By doing
	 * this the gSync to Master IP to be masked inactive.
	 * Note that this is done in order to overcome hardware bug related
	 * to mis-alignment of gSync and frame.
	 */
	if (mstr->link_sync_mask) {

		sync_reg = cnl_sdw_reg_readl(data->sdw_shim,  SDW_CNL_SYNC);
		sync_reg |= (1 << (data->inst_id + CNL_SYNC_CMDSYNC_SHIFT));
		cnl_sdw_reg_writel(data->sdw_shim, SDW_CNL_SYNC, sync_reg);
	}

	/* Set clock divider to default value in default bank */
	mcp_clockctrl = cnl_sdw_reg_readl(data->sdw_regs,
				SDW_CNL_MCP_CLOCKCTRL0);
	mcp_clockctrl |= SDW_CNL_DEFAULT_CLK_DIVIDER;
	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_CLOCKCTRL0,
							mcp_clockctrl);

	/* Set the Frame shape init to default value */
	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_FRAMESHAPEINIT,
						SDW_CNL_DEFAULT_FRAME_SHAPE);


	/* Set the SSP interval to default value for both banks */
	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_SSPCTRL0,
					SDW_CNL_DEFAULT_SSP_INTERVAL);
	cnl_sdw_reg_writel(data->sdw_regs, SDW_CNL_MCP_SSPCTRL1,
					SDW_CNL_DEFAULT_SSP_INTERVAL);

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

	/* Initialize the phy control registers. */
	sdw_init_phyctrl(sdw);

	if (is_first_init) {
		/* Initlaize the ports */
		ret = sdw_port_pdi_init(sdw);
		if (ret) {
			dev_err(&mstr->dev, "SoundWire controller init failed %d\n",
				data->inst_id);
			sdw_power_down_link(sdw);
			return ret;
		}
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
			(SDW_PCM_STRM_START_INDEX * pdi_stream->pdi_num);
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

static void cnl_sdw_bra_prep_crc(u8 *txdata_buf,
		struct sdw_bra_block *block, int data_offset, int addr_offset)
{

	int addr = addr_offset;

	txdata_buf[addr++] = sdw_bus_compute_crc8((block->values + data_offset),
					block->num_bytes);
	txdata_buf[addr++] = 0x0;
	txdata_buf[addr++] = 0x0;
	txdata_buf[addr] |= ((0x2 & SDW_BRA_SOP_EOP_PDI_MASK)
					<< SDW_BRA_SOP_EOP_PDI_SHIFT);
}

static void cnl_sdw_bra_prep_data(u8 *txdata_buf,
		struct sdw_bra_block *block, int data_offset, int addr_offset)
{

	int i;
	int addr = addr_offset;

	for (i = 0; i < block->num_bytes; i += 2) {

		txdata_buf[addr++] = block->values[i + data_offset];
		if ((block->num_bytes - 1) - i)
			txdata_buf[addr++] = block->values[i + data_offset + 1];
		else
			txdata_buf[addr++] = 0;

		txdata_buf[addr++] = 0;
		txdata_buf[addr++] = 0;
	}
}

static void cnl_sdw_bra_prep_hdr(u8 *txdata_buf,
		struct sdw_bra_block *block, int rolling_id, int offset)
{

	u8 tmp_hdr[6] = {0, 0, 0, 0, 0, 0};
	u8 temp = 0x0;

	/*
	 * 6 bytes header
	 * 1st byte: b11001010
	 *		b11: Header is active
	 *		b0010: Device number 2 is selected
	 *		b1: Write operation
	 *		b0: MSB of BRA_NumBytes is 0
	 * 2nd byte: LSB of number of bytes
	 * 3rd byte to 6th byte: Slave register offset
	 */
	temp |= (SDW_BRA_HDR_ACTIVE & SDW_BRA_HDR_ACTIVE_MASK) <<
						SDW_BRA_HDR_ACTIVE_SHIFT;
	temp |= (block->slave_addr & SDW_BRA_HDR_SLV_ADDR_MASK) <<
						SDW_BRA_HDR_SLV_ADDR_SHIFT;
	temp |= (block->cmd & SDW_BRA_HDR_RD_WR_MASK) <<
						SDW_BRA_HDR_RD_WR_SHIFT;

	if (block->num_bytes > SDW_BRA_HDR_MSB_BYTE_CHK)
		temp |= (SDW_BRA_HDR_MSB_BYTE_SET & SDW_BRA_HDR_MSB_BYTE_MASK);
	else
		temp |= (SDW_BRA_HDR_MSB_BYTE_UNSET &
						SDW_BRA_HDR_MSB_BYTE_MASK);

	txdata_buf[offset + 0] = tmp_hdr[0] = temp;
	txdata_buf[offset + 1] = tmp_hdr[1] = block->num_bytes;
	txdata_buf[offset + 3] |= ((SDW_BRA_SOP_EOP_PDI_STRT_VALUE &
					SDW_BRA_SOP_EOP_PDI_MASK) <<
					SDW_BRA_SOP_EOP_PDI_SHIFT);

	txdata_buf[offset + 3] |= ((rolling_id & SDW_BRA_ROLLINGID_PDI_MASK)
					<< SDW_BRA_ROLLINGID_PDI_SHIFT);

	txdata_buf[offset + 4] = tmp_hdr[2] = ((block->reg_offset &
					SDW_BRA_HDR_SLV_REG_OFF_MASK24)
					>> SDW_BRA_HDR_SLV_REG_OFF_SHIFT24);

	txdata_buf[offset + 5] = tmp_hdr[3] = ((block->reg_offset &
					SDW_BRA_HDR_SLV_REG_OFF_MASK16)
					>> SDW_BRA_HDR_SLV_REG_OFF_SHIFT16);

	txdata_buf[offset + 8] = tmp_hdr[4] = ((block->reg_offset &
					SDW_BRA_HDR_SLV_REG_OFF_MASK8)
					>> SDW_BRA_HDR_SLV_REG_OFF_SHIFT8);

	txdata_buf[offset + 9] = tmp_hdr[5] = (block->reg_offset &
						SDW_BRA_HDR_SLV_REG_OFF_MASK0);

	/* CRC check */
	txdata_buf[offset + 0xc] = sdw_bus_compute_crc8(tmp_hdr,
							SDW_BRA_HEADER_SIZE);

	if (!block->cmd)
		txdata_buf[offset + 0xf] = ((SDW_BRA_SOP_EOP_PDI_END_VALUE &
						SDW_BRA_SOP_EOP_PDI_MASK) <<
						SDW_BRA_SOP_EOP_PDI_SHIFT);
}

static void cnl_sdw_bra_pdi_tx_config(struct sdw_master *mstr,
					struct cnl_sdw *sdw, bool enable)
{
	struct cnl_sdw_pdi_stream tx_pdi_stream;
	unsigned int tx_ch_map_offset, port_ctrl_offset, tx_pdi_config_offset;
	unsigned int port_ctrl = 0, tx_pdi_config = 0, tx_stream_config;
	int tx_pdi_ch_map = 0;

	if (enable) {
		/* DP0 PORT CTRL REG */
		port_ctrl_offset =  SDW_CNL_PORTCTRL + (SDW_BRA_PORT_ID *
						SDW_CNL_PORT_REG_OFFSET);

		port_ctrl &= ~(PORTCTRL_PORT_DIRECTION_MASK <<
					PORTCTRL_PORT_DIRECTION_SHIFT);

		port_ctrl |= ((SDW_BRA_BULK_ENABLE & SDW_BRA_BLK_EN_MASK) <<
				SDW_BRA_BLK_EN_SHIFT);

		port_ctrl |= ((SDW_BRA_BPT_PAYLOAD_TYPE &
						SDW_BRA_BPT_PYLD_TY_MASK) <<
						SDW_BRA_BPT_PYLD_TY_SHIFT);

		cnl_sdw_reg_writel(sdw->data.sdw_regs, port_ctrl_offset,
								port_ctrl);

		/* PDI0 Programming */
		tx_pdi_stream.l_ch_num = 0;
		tx_pdi_stream.h_ch_num = 0xF;
		tx_pdi_stream.pdi_num = SDW_BRA_PDI_TX_ID;
		/* TODO: Remove hardcoding */
		tx_pdi_stream.sdw_pdi_num = mstr->nr * 16 +
						tx_pdi_stream.pdi_num + 3;

		/* SNDWxPCMS2CM SHIM REG */
		tx_ch_map_offset =  SDW_CNL_CTLS2CM +
			(SDW_CNL_PCMSCHM_REG_OFFSET * mstr->nr);

		tx_pdi_ch_map |= (tx_pdi_stream.sdw_pdi_num &
						CNL_PCMSYCM_STREAM_MASK) <<
						CNL_PCMSYCM_STREAM_SHIFT;

		tx_pdi_ch_map |= (tx_pdi_stream.l_ch_num &
						CNL_PCMSYCM_LCHAN_MASK) <<
						CNL_PCMSYCM_LCHAN_SHIFT;

		tx_pdi_ch_map |= (tx_pdi_stream.h_ch_num &
						CNL_PCMSYCM_HCHAN_MASK) <<
						CNL_PCMSYCM_HCHAN_SHIFT;

		cnl_sdw_reg_writew(sdw->data.sdw_shim, tx_ch_map_offset,
				tx_pdi_ch_map);

		/* TX PDI0 CONFIG REG BANK 0 */
		tx_pdi_config_offset =  (SDW_CNL_PDINCONFIG0 +
						(tx_pdi_stream.pdi_num * 16));

		tx_pdi_config |= ((SDW_BRA_PORT_ID &
					PDINCONFIG_PORT_NUMBER_MASK) <<
					PDINCONFIG_PORT_NUMBER_SHIFT);

		tx_pdi_config |= (SDW_BRA_CHN_MASK <<
					PDINCONFIG_CHANNEL_MASK_SHIFT);

		tx_pdi_config |= (SDW_BRA_SOFT_RESET <<
					PDINCONFIG_PORT_SOFT_RESET_SHIFT);

		cnl_sdw_reg_writel(sdw->data.sdw_regs,
				tx_pdi_config_offset, tx_pdi_config);

		/* ALH STRMzCFG REG */
		tx_stream_config = cnl_sdw_reg_readl(sdw->data.alh_base,
					(tx_pdi_stream.sdw_pdi_num *
					ALH_CNL_STRMZCFG_OFFSET));

		tx_stream_config |= (CNL_STRMZCFG_DMAT_VAL &
						CNL_STRMZCFG_DMAT_MASK) <<
						CNL_STRMZCFG_DMAT_SHIFT;

		tx_stream_config |=  (0x0 & CNL_STRMZCFG_CHAN_MASK) <<
						CNL_STRMZCFG_CHAN_SHIFT;

		cnl_sdw_reg_writel(sdw->data.alh_base,
					(tx_pdi_stream.sdw_pdi_num *
					ALH_CNL_STRMZCFG_OFFSET),
					tx_stream_config);


	} else {

		/*
		 * TODO: There is official workaround which needs to be
		 * performed for PDI config register. The workaround
		 * is to perform SoftRst twice in order to clear
		 * PDI fifo contents.
		 */

	}
}

static void cnl_sdw_bra_pdi_rx_config(struct sdw_master *mstr,
					struct cnl_sdw *sdw, bool enable)
{

	struct cnl_sdw_pdi_stream rx_pdi_stream;
	unsigned int rx_ch_map_offset, rx_pdi_config_offset, rx_stream_config;
	unsigned int rx_pdi_config = 0;
	int rx_pdi_ch_map = 0;

	if (enable) {

		/* RX PDI1 Configuration */
		rx_pdi_stream.l_ch_num = 0;
		rx_pdi_stream.h_ch_num = 0xF;
		rx_pdi_stream.pdi_num = SDW_BRA_PDI_RX_ID;
		rx_pdi_stream.sdw_pdi_num = mstr->nr * 16 +
						rx_pdi_stream.pdi_num + 3;

		/* SNDWxPCMS3CM SHIM REG */
		rx_ch_map_offset = SDW_CNL_CTLS3CM +
				(SDW_CNL_PCMSCHM_REG_OFFSET * mstr->nr);

		rx_pdi_ch_map |= (rx_pdi_stream.sdw_pdi_num &
						CNL_PCMSYCM_STREAM_MASK) <<
						CNL_PCMSYCM_STREAM_SHIFT;

		rx_pdi_ch_map |= (rx_pdi_stream.l_ch_num &
						CNL_PCMSYCM_LCHAN_MASK) <<
						CNL_PCMSYCM_LCHAN_SHIFT;

		rx_pdi_ch_map |= (rx_pdi_stream.h_ch_num &
						CNL_PCMSYCM_HCHAN_MASK) <<
						CNL_PCMSYCM_HCHAN_SHIFT;

		cnl_sdw_reg_writew(sdw->data.sdw_shim, rx_ch_map_offset,
				rx_pdi_ch_map);

		/* RX PDI1 CONFIG REG */
		rx_pdi_config_offset =  (SDW_CNL_PDINCONFIG0 +
				(rx_pdi_stream.pdi_num * 16));

		rx_pdi_config |= ((SDW_BRA_PORT_ID &
						PDINCONFIG_PORT_NUMBER_MASK) <<
						PDINCONFIG_PORT_NUMBER_SHIFT);

		rx_pdi_config |= (SDW_BRA_CHN_MASK <<
						PDINCONFIG_CHANNEL_MASK_SHIFT);

		rx_pdi_config |= (SDW_BRA_SOFT_RESET <<
					PDINCONFIG_PORT_SOFT_RESET_SHIFT);

		cnl_sdw_reg_writel(sdw->data.sdw_regs,
				rx_pdi_config_offset, rx_pdi_config);


		/* ALH STRMzCFG REG */
		rx_stream_config = cnl_sdw_reg_readl(sdw->data.alh_base,
						(rx_pdi_stream.sdw_pdi_num *
						ALH_CNL_STRMZCFG_OFFSET));

		rx_stream_config |= (CNL_STRMZCFG_DMAT_VAL &
						CNL_STRMZCFG_DMAT_MASK) <<
						CNL_STRMZCFG_DMAT_SHIFT;

		rx_stream_config |=  (0 & CNL_STRMZCFG_CHAN_MASK) <<
						CNL_STRMZCFG_CHAN_SHIFT;

		cnl_sdw_reg_writel(sdw->data.alh_base,
						(rx_pdi_stream.sdw_pdi_num *
						ALH_CNL_STRMZCFG_OFFSET),
						rx_stream_config);

	} else {

		/*
		 * TODO: There is official workaround which needs to be
		 * performed for PDI config register. The workaround
		 * is to perform SoftRst twice in order to clear
		 * PDI fifo contents.
		 */

	}
}

static void cnl_sdw_bra_pdi_config(struct sdw_master *mstr, bool enable)
{
	struct cnl_sdw *sdw;

	/* Get driver data for master */
	sdw = sdw_master_get_drvdata(mstr);

	/* PDI0 configuration */
	cnl_sdw_bra_pdi_tx_config(mstr, sdw, enable);

	/* PDI1 configuration */
	cnl_sdw_bra_pdi_rx_config(mstr, sdw, enable);
}

static int cnl_sdw_bra_verify_footer(u8 *rx_buf, int offset)
{
	int ret = 0;
	u8 ftr_response;
	u8 ack_nack = 0;
	u8 ftr_result = 0;

	ftr_response = rx_buf[offset];

	/*
	 * ACK/NACK check
	 * NACK+ACK value from target:
	 * 00 -> Ignored
	 * 01 -> OK
	 * 10 -> Failed (Header CRC check failed)
	 * 11 -> Reserved
	 * NACK+ACK values at Target or initiator
	 * 00 -> Ignored
	 * 01 -> OK
	 * 10 -> Abort (Header cannot be trusted)
	 * 11 -> Abort (Header cannot be trusted)
	 */
	ack_nack = ((ftr_response > SDW_BRA_FTR_RESP_ACK_SHIFT) &
						SDW_BRA_FTR_RESP_ACK_MASK);
	if (ack_nack == SDW_BRA_ACK_NAK_IGNORED) {
		pr_info("BRA Packet Ignored\n");
		ret = -EINVAL;
	} else if (ack_nack == SDW_BRA_ACK_NAK_OK)
		pr_info("BRA: Packet OK\n");
	else if (ack_nack == SDW_BRA_ACK_NAK_FAILED_ABORT) {
		pr_info("BRA: Packet Failed/Reserved\n");
		return -EINVAL;
	} else if (ack_nack == SDW_BRA_ACK_NAK_RSVD_ABORT) {
		pr_info("BRA: Packet Reserved/Abort\n");
		return -EINVAL;
	}

	/*
	 * BRA footer result check
	 * Writes:
	 * 0 -> Good. Target accepted write payload
	 * 1 -> Bad. Target did not accept write payload
	 * Reads:
	 * 0 -> Good. Target completed read operation successfully
	 * 1 -> Bad. Target failed to complete read operation successfully
	 */
	ftr_result = (ftr_response > SDW_BRA_FTR_RESP_RES_SHIFT) >
						SDW_BRA_FTR_RESP_RES_MASK;
	if (ftr_result == SDW_BRA_FTR_RESULT_BAD) {
		pr_info("BRA: Read/Write operation failed on target side\n");
		/* Error scenario */
		return -EINVAL;
	}

	pr_info("BRA: Read/Write operation complete on target side\n");

	return ret;
}

static int cnl_sdw_bra_verify_hdr(u8 *rx_buf, int offset, bool *chk_footer,
	int roll_id)
{
	int ret = 0;
	u8 hdr_response, rolling_id;
	u8 ack_nack = 0;
	u8 not_ready = 0;

	/* Match rolling ID */
	hdr_response = rx_buf[offset];
	rolling_id = rx_buf[offset + SDW_BRA_ROLLINGID_PDI_INDX];

	rolling_id = (rolling_id & SDW_BRA_ROLLINGID_PDI_MASK);
	if (roll_id != rolling_id) {
		pr_info("BRA: Rolling ID doesn't match, returning error\n");
		return -EINVAL;
	}

	/*
	 * ACK/NACK check
	 * NACK+ACK value from target:
	 * 00 -> Ignored
	 * 01 -> OK
	 * 10 -> Failed (Header CRC check failed)
	 * 11 -> Reserved
	 * NACK+ACK values at Target or initiator
	 * 00 -> Ignored
	 * 01 -> OK
	 * 10 -> Abort (Header cannot be trusted)
	 * 11 -> Abort (Header cannot be trusted)
	 */
	ack_nack = ((hdr_response > SDW_BRA_HDR_RESP_ACK_SHIFT) &
						SDW_BRA_HDR_RESP_ACK_MASK);
	if (ack_nack == SDW_BRA_ACK_NAK_IGNORED) {
		pr_info("BRA: Packet Ignored rolling_id:%d\n", rolling_id);
		ret = -EINVAL;
	} else if (ack_nack == SDW_BRA_ACK_NAK_OK)
		pr_info("BRA: Packet OK rolling_id:%d\n", rolling_id);
	else if (ack_nack == SDW_BRA_ACK_NAK_FAILED_ABORT) {
		pr_info("BRA: Packet Failed/Abort rolling_id:%d\n", rolling_id);
		return -EINVAL;
	} else if (ack_nack == SDW_BRA_ACK_NAK_RSVD_ABORT) {
		pr_info("BRA: Packet Reserved/Abort rolling_id:%d\n", rolling_id);
		return -EINVAL;
	}

	/* BRA not ready check */
	not_ready = (hdr_response > SDW_BRA_HDR_RESP_NRDY_SHIFT) >
						SDW_BRA_HDR_RESP_NRDY_MASK;
	if (not_ready == SDW_BRA_TARGET_NOT_READY) {
		pr_info("BRA: Target not ready for read/write operation rolling_id:%d\n",
								rolling_id);
		chk_footer = false;
		return -EBUSY;
	}

	pr_info("BRA: Target ready for read/write operation rolling_id:%d\n", rolling_id);
	return ret;
}

static void cnl_sdw_bra_remove_data_padding(u8 *src_buf, u8 *dst_buf,
						u8 size) {

	int i;

	for (i = 0; i < size/2; i++) {

		*dst_buf++ = *src_buf++;
		*dst_buf++ = *src_buf++;
		src_buf++;
		src_buf++;
	}
}


static int cnl_sdw_bra_check_data(struct sdw_master *mstr,
	struct sdw_bra_block *block, struct bra_info *info) {

	int offset = 0, rolling_id = 0, tmp_offset = 0;
	int rx_crc_comp = 0, rx_crc_rvd = 0;
	int i, ret;
	bool chk_footer = true;
	int rx_buf_size = info->rx_block_size;
	u8 *rx_buf = info->rx_ptr;
	u8 *tmp_buf = NULL;

	/* TODO: Remove below hex dump print */
	print_hex_dump(KERN_DEBUG, "BRA RX DATA:", DUMP_PREFIX_OFFSET, 8, 4,
			     rx_buf, rx_buf_size, false);

	/* Allocate temporary buffer in case of read request */
	if (!block->cmd) {
		tmp_buf = kzalloc(block->num_bytes, GFP_KERNEL);
		if (!tmp_buf) {
			ret = -ENOMEM;
			goto error;
		}
	}

	/*
	 * TODO: From the response header and footer there is no mention of
	 * read or write packet so controller needs to keep transmit packet
	 * information in order to verify rx packet. Also the current
	 * approach used for error mechanism is any of the packet response
	 * is not success, just report the whole transfer failed to Slave.
	 */

	/*
	 * Verification of response packet for one known
	 * hardcoded configuration. This needs to be extended
	 * once we have dynamic algorithm integrated.
	 */

	/* 2 valid read response */
	for (i = 0; i < info->valid_packets; i++) {


		pr_info("BRA: Verifying packet number:%d with rolling id:%d\n",
						info->packet_info[i].packet_num,
						rolling_id);
		chk_footer = true;
		ret = cnl_sdw_bra_verify_hdr(rx_buf, offset, &chk_footer,
								rolling_id);
		if (ret < 0) {
			dev_err(&mstr->dev, "BRA: Header verification failed for packet number:%d\n",
					info->packet_info[i].packet_num);
			goto error;
		}

		/* Increment offset for header response */
		offset = offset + SDW_BRA_HEADER_RESP_SIZE_PDI;

		if (!block->cmd) {

			/* Remove PDI padding for data */
			cnl_sdw_bra_remove_data_padding(&rx_buf[offset],
					&tmp_buf[tmp_offset],
					info->packet_info[i].num_data_bytes);

			/* Increment offset for consumed data */
			offset = offset +
				(info->packet_info[i].num_data_bytes * 2);

			rx_crc_comp = sdw_bus_compute_crc8(&tmp_buf[tmp_offset],
					info->packet_info[i].num_data_bytes);

			/* Match Data CRC */
			rx_crc_rvd = rx_buf[offset];
			if (rx_crc_comp != rx_crc_rvd) {
				ret = -EINVAL;
				dev_err(&mstr->dev, "BRA: Data CRC doesn't match for packet number:%d\n",
					info->packet_info[i].packet_num);
				goto error;
			}

			/* Increment destination buffer with copied data */
			tmp_offset = tmp_offset +
					info->packet_info[i].num_data_bytes;

			/* Increment offset for CRC */
			offset = offset + SDW_BRA_DATA_CRC_SIZE_PDI;
		}

		if (chk_footer) {
			ret = cnl_sdw_bra_verify_footer(rx_buf, offset);
			if (ret < 0) {
				ret = -EINVAL;
				dev_err(&mstr->dev, "BRA: Footer verification failed for packet number:%d\n",
					info->packet_info[i].packet_num);
				goto error;
			}

		}

		/* Increment offset for footer response */
		offset = offset + SDW_BRA_HEADER_RESP_SIZE_PDI;

		/* Increment rolling id for next packet */
		rolling_id++;
		if (rolling_id > 0xF)
			rolling_id = 0;
	}

	/*
	 * No need to check for dummy responses from codec
	 * Assumption made here is that dummy packets are
	 * added in 1ms buffer only after valid packets.
	 */

	/* Copy data to codec buffer in case of read request */
	if (!block->cmd)
		memcpy(block->values, tmp_buf, block->num_bytes);

error:
	/* Free up temp buffer allocated in case of read request */
	if (!block->cmd)
		kfree(tmp_buf);

	/* Free up buffer allocated in cnl_sdw_bra_data_ops */
	kfree(info->tx_ptr);
	kfree(info->rx_ptr);
	kfree(info->packet_info);

	return ret;
}

static int cnl_sdw_bra_data_ops(struct sdw_master *mstr,
		struct sdw_bra_block *block, struct bra_info *info)
{

	struct sdw_bra_block tmp_block;
	int i;
	int tx_buf_size = 384, rx_buf_size = 1152;
	u8 *tx_buf = NULL, *rx_buf = NULL;
	int rolling_id = 0, total_bytes = 0, offset = 0, reg_offset = 0;
	int dummy_read = 0x0000;
	int ret;

	/*
	 * TODO: Run an algorithm here to identify the buffer size
	 * for TX and RX buffers + number of dummy packets (read
	 * or write) to be added for to align buffers.
	 */

	info->tx_block_size = tx_buf_size;
	info->tx_ptr = tx_buf = kzalloc(tx_buf_size, GFP_KERNEL);
	if (!tx_buf) {
		ret = -ENOMEM;
		goto error;
	}

	info->rx_block_size = rx_buf_size;
	info->rx_ptr = rx_buf = kzalloc(rx_buf_size, GFP_KERNEL);
	if (!rx_buf) {
		ret = -ENOMEM;
		goto error;
	}

	/* Fill valid packets transferred per millisecond buffer */
	info->valid_packets = 2;
	info->packet_info = kcalloc(info->valid_packets,
			sizeof(*info->packet_info),
			GFP_KERNEL);
	if (!info->packet_info) {
		ret = -ENOMEM;
		goto error;
	}

	/*
	 * Below code performs packet preparation for one known
	 * configuration.
	 * 1. 2 Valid Read request with 18 bytes each.
	 * 2. 22 dummy read packets with 18 bytes each.
	 */
	for (i = 0; i < info->valid_packets; i++) {
		tmp_block.slave_addr = block->slave_addr;
		tmp_block.cmd = block->cmd; /* Read Request */
		tmp_block.num_bytes = 18;
		tmp_block.reg_offset = block->reg_offset + reg_offset;
		tmp_block.values = NULL;
		reg_offset += tmp_block.num_bytes;

		cnl_sdw_bra_prep_hdr(tx_buf, &tmp_block, rolling_id, offset);
		/* Total Header size: Header + Header CRC size on PDI */
		offset += SDW_BRA_HEADER_TOTAL_SZ_PDI;

		if (block->cmd) {
			/*
			 * PDI data preparation in case of write request
			 * Assumption made here is data size from codec will
			 * be always an even number.
			 */
			cnl_sdw_bra_prep_data(tx_buf, &tmp_block,
					total_bytes, offset);
			offset += tmp_block.num_bytes * 2;

			/* Data CRC */
			cnl_sdw_bra_prep_crc(tx_buf, &tmp_block,
					total_bytes, offset);
			offset += SDW_BRA_DATA_CRC_SIZE_PDI;
		}

		total_bytes += tmp_block.num_bytes;
		rolling_id++;

		/* Fill packet info data structure */
		info->packet_info[i].packet_num = i + 1;
		info->packet_info[i].num_data_bytes = tmp_block.num_bytes;
	}

	/* Prepare dummy packets */
	for (i = 0; i < 22; i++) {
		tmp_block.slave_addr = block->slave_addr;
		tmp_block.cmd = 0; /* Read request */
		tmp_block.num_bytes = 18;
		tmp_block.reg_offset = dummy_read++;
		tmp_block.values = NULL;

		cnl_sdw_bra_prep_hdr(tx_buf, &tmp_block, rolling_id, offset);

		/* Total Header size: RD header + RD header CRC size on PDI */
		offset += SDW_BRA_HEADER_TOTAL_SZ_PDI;

		total_bytes += tmp_block.num_bytes;
		rolling_id++;
	}

	/* TODO: Remove below hex dump print */
	print_hex_dump(KERN_DEBUG, "BRA PDI VALID TX DATA:",
			DUMP_PREFIX_OFFSET, 8, 4, tx_buf, tx_buf_size, false);

	return 0;

error:
	kfree(info->tx_ptr);
	kfree(info->rx_ptr);
	kfree(info->packet_info);

	return ret;
}

static int cnl_sdw_xfer_bulk(struct sdw_master *mstr,
	struct sdw_bra_block *block)
{
	struct cnl_sdw *sdw = sdw_master_get_platdata(mstr);
	struct cnl_sdw_data *data = &sdw->data;
	struct cnl_bra_operation *ops = data->bra_data->bra_ops;
	struct bra_info info;
	int ret;

	/*
	 * 1. PDI Configuration
	 * 2. Prepare BRA packets including CRC calculation.
	 * 3. Configure TX and RX DMA in one shot mode.
	 * 4. Configure TX and RX Pipeline.
	 * 5. Run TX and RX DMA.
	 * 6. Run TX and RX pipelines.
	 * 7. Wait on completion for RX buffer.
	 * 8. Match TX and RX buffer packets and check for errors.
	 */

	/* Memset bra_info data structure */
	memset(&info, 0x0, sizeof(info));

	/* Fill master number in bra info data structure */
	info.mstr_num = mstr->nr;

	/* PDI Configuration (ON) */
	cnl_sdw_bra_pdi_config(mstr, true);

	/* Prepare TX buffer */
	ret = cnl_sdw_bra_data_ops(mstr, block, &info);
	if (ret < 0) {
		dev_err(&mstr->dev, "BRA: Request packet(s) creation failed\n");
		goto out;
	}

	/* Pipeline Setup  (ON) */
	ret = ops->bra_platform_setup(data->bra_data->drv_data, true, &info);
	if (ret < 0) {
		dev_err(&mstr->dev, "BRA: Pipeline setup failed\n");
		goto out;
	}

	/* Trigger START host DMA and pipeline */
	ret = ops->bra_platform_xfer(data->bra_data->drv_data, true, &info);
	if (ret < 0) {
		dev_err(&mstr->dev, "BRA: Pipeline start failed\n");
		goto out;
	}

	/* Trigger STOP host DMA and pipeline */
	ret = ops->bra_platform_xfer(data->bra_data->drv_data, false, &info);
	if (ret < 0) {
		dev_err(&mstr->dev, "BRA: Pipeline stop failed\n");
		goto out;
	}

	/* Pipeline Setup  (OFF) */
	ret = ops->bra_platform_setup(data->bra_data->drv_data, false, &info);
	if (ret < 0) {
		dev_err(&mstr->dev, "BRA: Pipeline de-setup failed\n");
		goto out;
	}

	/* Verify RX buffer */
	ret = cnl_sdw_bra_check_data(mstr, block, &info);
	if (ret < 0) {
		dev_err(&mstr->dev, "BRA: Response packet(s) incorrect\n");
		goto out;
	}

	/* PDI Configuration (OFF) */
	cnl_sdw_bra_pdi_config(mstr, false);

out:
	return ret;
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
			int cur_clk_div, int bank)
{
	struct cnl_sdw *sdw = sdw_master_get_drvdata(mstr);
	struct cnl_sdw_data *data = &sdw->data;
	int mcp_clockctrl_offset, mcp_clockctrl;


	/* TODO: Retrieve divider value or get value directly from calling
	 * function
	 */
	int divider = (cur_clk_div - 1);

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
	volatile int sync_update = 0;
	int timeout = 10;


	sync_reg = cnl_sdw_reg_readl(data->sdw_shim,  SDW_CNL_SYNC);
	/* If waiting for synchronization set the go bit, else return */
	if (!(sync_reg & SDW_CMDSYNC_SET_MASK))
		return 0;
	sync_reg |= (CNL_SYNC_SYNCGO_MASK << CNL_SYNC_SYNCGO_SHIFT);
	cnl_sdw_reg_writel(data->sdw_shim, SDW_CNL_SYNC, sync_reg);

	do {
		sync_update = cnl_sdw_reg_readl(data->sdw_shim,  SDW_CNL_SYNC);
		if ((sync_update &
			(CNL_SYNC_SYNCGO_MASK << CNL_SYNC_SYNCGO_SHIFT)) == 0)
			break;
		msleep(20);
		timeout--;

	}  while (timeout);

	if ((sync_update &
		(CNL_SYNC_SYNCGO_MASK << CNL_SYNC_SYNCGO_SHIFT)) != 0) {
		dev_err(&mstr->dev, "Failed to set sync go\n");
		return -EIO;
	}
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
	ret = sdw_init(sdw, true);
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
	ret = sdw_master_prep_for_clk_stop(sdw->mstr);
	if (ret)
		return ret;

	/* Call bus function to broadcast the clock stop now */
	ret = sdw_master_stop_clock(sdw->mstr);
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
	u16 wake_en, wake_sts;
	int ret;
	struct cnl_sdw_data *data = &sdw->data;

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
	ret = sdw_init(sdw, false);
	if (ret < 0) {
		pr_err("sdw_init fail: %d\n", ret);
		return ret;
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
	ret = sdw_mstr_deprep_after_clk_start(sdw->mstr);
	if (ret)
		return ret;

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int cnl_sdw_sleep_resume(struct device *dev)
{
	return cnl_sdw_runtime_resume(dev);
}
static int cnl_sdw_sleep_suspend(struct device *dev)
{
	return cnl_sdw_runtime_suspend(dev);
}
#else
#define cnl_sdw_sleep_suspend NULL
#define cnl_sdw_sleep_resume NULL
#endif /* CONFIG_PM_SLEEP */
#else
#define cnl_sdw_runtime_suspend NULL
#define cnl_sdw_runtime_resume NULL
#endif /* CONFIG_PM */


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
