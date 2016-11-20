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
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sdw_bus.h>
#include "sdw_priv.h"
#include <linux/delay.h>
#include <linux/sdw/sdw_registers.h>



#ifndef CONFIG_SND_SOC_SVFPGA /* Original */
#if IS_ENABLED(CONFIG_SND_SOC_INTEL_CNL_FPGA)
int rows[MAX_NUM_ROWS] = {48, 50, 60, 64, 72, 75, 80, 90,
		     96, 125, 144, 147, 100, 120, 128, 150,
		     160, 180, 192, 200, 240, 250, 256};
#define SDW_DEFAULT_SSP		50
#else
int rows[MAX_NUM_ROWS] = {125, 64, 48, 50, 60, 72, 75, 80, 90,
		     96, 144, 147, 100, 120, 128, 150,
		     160, 180, 192, 200, 240, 250, 256};
#define SDW_DEFAULT_SSP		24
#endif /* IS_ENABLED(CONFIG_SND_SOC_INTEL_CNL_FPGA) */

int cols[MAX_NUM_COLS] = {2, 4, 6, 8, 10, 12, 14, 16};

#else
/* For PDM Capture, frameshape used is 50x10 */
int rows[MAX_NUM_ROWS] = {50, 100, 48, 60, 64, 72, 75, 80, 90,
		     96, 125, 144, 147, 120, 128, 150,
		     160, 180, 192, 200, 240, 250, 256};

int cols[MAX_NUM_COLS] = {10, 2, 4, 6, 8, 12, 14, 16};
#define SDW_DEFAULT_SSP		50
#endif

/*
 * TBD: Get supported clock frequency from ACPI and store
 * it in master data structure.
 */
#define MAXCLOCKDIVS		1
int clock_div[MAXCLOCKDIVS] = {1};

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
	sdw_mstr_cap = &sdw_bs->mstr->mstr_capabilities;
	sdw_bs->clk_freq = (sdw_mstr_cap->base_clk_freq * 2);

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
		struct sdw_transport_params *t_slv_params,
		struct sdw_port_params *p_slv_params, int slv_number)
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
	wbuf[4] = t_slv_params->offset2; /* DPN_OffsetCtrl2 */
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
	rd_msg.slave_addr = slv_number;

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

	wr_msg.slave_addr = slv_number;
	wr_msg.buf = &wbuf[0 + (1 * (!t_slv_params->blockgroupcontrol_valid))];
	wr_msg.addr_page1 = 0x0;
	wr_msg.addr_page2 = 0x0;

	wr_msg1.ssp_tag = 0x0;
	wr_msg1.flag = SDW_MSG_FLAG_WRITE;
	wr_msg1.len = 2;

	wr_msg1.slave_addr = slv_number;
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
 * sdw_cfg_params_mstr_slv - returns Success
 *
 * This function copies/configure master/slave transport &
 * port params.
 *
 */
int sdw_cfg_params_mstr_slv(struct sdw_bus *sdw_mstr_bs,
		struct sdw_mstr_runtime *sdw_mstr_bs_rt,
		bool state_check)
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

		/* configure transport params based on state */
		if ((state_check) &&
			(slv_rt->rt_state == SDW_STATE_UNPREPARE_RT))
			continue;

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
			ret = sdw_cfg_slv_params(sdw_mstr_bs, t_slv_params,
				p_slv_params, slv_rt->slave->slv_number);
			if (ret < 0)
				return ret;

		}
	}

	if ((state_check) &&
		(sdw_mstr_bs_rt->rt_state == SDW_STATE_UNPREPARE_RT))
		return 0;

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

	/* 2. Master port enable */
	if (ops->mstr_port_ops->dpn_port_activate_ch) {
		ret = ops->mstr_port_ops->dpn_port_activate_ch(mstr_bs->mstr,
				&activate_ch, banktouse);
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
		struct sdw_mstr_runtime *sdw_mstr_rt)
{
	struct sdw_master_capabilities *sdw_mstr_cap = NULL;
	struct sdw_slv_dpn_capabilities *sdw_slv_dpn_cap = NULL;
	struct port_audio_mode_properties *mode_prop = NULL;
	struct sdw_slave_runtime *slv_rt = NULL;
	struct sdw_port_runtime *port_slv_rt = NULL;
	int i, j, rc;
	int clock_reqd = 0, frame_interval = 0, frame_frequency = 0;
	int sel_row = 0, sel_col = 0, pn = 0;
	int value;
	bool clock_ok = false;

	sdw_mstr_cap = &sdw_mstr_bs->mstr->mstr_capabilities;

	/*
	 * Find nearest clock frequency needed by master for
	 * given bandwidth
	 */
	for (i = 0; i < MAXCLOCKDIVS; i++) {

		/* TBD: Check why 3000 */
		if ((((sdw_mstr_cap->base_clk_freq * 2) / clock_div[i]) <=
			sdw_mstr_bs->bandwidth) ||
			((((sdw_mstr_cap->base_clk_freq * 2) / clock_div[i])
			% 3000) != 0))
			continue;

		clock_reqd = ((sdw_mstr_cap->base_clk_freq * 2) / clock_div[i]);

		/*
		 * Check all the slave device capabilities
		 * here and find whether given frequency is
		 * supported by all slaves
		 */
		list_for_each_entry(slv_rt, &sdw_mstr_rt->slv_rt_list,
								slave_node) {

			/* check for valid slave */
			if (slv_rt->slave == NULL)
				break;

			/* check clock req for each port */
			list_for_each_entry(port_slv_rt,
					&slv_rt->port_rt_list, port_node) {

				pn = port_slv_rt->port_num;


				sdw_slv_dpn_cap =
				&slv_rt->slave->sdw_slv_cap.sdw_dpn_cap[pn];
				mode_prop = sdw_slv_dpn_cap->mode_properties;

				/*
				 * TBD: Indentation to be fixed,
				 * code refactoring to be considered.
				 */
				if (mode_prop->num_freq_configs) {
					for (j = 0; j <
					mode_prop->num_freq_configs; j++) {
						value =
						mode_prop->freq_supported[j];
						if (clock_reqd == value) {
							clock_ok = true;
							break;
						}
						if (j ==
						mode_prop->num_freq_configs) {
							clock_ok = false;
							break;
						}

					}

				} else {
					if ((clock_reqd <
						mode_prop->min_frequency) ||
						(clock_reqd >
						 mode_prop->max_frequency)) {
						clock_ok = false;
					} else
						clock_ok = true;
				}

				/* Go for next clock frequency */
				if (!clock_ok)
					break;
			}

			/*
			 * Dont check next slave, go for next clock
			 * frequency
			 */
			if (!clock_ok)
				break;
		}

		/* check for next clock divider */
		if (!clock_ok)
			continue;

		/* Find frame shape based on bandwidth per controller */
		for (rc = 0; rc < MAX_NUM_ROW_COLS; rc++) {
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

		/* Valid frameshape not found, check for next clock freq */
		if (rc == MAX_NUM_ROW_COLS)
			continue;

		sel_row = sdw_core.rowcolcomb[rc].row;
		sel_col = sdw_core.rowcolcomb[rc].col;
		sdw_mstr_bs->frame_freq = frame_frequency;
		sdw_mstr_bs->clk_freq = clock_reqd;
		sdw_mstr_bs->clk_div = clock_div[i];
		clock_ok = false;
		*frame_int = frame_interval;
		sdw_mstr_bs->col = sel_col;
		sdw_mstr_bs->row = sel_row;

		return 0;
	}

	/* None of clock frequency matches, return error */
	if (i == MAXCLOCKDIVS)
		return -EINVAL;

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
	struct sdw_mstr_runtime *sdw_mstr_rt = NULL;
	struct sdw_slave_runtime *slv_rt = NULL;
	struct sdw_transport_params *t_params = NULL, *t_slv_params = NULL;
	struct sdw_port_runtime *port_rt, *port_slv_rt;
	int lcmnum1 = 0, lcmnum2 = 0, div = 0, lcm = 0;
	int sample_interval;

	/*
	 * once you got bandwidth frame shape for bus,
	 * run a loop for all the active streams running
	 * on bus and compute stream interval & sample_interval.
	 */
	list_for_each_entry(sdw_mstr_rt,
			&sdw_mstr->mstr_rt_list, mstr_node) {

		if (sdw_mstr_rt->mstr == NULL)
			break;

		/*
		 * Calculate sample interval for stream
		 * running on given master.
		 */
		if (sdw_mstr_rt->stream_params.rate)
			sample_interval = (sdw_mstr_bs->clk_freq/
					sdw_mstr_rt->stream_params.rate);
		else
			return -EINVAL;

		/* Run port loop to assign sample interval per port */
		list_for_each_entry(port_rt,
				&sdw_mstr_rt->port_rt_list, port_node) {

			t_params = &port_rt->transport_params;

			/*
			 * Assign sample interval each port transport
			 * properties. Assumption is that sample interval
			 * per port for given master will be same.
			 */
			t_params->sample_interval = sample_interval;
		}

		/* Calculate LCM */
		lcmnum2 = sample_interval;
		if (!lcmnum1)
			lcmnum1 = sdw_lcm(lcmnum2, lcmnum2);
		else
			lcmnum1 = sdw_lcm(lcmnum1, lcmnum2);

		/* Run loop for slave per master runtime */
		list_for_each_entry(slv_rt,
				&sdw_mstr_rt->slv_rt_list, slave_node)  {

			if (slv_rt->slave == NULL)
				break;

			/* Assign sample interval for each port of slave  */
			list_for_each_entry(port_slv_rt,
					&slv_rt->port_rt_list, port_node) {

				t_slv_params = &port_slv_rt->transport_params;

				 /* Assign sample interval each port */
				t_slv_params->sample_interval = sample_interval;
			}
		}
	}

	/*
	 * If system interval already calculated
	 * In pause/resume, underrun scenario
	 */
	if (sdw_mstr_bs->system_interval)
		return 0;

	/* Assign frame stream interval */
	sdw_mstr_bs->stream_interval = lcmnum1;

	/* 6. compute system_interval */
	if ((sdw_mstr_cap) && (sdw_mstr_bs->clk_freq)) {

		div = ((sdw_mstr_cap->base_clk_freq * 2) /
					sdw_mstr_bs->clk_freq);

		if ((lcmnum1) && (frame_interval))
			lcm = sdw_lcm(lcmnum1, frame_interval);
		else
			return -EINVAL;

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

/**
 * sdw_chk_first_node - returns True or false
 *
 * This function returns true in case of first node
 * else returns false.
 */
bool sdw_chk_first_node(struct sdw_mstr_runtime *sdw_mstr_rt,
	struct sdw_master *sdw_mstr)
{
	struct sdw_mstr_runtime	*first_rt = NULL;

	first_rt = list_first_entry(&sdw_mstr->mstr_rt_list,
			struct sdw_mstr_runtime, mstr_node);
	if (sdw_mstr_rt == first_rt)
		return true;
	else
		return false;

}

/*
 * sdw_compute_hstart_hstop - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function computes hstart and hstop for running
 * streams per master & slaves.
 */
int sdw_compute_hstart_hstop(struct sdw_bus *sdw_mstr_bs)
{
	struct sdw_master *sdw_mstr = sdw_mstr_bs->mstr;
	struct sdw_mstr_runtime *sdw_mstr_rt;
	struct sdw_transport_params *t_params = NULL, *t_slv_params = NULL;
	struct sdw_slave_runtime *slv_rt = NULL;
	struct sdw_port_runtime *port_rt, *port_slv_rt;
	int hstart = 0, hstop = 0;
	int column_needed = 0;
	int sel_col = sdw_mstr_bs->col;
	int group_count = 0, no_of_channels = 0;
	struct temp_elements *temp, *element;
	int rates[10];
	int num, ch_mask, block_offset, i, port_block_offset;

	/* Run loop for all master runtimes for given master */
	list_for_each_entry(sdw_mstr_rt,
			&sdw_mstr->mstr_rt_list, mstr_node) {

		if (sdw_mstr_rt->mstr == NULL)
			break;

		/* should not compute any transport params */
		if (sdw_mstr_rt->rt_state == SDW_STATE_UNPREPARE_RT)
			continue;

		/* Perform grouping of streams based on stream rate */
		if (sdw_mstr_rt == list_first_entry(&sdw_mstr->mstr_rt_list,
					struct sdw_mstr_runtime, mstr_node))
			rates[group_count++] = sdw_mstr_rt->stream_params.rate;
		else {
			num = group_count;
			for (i = 0; i < num; i++) {
				if (sdw_mstr_rt->stream_params.rate == rates[i])
					break;

				if (i == num)
					rates[group_count++] =
					sdw_mstr_rt->stream_params.rate;
			}
		}
	}

	/* check for number of streams and number of group count */
	if (group_count == 0)
		return 0;

	/* Allocate temporary memory holding temp variables */
	temp = kzalloc((sizeof(struct temp_elements) * group_count),
			GFP_KERNEL);
	if (!temp)
		return -ENOMEM;

	/* Calculate full bandwidth per group */
	for (i = 0; i < group_count; i++) {
		element = &temp[i];
		element->rate = rates[i];
		element->full_bw = sdw_mstr_bs->clk_freq/element->rate;
	}

	/* Calculate payload bandwidth per group */
	list_for_each_entry(sdw_mstr_rt,
			&sdw_mstr->mstr_rt_list, mstr_node) {

		if (sdw_mstr_rt->mstr == NULL)
			break;

		/* should not compute any transport params */
		if (sdw_mstr_rt->rt_state == SDW_STATE_UNPREPARE_RT)
			continue;

		for (i = 0; i < group_count; i++) {
			element = &temp[i];
			if (sdw_mstr_rt->stream_params.rate == element->rate) {
				element->payload_bw +=
				sdw_mstr_rt->stream_params.bps *
				sdw_mstr_rt->stream_params.channel_count;
			}

			/* Any of stream rate should match */
			if (i == group_count)
				return -EINVAL;
		}
	}

	/* Calculate hwidth per group and total column needed per master */
	for (i = 0; i < group_count; i++) {
		element = &temp[i];
		element->hwidth =
			(sel_col * element->payload_bw +
			element->full_bw - 1)/element->full_bw;
		column_needed += element->hwidth;
	}

	/* Check column required should not be greater than selected columns*/
	if  (column_needed > sel_col - 1)
		return -EINVAL;

	/* Compute hstop */
	hstop = sel_col - 1;

	/* Run loop for all groups to compute transport parameters */
	for (i = 0; i < group_count; i++) {
		port_block_offset = block_offset = 1;
		element = &temp[i];

		/* Find streams associated with each group */
		list_for_each_entry(sdw_mstr_rt,
				&sdw_mstr->mstr_rt_list, mstr_node) {

			if (sdw_mstr_rt->mstr == NULL)
				break;

			/* should not compute any transport params */
			if (sdw_mstr_rt->rt_state == SDW_STATE_UNPREPARE_RT)
				continue;

			if (sdw_mstr_rt->stream_params.rate != element->rate)
				continue;

			/* Compute hstart */
			sdw_mstr_rt->hstart = hstart =
					hstop - element->hwidth + 1;
			sdw_mstr_rt->hstop = hstop;

			/* Assign hstart, hstop, block offset for each port */
			list_for_each_entry(port_rt,
					&sdw_mstr_rt->port_rt_list, port_node) {

				t_params = &port_rt->transport_params;
				t_params->num = port_rt->port_num;
				t_params->hstart = hstart;
				t_params->hstop = hstop;
				t_params->offset1 = port_block_offset;
				t_params->offset2 = port_block_offset >> 8;

				/* Only BlockPerPort supported */
				t_params->blockgroupcontrol_valid = true;
				t_params->blockgroupcontrol = 0x0;
				t_params->lanecontrol = 0x0;
				/* Copy parameters if first node */
				if (port_rt == list_first_entry
						(&sdw_mstr_rt->port_rt_list,
					struct sdw_port_runtime, port_node)) {

					sdw_mstr_rt->hstart = hstart;
					sdw_mstr_rt->hstop = hstop;

					sdw_mstr_rt->block_offset =
							port_block_offset;

				}

				/* Get no. of channels running on curr. port */
				ch_mask = port_rt->channel_mask;
				no_of_channels = (((ch_mask >> 3) & 1) +
						((ch_mask >> 2) & 1) +
						((ch_mask >> 1) & 1) +
						(ch_mask & 1));


			port_block_offset +=
				sdw_mstr_rt->stream_params.bps *
				no_of_channels;
			}

			/* Compute block offset */
			block_offset += sdw_mstr_rt->stream_params.bps *
				sdw_mstr_rt->stream_params.channel_count;

			/*
			 * Re-assign port_block_offset for next stream
			 * under same group
			 */
			port_block_offset = block_offset;
		}

		/* Compute hstop for next group */
		hstop = hstop - element->hwidth;
	}

	/* Compute transport params for slave */

	/* Run loop for master runtime streams running on master */
	list_for_each_entry(sdw_mstr_rt,
			&sdw_mstr->mstr_rt_list, mstr_node) {

		/* Get block offset from master runtime */
		port_block_offset = sdw_mstr_rt->block_offset;

		/* Run loop for slave per master runtime */
		list_for_each_entry(slv_rt,
				&sdw_mstr_rt->slv_rt_list, slave_node)  {

			if (slv_rt->slave == NULL)
				break;

			if (slv_rt->rt_state == SDW_STATE_UNPREPARE_RT)
				continue;

			/* Run loop for each port of slave */
			list_for_each_entry(port_slv_rt,
					&slv_rt->port_rt_list, port_node) {

				t_slv_params = &port_slv_rt->transport_params;
				t_slv_params->num = port_slv_rt->port_num;

				/* Assign transport parameters */
				t_slv_params->hstart = sdw_mstr_rt->hstart;
				t_slv_params->hstop = sdw_mstr_rt->hstop;
				t_slv_params->offset1 = port_block_offset;
				t_slv_params->offset2 = port_block_offset >> 8;

				/* Only BlockPerPort supported */
				t_slv_params->blockgroupcontrol_valid = true;
				t_slv_params->blockgroupcontrol = 0x0;
				t_slv_params->lanecontrol = 0x0;

				/* Get no. of channels running on curr. port */
				ch_mask = port_slv_rt->channel_mask;
				no_of_channels = (((ch_mask >> 3) & 1) +
						((ch_mask >> 2) & 1) +
						((ch_mask >> 1) & 1) +
						(ch_mask & 1));

				/* Increment block offset for next port/slave */
				port_block_offset += slv_rt->stream_params.bps *
							no_of_channels;
			}
		}
	}

	kfree(temp);

	return 0;
}

/*
 * sdw_cfg_frmshp_bnkswtch - returns Success
 * -EINVAL - In case of error.
 * -ENOMEM - In case of memory alloc failure.
 * -EAGAIN - In case of activity ongoing.
 *
 *
 * This function broadcast frameshape on framectrl
 * register and performs bank switch.
 */
int sdw_cfg_frmshp_bnkswtch(struct sdw_bus *mstr_bs, bool is_wait)
{
	struct sdw_msg *wr_msg;
	int ret = 0;
	int banktouse, numcol, numrow;
	u8 *wbuf;

	wr_msg = kzalloc(sizeof(struct sdw_msg), GFP_KERNEL);
	if (!wr_msg)
		return -ENOMEM;

	mstr_bs->async_data.msg = wr_msg;

	wbuf = kzalloc(sizeof(*wbuf), GFP_KERNEL);
		if (!wbuf)
			return -ENOMEM;

	numcol = sdw_get_col_to_num(mstr_bs->col);
	numrow = sdw_get_row_to_num(mstr_bs->row);

	wbuf[0] = numcol | (numrow << 3);
	/* Get current bank in use from bus structure*/
	banktouse = mstr_bs->active_bank;
	banktouse = !banktouse;

	if (banktouse) {
		wr_msg->addr = (SDW_SCP_FRAMECTRL + SDW_BANK1_REGISTER_OFFSET) +
			(SDW_NUM_DATA_PORT_REGISTERS * 0); /* Data port 0 */
	} else {

		wr_msg->addr = SDW_SCP_FRAMECTRL +
			(SDW_NUM_DATA_PORT_REGISTERS * 0); /* Data port 0 */
	}

	wr_msg->ssp_tag = 0x1;
	wr_msg->flag = SDW_MSG_FLAG_WRITE;
	wr_msg->len = 1;
	wr_msg->slave_addr = 0xF; /* Broadcast address*/
	wr_msg->buf = wbuf;
	wr_msg->addr_page1 = 0x0;
	wr_msg->addr_page2 = 0x0;

	if (is_wait) {

		if (in_atomic() || irqs_disabled()) {
			ret = sdw_trylock_mstr(mstr_bs->mstr);
			if (!ret) {
				/* SDW activity is ongoing. */
				ret = -EAGAIN;
				goto out;
			}
		} else
			sdw_lock_mstr(mstr_bs->mstr);

		ret = sdw_slave_transfer_async(mstr_bs->mstr, wr_msg,
				1, &mstr_bs->async_data);
		if (ret != 1) {
			ret = -EINVAL;
			dev_err(&mstr_bs->mstr->dev, "Register transfer failed\n");
			goto out;
		}

	} else {
		ret = sdw_slave_transfer(mstr_bs->mstr, wr_msg, 1);
		if (ret != 1) {
			ret = -EINVAL;
			dev_err(&mstr_bs->mstr->dev, "Register transfer failed\n");
			goto out;
		}

	}

	msleep(100); /* TBD: Remove this */

	/*
	 * TBD: check whether we need to poll on
	 * mcp active bank bit to switch bank
	 */
	mstr_bs->active_bank = banktouse;

	if (!is_wait) {
		kfree(mstr_bs->async_data.msg->buf);
		kfree(mstr_bs->async_data.msg);
	}


out:

	return ret;
}

/*
 * sdw_cfg_frmshp_bnkswtch_wait - returns Success
 * -ETIMEDOUT - In case of timeout
 *
 * This function waits on completion of
 * bank switch.
 */
int sdw_cfg_frmshp_bnkswtch_wait(struct sdw_bus *mstr_bs)
{
	unsigned long time_left;
	struct sdw_master *mstr = mstr_bs->mstr;

	time_left = wait_for_completion_timeout(
			&mstr_bs->async_data.xfer_complete,
			3000);
	if (!time_left) {
		dev_err(&mstr->dev, "Controller Timed out\n");
		sdw_unlock_mstr(mstr);
		return -ETIMEDOUT;
	}
	kfree(mstr_bs->async_data.msg->buf);
	kfree(mstr_bs->async_data.msg);
	sdw_unlock_mstr(mstr);
	return 0;
}

/*
 * sdw_config_bs_prms - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function performs master/slave transport
 * params config, set SSP interval, set Clock
 * frequency, enable channel. This API is called
 * from sdw_bus_calc_bw & sdw_bus_calc_bw_dis API.
 *
 */
int sdw_config_bs_prms(struct sdw_bus *sdw_mstr_bs, bool state_check)
{
	struct port_chn_en_state chn_en;
	struct sdw_master *sdw_mstr = sdw_mstr_bs->mstr;
	struct sdw_mstr_runtime *sdw_mstr_bs_rt = NULL;
	struct sdw_mstr_driver *ops;
	int banktouse, ret = 0;

	list_for_each_entry(sdw_mstr_bs_rt,
			&sdw_mstr->mstr_rt_list, mstr_node) {

		if (sdw_mstr_bs_rt->mstr == NULL)
			continue;

		/*
		 * Configure transport and port params
		 * for master and slave ports.
		 */
		ret = sdw_cfg_params_mstr_slv(sdw_mstr_bs,
				sdw_mstr_bs_rt, state_check);
		if (ret < 0) {
			/* TBD: Undo all the computation */
			dev_err(&sdw_mstr_bs->mstr->dev,
					"slave/master config params failed\n");
			return ret;
		}

		/* Get master driver ops */
		ops = sdw_mstr_bs->mstr->driver;

		/* Configure SSP */
		banktouse = sdw_mstr_bs->active_bank;
		banktouse = !banktouse;

		/*
		 * TBD: Currently harcoded SSP interval,
		 * computed value to be taken from system_interval in
		 * bus data structure.
		 * Add error check.
		 */
		if (ops->mstr_ops->set_ssp_interval)
			ops->mstr_ops->set_ssp_interval(sdw_mstr_bs->mstr,
					SDW_DEFAULT_SSP, banktouse);

		/*
		 * Configure Clock
		 * TBD: Add error check
		 */
		if (ops->mstr_ops->set_clock_freq)
			ops->mstr_ops->set_clock_freq(sdw_mstr_bs->mstr,
					sdw_mstr_bs->clk_div, banktouse);

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
int sdw_dis_chan(struct sdw_bus *sdw_mstr_bs)
{
	struct sdw_master *sdw_mstr = sdw_mstr_bs->mstr;
	struct sdw_mstr_runtime *sdw_mstr_bs_rt = NULL;
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
			}

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
		}
	}

	return 0;
}

struct sdw_bus *master_to_bus(struct sdw_master *mstr)
{
	struct sdw_bus *sdw_mstr_bs = NULL;

	list_for_each_entry(sdw_mstr_bs, &sdw_core.bus_list, bus_node) {
		/* Match master structure pointer */
		if (sdw_mstr_bs->mstr != mstr)
			continue;
		return sdw_mstr_bs;
	}
	/* This should never happen, added to suppress warning */
	WARN_ON(1);

	return NULL;
}

/*
 * sdw_chk_strm_prms - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function performs all the required
 * check such as isynchronous mode support,
 * stream rates etc. This API is called
 * from sdw_bus_calc_bw API.
 *
 */
int sdw_chk_strm_prms(struct sdw_master_capabilities *sdw_mstr_cap,
			struct sdw_stream_params *mstr_params,
			struct sdw_stream_params *stream_params)
{
	/* Asynchronous mode not supported, return Error */
	if (((sdw_mstr_cap->base_clk_freq * 2) % mstr_params->rate) != 0)
		return -EINVAL;

	/* Check for sampling frequency */
	if (stream_params->rate != mstr_params->rate)
		return -EINVAL;

	return 0;
}

/*
 * sdw_compute_bs_prms - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function performs master/slave transport
 * params computation. This API is called
 * from sdw_bus_calc_bw & sdw_bus_calc_bw_dis API.
 *
 */
int sdw_compute_bs_prms(struct sdw_bus *sdw_mstr_bs,
		struct sdw_mstr_runtime *sdw_mstr_rt)
{

	struct sdw_master_capabilities *sdw_mstr_cap = NULL;
	struct sdw_master *sdw_mstr = sdw_mstr_bs->mstr;
	int ret = 0, frame_interval = 0;

	sdw_mstr_cap = &sdw_mstr->mstr_capabilities;

	ret = sdw_get_clock_frmshp(sdw_mstr_bs, &frame_interval,
			sdw_mstr_rt);
	if (ret < 0) {
		/* TBD: Undo all the computation */
		dev_err(&sdw_mstr->dev, "clock/frameshape config failed\n");
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
	ret = sdw_compute_sys_interval(sdw_mstr_bs, sdw_mstr_cap,
			frame_interval);
	if (ret < 0) {
		/* TBD: Undo all the computation */
		dev_err(&sdw_mstr->dev, "compute system interval failed\n");
		return ret;
	}

	/* Compute hstart/hstop */
	ret = sdw_compute_hstart_hstop(sdw_mstr_bs);
	if (ret < 0) {
		/* TBD: Undo all the computation */
		dev_err(&sdw_mstr->dev, "compute hstart/hstop failed\n");
		return ret;
	}

	return 0;
}

/*
 * sdw_bs_pre_bnkswtch_post - returns Success
 * -EINVAL or ret value - In case of error.
 *
 * This API performs on of the following operation
 * based on bs_state value:
 * pre-activate port
 * bank switch operation
 * post-activate port
 * bankswitch wait operation
 * disable channel operation
 */
int sdw_bs_pre_bnkswtch_post(struct sdw_runtime *sdw_rt, int bs_state)
{
	struct sdw_mstr_runtime	*mstr_rt_act = NULL;
	struct sdw_bus *mstr_bs_act = NULL;
	struct sdw_master_port_ops *ops;
	int ret = 0;

	list_for_each_entry(mstr_rt_act, &sdw_rt->mstr_rt_list,
			mstr_sdw_node) {

		if (mstr_rt_act->mstr == NULL)
			break;

		/* Get bus structure for master */
		mstr_bs_act = master_to_bus(mstr_rt_act->mstr);
		ops = mstr_bs_act->mstr->driver->mstr_port_ops;

		/*
		 * Note that current all the operations
		 * of pre->bankswitch->post->wait->disable
		 * are performed sequentially.The switch case
		 * is kept in order for code to scale where
		 * pre->bankswitch->post->wait->disable are
		 * not sequential and called from different
		 * instances.
		 */
		switch (bs_state) {

		case SDW_UPDATE_BS_PRE:
			/* Pre activate ports */
			if (ops->dpn_port_activate_ch_pre) {
				ret = ops->dpn_port_activate_ch_pre
					(mstr_bs_act->mstr, NULL, 0);
				if (ret < 0)
					return ret;
			}
			break;
		case SDW_UPDATE_BS_BNKSWTCH:
			/* Configure Frame Shape/Switch Bank */
			ret = sdw_cfg_frmshp_bnkswtch(mstr_bs_act, true);
			if (ret < 0)
				return ret;
			break;
		case SDW_UPDATE_BS_POST:
			/* Post activate ports */
			if (ops->dpn_port_activate_ch_post) {
				ret = ops->dpn_port_activate_ch_post
					(mstr_bs_act->mstr, NULL, 0);
				if (ret < 0)
					return ret;
			}
			break;
		case SDW_UPDATE_BS_BNKSWTCH_WAIT:
			/* Post Bankswitch wait operation */
			ret = sdw_cfg_frmshp_bnkswtch_wait(mstr_bs_act);
			if (ret < 0)
				return ret;
			break;
		case SDW_UPDATE_BS_DIS_CHN:
			/* Disable channel on previous bank */
			ret = sdw_dis_chan(mstr_bs_act);
			if (ret < 0)
				return ret;
			break;
		default:
			return -EINVAL;
			break;
		}
	}

	return ret;

}

/*
 * sdw_update_bs_prms - returns Success
 * -EINVAL - In case of error.
 *
 * Once all the parameters are configured
 * for ports, this function performs bankswitch
 * where all the new configured parameters
 * gets in effect. This function is called
 * from sdw_bus_calc_bw & sdw_bus_calc_bw_dis API.
 * This function also disables all the channels
 * enabled on previous bank after bankswitch.
 */
int sdw_update_bs_prms(struct sdw_bus *sdw_mstr_bs,
		struct sdw_runtime *sdw_rt,
		int last_node)
{

	struct sdw_master *sdw_mstr = sdw_mstr_bs->mstr;
	int ret = 0;

	/*
	 * Optimization scope.
	 * Check whether we can assign function pointer
	 * link sync value is 1, and call that function
	 * if its not NULL.
	 */
	if ((last_node) && (sdw_mstr->link_sync_mask)) {

		/* Perform pre-activate ports */
		ret = sdw_bs_pre_bnkswtch_post(sdw_rt, SDW_UPDATE_BS_PRE);
		if (ret < 0) {
			dev_err(&sdw_mstr->dev, "Pre-activate port failed\n");
			return ret;
		}

		/* Perform bankswitch operation*/
		ret = sdw_bs_pre_bnkswtch_post(sdw_rt, SDW_UPDATE_BS_BNKSWTCH);
		if (ret < 0) {
			dev_err(&sdw_mstr->dev, "Bank Switch operation failed\n");
			return ret;
		}

		/* Perform post-activate ports */
		ret = sdw_bs_pre_bnkswtch_post(sdw_rt, SDW_UPDATE_BS_POST);
		if (ret < 0) {
			dev_err(&sdw_mstr->dev, "Pre-activate port failed\n");
			return ret;
		}

		/* Perform bankswitch post wait opearation */
		ret = sdw_bs_pre_bnkswtch_post(sdw_rt,
				SDW_UPDATE_BS_BNKSWTCH_WAIT);
		if (ret < 0) {
			dev_err(&sdw_mstr->dev, "BnkSwtch wait op failed\n");
			return ret;
		}

		/* Disable channels on previous bank */
		ret = sdw_bs_pre_bnkswtch_post(sdw_rt, SDW_UPDATE_BS_DIS_CHN);
		if (ret < 0) {
			dev_err(&sdw_mstr->dev, "Channel disabled failed\n");
			return ret;
		}

	}

	if (!sdw_mstr->link_sync_mask) {

		/* Configure Frame Shape/Switch Bank */
		ret = sdw_cfg_frmshp_bnkswtch(sdw_mstr_bs, false);
		if (ret < 0) {
			/* TBD: Undo all the computation */
			dev_err(&sdw_mstr->dev, "bank switch failed\n");
			return ret;
		}

		/* Disable all channels enabled on previous bank */
		ret = sdw_dis_chan(sdw_mstr_bs);
		if (ret < 0) {
			/* TBD: Undo all the computation */
			dev_err(&sdw_mstr->dev, "Channel disabled failed\n");
			return ret;
		}
	}

	return ret;
}

/**
 * sdw_chk_last_node - returns True or false
 *
 * This function returns true in case of last node
 * else returns false.
 */
bool sdw_chk_last_node(struct sdw_mstr_runtime *sdw_mstr_rt,
	struct sdw_runtime *sdw_rt)
{
	struct sdw_mstr_runtime	*last_rt = NULL;

	last_rt = list_last_entry(&sdw_rt->mstr_rt_list,
			struct sdw_mstr_runtime, mstr_sdw_node);
	if (sdw_mstr_rt == last_rt)
		return true;
	else
		return false;

}

/**
 * sdw_unprepare_op - returns Success
 * -EINVAL - In case of error.
 *
 * This function perform all operations required
 * to unprepare ports and does recomputation of
 * bus parameters.
 */
int sdw_unprepare_op(struct sdw_bus *sdw_mstr_bs,
	struct sdw_mstr_runtime *sdw_mstr_rt,
	struct sdw_runtime *sdw_rt)
{

	struct sdw_master *sdw_mstr = sdw_mstr_bs->mstr;
	struct sdw_stream_params *mstr_params;
	bool last_node = false;
	int ret = 0;

	last_node = sdw_chk_last_node(sdw_mstr_rt, sdw_rt);
	mstr_params = &sdw_mstr_rt->stream_params;

	/* 1. Un-prepare master and slave port */
	ret = sdw_prep_unprep_mstr_slv(sdw_mstr_bs,
			sdw_rt, false);
	if (ret < 0) {
		/* TBD: Undo all the computation */
		dev_err(&sdw_mstr->dev, "Ch unprep failed\n");
		return ret;
	}

	/* change stream state to unprepare */
	if (last_node)
		sdw_rt->stream_state =
			SDW_STATE_UNPREPARE_STREAM;

	/*
	 * Calculate new bandwidth, frame size
	 * and total BW required for master controller
	 */
	sdw_mstr_rt->stream_bw = mstr_params->rate *
		mstr_params->channel_count * mstr_params->bps;
	sdw_mstr_bs->bandwidth -= sdw_mstr_rt->stream_bw;

	/* Something went wrong in bandwidth calulation */
	if (sdw_mstr_bs->bandwidth < 0) {
		dev_err(&sdw_mstr->dev, "BW calculation failed\n");
		return -EINVAL;
	}

	if (!sdw_mstr_bs->bandwidth) {
		/*
		 * Last stream on master should
		 * return successfully
		 */
		sdw_mstr_bs->system_interval = 0;
		sdw_mstr_bs->stream_interval = 0;
		sdw_mstr_bs->frame_freq = 0;
		sdw_mstr_bs->row = 0;
		sdw_mstr_bs->col = 0;
		return 0;
	}

	/* Compute transport params */
	ret = sdw_compute_bs_prms(sdw_mstr_bs, sdw_mstr_rt);
	if (ret < 0) {
		/* TBD: Undo all the computation */
		dev_err(&sdw_mstr->dev, "Params computation failed\n");
		return -EINVAL;
	}

	/* Configure bus params */
	ret = sdw_config_bs_prms(sdw_mstr_bs, true);
	if (ret < 0) {
		/* TBD: Undo all the computation */
		dev_err(&sdw_mstr->dev, "xport params config failed\n");
		return ret;
	}

	/*
	 * Perform SDW bus update
	 * For Aggregation flow:
	 * Pre-> Bankswitch -> Post -> Disable channel
	 * For normal flow:
	 * Bankswitch -> Disable channel
	 */
	ret = sdw_update_bs_prms(sdw_mstr_bs, sdw_rt, last_node);

	return ret;
}

/**
 * sdw_disable_op - returns Success
 * -EINVAL - In case of error.
 *
 * This function perform all operations required
 * to disable ports.
 */
int sdw_disable_op(struct sdw_bus *sdw_mstr_bs,
	struct sdw_mstr_runtime *sdw_mstr_rt,
	struct sdw_runtime *sdw_rt)
{

	struct sdw_master *sdw_mstr = sdw_mstr_bs->mstr;
	struct sdw_master_capabilities *sdw_mstr_cap = NULL;
	struct sdw_stream_params *mstr_params;
	bool last_node = false;
	int ret = 0;


	last_node = sdw_chk_last_node(sdw_mstr_rt, sdw_rt);
	sdw_mstr_cap = &sdw_mstr_bs->mstr->mstr_capabilities;
	mstr_params = &sdw_mstr_rt->stream_params;

	/* Lets do disabling of port for stream to be freed */
	ret = sdw_en_dis_mstr_slv(sdw_mstr_bs, sdw_rt, false);
	if (ret < 0) {
		/* TBD: Undo all the computation */
		dev_err(&sdw_mstr->dev, "Ch dis failed\n");
		return ret;
	}

	/* Change stream state to disable */
	if (last_node)
		sdw_rt->stream_state = SDW_STATE_DISABLE_STREAM;

	ret = sdw_config_bs_prms(sdw_mstr_bs, false);
	if (ret < 0) {
		/* TBD: Undo all the computation */
		dev_err(&sdw_mstr->dev, "xport params config failed\n");
		return ret;
	}

	/*
	 * Perform SDW bus update
	 * For Aggregation flow:
	 * Pre-> Bankswitch -> Post -> Disable channel
	 * For normal flow:
	 * Bankswitch -> Disable channel
	 */
	ret = sdw_update_bs_prms(sdw_mstr_bs, sdw_rt, last_node);

	return ret;
}

/**
 * sdw_enable_op - returns Success
 * -EINVAL - In case of error.
 *
 * This function perform all operations required
 * to enable ports.
 */
int sdw_enable_op(struct sdw_bus *sdw_mstr_bs,
	struct sdw_mstr_runtime *sdw_mstr_rt,
	struct sdw_runtime *sdw_rt)
{

	struct sdw_master *sdw_mstr = sdw_mstr_bs->mstr;
	bool last_node = false;
	int ret = 0;

	last_node = sdw_chk_last_node(sdw_mstr_rt, sdw_rt);

	ret = sdw_config_bs_prms(sdw_mstr_bs, false);
	if (ret < 0) {
		/* TBD: Undo all the computation */
		dev_err(&sdw_mstr->dev, "xport params config failed\n");
		return ret;
	}

	/* Enable new port for master and slave */
	ret = sdw_en_dis_mstr_slv(sdw_mstr_bs, sdw_rt, true);
	if (ret < 0) {
		/* TBD: Undo all the computation */
		dev_err(&sdw_mstr->dev, "Channel enable failed\n");
		return ret;
	}

	/* change stream state to enable */
	if (last_node)
		sdw_rt->stream_state = SDW_STATE_ENABLE_STREAM;
	/*
	 * Perform SDW bus update
	 * For Aggregation flow:
	 * Pre-> Bankswitch -> Post -> Disable channel
	 * For normal flow:
	 * Bankswitch -> Disable channel
	 */
	ret = sdw_update_bs_prms(sdw_mstr_bs, sdw_rt, last_node);

	return ret;
}

/**
 * sdw_prepare_op - returns Success
 * -EINVAL - In case of error.
 *
 * This function perform all operations required
 * to prepare ports and does computation of
 * bus parameters.
 */
int sdw_prepare_op(struct sdw_bus *sdw_mstr_bs,
	struct sdw_mstr_runtime *sdw_mstr_rt,
	struct sdw_runtime *sdw_rt)
{
	struct sdw_stream_params *stream_params = &sdw_rt->stream_params;
	struct sdw_master *sdw_mstr = sdw_mstr_bs->mstr;
	struct sdw_master_capabilities *sdw_mstr_cap = NULL;
	struct sdw_stream_params *mstr_params;

	bool last_node = false;
	int ret = 0;

	last_node = sdw_chk_last_node(sdw_mstr_rt, sdw_rt);
	sdw_mstr_cap = &sdw_mstr_bs->mstr->mstr_capabilities;
	mstr_params = &sdw_mstr_rt->stream_params;

	/*
	 * check all the stream parameters received
	 * Check for isochronous mode, sample rate etc
	 */
	ret = sdw_chk_strm_prms(sdw_mstr_cap, mstr_params,
			stream_params);
	if (ret < 0) {
		dev_err(&sdw_mstr->dev, "Stream param check failed\n");
		return -EINVAL;
	}

	/*
	 * Calculate stream bandwidth, frame size and
	 * total BW required for master controller
	 */
	sdw_mstr_rt->stream_bw = mstr_params->rate *
		mstr_params->channel_count * mstr_params->bps;
	sdw_mstr_bs->bandwidth += sdw_mstr_rt->stream_bw;

	/* Compute transport params */
	ret = sdw_compute_bs_prms(sdw_mstr_bs, sdw_mstr_rt);
	if (ret < 0) {
		/* TBD: Undo all the computation */
		dev_err(&sdw_mstr->dev, "Params computation failed\n");
		return -EINVAL;
	}

	/* Configure bus parameters */
	ret = sdw_config_bs_prms(sdw_mstr_bs, true);
	if (ret < 0) {
		/* TBD: Undo all the computation */
		dev_err(&sdw_mstr->dev, "xport param config failed\n");
		return ret;
	}

	/*
	 * Perform SDW bus update
	 * For Aggregation flow:
	 * Pre-> Bankswitch -> Post -> Disable channel
	 * For normal flow:
	 * Bankswitch -> Disable channel
	 */
	ret = sdw_update_bs_prms(sdw_mstr_bs, sdw_rt, last_node);

	/* Prepare new port for master and slave */
	ret = sdw_prep_unprep_mstr_slv(sdw_mstr_bs, sdw_rt, true);
	if (ret < 0) {
		/* TBD: Undo all the computation */
		dev_err(&sdw_mstr->dev, "Channel prepare failed\n");
		return ret;
	}

	/* change stream state to prepare */
	if (last_node)
		sdw_rt->stream_state = SDW_STATE_PREPARE_STREAM;


	return ret;
}

/**
 * sdw_pre_en_dis_unprep_op - returns Success
 * -EINVAL - In case of error.
 *
 * This function is called by sdw_bus_calc_bw
 * and sdw_bus_calc_bw_dis to prepare, enable,
 * unprepare and disable ports. Based on state
 * value, individual APIs are called.
 */
int sdw_pre_en_dis_unprep_op(struct sdw_mstr_runtime *sdw_mstr_rt,
	struct sdw_runtime *sdw_rt, int state)
{
	struct sdw_master *sdw_mstr = NULL;
	struct sdw_bus *sdw_mstr_bs = NULL;
	int ret = 0;

	/* Get bus structure for master */
	sdw_mstr_bs = master_to_bus(sdw_mstr_rt->mstr);
	sdw_mstr = sdw_mstr_bs->mstr;

	/*
	 * All data structures required available,
	 * lets calculate BW for master controller
	 */

	switch (state) {

	case SDW_STATE_PREPARE_STREAM: /* Prepare */
		ret = sdw_prepare_op(sdw_mstr_bs, sdw_mstr_rt, sdw_rt);
		break;
	case SDW_STATE_ENABLE_STREAM: /* Enable */
		ret = sdw_enable_op(sdw_mstr_bs, sdw_mstr_rt, sdw_rt);
		break;
	case SDW_STATE_DISABLE_STREAM: /* Disable */
		ret = sdw_disable_op(sdw_mstr_bs, sdw_mstr_rt, sdw_rt);
		break;
	case SDW_STATE_UNPREPARE_STREAM: /* UnPrepare */
		ret = sdw_unprepare_op(sdw_mstr_bs, sdw_mstr_rt, sdw_rt);
		break;
	default:
		ret = -EINVAL;
		break;

	}

	return ret;
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
	struct sdw_mstr_runtime *sdw_mstr_rt = NULL;
	struct sdw_bus *sdw_mstr_bs = NULL;
	struct sdw_master *sdw_mstr = NULL;
	int ret = 0;


	/*
	 * TBD: check for mstr_rt is in configured state or not
	 * If yes, then configure masters as well
	 * If no, then do not configure/enable master related parameters
	 */

	/* BW calulation for active master controller for given stream tag */
	list_for_each_entry(sdw_mstr_rt, &sdw_rt->mstr_rt_list,
							mstr_sdw_node) {

		if (sdw_mstr_rt->mstr == NULL)
			break;

		if ((sdw_rt->stream_state != SDW_STATE_CONFIG_STREAM) &&
			(sdw_rt->stream_state != SDW_STATE_UNPREPARE_STREAM))
			goto enable_stream;

		/* Get bus structure for master */
		sdw_mstr_bs = master_to_bus(sdw_mstr_rt->mstr);
		sdw_mstr = sdw_mstr_bs->mstr;
		ret = sdw_pre_en_dis_unprep_op(sdw_mstr_rt, sdw_rt,
				SDW_STATE_PREPARE_STREAM);
		if (ret < 0) {
			dev_err(&sdw_mstr->dev, "Prepare Operation failed\n");
			return -EINVAL;
		}
	}

enable_stream:

	list_for_each_entry(sdw_mstr_rt, &sdw_rt->mstr_rt_list, mstr_sdw_node) {


		if (sdw_mstr_rt->mstr == NULL)
			break;

		if ((!enable) ||
			(sdw_rt->stream_state != SDW_STATE_PREPARE_STREAM))
			return 0;
		sdw_mstr_bs = master_to_bus(sdw_mstr_rt->mstr);
		sdw_mstr = sdw_mstr_bs->mstr;

		ret = sdw_pre_en_dis_unprep_op(sdw_mstr_rt, sdw_rt,
				SDW_STATE_ENABLE_STREAM);
		if (ret < 0) {
			dev_err(&sdw_mstr->dev, "Enable Operation failed\n");
			return -EINVAL;
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
	struct sdw_mstr_runtime *sdw_mstr_rt = NULL;
	struct sdw_bus *sdw_mstr_bs = NULL;
	struct sdw_master *sdw_mstr = NULL;
	int ret = 0;


	/* BW calulation for active master controller for given stream tag */
	list_for_each_entry(sdw_mstr_rt,
			&sdw_rt->mstr_rt_list, mstr_sdw_node) {


		if (sdw_mstr_rt->mstr == NULL)
			break;

		if (sdw_rt->stream_state != SDW_STATE_ENABLE_STREAM)
			goto unprepare_stream;

		/* Get bus structure for master */
		sdw_mstr_bs = master_to_bus(sdw_mstr_rt->mstr);
		sdw_mstr = sdw_mstr_bs->mstr;
		ret = sdw_pre_en_dis_unprep_op(sdw_mstr_rt, sdw_rt,
				SDW_STATE_DISABLE_STREAM);
		if (ret < 0) {
			dev_err(&sdw_mstr->dev, "Disable Operation failed\n");
			return -EINVAL;
		}
	}

unprepare_stream:
	list_for_each_entry(sdw_mstr_rt,
				&sdw_rt->mstr_rt_list, mstr_sdw_node) {
		if (sdw_mstr_rt->mstr == NULL)
			break;

		if ((!unprepare) ||
			(sdw_rt->stream_state != SDW_STATE_DISABLE_STREAM))
			return 0;

		sdw_mstr_bs = master_to_bus(sdw_mstr_rt->mstr);
		sdw_mstr = sdw_mstr_bs->mstr;
		ret = sdw_pre_en_dis_unprep_op(sdw_mstr_rt, sdw_rt,
				SDW_STATE_UNPREPARE_STREAM);
		if (ret < 0) {
			dev_err(&sdw_mstr->dev, "Unprepare Operation failed\n");
			return -EINVAL;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sdw_bus_calc_bw_dis);

/*
 * sdw_slv_dp0_en_dis - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function enable/disable Slave DP0 channels.
 */
int sdw_slv_dp0_en_dis(struct sdw_bus *mstr_bs,
	bool is_enable, u8 slv_number)
{
	struct sdw_msg wr_msg, rd_msg;
	int ret = 0;
	int banktouse;
	u8 wbuf[1] = {0};
	u8 rbuf[1] = {0};

	/* Get current bank in use from bus structure*/
	banktouse = mstr_bs->active_bank;
	banktouse = !banktouse;

	rd_msg.addr = wr_msg.addr = ((SDW_DPN_CHANNELEN +
				(SDW_BANK1_REGISTER_OFFSET * banktouse)) +
			(SDW_NUM_DATA_PORT_REGISTERS *
			 0x0));
	rd_msg.ssp_tag = 0x0;
	rd_msg.flag = SDW_MSG_FLAG_READ;
	rd_msg.len = 1;
	rd_msg.slave_addr = slv_number;
	rd_msg.buf = rbuf;
	rd_msg.addr_page1 = 0x0;
	rd_msg.addr_page2 = 0x0;

	wr_msg.ssp_tag = 0x0;
	wr_msg.flag = SDW_MSG_FLAG_WRITE;
	wr_msg.len = 1;
	wr_msg.slave_addr = slv_number;
	wr_msg.buf = wbuf;
	wr_msg.addr_page1 = 0x0;
	wr_msg.addr_page2 = 0x0;

	ret = sdw_slave_transfer(mstr_bs->mstr, &rd_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&mstr_bs->mstr->dev,
				"Register transfer failed\n");
		goto out;
	}

	if (is_enable)
		wbuf[0] = (rbuf[0] | 0x1);
	else
		wbuf[0] = (rbuf[0] & ~(0x1));

	ret = sdw_slave_transfer(mstr_bs->mstr, &wr_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&mstr_bs->mstr->dev,
				"Register transfer failed\n");
		goto out;
	}

	rbuf[0] = 0;
	/* This is just status read, can be removed later */
	ret = sdw_slave_transfer(mstr_bs->mstr, &rd_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&mstr_bs->mstr->dev,
				"Register transfer failed\n");
		goto out;
	}

out:
	return ret;

}


/*
 * sdw_mstr_dp0_act_dis - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function enable/disable Master DP0 channels.
 */
int sdw_mstr_dp0_act_dis(struct sdw_bus *mstr_bs, bool is_enable)
{
	struct sdw_mstr_driver *ops = mstr_bs->mstr->driver;
	struct sdw_activate_ch activate_ch;
	int banktouse, ret = 0;

	activate_ch.num = 0;
	activate_ch.ch_mask = 0x1;
	activate_ch.activate = is_enable; /* Enable/Disable */

	/* Get current bank in use from bus structure*/
	banktouse = mstr_bs->active_bank;
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

	return 0;
}

/*
 * sdw_slv_dp0_prep_unprep - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function prepare/unprepare Slave DP0.
 */
int sdw_slv_dp0_prep_unprep(struct sdw_bus *mstr_bs,
	u8 slv_number, bool prepare)
{
	struct sdw_msg wr_msg, rd_msg;
	int ret = 0;
	int banktouse;
	u8 wbuf[1] = {0};
	u8 rbuf[1] = {0};

	/* Get current bank in use from bus structure*/
	banktouse = mstr_bs->active_bank;
	banktouse = !banktouse;

	/* Read SDW_DPN_PREPARECTRL register */
	rd_msg.addr = wr_msg.addr = SDW_DPN_PREPARECTRL +
		(SDW_NUM_DATA_PORT_REGISTERS * 0x0);
	rd_msg.ssp_tag = 0x0;
	rd_msg.flag = SDW_MSG_FLAG_READ;
	rd_msg.len = 1;
	rd_msg.slave_addr = slv_number;
	rd_msg.buf = rbuf;
	rd_msg.addr_page1 = 0x0;
	rd_msg.addr_page2 = 0x0;

	wr_msg.ssp_tag = 0x0;
	wr_msg.flag = SDW_MSG_FLAG_WRITE;
	wr_msg.len = 1;
	wr_msg.slave_addr = slv_number;
	wr_msg.buf = wbuf;
	wr_msg.addr_page1 = 0x0;
	wr_msg.addr_page2 = 0x0;

	ret = sdw_slave_transfer(mstr_bs->mstr, &rd_msg, 1);
	if (ret != 1) {
		ret = -EINVAL;
		dev_err(&mstr_bs->mstr->dev,
				"Register transfer failed\n");
		goto out;
	}

	if (prepare)
		wbuf[0] = (rbuf[0] | 0x1);
	else
		wbuf[0] = (rbuf[0] & ~(0x1));

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
	 * Sleep for 100ms.
	 * TODO: check on check on prepare status for port_ready
	 */
	msleep(100);

out:
	return ret;

}

/*
 * sdw_mstr_dp0_prep_unprep - returns Success
 * -EINVAL - In case of error.
 *
 *
 * This function prepare/unprepare Master DP0.
 */
int sdw_mstr_dp0_prep_unprep(struct sdw_bus *mstr_bs,
	bool prep)
{
	struct sdw_mstr_driver *ops = mstr_bs->mstr->driver;
	struct sdw_prepare_ch prep_ch;
	int ret = 0;

	prep_ch.num = 0x0;
	prep_ch.ch_mask = 0x1;
	prep_ch.prepare = prep; /* Prepare/Unprepare */

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

	return 0;
}

static int sdw_bra_config_ops(struct sdw_bus *sdw_mstr_bs,
					struct sdw_bra_block *block,
					struct sdw_transport_params *t_params,
					struct sdw_port_params *p_params)
{
	struct sdw_mstr_driver *ops;
	int ret, banktouse;

	/* configure Master transport params */
	ret = sdw_cfg_mstr_params(sdw_mstr_bs, t_params, p_params);
	if (ret < 0) {
		dev_err(&sdw_mstr_bs->mstr->dev, "BRA: Master xport params config failed\n");
		return ret;
	}

	/* configure Slave transport params */
	ret = sdw_cfg_slv_params(sdw_mstr_bs, t_params,
			p_params, block->slave_addr);
	if (ret < 0) {
		dev_err(&sdw_mstr_bs->mstr->dev, "BRA: Slave xport params config failed\n");
		return ret;
	}

	/* Get master driver ops */
	ops = sdw_mstr_bs->mstr->driver;

	/* Configure SSP */
	banktouse = sdw_mstr_bs->active_bank;
	banktouse = !banktouse;

	if (ops->mstr_ops->set_ssp_interval) {
		ret = ops->mstr_ops->set_ssp_interval(sdw_mstr_bs->mstr,
				24, banktouse);
		if (ret < 0) {
			dev_err(&sdw_mstr_bs->mstr->dev, "BRA: SSP interval config failed\n");
			return ret;
		}
	}

	/* Configure Clock */
	if (ops->mstr_ops->set_clock_freq) {
		ret = ops->mstr_ops->set_clock_freq(sdw_mstr_bs->mstr,
				sdw_mstr_bs->clk_div, banktouse);
		if (ret < 0) {
			dev_err(&sdw_mstr_bs->mstr->dev, "BRA: Clock config failed\n");
			return ret;
		}
	}

	return 0;
}

static int sdw_bra_xport_config_enable(struct sdw_bus *sdw_mstr_bs,
					struct sdw_bra_block *block,
					struct sdw_transport_params *t_params,
					struct sdw_port_params *p_params)
{
	int ret;

	/* Prepare sequence */
	ret = sdw_bra_config_ops(sdw_mstr_bs, block, t_params, p_params);
	if (ret < 0) {
		dev_err(&sdw_mstr_bs->mstr->dev, "BRA: config operation failed\n");
		return ret;
	}

	/* Bank Switch */
	ret = sdw_cfg_frmshp_bnkswtch(sdw_mstr_bs, false);
	if (ret < 0) {
		dev_err(&sdw_mstr_bs->mstr->dev, "BRA: bank switch failed\n");
		return ret;
	}

	/*
	 * TODO: There may be some slave which doesn't support
	 * prepare for DP0. We have two options here.
	 * 1. Just call prepare and ignore error from those
	 * codec who doesn't support prepare for DP0.
	 * 2. Get slave capabilities and based on prepare DP0
	 * support, Program Slave prepare register.
	 * Currently going with approach 1, not checking return
	 * value.
	 * 3. Try to use existing prep_unprep API both for master
	 * and slave.
	 */
	sdw_slv_dp0_prep_unprep(sdw_mstr_bs, block->slave_addr, true);

	/* Prepare Master port */
	ret = sdw_mstr_dp0_prep_unprep(sdw_mstr_bs, true);
	if (ret < 0) {
		dev_err(&sdw_mstr_bs->mstr->dev, "BRA: Master prepare failed\n");
		return ret;
	}

	/* Enable sequence */
	ret = sdw_bra_config_ops(sdw_mstr_bs, block, t_params, p_params);
	if (ret < 0) {
		dev_err(&sdw_mstr_bs->mstr->dev, "BRA: config operation failed\n");
		return ret;
	}

	/* Enable DP0 channel (Slave) */
	ret = sdw_slv_dp0_en_dis(sdw_mstr_bs, true, block->slave_addr);
	if (ret < 0) {
		dev_err(&sdw_mstr_bs->mstr->dev, "BRA: Slave DP0 enable failed\n");
		return ret;
	}

	/* Enable DP0 channel (Master) */
	ret = sdw_mstr_dp0_act_dis(sdw_mstr_bs, true);
	if (ret < 0) {
		dev_err(&sdw_mstr_bs->mstr->dev, "BRA: Master DP0 enable failed\n");
		return ret;
	}

	/* Bank Switch */
	ret = sdw_cfg_frmshp_bnkswtch(sdw_mstr_bs, false);
	if (ret < 0) {
		dev_err(&sdw_mstr_bs->mstr->dev, "BRA: bank switch failed\n");
		return ret;
	}

	return 0;
}

static int sdw_bra_xport_config_disable(struct sdw_bus *sdw_mstr_bs,
	struct sdw_bra_block *block)
{
	int ret;

	/* Disable DP0 channel (Slave) */
	ret = sdw_slv_dp0_en_dis(sdw_mstr_bs, false, block->slave_addr);
	if (ret < 0) {
		dev_err(&sdw_mstr_bs->mstr->dev, "BRA: Slave DP0 disable failed\n");
		return ret;
	}

	/* Disable DP0 channel (Master) */
	ret = sdw_mstr_dp0_act_dis(sdw_mstr_bs, false);
	if (ret < 0) {
		dev_err(&sdw_mstr_bs->mstr->dev, "BRA: Master DP0 disable failed\n");
		return ret;
	}

	/* Bank Switch */
	ret = sdw_cfg_frmshp_bnkswtch(sdw_mstr_bs, false);
	if (ret < 0) {
		dev_err(&sdw_mstr_bs->mstr->dev, "BRA: bank switch failed\n");
		return ret;
	}

	/*
	 * TODO: There may be some slave which doesn't support
	 * de-prepare for DP0. We have two options here.
	 * 1. Just call prepare and ignore error from those
	 * codec who doesn't support de-prepare for DP0.
	 * 2. Get slave capabilities and based on prepare DP0
	 * support, Program Slave prepare register.
	 * Currently going with approach 1, not checking return
	 * value.
	 */
	sdw_slv_dp0_prep_unprep(sdw_mstr_bs, block->slave_addr, false);

	/* De-prepare Master port */
	ret = sdw_mstr_dp0_prep_unprep(sdw_mstr_bs, false);
	if (ret < 0) {
		dev_err(&sdw_mstr_bs->mstr->dev, "BRA: Master de-prepare failed\n");
		return ret;
	}

	return 0;
}

int sdw_bus_bra_xport_config(struct sdw_bus *sdw_mstr_bs,
	struct sdw_bra_block *block, bool enable)
{
	struct sdw_transport_params t_params;
	struct sdw_port_params p_params;
	int ret;

	/* TODO:
	 * compute transport parameters based on current clock and
	 * frameshape. need to check how algorithm should be designed
	 * for BRA for computing clock, frameshape, SSP and transport params.
	 */

	/* Transport Parameters */
	t_params.num = 0x0; /* DP 0 */
	t_params.blockpackingmode = 0x0;
	t_params.blockgroupcontrol_valid = false;
	t_params.blockgroupcontrol = 0x0;
	t_params.lanecontrol = 0;
	t_params.sample_interval = 10;

	t_params.hstart = 7;
	t_params.hstop = 9;
	t_params.offset1 = 0;
	t_params.offset2 = 0;

	/* Port Parameters */
	p_params.num = 0x0; /* DP 0 */

	/* Isochronous Mode */
	p_params.port_flow_mode = 0x0;

	/* Normal Mode */
	p_params.port_data_mode = 0x0;

	/* Word length */
	p_params.word_length = 3;

	/* Frameshape and clock params */
	sdw_mstr_bs->clk_div = 1;
	sdw_mstr_bs->col = 10;
	sdw_mstr_bs->row = 80;

#if IS_ENABLED(CONFIG_SND_SOC_INTEL_CNL_FPGA)
	sdw_mstr_bs->bandwidth = 9.6 * 1000 * 1000;
#else
	sdw_mstr_bs->bandwidth = 12 * 1000 * 1000;
#endif

	if (enable) {
		ret = sdw_bra_xport_config_enable(sdw_mstr_bs, block,
				&t_params, &p_params);
		if (ret < 0) {
			dev_err(&sdw_mstr_bs->mstr->dev, "BRA: Xport params config failed\n");
			return ret;
		}

	} else {
		ret = sdw_bra_xport_config_disable(sdw_mstr_bs, block);
		if (ret < 0) {
			dev_err(&sdw_mstr_bs->mstr->dev, "BRA: Xport params de-config failed\n");
			return ret;
		}
	}

	return 0;
}
