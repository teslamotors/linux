/*
 *  sdw_bwcalc.c - SoundWire Bus BW calculation & CHN Enabling implementation
 *
 *  Copyright (C) 2015-2016 Intel Corp
 *  Author:  Sanyog Kale <sanyog.r.kale@intel.com>
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
 */

#include <linux/kernel.h>
#include <linux/sdw_bus.h>
#include "sdw_priv.h"
#include <linux/delay.h>
#include <linux/sdw/sdw_registers.h>


#define MAXCLOCKFREQ		6

#ifdef CONFIG_SND_SOC_SVFPGA
/* For PDM Capture, frameshape used is 50x10 */
int rows[MAX_NUM_ROWS] = {50, 100, 48, 60, 64, 72, 75, 80, 90,
		     96, 125, 144, 147, 120, 128, 150,
		     160, 180, 192, 200, 240, 250, 256};

int cols[MAX_NUM_COLS] = {10, 2, 4, 6, 8, 12, 14, 16};

int clock_freq[MAXCLOCKFREQ] = {19200000, 19200000,
				19200000, 19200000,
				19200000, 19200000};

#else
/* TBD: Currently we are using 100x2 as frame shape. to be removed later */
int rows[MAX_NUM_ROWS] = {100, 48, 50, 60, 64, 72, 75, 80, 90,
		     96, 125, 144, 147, 120, 128, 150,
		     160, 180, 192, 200, 240, 250, 256};

int cols[MAX_NUM_COLS] = {2, 4, 6, 8, 10, 12, 14, 16};

/*
 * TBD: Get supported clock frequency from ACPI and store
 * it in master data structure.
 */
/* Currently only 9.6MHz clock frequency used */
int clock_freq[MAXCLOCKFREQ] = {9600000, 9600000,
				9600000, 9600000,
				9600000, 9600000};
#endif

struct sdw_num_to_col sdw_num_col_mapping[MAX_NUM_COLS] = {
	{0, 2}, {1, 4}, {2, 6}, {3, 8}, {4, 10}, {5, 12}, {6, 14}, {7, 16},
};

struct sdw_num_to_row sdw_num_row_mapping[MAX_NUM_ROWS] = {
	{0, 48}, {1, 50}, {2, 60}, {3, 64}, {4, 75}, {5, 80}, {6, 125},
	{7, 147}, {8, 96}, {9, 100}, {10, 120}, {11, 128}, {12, 150},
	{13, 160}, {14, 250}, {16, 192}, {17, 200}, {18, 240}, {19, 256},
	{20, 72}, {21, 144}, {22, 90}, {23, 180},
};

/**
 * sdw_bus_bw_init - returns Success
 *
 *
 * This function is called from sdw_init function when bus driver
 * gets intitalized. This function performs all the generic
 * intializations required for BW control.
 */
int sdw_bus_bw_init(void)
{
	int r, c, rowcolcount = 0;
	int control_bits = 48;

	for (c = 0; c < MAX_NUM_COLS; c++) {

		for (r = 0; r < MAX_NUM_ROWS; r++) {
			sdw_core.rowcolcomb[rowcolcount].col = cols[c];
			sdw_core.rowcolcomb[rowcolcount].row = rows[r];
			sdw_core.rowcolcomb[rowcolcount].control_bits =
				control_bits;
			sdw_core.rowcolcomb[rowcolcount].data_bits =
				(cols[c] * rows[r]) - control_bits;
			rowcolcount++;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sdw_bus_bw_init);


/**
 * sdw_mstr_bw_init - returns Success
 *
 *
 * This function is called from sdw_register_master function
 * for each master controller gets register. This function performs
 * all the intializations per master controller required for BW control.
 */
int sdw_mstr_bw_init(struct sdw_bus *sdw_bs)
{
	struct sdw_master_capabilities *sdw_mstr_cap = NULL;

	/* Initialize required parameters in bus structure */
	sdw_bs->bandwidth = 0;
	sdw_bs->system_interval = 0;
	sdw_bs->frame_freq = 0;
	sdw_bs->clk_state = SDW_CLK_STATE_ON;

	/* TBD: to be removed later */
	/* Assumption is these should be already filled */
	sdw_mstr_cap = &sdw_bs->mstr->mstr_capabilities;
	sdw_mstr_cap->monitor_handover_supported = false;
	sdw_mstr_cap->highphy_capable = false;

#ifdef CONFIG_SND_SOC_SVFPGA
	/* TBD: For PDM capture to be removed later */
	sdw_bs->clk_freq = 9.6 * 1000 * 1000 * 2;
	sdw_mstr_cap->base_clk_freq = 9.6 * 1000 * 1000 * 2;
#else
	/* TBD: Base Clock frequency should be read from
	 * master capabilities
	 * Currenly hardcoding to 9.6MHz
	 */
	sdw_bs->clk_freq = 9.6 * 1000 * 1000;
	sdw_mstr_cap->base_clk_freq = 9.6 * 1000 * 1000;

#endif
	return 0;
}
EXPORT_SYMBOL_GPL(sdw_mstr_bw_init);


/**
 * sdw_get_col_to_num
 *
 * Returns column number from the mapping.
 */
int sdw_get_col_to_num(int col)
{
	int i;

	for (i = 0; i <= MAX_NUM_COLS; i++) {
		if (sdw_num_col_mapping[i].col == col)
			return sdw_num_col_mapping[i].num;
	}

	return 0; /* Lowest Column number = 2 */
}


/**
 * sdw_get_row_to_num
 *
 * Returns row number from the mapping.
 */
int sdw_get_row_to_num(int row)
{
	int i;

	for (i = 0; i <= MAX_NUM_ROWS; i++) {
		if (sdw_num_row_mapping[i].row == row)
			return sdw_num_row_mapping[i].num;
	}

	return 0; /* Lowest Row number = 48 */
}

/*
 * sdw_lcm - returns LCM of two numbers
 *
 *
 * This function is called BW calculation function to find LCM
 * of two numbers.
 */
int sdw_lcm(int num1, int num2)
{
	int max;

	/* maximum value is stored in variable max */
	max = (num1 > num2) ? num1 : num2;

	while (1) {
		if (max%num1 == 0 && max%num2 == 0)
			break;
		++max;
	}

	return max;
}


/*
 * sdw_cfg_slv_params - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function configures slave registers for
 * transport and port parameters.
 */
int sdw_cfg_slv_params(struct sdw_bus *mstr_bs,
		struct sdw_slave_runtime *slv_rt,
		struct sdw_transport_params *t_slv_params,
		struct sdw_port_params *p_slv_params)
{
	struct sdw_msg wr_msg, wr_msg1, rd_msg;
	int ret = 0;
	int banktouse;
	u8 wbuf[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	u8 wbuf1[2] = {0, 0};
	u8 rbuf[1] = {0};


#ifdef CONFIG_SND_SOC_SVFPGA
	/*
	 * The below hardcoding is required
	 * for running PDM capture with SV conora card
	 * because the transport params of card is not
	 * same as master parameters. Also not all
	 * standard registers are valid.
	 */
	t_slv_params->blockgroupcontrol_valid = false;
	t_slv_params->sample_interval = 50;
	t_slv_params->offset1 = 0;
	t_slv_params->offset2 = 0;
	t_slv_params->hstart = 1;
	t_slv_params->hstop = 6;
	p_slv_params->word_length = 30;
#endif

	/* Program slave alternate bank with all transport parameters */
	/* DPN_BlockCtrl2 */
	wbuf[0] = t_slv_params->blockgroupcontrol;
	/* DPN_SampleCtrl1 */
	wbuf[1] = (t_slv_params->sample_interval - 1) &
			SDW_DPN_SAMPLECTRL1_LOW_MASK;
	wbuf[2] = ((t_slv_params->sample_interval - 1) >> 8) &
			SDW_DPN_SAMPLECTRL1_LOW_MASK; /* DPN_SampleCtrl2 */
	wbuf[3] = t_slv_params->offset1; /* DPN_OffsetCtrl1 */
	wbuf[4] = t_slv_params->offset2; /* DPN_OffsetCtrl1 */
	/*  DPN_HCtrl  */
	wbuf[5] = (t_slv_params->hstop | (t_slv_params->hstart << 4));
	wbuf[6] = t_slv_params->blockpackingmode; /* DPN_BlockCtrl3 */
	wbuf[7] = t_slv_params->lanecontrol; /* DPN_LaneCtrl */

	/* Get current bank in use from bus structure*/
	banktouse = mstr_bs->active_bank;
	banktouse = !banktouse;
	/* Program slave alternate bank with all port parameters */
	rd_msg.addr = SDW_DPN_PORTCTRL +
		(SDW_NUM_DATA_PORT_REGISTERS * t_slv_params->num);
	rd_msg.ssp_tag = 0x0;
	rd_msg.flag = SDW_MSG_FLAG_READ;
	rd_msg.len = 1;
	rd_msg.slave_addr =  slv_rt->slave->slv_number;
	rd_msg.buf = rbuf;
	rd_msg.addr_page1 = 0x0;
	rd_msg.addr_page2 = 0x0;

	ret = sdw_slave_transfer(mstr_bs->mstr, &rd_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&mstr_bs->mstr->dev, "Register transfer failed\n");
		goto out;
	}


	wbuf1[0] = (p_slv_params->port_flow_mode |
			(p_slv_params->port_data_mode <<
			SDW_DPN_PORTCTRL_PORTDATAMODE_SHIFT) |
			(rbuf[0]));

	wbuf1[1] = (p_slv_params->word_length - 1);

	/* Check whether address computed is correct for both cases */
	wr_msg.addr = ((SDW_DPN_BLOCKCTRL2 +
				(1 * (!t_slv_params->blockgroupcontrol_valid))
				+ (SDW_BANK1_REGISTER_OFFSET * banktouse)) +
			(SDW_NUM_DATA_PORT_REGISTERS * t_slv_params->num));

	wr_msg1.addr =  SDW_DPN_PORTCTRL +
		(SDW_NUM_DATA_PORT_REGISTERS * t_slv_params->num);

	wr_msg.ssp_tag = 0x0;
	wr_msg.flag = SDW_MSG_FLAG_WRITE;
#ifdef CONFIG_SND_SOC_SVFPGA
	wr_msg.len = (5 + (1 * (t_slv_params->blockgroupcontrol_valid)));
#else
	wr_msg.len = (7 + (1 * (t_slv_params->blockgroupcontrol_valid)));
#endif
	wr_msg.slave_addr = slv_rt->slave->slv_number;
	wr_msg.buf = &wbuf[0 + (1 * (!t_slv_params->blockgroupcontrol_valid))];
	wr_msg.addr_page1 = 0x0;
	wr_msg.addr_page2 = 0x0;

	wr_msg1.ssp_tag = 0x0;
	wr_msg1.flag = SDW_MSG_FLAG_WRITE;
	wr_msg1.len = 2;
	wr_msg1.slave_addr = slv_rt->slave->slv_number;
	wr_msg1.buf = &wbuf1[0];
	wr_msg1.addr_page1 = 0x0;
	wr_msg1.addr_page2 = 0x0;

	ret = sdw_slave_transfer(mstr_bs->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&mstr_bs->mstr->dev, "Register transfer failed\n");
		goto out;
	}


	ret = sdw_slave_transfer(mstr_bs->mstr, &wr_msg1, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&mstr_bs->mstr->dev, "Register transfer failed\n");
		goto out;
	}

out:
	return ret;
}


/*
 * sdw_cfg_mstr_params - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function configures master registers for
 * transport and port parameters.
 */
int sdw_cfg_mstr_params(struct sdw_bus *mstr_bs,
		struct sdw_transport_params *t_mstr_params,
		struct sdw_port_params *p_mstr_params)
{
	struct sdw_mstr_driver *ops = mstr_bs->mstr->driver;
	int banktouse, ret = 0;

	/* 1. Get current bank in use from bus structure*/
	banktouse = mstr_bs->active_bank;
	banktouse = !banktouse;

	/* 2. Set Master Xport Params */
	if (ops->mstr_port_ops->dpn_set_port_transport_params) {
		ret = ops->mstr_port_ops->dpn_set_port_transport_params
				(mstr_bs->mstr, t_mstr_params, banktouse);
		if (ret < 0)
			return ret;
	}

	/* 3. Set Master Port Params */
	if (ops->mstr_port_ops->dpn_set_port_params) {
		ret = ops->mstr_port_ops->dpn_set_port_params
				(mstr_bs->mstr, p_mstr_params, banktouse);
		if (ret < 0)
			return ret;
	}

	return 0;
}


/*
 * sdw_cfg_mstr_slv - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function call master/slave transport/port
 * params configuration API's, called from sdw_bus_calc_bw
 * & sdw_bus_calc_bw_dis API's.
 */
int sdw_cfg_mstr_slv(struct sdw_bus *sdw_mstr_bs,
		struct sdw_mstr_runtime *sdw_mstr_bs_rt,
		bool is_master)
{
	struct sdw_transport_params *t_params, *t_slv_params;
	struct sdw_port_params *p_params, *p_slv_params;
	struct sdw_slave_runtime *slv_rt = NULL;
	struct sdw_port_runtime *port_rt, *port_slv_rt;
	int ret = 0;

	if (is_master) {
		/* should not compute any transport params */
		if (sdw_mstr_bs_rt->rt_state == SDW_STATE_UNPREPARE_RT)
			return 0;

		list_for_each_entry(port_rt,
			&sdw_mstr_bs_rt->port_rt_list, port_node) {

			/* Transport and port parameters */
			t_params = &port_rt->transport_params;
			p_params = &port_rt->port_params;

			p_params->num = port_rt->port_num;
			p_params->word_length =
				sdw_mstr_bs_rt->stream_params.bps;
			p_params->port_flow_mode = 0x0; /* Isochronous Mode */
			p_params->port_data_mode = 0x0; /* Normal Mode */

			/* Configure xport params and port params for master */
			ret = sdw_cfg_mstr_params(sdw_mstr_bs,
					t_params, p_params);
			if (ret < 0)
				return ret;

			/* Since one port per master runtime,
			 * breaking port_list loop
			 * TBD: to be extended for multiple port support
			 */

			break;
		}

	} else {


		list_for_each_entry(slv_rt,
			&sdw_mstr_bs_rt->slv_rt_list, slave_node) {

			if (slv_rt->slave == NULL)
				break;

			/* should not compute any transport params */
			if (slv_rt->rt_state == SDW_STATE_UNPREPARE_RT)
				continue;

			list_for_each_entry(port_slv_rt,
				&slv_rt->port_rt_list, port_node) {

				/* Fill in port params here */
				port_slv_rt->port_params.num =
					port_slv_rt->port_num;
				port_slv_rt->port_params.word_length =
					slv_rt->stream_params.bps;
				/* Isochronous Mode */
				port_slv_rt->port_params.port_flow_mode = 0x0;
				/* Normal Mode */
				port_slv_rt->port_params.port_data_mode = 0x0;
				t_slv_params = &port_slv_rt->transport_params;
				p_slv_params = &port_slv_rt->port_params;

				/* Configure xport  & port params for slave */
				ret = sdw_cfg_slv_params(sdw_mstr_bs,
					slv_rt, t_slv_params, p_slv_params);
				if (ret < 0)
					return ret;

				/* Since one port per slave runtime,
				 * breaking port_list loop
				 * TBD: to be extended for multiple
				 * port support
				 */

				break;
			}
		}

	}

	return 0;
}


/*
 * sdw_cpy_params_mstr_slv - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function copies/configure master/slave transport &
 * port params to alternate bank.
 *
 */
int sdw_cpy_params_mstr_slv(struct sdw_bus *sdw_mstr_bs,
		struct sdw_mstr_runtime *sdw_mstr_bs_rt)
{
	struct sdw_slave_runtime *slv_rt = NULL;
	struct sdw_port_runtime *port_rt, *port_slv_rt;
	struct sdw_transport_params *t_params, *t_slv_params;
	struct sdw_port_params *p_params, *p_slv_params;
	int ret = 0;

	list_for_each_entry(slv_rt,
			&sdw_mstr_bs_rt->slv_rt_list, slave_node) {

		if (slv_rt->slave == NULL)
			break;

		list_for_each_entry(port_slv_rt,
				&slv_rt->port_rt_list, port_node) {

			/* Fill in port params here */
			port_slv_rt->port_params.num = port_slv_rt->port_num;
			port_slv_rt->port_params.word_length =
				slv_rt->stream_params.bps;
			/* Normal/Isochronous Mode */
			port_slv_rt->port_params.port_flow_mode = 0x0;
			/* Normal Mode */
			port_slv_rt->port_params.port_data_mode = 0x0;
			t_slv_params = &port_slv_rt->transport_params;
			p_slv_params = &port_slv_rt->port_params;

			/* Configure xport & port params for slave */
			ret = sdw_cfg_slv_params(sdw_mstr_bs,
					slv_rt, t_slv_params, p_slv_params);
			if (ret < 0)
				return ret;

			/*
			 * Since one port per slave runtime,
			 * breaking port_list loop
			 * TBD: to be extended for multiple port support
			 */
			break;
		}
	}


	list_for_each_entry(port_rt,
			&sdw_mstr_bs_rt->port_rt_list, port_node) {

		/* Transport and port parameters */
		t_params = &port_rt->transport_params;
		p_params = &port_rt->port_params;


		p_params->num = port_rt->port_num;
		p_params->word_length = sdw_mstr_bs_rt->stream_params.bps;
		p_params->port_flow_mode = 0x0; /* Normal/Isochronous Mode */
		p_params->port_data_mode = 0x0; /* Normal Mode */

		/* Configure xport params and port params for master */
		ret = sdw_cfg_mstr_params(sdw_mstr_bs, t_params, p_params);
		if (ret < 0)
			return ret;

		/* Since one port per slave runtime, breaking port_list loop
		 * TBD: to be extended for multiple port support
		 */
		break;
	}

	return 0;
}


/*
 * sdw_cfg_slv_enable_disable - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function enable/disable slave port channels.
 */
int sdw_cfg_slv_enable_disable(struct sdw_bus *mstr_bs,
	struct sdw_slave_runtime *slv_rt_strm,
	struct sdw_port_runtime *port_slv_strm,
	struct port_chn_en_state *chn_en)
{
	struct sdw_msg wr_msg, rd_msg;
	int ret = 0;
	int banktouse;
	u8 wbuf[1] = {0};
	u8 rbuf[1] = {0};

	/* Get current bank in use from bus structure*/
	banktouse = mstr_bs->active_bank;
	if ((chn_en->is_activate) || (chn_en->is_bank_sw))
		banktouse = !banktouse;

	rd_msg.addr = wr_msg.addr = ((SDW_DPN_CHANNELEN +
				(SDW_BANK1_REGISTER_OFFSET * banktouse)) +
			(SDW_NUM_DATA_PORT_REGISTERS *
			 port_slv_strm->port_num));

	rd_msg.ssp_tag = 0x0;
	rd_msg.flag = SDW_MSG_FLAG_READ;
	rd_msg.len = 1;
	rd_msg.slave_addr = slv_rt_strm->slave->slv_number;
	rd_msg.buf = rbuf;
	rd_msg.addr_page1 = 0x0;
	rd_msg.addr_page2 = 0x0;

	wr_msg.ssp_tag = 0x0;
	wr_msg.flag = SDW_MSG_FLAG_WRITE;
	wr_msg.len = 1;
	wr_msg.slave_addr = slv_rt_strm->slave->slv_number;
	wr_msg.buf = wbuf;
	wr_msg.addr_page1 = 0x0;
	wr_msg.addr_page2 = 0x0;


	if (chn_en->is_activate) {

		/*
		 * 1. slave port enable_ch_pre
		 * --> callback
		 * --> no callback available
		 */

		/* 2. slave port enable */
		ret = sdw_slave_transfer(mstr_bs->mstr, &rd_msg, 1);
		if (ret != 1) {
			ret = -EINVAL;
			dev_err(&mstr_bs->mstr->dev,
					"Register transfer failed\n");
			goto out;
		}

		wbuf[0] = (rbuf[0] | port_slv_strm->channel_mask);

		ret = sdw_slave_transfer(mstr_bs->mstr, &wr_msg, 1);
		if (ret != 1) {
			ret = -EINVAL;
			dev_err(&mstr_bs->mstr->dev,
					"Register transfer failed\n");
			goto out;
		}

		rbuf[0] = 0;
		ret = sdw_slave_transfer(mstr_bs->mstr, &rd_msg, 1);
		if (ret != 1) {
			ret = -EINVAL;
			dev_err(&mstr_bs->mstr->dev,
					"Register transfer failed\n");
			goto out;
		}

		/*
		 * 3. slave port enable post pre
		 * --> callback
		 * --> no callback available
		 */
		slv_rt_strm->rt_state = SDW_STATE_ENABLE_RT;

	} else {

		/*
		 * 1. slave port enable_ch_unpre
		 * --> callback
		 * --> no callback available
		 */

		/* 2. slave port disable */
		ret = sdw_slave_transfer(mstr_bs->mstr, &rd_msg, 1);
		if (ret != 1) {
			ret = -EINVAL;
			dev_err(&mstr_bs->mstr->dev,
					"Register transfer failed\n");
			goto out;
		}

		wbuf[0] = (rbuf[0] & ~(port_slv_strm->channel_mask));

		ret = sdw_slave_transfer(mstr_bs->mstr, &wr_msg, 1);
		if (ret != 1) {
			ret = -EINVAL;
			dev_err(&mstr_bs->mstr->dev,
					"Register transfer failed\n");
			goto out;
		}

		rbuf[0] = 0;
		ret = sdw_slave_transfer(mstr_bs->mstr, &rd_msg, 1);
		if (ret != 1) {
			ret = -EINVAL;
			dev_err(&mstr_bs->mstr->dev,
					"Register transfer failed\n");
			goto out;
		}

		/*
		 * 3. slave port enable post unpre
		 * --> callback
		 * --> no callback available
		 */
		if (!chn_en->is_bank_sw)
			slv_rt_strm->rt_state = SDW_STATE_DISABLE_RT;

	}

out:
	return ret;

}


/*
 * sdw_cfg_mstr_activate_disable - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function enable/disable master port channels.
 */
int sdw_cfg_mstr_activate_disable(struct sdw_bus *mstr_bs,
		struct sdw_mstr_runtime *mstr_rt_strm,
		struct sdw_port_runtime *port_mstr_strm,
		struct port_chn_en_state *chn_en)
{
	struct sdw_mstr_driver *ops = mstr_bs->mstr->driver;
	struct sdw_activate_ch activate_ch;
	int banktouse, ret = 0;

	activate_ch.num = port_mstr_strm->port_num;
	activate_ch.ch_mask = port_mstr_strm->channel_mask;
	activate_ch.activate = chn_en->is_activate; /* Enable/Disable */

	/* Get current bank in use from bus structure*/
	banktouse = mstr_bs->active_bank;
	if ((chn_en->is_activate) || (chn_en->is_bank_sw))
		banktouse = !banktouse;


	/* 1. Master port enable_ch_pre */
	if (ops->mstr_port_ops->dpn_port_activate_ch_pre) {
		ret = ops->mstr_port_ops->dpn_port_activate_ch_pre
			(mstr_bs->mstr, &activate_ch, banktouse);
		if (ret < 0)
			return ret;
	}

	/* 2. Master port enable */
	if (ops->mstr_port_ops->dpn_port_activate_ch) {
		ret = ops->mstr_port_ops->dpn_port_activate_ch(mstr_bs->mstr,
				&activate_ch, banktouse);
		if (ret < 0)
			return ret;
	}

	/* 3. Master port enable_ch_post */
	if (ops->mstr_port_ops->dpn_port_activate_ch_post) {
		ret = ops->mstr_port_ops->dpn_port_activate_ch_post
			(mstr_bs->mstr, &activate_ch, banktouse);
		if (ret < 0)
			return ret;
	}

	if (chn_en->is_activate)
		mstr_rt_strm->rt_state = SDW_STATE_ENABLE_RT;
	else if (!chn_en->is_bank_sw)
		mstr_rt_strm->rt_state = SDW_STATE_DISABLE_RT;

	return 0;
}


/*
 * sdw_en_dis_mstr_slv - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function call master/slave enable/disable
 * channel API's.
 */
int sdw_en_dis_mstr_slv(struct sdw_bus *sdw_mstr_bs,
		struct sdw_runtime *sdw_rt, bool is_act)
{
	struct sdw_slave_runtime *slv_rt_strm = NULL;
	struct sdw_port_runtime *port_slv_strm, *port_mstr_strm;
	struct sdw_mstr_runtime *mstr_rt_strm = NULL;
	struct port_chn_en_state chn_en;
	int ret = 0;

	if (is_act)
		chn_en.is_bank_sw = true;
	else
		chn_en.is_bank_sw = false;

	chn_en.is_activate = is_act;

	list_for_each_entry(slv_rt_strm, &sdw_rt->slv_rt_list, slave_sdw_node) {

		if (slv_rt_strm->slave == NULL)
			break;

		list_for_each_entry(port_slv_strm,
				&slv_rt_strm->port_rt_list, port_node) {

			ret = sdw_cfg_slv_enable_disable
				(sdw_mstr_bs, slv_rt_strm,
					port_slv_strm, &chn_en);
			if (ret < 0)
				return ret;

			/*
			 * Since one port per slave runtime,
			 * breaking port_list loop
			 * TBD: to be extended for multiple port support
			 */
			break;

		}

		break;

	}

	list_for_each_entry(mstr_rt_strm,
			&sdw_rt->mstr_rt_list, mstr_sdw_node) {

		if (mstr_rt_strm->mstr == NULL)
			break;

		list_for_each_entry(port_mstr_strm,
			&mstr_rt_strm->port_rt_list, port_node) {

			ret = sdw_cfg_mstr_activate_disable
				(sdw_mstr_bs, mstr_rt_strm,
				port_mstr_strm, &chn_en);
			if (ret < 0)
				return ret;

			/*
			 * Since one port per master runtime,
			 * breaking port_list loop
			 * TBD: to be extended for multiple port support
			 */
			break;

		}

	}

	return 0;
}


/*
 * sdw_en_dis_mstr_slv_state - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function call master/slave enable/disable
 * channel API's based on runtime state.
 */
int sdw_en_dis_mstr_slv_state(struct sdw_bus *sdw_mstr_bs,
	struct sdw_mstr_runtime *sdw_mstr_bs_rt,
	struct port_chn_en_state *chn_en)
{
	struct sdw_slave_runtime *slv_rt = NULL;
	struct sdw_port_runtime *port_slv_rt, *port_rt;
	int ret = 0;

	list_for_each_entry(slv_rt, &sdw_mstr_bs_rt->slv_rt_list, slave_node)  {

		if (slv_rt->slave == NULL)
			break;

		if (slv_rt->rt_state == SDW_STATE_ENABLE_RT) {

			list_for_each_entry(port_slv_rt,
				&slv_rt->port_rt_list, port_node) {

				ret = sdw_cfg_slv_enable_disable
					(sdw_mstr_bs, slv_rt,
					port_slv_rt, chn_en);
				if (ret < 0)
					return ret;

				/*
				 * Since one port per slave runtime,
				 * breaking port_list loop
				 * TBD: to be extended for multiple
				 * port support
				 */
				break;
			}
		}
	}

	if (sdw_mstr_bs_rt->rt_state == SDW_STATE_ENABLE_RT) {

		list_for_each_entry(port_rt,
			&sdw_mstr_bs_rt->port_rt_list, port_node) {

			ret = sdw_cfg_mstr_activate_disable
				(sdw_mstr_bs, sdw_mstr_bs_rt, port_rt, chn_en);
			if (ret < 0)
				return ret;

			/*
			 * Since one port per master runtime,
			 * breaking port_list loop
			 * TBD: to be extended for multiple port support
			 */

			break;
		}
	}

	return 0;
}


/*
 * sdw_get_clock_frmshp - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function computes clock and frame shape based on
 * clock frequency.
 */
int sdw_get_clock_frmshp(struct sdw_bus *sdw_mstr_bs, int *frame_int,
		int *col, int *row)
{
	int i, rc, clock_reqd = 0, frame_interval = 0, frame_frequency = 0;
	int sel_row = 0, sel_col = 0;
	bool clock_ok = false;

	/*
	 * Find nearest clock frequency needed by master for
	 * given bandwidth
	 */

	/*
	 * TBD: Need to run efficient algorithm to make sure we have
	 * only 1 to 10 percent of control bandwidth usage
	 */
	for (i = 0; i < MAXCLOCKFREQ; i++) {

		/* TBD: Check why 3000 */
		if ((clock_freq[i] <= sdw_mstr_bs->bandwidth) ||
				((clock_freq[i] % 3000) != 0))
			continue;
		clock_reqd = clock_freq[i];

		/*
		 * TBD: Check all the slave device capabilities
		 * here and find whether given frequency is
		 * supported by all slaves
		 */

		/* Find frame shape based on bandwidth per controller */
		/*
		 * TBD: Need to run efficient algorithm to make sure we have
		 * only 1 to 10 percent of control bandwidth usage
		 */
		for (rc = 0; rc <= MAX_NUM_ROW_COLS; rc++) {
			frame_interval =
				sdw_core.rowcolcomb[rc].row *
				sdw_core.rowcolcomb[rc].col;
			frame_frequency = clock_reqd/frame_interval;

			if ((clock_reqd -
						(frame_frequency *
						 sdw_core.rowcolcomb[rc].
						 control_bits)) <
					sdw_mstr_bs->bandwidth)
				continue;

			break;
		}

		sel_row = sdw_core.rowcolcomb[rc].row;
		sel_col = sdw_core.rowcolcomb[rc].col;
		sdw_mstr_bs->frame_freq = frame_frequency;
		sdw_mstr_bs->clk_freq = clock_reqd;
		clock_ok = false;
		*frame_int = frame_interval;
		*col = sel_col;
		*row = sel_row;
		sdw_mstr_bs->col = sel_col;
		sdw_mstr_bs->row = sel_row;

		break;

	}

	return 0;
}

/*
 * sdw_compute_sys_interval - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function computes system interval.
 */
int sdw_compute_sys_interval(struct sdw_bus *sdw_mstr_bs,
		struct sdw_master_capabilities *sdw_mstr_cap,
		int frame_interval)
{
	struct sdw_master *sdw_mstr = sdw_mstr_bs->mstr;
	struct sdw_mstr_runtime *sdw_mstr_bs_rt;
	struct sdw_transport_params *t_params;
	struct sdw_port_runtime *port_rt;
	int lcmnum1 = 0, lcmnum2 = 0, div = 0, lcm = 0;

	/*
	 * once you got bandwidth frame shape for bus,
	 * run a loop for all the active streams running
	 * on bus and compute sample_interval & other transport parameters.
	 */
	list_for_each_entry(sdw_mstr_bs_rt,
			&sdw_mstr->mstr_rt_list, mstr_node) {

		if (sdw_mstr_bs_rt->mstr == NULL)
			break;

		/* should not compute any transport params */
		if (sdw_mstr_bs_rt->rt_state == SDW_STATE_UNPREPARE_RT)
			continue;

		list_for_each_entry(port_rt,
				&sdw_mstr_bs_rt->port_rt_list, port_node) {

			t_params = &port_rt->transport_params;

			/*
			 * Current Assumption:
			 * One port per bus runtime structure
			 */
			/* Calculate sample interval */
#ifdef CONFIG_SND_SOC_SVFPGA
			t_params->sample_interval =
				((sdw_mstr_bs->clk_freq/
				  sdw_mstr_bs_rt->stream_params.rate));
#else
			t_params->sample_interval =
				((sdw_mstr_bs->clk_freq/
				  sdw_mstr_bs_rt->stream_params.rate) * 2);

#endif
			/* Only BlockPerPort supported */
			t_params->blockpackingmode = 0;
			t_params->lanecontrol = 0;

			/* Calculate LCM */
			lcmnum2 = t_params->sample_interval;
			if (!lcmnum1)
				lcmnum1 = sdw_lcm(lcmnum2, lcmnum2);
			else
				lcmnum1 = sdw_lcm(lcmnum1, lcmnum2);

			/*
			 * Since one port per bus runtime, breaking
			 *  port_list loop
			 * TBD: to be extended for multiple port support
			 */
			break;

		}
	}


	/* 6. compute system_interval */
	if ((sdw_mstr_cap) && (sdw_mstr_bs->clk_freq)) {

		div = ((sdw_mstr_cap->base_clk_freq * 2) /
					sdw_mstr_bs->clk_freq);
		lcm = sdw_lcm(lcmnum1, frame_interval);
		sdw_mstr_bs->system_interval = (div  * lcm);

	}

	/*
	 * Something went wrong, may be sdw_lcm value may be 0,
	 * return error accordingly
	 */
	if (!sdw_mstr_bs->system_interval)
		return -EINVAL;


	return 0;
}


/*
 * sdw_compute_hstart_hstop - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function computes hstart and hstop for running
 * streams per master & slaves.
 */
int sdw_compute_hstart_hstop(struct sdw_bus *sdw_mstr_bs, int sel_col)
{
	struct sdw_master *sdw_mstr = sdw_mstr_bs->mstr;
	struct sdw_mstr_runtime *sdw_mstr_bs_rt;
	struct sdw_transport_params *t_params = NULL, *t_slv_params = NULL;
	struct sdw_slave_runtime *slv_rt = NULL;
	struct sdw_port_runtime *port_rt, *port_slv_rt;
	int hstop = 0, hwidth = 0;
	int payload_bw = 0, full_bw = 0, column_needed = 0;
	bool hstop_flag = false;

	/* Calculate hwidth, hstart and hstop */
	list_for_each_entry(sdw_mstr_bs_rt,
			&sdw_mstr->mstr_rt_list, mstr_node) {

		if (sdw_mstr_bs_rt->mstr == NULL)
			break;

		/* should not compute any transport params */
		if (sdw_mstr_bs_rt->rt_state == SDW_STATE_UNPREPARE_RT)
			continue;

		list_for_each_entry(port_rt,
				&sdw_mstr_bs_rt->port_rt_list, port_node) {

			t_params = &port_rt->transport_params;
			t_params->num = port_rt->port_num;

			/*
			 * 1. find full_bw and payload_bw per stream
			 * 2. find h_width per stream
			 * 3. find hstart, hstop, block_offset,sub_block_offset
			 * Note: full_bw is nothing but sampling interval
			 * of stream.
			 * payload_bw is serving size no.
			 * of channels * bps per stream
			 */
			full_bw = sdw_mstr_bs->clk_freq/
				sdw_mstr_bs_rt->stream_params.rate;
			payload_bw =
				sdw_mstr_bs_rt->stream_params.bps *
				sdw_mstr_bs_rt->stream_params.channel_count;

			hwidth = (sel_col * payload_bw + full_bw - 1)/full_bw;
			column_needed += hwidth;

			/*
			 * These needs to be done only for
			 * 1st entry in link list
			 */
			if (!hstop_flag) {
				hstop = sel_col - 1;
				hstop_flag = true;
			}

			/* Assumption: Only block per port is supported
			 * For blockperport:
			 * offset1 value = LSB 8 bits of block_offset value
			 * offset2 value = MSB 8 bits of block_offset value
			 * For blockperchannel:
			 * offset1 = LSB 8 bit of block_offset value
			 * offset2 = MSB 8 bit of sub_block_offload value
			 * if hstart and hstop of different streams in
			 * master are different, then block_offset is zero.
			 * if not then block_offset value for 2nd stream
			 * is block_offset += payload_bw
			 */

			t_params->hstop = hstop;
#ifdef CONFIG_SND_SOC_SVFPGA
			/* For PDM capture, 0th col is also used */
			t_params->hstart = 0;
#else
			t_params->hstart = hstop - hwidth + 1;
#endif

			/*
			 * TBD: perform this when you have 2 ports
			 * and accordingly configure hstart hstop for slave
			 * removing for now
			 */
#if 0
			hstop = hstop - hwidth;
#endif
			/* Since one port per bus runtime,
			 * breaking port_list loop
			 * TBD: to be extended for multiple port support
			 */
			break;
		}

		/*
		 * Run loop for slave_rt_list for given master_list
		 * to compute hstart hstop for slave
		 */
		list_for_each_entry(slv_rt,
				&sdw_mstr_bs_rt->slv_rt_list, slave_node)  {

			if (slv_rt->slave == NULL)
				break;

			if (slv_rt->rt_state == SDW_STATE_UNPREPARE_RT)
				continue;

			list_for_each_entry(port_slv_rt,
					&slv_rt->port_rt_list, port_node) {

				t_slv_params = &port_slv_rt->transport_params;
				t_slv_params->num = port_slv_rt->port_num;

				/*
				 * TBD: Needs to be verifid for
				 * multiple combination
				 * 1. 1 master port, 1 slave rt,
				 * 1 port per slave rt -->
				 * In this case, use hstart hstop same as master
				 * for 1 slave rt
				 * 2. 1 master port, 2 slave rt,
				 * 1 port per slave rt -->
				 * In this case, use hstart hstop same as master
				 * for 2 slave rt
				 * only offset will change for 2nd slave rt
				 * Current assumption is one port per rt,
				 * hence no multiple port combination
				 * considered.
				 */
				t_slv_params->hstop = hstop;
				t_slv_params->hstart = hstop - hwidth + 1;

				/* Only BlockPerPort supported */
				t_slv_params->blockpackingmode = 0;
				t_slv_params->lanecontrol = 0;

				/*
				 * below copy needs to be changed when
				 * more than one port is supported
				 */
				if (t_params)
					t_slv_params->sample_interval =
						t_params->sample_interval;

				/* Since one port per slave runtime,
				 * breaking port_list loop
				 * TBD: to be extended for multiple
				 * port support
				 */
				break;
			}

		}
	}

#if 0
	/* TBD: To be verified */
	if  (column_needed > sel_col - 1)
		return -EINVAL; /* Error case, check what has gone wrong */
#endif

	return 0;
}


/*
 * sdw_compute_blk_subblk_offset - returns Success
 *
 *
 * This function computes block offset and sub block
 * offset for running streams per master & slaves.
 */
int sdw_compute_blk_subblk_offset(struct sdw_bus *sdw_mstr_bs)
{
	struct sdw_master *sdw_mstr = sdw_mstr_bs->mstr;
	struct sdw_mstr_runtime *sdw_mstr_bs_rt;
	struct sdw_transport_params *t_params, *t_slv_params;
	struct sdw_slave_runtime *slv_rt = NULL;
	struct sdw_port_runtime *port_rt, *port_slv_rt;
	int hstart1 = 0, hstop1 = 0, hstart2 = 0, hstop2 = 0;
	int block_offset = 1;


	/* Calculate block_offset and subblock_offset */
	list_for_each_entry(sdw_mstr_bs_rt,
			&sdw_mstr->mstr_rt_list, mstr_node) {

		if (sdw_mstr_bs_rt->mstr == NULL)
			break;

		/* should not compute any transport params */
		if (sdw_mstr_bs_rt->rt_state == SDW_STATE_UNPREPARE_RT)
			continue;

		list_for_each_entry(port_rt,
				&sdw_mstr_bs_rt->port_rt_list, port_node) {

			t_params = &port_rt->transport_params;


			if ((!hstart2) && (!hstop2)) {
				hstart1 = hstart2 = t_params->hstart;
				hstop1  = hstop2 = t_params->hstop;
				/* TBD: Verify this condition */
#ifdef CONFIG_SND_SOC_SVFPGA
				block_offset = 1;
#else
				block_offset = 0;
#endif
			} else {

				hstart1 = t_params->hstart;
				hstop1 = t_params->hstop;

#ifndef CONFIG_SND_SOC_SVFPGA
				/* hstart/stop not same */
				if ((hstart1 != hstart2) &&
					(hstop1 != hstop2)) {
					/* TBD: Harcoding to 0, to be removed*/
					block_offset = 0;
				} else {
					/* TBD: Harcoding to 0, to be removed*/
					block_offset = 0;
				}
#else
				if ((hstart1 != hstart2) &&
					(hstop1 != hstop2)) {
					block_offset = 1;
				} else {
					block_offset +=
						(sdw_mstr_bs_rt->stream_params.
						bps
						*
						sdw_mstr_bs_rt->stream_params.
						channel_count);
				}
#endif

			}


			/*
			 * TBD: Hardcding block control group as true,
			 * to be changed later
			 */
			t_params->blockgroupcontrol_valid = true;
			t_params->blockgroupcontrol = 0x0; /* Hardcoding to 0 */

			/*
			 * Since one port per bus runtime,
			 * breaking port_list loop
			 * TBD: to be extended for multiple port support
			 */
			break;
		}

		/*
		 * Run loop for slave_rt_list for given master_list
		 * to compute block and sub block offset for slave
		 */
		list_for_each_entry(slv_rt,
				&sdw_mstr_bs_rt->slv_rt_list, slave_node)  {

			if (slv_rt->slave == NULL)
				break;

			if (slv_rt->rt_state == SDW_STATE_UNPREPARE_RT)
				continue;

			list_for_each_entry(port_slv_rt,
					&slv_rt->port_rt_list, port_node) {

				t_slv_params = &port_slv_rt->transport_params;

				/*
				 * TBD: Needs to be verifid for
				 * multiple combination
				 * 1. 1 master port, 1 slave rt,
				 * 1 port per slave rt -->
				 * In this case, use block_offset same as
				 * master for 1 slave rt
				 * 2. 1 master port, 2 slave rt,
				 * 1 port per slave rt -->
				 * In this case, use block_offset same as
				 * master for 1st slave rt and compute for 2nd.
				 */

				/*
				 * Current assumption is one port per rt,
				 * hence no multiple port combination.
				 * TBD: block offset to be computed for
				 * more than 1 slave_rt list.
				 */
				t_slv_params->offset1 = block_offset;
				t_slv_params->offset2 = block_offset >> 8;


				/*
				 * TBD: Hardcding block control group as true,
				 * to be changed later
				 */
				t_slv_params->blockgroupcontrol_valid = true;
				/* Hardcoding to 0 */
				t_slv_params->blockgroupcontrol = 0x0;
				/* Since one port per slave runtime,
				 * breaking port_list loop
				 * TBD:to be extended for multiple port support
				 */
				break;
			}
		}
	}

	return 0;
}


/*
 * sdw_configure_frmshp_bnkswtch - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function broadcast frameshape on framectrl
 * register and performs bank switch.
 */
int sdw_configure_frmshp_bnkswtch(struct sdw_bus *mstr_bs, int col, int row)
{
	struct sdw_msg wr_msg;
	int ret = 0;
	int banktouse, numcol, numrow;
	u8 wbuf[1] = {0};

	numcol = sdw_get_col_to_num(col);
	numrow = sdw_get_row_to_num(row);

	wbuf[0] = numcol | (numrow << 3);
	/* Get current bank in use from bus structure*/
	banktouse = mstr_bs->active_bank;
	banktouse = !banktouse;

	if (banktouse) {
		wr_msg.addr = (SDW_SCP_FRAMECTRL + SDW_BANK1_REGISTER_OFFSET) +
			(SDW_NUM_DATA_PORT_REGISTERS * 0); /* Data port 0 */
	} else {

		wr_msg.addr = SDW_SCP_FRAMECTRL +
			(SDW_NUM_DATA_PORT_REGISTERS * 0); /* Data port 0 */
	}

	wr_msg.ssp_tag = 0x1;
	wr_msg.flag = SDW_MSG_FLAG_WRITE;
	wr_msg.len = 1;
	wr_msg.slave_addr = 0xF; /* Broadcast address*/
	wr_msg.buf = wbuf;
	wr_msg.addr_page1 = 0x0;
	wr_msg.addr_page2 = 0x0;


	ret = sdw_slave_transfer(mstr_bs->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&mstr_bs->mstr->dev, "Register transfer failed\n");
		goto out;
	}

	msleep(100); /* TBD: Remove this */

	/*
	 * TBD: check whether we need to poll on
	 * mcp active bank bit to switch bank
	 */
	mstr_bs->active_bank = banktouse;

out:

	return ret;
}


/*
 * sdw_cfg_bs_params - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function performs master/slave transport
 * params config, set SSP interval, set Clock
 * frequency, enable channel. This API is called
 * from sdw_bus_calc_bw & sdw_bus_calc_bw_dis API.
 *
 */
int sdw_cfg_bs_params(struct sdw_bus *sdw_mstr_bs,
		struct sdw_mstr_runtime *sdw_mstr_bs_rt,
		bool is_strm_cpy)
{
	struct port_chn_en_state chn_en;
	struct sdw_master *sdw_mstr = sdw_mstr_bs->mstr;
	struct sdw_mstr_driver *ops;
	int banktouse, ret = 0;

	list_for_each_entry(sdw_mstr_bs_rt,
		&sdw_mstr->mstr_rt_list, mstr_node) {

		if (sdw_mstr_bs_rt->mstr == NULL)
			continue;

		if (is_strm_cpy) {
			/*
			 * Configure and enable all slave
			 * transport params first
			 */
			ret = sdw_cfg_mstr_slv(sdw_mstr_bs,
				sdw_mstr_bs_rt, false);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
					"slave config params failed\n");
				return ret;
			}

			/* Configure and enable all master params */
			ret = sdw_cfg_mstr_slv(sdw_mstr_bs,
				sdw_mstr_bs_rt, true);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
					"master config params failed\n");
				return ret;
			}

		} else {

			/*
			 * 7.1 Copy all slave transport and port params
			 * to alternate bank
			 * 7.2 copy all master transport and port params
			 * to alternate bank
			 */
			ret = sdw_cpy_params_mstr_slv(sdw_mstr_bs,
				sdw_mstr_bs_rt);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
					"slave/master copy params failed\n");
				return ret;
			}
		}

		/* Get master driver ops */
		ops = sdw_mstr_bs->mstr->driver;

		/* Configure SSP */
		banktouse = sdw_mstr_bs->active_bank;
		banktouse = !banktouse;

		/*
		 * TBD: Currently harcoded SSP interval to 50,
		 * computed value to be taken from system_interval in
		 * bus data structure.
		 * Add error check.
		 */
		if (ops->mstr_ops->set_ssp_interval)
			ops->mstr_ops->set_ssp_interval(sdw_mstr_bs->mstr,
					50, banktouse);

		/*
		 * Configure Clock
		 * TBD: Add error check
		 */
		if (ops->mstr_ops->set_clock_freq)
			ops->mstr_ops->set_clock_freq(sdw_mstr_bs->mstr,
					sdw_mstr_bs->clk_freq, banktouse);

		/* Enable channel on alternate bank for running streams */
		chn_en.is_activate = true;
		chn_en.is_bank_sw = true;
		ret = sdw_en_dis_mstr_slv_state
				(sdw_mstr_bs, sdw_mstr_bs_rt, &chn_en);
		if (ret < 0) {
			/* TBD: Undo all the computation */
			dev_err(&sdw_mstr_bs->mstr->dev,
					"Channel enable failed\n");
			return ret;
		}

	}

	return 0;
}

/*
 * sdw_dis_chan - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function disables channel on alternate
 * bank. This API is called from sdw_bus_calc_bw
 * & sdw_bus_calc_bw_dis when channel on current
 * bank is enabled.
 *
 */
int sdw_dis_chan(struct sdw_bus *sdw_mstr_bs,
	struct sdw_mstr_runtime *sdw_mstr_bs_rt)
{
	struct sdw_master *sdw_mstr = sdw_mstr_bs->mstr;
	struct port_chn_en_state chn_en;
	int ret = 0;

	list_for_each_entry(sdw_mstr_bs_rt,
			&sdw_mstr->mstr_rt_list, mstr_node) {

		if (sdw_mstr_bs_rt->mstr == NULL)
			continue;

		chn_en.is_activate = false;
		chn_en.is_bank_sw = true;
		ret = sdw_en_dis_mstr_slv_state(sdw_mstr_bs,
				sdw_mstr_bs_rt, &chn_en);
		if (ret < 0)
			return ret;
	}

	return 0;
}


/*
 * sdw_cfg_slv_prep_unprep - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function prepare/unprepare slave ports.
 */
int sdw_cfg_slv_prep_unprep(struct sdw_bus *mstr_bs,
	struct sdw_slave_runtime *slv_rt_strm,
	struct sdw_port_runtime *port_slv_strm,
	bool prep)
{
	struct sdw_slave_driver	*slv_ops = slv_rt_strm->slave->driver;
	struct sdw_slv_capabilities *slv_cap =
			&slv_rt_strm->slave->sdw_slv_cap;
	struct sdw_slv_dpn_capabilities *sdw_slv_dpn_cap =
			slv_cap->sdw_dpn_cap;

	struct sdw_msg wr_msg, rd_msg, rd_msg1;
	int ret = 0;
	int banktouse;
	u8 wbuf[1] = {0};
	u8 rbuf[1] = {0};
	u8 rbuf1[1] = {0};

	/* Get current bank in use from bus structure*/
	banktouse = mstr_bs->active_bank;
	banktouse = !banktouse;

	/* Read SDW_DPN_PREPARECTRL register */
	rd_msg.addr = wr_msg.addr = SDW_DPN_PREPARECTRL +
		(SDW_NUM_DATA_PORT_REGISTERS * port_slv_strm->port_num);

	rd_msg.ssp_tag = 0x0;
	rd_msg.flag = SDW_MSG_FLAG_READ;
	rd_msg.len = 1;
	rd_msg.slave_addr = slv_rt_strm->slave->slv_number;
	rd_msg.buf = rbuf;
	rd_msg.addr_page1 = 0x0;
	rd_msg.addr_page2 = 0x0;

	rd_msg1.ssp_tag = 0x0;
	rd_msg1.flag = SDW_MSG_FLAG_READ;
	rd_msg1.len = 1;
	rd_msg1.slave_addr = slv_rt_strm->slave->slv_number;
	rd_msg1.buf = rbuf1;
	rd_msg1.addr_page1 = 0x0;
	rd_msg1.addr_page2 = 0x0;


	rd_msg1.addr = SDW_DPN_PREPARESTATUS +
		(SDW_NUM_DATA_PORT_REGISTERS * port_slv_strm->port_num);

	wr_msg.ssp_tag = 0x0;
	wr_msg.flag = SDW_MSG_FLAG_WRITE;
	wr_msg.len = 1;
	wr_msg.slave_addr = slv_rt_strm->slave->slv_number;
	wr_msg.buf = wbuf;
	wr_msg.addr_page1 = 0x0;
	wr_msg.addr_page2 = 0x0;


	if (prep) { /* PREPARE */

		/*
		 * 1. slave port prepare_ch_pre
		 * --> callback
		 * --> handle_pre_port_prepare
		 */
		if (slv_ops->handle_pre_port_prepare) {
			slv_ops->handle_pre_port_prepare(slv_rt_strm->slave,
					port_slv_strm->port_num,
					port_slv_strm->channel_mask,
					banktouse);
		}

		/* 2. slave port prepare --> to write */
		if (sdw_slv_dpn_cap->prepare_ch) {

			/* NON SIMPLIFIED CM, prepare required */
			ret = sdw_slave_transfer(mstr_bs->mstr, &rd_msg, 1);
			if (ret != 1) {
				ret = -EINVAL;
				dev_err(&mstr_bs->mstr->dev,
					"Register transfer failed\n");
				goto out;
			}

			ret = sdw_slave_transfer(mstr_bs->mstr, &rd_msg1, 1);
			if (ret != 1) {
				ret = -EINVAL;
				dev_err(&mstr_bs->mstr->dev,
						"Register transfer failed\n");
				goto out;
			}

			wbuf[0] = (rbuf[0] | port_slv_strm->channel_mask);

			/*
			 * TBD: poll for prepare interrupt bit
			 * before calling post_prepare
			 * 2. check capabilities if simplified
			 * CM no need to prepare
			 */
			ret = sdw_slave_transfer(mstr_bs->mstr, &wr_msg, 1);
			if (ret != 1) {
				ret = -EINVAL;
				dev_err(&mstr_bs->mstr->dev,
					"Register transfer failed\n");
				goto out;
			}

			/*
			 * TBD: check on port ready,
			 * ideally we should check on prepare
			 * status for port_ready
			 */

			/* wait for completion on port ready*/
			msleep(100); /* TBD: Remove this */

			ret = sdw_slave_transfer(mstr_bs->mstr, &rd_msg1, 1);
			if (ret != 1) {
				ret = -EINVAL;
				dev_err(&mstr_bs->mstr->dev,
					"Register transfer failed\n");
				goto out;
			}
		}

		/*
		 * 3. slave port post pre
		 * --> callback
		 * --> handle_post_port_prepare
		 */
		if (slv_ops->handle_post_port_prepare) {
			slv_ops->handle_post_port_prepare
				(slv_rt_strm->slave,
				port_slv_strm->port_num,
				port_slv_strm->channel_mask, banktouse);
		}

		slv_rt_strm->rt_state = SDW_STATE_PREPARE_RT;

	} else {
		/* UNPREPARE */
		/*
		 * 1. slave port unprepare_ch_pre
		 * --> callback
		 * --> handle_pre_port_prepare
		 */
		if (slv_ops->handle_pre_port_unprepare) {
			slv_ops->handle_pre_port_unprepare(slv_rt_strm->slave,
						port_slv_strm->port_num,
						port_slv_strm->channel_mask,
						banktouse);
		}

		/* 2. slave port unprepare --> to write */
		if (sdw_slv_dpn_cap->prepare_ch) {

			/* NON SIMPLIFIED CM, unprepare required */

			/* Read SDW_DPN_PREPARECTRL register */
			ret = sdw_slave_transfer(mstr_bs->mstr, &rd_msg, 1);
			if (ret != 1) {
				ret = -EINVAL;
				dev_err(&mstr_bs->mstr->dev,
					"Register transfer failed\n");
				goto out;
			}

			wbuf[0] = (rbuf[0] & ~(port_slv_strm->channel_mask));

			/*
			 * TBD: poll for prepare interrupt bit before
			 * calling post_prepare
			 * Does it apply for unprepare aswell?
			 * 2. check capabilities if simplified CM
			 * no need to unprepare
			 */
			ret = sdw_slave_transfer(mstr_bs->mstr, &wr_msg, 1);
			if (ret != 1) {
				ret = -EINVAL;
				dev_err(&mstr_bs->mstr->dev,
					"Register transfer failed\n");
				goto out;
			}
		}

		/*
		 * 3. slave port post unpre
		 * --> callback
		 * --> handle_post_port_unprepare
		 */
		if (slv_ops->handle_post_port_unprepare) {
			slv_ops->handle_post_port_unprepare(slv_rt_strm->slave,
					port_slv_strm->port_num,
					port_slv_strm->channel_mask,
					banktouse);
		}

		slv_rt_strm->rt_state = SDW_STATE_UNPREPARE_RT;
	}
out:
	return ret;

}


/*
 * sdw_cfg_mstr_prep_unprep - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function prepare/unprepare master ports.
 */
int sdw_cfg_mstr_prep_unprep(struct sdw_bus *mstr_bs,
	struct sdw_mstr_runtime *mstr_rt_strm,
	struct sdw_port_runtime *port_mstr_strm,
	bool prep)
{
	struct sdw_mstr_driver *ops = mstr_bs->mstr->driver;
	struct sdw_prepare_ch prep_ch;
	int ret = 0;

	prep_ch.num = port_mstr_strm->port_num;
	prep_ch.ch_mask = port_mstr_strm->channel_mask;
	prep_ch.prepare = prep; /* Prepare/Unprepare */

	/* TBD: Bank configuration */

	/* 1. Master port prepare_ch_pre */
	if (ops->mstr_port_ops->dpn_port_prepare_ch_pre) {
		ret = ops->mstr_port_ops->dpn_port_prepare_ch_pre
				(mstr_bs->mstr, &prep_ch);
		if (ret < 0)
			return ret;
	}

	/* 2. Master port prepare */
	if (ops->mstr_port_ops->dpn_port_prepare_ch) {
		ret = ops->mstr_port_ops->dpn_port_prepare_ch
				(mstr_bs->mstr, &prep_ch);
		if (ret < 0)
			return ret;
	}

	/* 3. Master port prepare_ch_post */
	if (ops->mstr_port_ops->dpn_port_prepare_ch_post) {
		ret = ops->mstr_port_ops->dpn_port_prepare_ch_post
				(mstr_bs->mstr, &prep_ch);
		if (ret < 0)
			return ret;
	}

	if (prep)
		mstr_rt_strm->rt_state = SDW_STATE_PREPARE_RT;
	else
		mstr_rt_strm->rt_state = SDW_STATE_UNPREPARE_RT;

	return 0;
}


/*
 * sdw_prep_unprep_mstr_slv - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function call master/slave prepare/unprepare
 * port configuration API's, called from sdw_bus_calc_bw
 * & sdw_bus_calc_bw_dis API's.
 */
int sdw_prep_unprep_mstr_slv(struct sdw_bus *sdw_mstr_bs,
		struct sdw_runtime *sdw_rt, bool is_prep)
{
	struct sdw_slave_runtime *slv_rt_strm = NULL;
	struct sdw_port_runtime *port_slv_strm, *port_mstr_strm;
	struct sdw_mstr_runtime *mstr_rt_strm = NULL;
	int ret = 0;

	list_for_each_entry(slv_rt_strm,
			&sdw_rt->slv_rt_list, slave_sdw_node) {

		if (slv_rt_strm->slave == NULL)
			break;

		list_for_each_entry(port_slv_strm,
				&slv_rt_strm->port_rt_list, port_node) {

			ret = sdw_cfg_slv_prep_unprep(sdw_mstr_bs,
					slv_rt_strm, port_slv_strm, is_prep);
			if (ret < 0)
				return ret;

			/* Since one port per slave runtime,
			 * breaking port_list loop
			 * TBD: to be extended for multiple port support
			 */
			break;
		}

		break;
	}

	list_for_each_entry(mstr_rt_strm,
			&sdw_rt->mstr_rt_list, mstr_sdw_node) {

		if (mstr_rt_strm->mstr == NULL)
			break;

		list_for_each_entry(port_mstr_strm,
			&mstr_rt_strm->port_rt_list, port_node) {

			ret = sdw_cfg_mstr_prep_unprep(sdw_mstr_bs,
				mstr_rt_strm, port_mstr_strm, is_prep);
			if (ret < 0)
				return ret;

			/* Since one port per master runtime,
			 * breaking port_list loop
			 * TBD: to be extended for multiple port support
			 */
			break;
		}
	}

	return 0;
}


/**
 * sdw_bus_calc_bw - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function is called from sdw_prepare_and_enable
 * whenever new stream is processed. The function based
 * on the stream associated with controller calculates
 * required bandwidth, clock, frameshape, computes
 * all transport params for a given port, enable channel
 * & perform bankswitch.
 */
int sdw_bus_calc_bw(struct sdw_stream_tag *stream_tag, bool enable)
{

	struct sdw_runtime *sdw_rt = stream_tag->sdw_rt;
	struct sdw_stream_params *stream_params = &sdw_rt->stream_params;
	struct sdw_mstr_runtime *sdw_mstr_rt = NULL, *sdw_mstr_bs_rt = NULL;
	struct sdw_bus *sdw_mstr_bs = NULL;
	struct sdw_master *sdw_mstr = NULL;
	struct sdw_master_capabilities *sdw_mstr_cap = NULL;
	struct sdw_stream_params *mstr_params;
	int stream_frame_size;
	int frame_interval = 0, sel_row = 0, sel_col = 0;
	int ret = 0;

	/* TBD: Add PCM/PDM flag in sdw_config_stream */

	/*
	 * TBD: check for mstr_rt is in configured state or not
	 * If yes, then configure masters as well
	 * If no, then do not configure/enable master related parameters
	 */

	/* BW calulation for active master controller for given stream tag */
	list_for_each_entry(sdw_mstr_rt, &sdw_rt->mstr_rt_list, mstr_sdw_node) {

		if (sdw_mstr_rt->mstr == NULL)
			break;

		/* Get bus structure for master */
		list_for_each_entry(sdw_mstr_bs, &sdw_core.bus_list, bus_node) {

			/* Match master structure pointer */
			if (sdw_mstr_bs->mstr != sdw_mstr_rt->mstr)
				continue;


			sdw_mstr = sdw_mstr_bs->mstr;
			break;
		}

		/*
		 * All data structures required available,
		 * lets calculate BW for master controller
		 */

		/* Check for isochronous mode plus other checks if required */
		sdw_mstr_cap = &sdw_mstr_bs->mstr->mstr_capabilities;
		mstr_params = &sdw_mstr_rt->stream_params;

		if ((sdw_rt->stream_state == SDW_STATE_CONFIG_STREAM) ||
				(sdw_rt->stream_state ==
					SDW_STATE_UNPREPARE_STREAM)) {

			/* we do not support asynchronous mode Return Error */
			if ((sdw_mstr_cap->base_clk_freq % mstr_params->rate)
					!= 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"Asynchronous mode not supported\n");
				return -EINVAL;
			}

			/* Check for sampling frequency */
			if (stream_params->rate != mstr_params->rate) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"Sampling frequency mismatch\n");
				return -EINVAL;
			}

			/*
			 * Calculate stream bandwidth, frame size and
			 * total BW required for master controller
			 */
			sdw_mstr_rt->stream_bw = mstr_params->rate *
				mstr_params->channel_count * mstr_params->bps;
			stream_frame_size = mstr_params->channel_count *
				mstr_params->bps;

			sdw_mstr_bs->bandwidth += sdw_mstr_rt->stream_bw;

			ret = sdw_get_clock_frmshp(sdw_mstr_bs,
					&frame_interval, &sel_col, &sel_row);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev, "clock/frameshape config failed\n");
				return ret;
			}


			/*
			 * TBD: find right place to run sorting on
			 * master rt_list. Below sorting is done based on
			 * bps from low to high, that means PDM streams
			 * will be placed before PCM.
			 */

			/*
			 * TBD Should we also perform sorting based on rate
			 * for PCM stream check. if yes then how??
			 * creating two different list.
			 */

			/* Compute system interval */
			ret = sdw_compute_sys_interval(sdw_mstr_bs,
					sdw_mstr_cap, frame_interval);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev, "compute system interval failed\n");
				return ret;
			}

			/* Compute hstart/hstop */
			ret = sdw_compute_hstart_hstop(sdw_mstr_bs, sel_col);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"compute hstart/hstop failed\n");
				return ret;
			}

			/* Compute block offset */
			ret = sdw_compute_blk_subblk_offset(sdw_mstr_bs);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(
						&sdw_mstr_bs->mstr->dev,
						"compute block offset failed\n");
				return ret;
			}

			/* Change Stream State */
			sdw_rt->stream_state = SDW_STATE_COMPUTE_STREAM;

			/* Configure bus parameters */
			ret = sdw_cfg_bs_params(sdw_mstr_bs,
					sdw_mstr_bs_rt, true);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"xport params config failed\n");
				return ret;
			}

			sel_col = sdw_mstr_bs->col;
			sel_row = sdw_mstr_bs->row;

			/* Configure Frame Shape/Switch Bank */
			ret = sdw_configure_frmshp_bnkswtch(sdw_mstr_bs,
					sel_col, sel_row);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"bank switch failed\n");
				return ret;
			}

			/* Disable all channels enabled on previous bank */
			ret = sdw_dis_chan(sdw_mstr_bs, sdw_mstr_bs_rt);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"Channel disabled failed\n");
				return ret;
			}

			/* Prepare new port for master and slave */
			ret = sdw_prep_unprep_mstr_slv(sdw_mstr_bs,
					sdw_rt, true);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"Channel prepare failed\n");
				return ret;
			}

			/* change stream state to prepare */
			sdw_rt->stream_state = SDW_STATE_PREPARE_STREAM;
		}

		if ((enable) && (SDW_STATE_PREPARE_STREAM
					== sdw_rt->stream_state)) {

			ret = sdw_cfg_bs_params(sdw_mstr_bs,
					sdw_mstr_bs_rt, false);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"xport params config failed\n");
				return ret;
			}

			/* Enable new port for master and slave */
			ret = sdw_en_dis_mstr_slv(sdw_mstr_bs, sdw_rt, true);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"Channel enable failed\n");
				return ret;
			}

			/* change stream state to enable */
			sdw_rt->stream_state = SDW_STATE_ENABLE_STREAM;

			sel_col = sdw_mstr_bs->col;
			sel_row = sdw_mstr_bs->row;

			/* Configure Frame Shape/Switch Bank */
			ret = sdw_configure_frmshp_bnkswtch(sdw_mstr_bs,
					sel_col, sel_row);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"bank switch failed\n");
				return ret;
			}

			/* Disable all channels enabled on previous bank */
			ret = sdw_dis_chan(sdw_mstr_bs, sdw_mstr_bs_rt);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"Channel disabled faile\n");
				return ret;
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sdw_bus_calc_bw);


/**
 * sdw_bus_calc_bw_dis - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function is called from sdw_disable_and_unprepare
 * whenever stream is ended. The function based disables/
 * unprepare port/channel of associated stream and computes
 * required bandwidth, clock, frameshape, computes
 * all transport params for a given port, enable channel
 * & perform bankswitch for remaining streams on given
 * controller.
 */
int sdw_bus_calc_bw_dis(struct sdw_stream_tag *stream_tag, bool unprepare)
{
	struct sdw_runtime *sdw_rt = stream_tag->sdw_rt;
	struct sdw_mstr_runtime *sdw_mstr_rt = NULL, *sdw_mstr_bs_rt = NULL;
	struct sdw_bus *sdw_mstr_bs = NULL;
	struct sdw_master *sdw_mstr = NULL;
	struct sdw_master_capabilities *sdw_mstr_cap = NULL;
	struct sdw_stream_params *mstr_params;
	int stream_frame_size;
	int frame_interval = 0, sel_row = 0, sel_col = 0;
	int ret = 0;


	/* BW calulation for active master controller for given stream tag */
	list_for_each_entry(sdw_mstr_rt, &sdw_rt->mstr_rt_list, mstr_sdw_node) {

		if (sdw_mstr_rt->mstr == NULL)
			break;

		/* Get bus structure for master */
		list_for_each_entry(sdw_mstr_bs, &sdw_core.bus_list, bus_node) {

			/* Match master structure pointer */
			if (sdw_mstr_bs->mstr != sdw_mstr_rt->mstr)
				continue;


			sdw_mstr = sdw_mstr_bs->mstr;
			break;
		}


		sdw_mstr_cap = &sdw_mstr_bs->mstr->mstr_capabilities;
		mstr_params = &sdw_mstr_rt->stream_params;

		if (sdw_rt->stream_state == SDW_STATE_ENABLE_STREAM) {

			/* Lets do disabling of port for stream to be freed */
			list_for_each_entry(sdw_mstr_bs_rt,
					&sdw_mstr->mstr_rt_list, mstr_node) {

				if (sdw_mstr_bs_rt->mstr == NULL)
					continue;

				/*
				 * Disable channel for slave and
				 * master on current bank
				 */
				ret = sdw_en_dis_mstr_slv(sdw_mstr_bs,
						sdw_rt, false);
				if (ret < 0) {
					/* TBD: Undo all the computation */
					dev_err(&sdw_mstr_bs->mstr->dev,
							"Channel dis failed\n");
					return ret;
				}

				/* Change stream state to disable */
				sdw_rt->stream_state = SDW_STATE_DISABLE_STREAM;
			}

			ret = sdw_cfg_bs_params(sdw_mstr_bs,
					sdw_mstr_bs_rt, false);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"xport params config failed\n");
				return ret;
			}

			sel_col = sdw_mstr_bs->col;
			sel_row = sdw_mstr_bs->row;

			/* Configure frame shape/Switch Bank  */
			ret = sdw_configure_frmshp_bnkswtch(sdw_mstr_bs,
					sel_col, sel_row);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"bank switch failed\n");
				return ret;
			}

			/* Disable all channels enabled on previous bank */
			ret = sdw_dis_chan(sdw_mstr_bs, sdw_mstr_bs_rt);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"Channel disabled failed\n");
				return ret;
			}
		}

		if ((unprepare) &&
				(SDW_STATE_DISABLE_STREAM ==
				 sdw_rt->stream_state)) {

			/* 1. Un-prepare master and slave port */
			list_for_each_entry(sdw_mstr_bs_rt,
					&sdw_mstr->mstr_rt_list, mstr_node) {

				if (sdw_mstr_bs_rt->mstr == NULL)
					continue;

				ret = sdw_prep_unprep_mstr_slv(sdw_mstr_bs,
						sdw_rt, false);
				if (ret < 0) {
					/* TBD: Undo all the computation */
					dev_err(&sdw_mstr_bs->mstr->dev,
							"Chan unprep failed\n");
					return ret;
				}

				/* change stream state to unprepare */
				sdw_rt->stream_state =
					SDW_STATE_UNPREPARE_STREAM;
			}

			/*
			 * Calculate new bandwidth, frame size
			 * and total BW required for master controller
			 */
			sdw_mstr_rt->stream_bw = mstr_params->rate *
				mstr_params->channel_count * mstr_params->bps;
			stream_frame_size = mstr_params->channel_count *
				mstr_params->bps;

			sdw_mstr_bs->bandwidth -= sdw_mstr_rt->stream_bw;

			/* Something went wrong in bandwidth calulation */
			if (sdw_mstr_bs->bandwidth < 0) {
				dev_err(&sdw_mstr_bs->mstr->dev,
						"BW calculation failed\n");
				return -EINVAL;
			}

			if (!sdw_mstr_bs->bandwidth) {
				/*
				 * Last stream on master should
				 * return successfully
				 */
				sdw_rt->stream_state =
					SDW_STATE_UNCOMPUTE_STREAM;
				return 0;
			}

			ret = sdw_get_clock_frmshp(sdw_mstr_bs,
					&frame_interval, &sel_col, &sel_row);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"clock/frameshape failed\n");
				return ret;
			}

			/* Compute new transport params for running streams */
			/* No sorting required here */

			/* Compute system interval */
			ret = sdw_compute_sys_interval(sdw_mstr_bs,
					sdw_mstr_cap, frame_interval);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"compute SI failed\n");
				return ret;
			}

			/* Compute hstart/hstop */
			ret = sdw_compute_hstart_hstop(sdw_mstr_bs, sel_col);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"compute hstart/hstop fail\n");
				return ret;
			}

			/* Compute block offset */
			ret = sdw_compute_blk_subblk_offset(sdw_mstr_bs);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"compute block offset failed\n");
				return ret;
			}

			/* Configure bus params */
			ret = sdw_cfg_bs_params(sdw_mstr_bs,
					sdw_mstr_bs_rt, true);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"xport params config failed\n");
				return ret;
			}

			/* Configure Frame Shape/Switch Bank */
			ret = sdw_configure_frmshp_bnkswtch(sdw_mstr_bs,
					sel_col, sel_row);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"bank switch failed\n");
				return ret;
			}

			/* Change stream state to uncompute */
			sdw_rt->stream_state = SDW_STATE_UNCOMPUTE_STREAM;

			/* Disable all channels enabled on previous bank */
			ret = sdw_dis_chan(sdw_mstr_bs, sdw_mstr_bs_rt);
			if (ret < 0) {
				/* TBD: Undo all the computation */
				dev_err(&sdw_mstr_bs->mstr->dev,
						"Channel disabled failed\n");
				return ret;
			}
		}

	}

	return 0;
}
EXPORT_SYMBOL_GPL(sdw_bus_calc_bw_dis);
