/*
 *  sdw_cnl.h - Shared header file for intel soundwire controller driver.
 *
 *  Copyright (C) 2014-2015 Intel Corp
 *  Author:  Hardik Shah  <hardik.t.shah@intel.com>
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

#ifndef _LINUX_SDW_CNL_H
#define _LINUX_SDW_CNL_H

#include <linux/sdw_bus.h>


#define SDW_CNL_PM_TIMEOUT	3000 /* ms */

#define CNL_SDW_MAX_PORTS				9

/* Maximum number hardware tries to send command if the command failed */
#define CNL_SDW_MAX_CMD_RETRIES			15
/* Standard allows 32 frames delay max between PREQ and Ping command
 * We kept midway in hardware
 */
#define CNL_SDW_MAX_PREQ_DELAY			15

/* Reset Delay for hw controlled reset
 * Reset length = 4096+(ResetDelay*256) clock cycles
 */
#define CNL_SDW_RESET_DELAY				15

#define CNL_SDW_SHIM_OFFSET		0x2C000
#define CNL_SDW_LINK_0_OFFSET		0x30000
#define CNL_SDW_LINK_1_OFFSET		0x40000
#define CNL_SDW_LINK_2_OFFSET		0x50000
#define CNL_SDW_LINK_3_OFFSET		0x60000

enum cnl_sdw_pdi_stream_type {
	CNL_SDW_PDI_TYPE_PCM = 0,
	CNL_SDW_PDI_TYPE_PDM = 1,
};

struct cnl_sdw_pdi_stream {
	int pdi_num;
	int sdw_pdi_num;
	int ch_cnt;
	bool allocated;
	int port_num;
	enum sdw_data_direction direction;
	int h_ch_num, l_ch_num;
	struct list_head node;

};

struct cnl_sdw_port {
	int port_num;
	int allocated;
	bool port_type;
	int ch_cnt;
	enum sdw_data_direction direction;
	struct cnl_sdw_pdi_stream *pdi_stream;
};

struct bra_packet_info {
	u8 packet_num;
	u8 num_data_bytes;
};

struct bra_info {
	unsigned int mstr_num;
	u8 *tx_ptr;
	u8 *rx_ptr;
	unsigned int tx_block_size;
	unsigned int rx_block_size;
	u8 valid_packets;
	struct bra_packet_info *packet_info;
};

struct cnl_bra_operation {
	int (*bra_platform_setup)(void *context, bool is_enable,
						struct bra_info *info);
	int (*bra_platform_xfer)(void *context, bool is_enable,
						struct bra_info *info);
};

struct cnl_sdw_bra_cfg {
	void *drv_data;
	struct cnl_bra_operation *bra_ops;
};

struct cnl_sdw_data {
	/* SoundWire IP registers per instance */
	void __iomem *sdw_regs;
	/* SoundWire shim registers */
	void __iomem *sdw_shim;
	/* This is just for enaling SoundWire interrupts */
	void __iomem *alh_base;
	/* HDA interrupt */
	int irq;
	/* Instance id */
	int inst_id;
	/* BRA data pointer */
	struct cnl_sdw_bra_cfg *bra_data;
};

struct cnl_sdw_port *cnl_sdw_alloc_port(struct sdw_master *mstr, int ch_count,
				enum sdw_data_direction direction,
				enum cnl_sdw_pdi_stream_type stream_type);
void cnl_sdw_free_port(struct sdw_master *mstr, int port_num);
int cnl_sdw_get_master_caps(struct device *dev,
				struct sdw_master_capabilities *m_cap);
int cnl_sdw_get_master_dev_caps(struct device *dev,
		struct sdw_master_capabilities *m_cap, int dev_port_num);
#endif

