/* SDW message transfer tracepoints
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
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM sdw

#if !defined(_TRACE_SDW_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SDW_H

#include <linux/mod_devicetable.h>
#include <linux/sdw_bus.h>
#include <linux/tracepoint.h>

/*
 * drivers/sdw/sdw.c
 */
extern int sdw_transfer_trace_reg(void);
extern void sdw_transfer_trace_unreg(void);
/*
 * __sdw_transfer() write request
 */
TRACE_EVENT_FN(sdw_write,
	       TP_PROTO(const struct sdw_master *mstr, const struct sdw_msg *msg,
			int num),
	       TP_ARGS(mstr, msg, num),
	       TP_STRUCT__entry(
		       __field(int,	master_nr)
		       __field(__u16,	msg_nr)
		       __field(__u8,	addr_page1)
		       __field(__u8,	addr_page2)
		       __field(__u16,	addr)
		       __field(__u16,	flag)
		       __field(__u16,	len)
		       __dynamic_array(__u8, buf, msg->len)),
	       TP_fast_assign(
		       __entry->master_nr = mstr->nr;
		       __entry->msg_nr = num;
		       __entry->addr = msg->addr;
		       __entry->flag = msg->flag;
		       __entry->len = msg->len;
		       __entry->addr_page1 = msg->addr_page1;
		       __entry->addr_page2 = msg->addr_page2;
		       memcpy(__get_dynamic_array(buf), msg->buf, msg->len);
			      ),
	       TP_printk("sdw-%d #%u a=%03x addr_page1=%04x addr_page2=%04x f=%04x l=%u [%*phD]",
			 __entry->master_nr,
			 __entry->msg_nr,
			 __entry->addr,
			 __entry->addr_page1,
			 __entry->addr_page2,
			 __entry->flag,
			 __entry->len,
			 __entry->len, __get_dynamic_array(buf)
			 ),
	       sdw_transfer_trace_reg,
	       sdw_transfer_trace_unreg);

/*
 * __sdw_transfer() read request
 */
TRACE_EVENT_FN(sdw_read,
	       TP_PROTO(const struct sdw_master *mstr, const struct sdw_msg *msg,
			int num),
	       TP_ARGS(mstr, msg, num),
	       TP_STRUCT__entry(
		       __field(int,	master_nr)
		       __field(__u16,	msg_nr)
		       __field(__u8,	addr_page1)
		       __field(__u8,	addr_page2)
		       __field(__u16,	addr)
		       __field(__u16,	flag)
		       __field(__u16,	len)
		       __dynamic_array(__u8, buf, msg->len)),
	       TP_fast_assign(
		       __entry->master_nr = mstr->nr;
		       __entry->msg_nr = num;
		       __entry->addr = msg->addr;
		       __entry->flag = msg->flag;
		       __entry->len = msg->len;
		       __entry->addr_page1 = msg->addr_page1;
		       __entry->addr_page2 = msg->addr_page2;
		       memcpy(__get_dynamic_array(buf), msg->buf, msg->len);
			      ),
	       TP_printk("sdw-%d #%u a=%03x addr_page1=%04x addr_page2=%04x f=%04x l=%u [%*phD]",
			 __entry->master_nr,
			 __entry->msg_nr,
			 __entry->addr,
			 __entry->addr_page1,
			 __entry->addr_page2,
			 __entry->flag,
			 __entry->len,
			 __entry->len, __get_dynamic_array(buf)
			 ),
	       sdw_transfer_trace_reg,
	sdw_transfer_trace_unreg);

/*
 * __sdw_transfer() read reply
 */
TRACE_EVENT_FN(sdw_reply,
	       TP_PROTO(const struct sdw_master *mstr, const struct sdw_msg *msg,
			int num),
	       TP_ARGS(mstr, msg, num),
	       TP_STRUCT__entry(
		       __field(int,	master_nr)
		       __field(__u16,	msg_nr)
		       __field(__u16,	addr)
		       __field(__u16,	flag)
		       __field(__u16,	len)
		       __dynamic_array(__u8, buf, msg->len)),
	       TP_fast_assign(
		       __entry->master_nr = mstr->nr;
		       __entry->msg_nr = num;
		       __entry->addr = msg->addr;
		       __entry->flag = msg->flag;
		       __entry->len = msg->len;
		       memcpy(__get_dynamic_array(buf), msg->buf, msg->len);
			      ),
	       TP_printk("sdw-%d #%u a=%03x f=%04x l=%u [%*phD]",
			 __entry->master_nr,
			 __entry->msg_nr,
			 __entry->addr,
			 __entry->flag,
			 __entry->len,
			 __entry->len, __get_dynamic_array(buf)
			 ),
	       sdw_transfer_trace_reg,
	       sdw_transfer_trace_unreg);

/*
 * __sdw_transfer() result
 */
TRACE_EVENT_FN(sdw_result,
	       TP_PROTO(const struct sdw_master *mstr, int num, int ret),
	       TP_ARGS(mstr, num, ret),
	       TP_STRUCT__entry(
		       __field(int,	master_nr)
		       __field(__u16,	nr_msgs)
		       __field(__s16,	ret)
				),
	       TP_fast_assign(
		       __entry->master_nr = mstr->nr;
		       __entry->nr_msgs = num;
		       __entry->ret = ret;
			      ),
	       TP_printk("sdw-%d n=%u ret=%d",
			 __entry->master_nr,
			 __entry->nr_msgs,
			 __entry->ret
			 ),
	       sdw_transfer_trace_reg,
	       sdw_transfer_trace_unreg);

/*
 * sdw_stream_config() configuration
 */
TRACE_EVENT_FN(sdw_config_stream,
	       TP_PROTO(const struct sdw_master *mstr, const struct sdw_slave *slv, const struct sdw_stream_config *str_cfg, int stream_tag),
	       TP_ARGS(mstr, slv, str_cfg, stream_tag),
	       TP_STRUCT__entry(
		       __field(unsigned int,	frame_rate)
		       __field(unsigned int,	ch_cnt)
		       __field(unsigned int,	bps)
		       __field(unsigned int,	direction)
		       __field(unsigned int,	stream_tag)
		       __array(char,		name,	SOUNDWIRE_NAME_SIZE)
				),
	       TP_fast_assign(
		       __entry->frame_rate = str_cfg->frame_rate;
		       __entry->ch_cnt = str_cfg->channel_count;
		       __entry->bps = str_cfg->bps;
		       __entry->direction = str_cfg->direction;
		       __entry->stream_tag = stream_tag;
		       slv ? strncpy(entry->name, dev_name(&slv->dev), SOUNDWIRE_NAME_SIZE) : strncpy(entry->name, dev_name(&mstr->dev), SOUNDWIRE_NAME_SIZE);
			      ),
	       TP_printk("Stream_config dev = %s stream_tag = %d, frame_rate = %d, ch_count = %d bps = %d dir = %d",
			__entry->name,
			__entry->stream_tag,
			 __entry->frame_rate,
			 __entry->ch_cnt,
			 __entry->bps,
			 __entry->direction
			 ),
	       sdw_transfer_trace_reg,
	       sdw_transfer_trace_unreg);

/*
 * sdw_port_config() configuration
 */
TRACE_EVENT_FN(sdw_config_port,
	       TP_PROTO(const struct sdw_master *mstr, const struct sdw_slave *slv, const struct sdw_port_cfg *port_cfg, int stream_tag),
	       TP_ARGS(mstr, slv, port_cfg, stream_tag),
	       TP_STRUCT__entry(
		       __field(unsigned int,	port_num)
		       __field(unsigned int,	ch_mask)
		       __field(unsigned int,	stream_tag)
		       __array(char,		name,	SOUNDWIRE_NAME_SIZE)
				),
	       TP_fast_assign(
		       __entry->port_num = port_cfg->port_num;
		       __entry->ch_mask = port_cfg->ch_mask;
		       __entry->stream_tag = stream_tag;
		       slv ? strncpy(entry->name, dev_name(&slv->dev), SOUNDWIRE_NAME_SIZE) : strncpy(entry->name, dev_name(&mstr->dev), SOUNDWIRE_NAME_SIZE);
			      ),
	       TP_printk("Port_config dev = %s stream_tag = %d, port = %d, ch_mask = %d",
			__entry->name,
			__entry->stream_tag,
			 __entry->port_num,
			 __entry->ch_mask
			 ),
	       sdw_transfer_trace_reg,
	       sdw_transfer_trace_unreg);

#endif /* _TRACE_SDW_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
