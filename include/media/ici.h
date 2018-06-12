/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef __ICI_PRIV_H__
#define __ICI_PRIV_H__

#include <linux/list.h>
#include <uapi/linux/ici.h>

struct ici_isys_node;
struct ici_isys_pipeline_device;

struct node_pad {
	struct ici_isys_node *node;
	unsigned pad_id;
	unsigned flags;
};

struct node_pipe {
	struct node_pad *src_pad;
	struct node_pad *sink_pad;
	struct list_head list_entry;
	unsigned flags;
	struct node_pipe *rev_pipe;
};

struct node_pipeline {
};

struct ici_isys_node {
	struct list_head node_entry;
	struct list_head iterate_node;
	struct ici_isys_pipeline_device *parent;
	unsigned node_id;
	char name[ICI_MAX_NODE_NAME];
	void *sd;
	bool external;
	struct node_pad *node_pad;
	unsigned nr_pads;
	struct list_head node_pipes;
	unsigned nr_pipes;
	unsigned stream_count;
	unsigned use_count;
	struct node_pipeline *pipe;	/* Pipeline this node belongs to. */
	int (*node_pipeline_validate)(struct node_pipeline *inp,
		struct ici_isys_node *node);
	int (*node_set_power)(struct ici_isys_node* node,
		int power);
	int (*node_set_streaming)(struct ici_isys_node* node,
		void* ip,
		int streaming);
	int (*node_get_pad_supported_format)(
		struct ici_isys_node* node,
		struct ici_pad_supported_format_desc* psfd);
	int (*node_set_pad_ffmt)(struct ici_isys_node* node,
		struct ici_pad_framefmt* pff);
	int (*node_get_pad_ffmt)(struct ici_isys_node* node,
		struct ici_pad_framefmt* pff);
	int (*node_set_pad_sel)(struct ici_isys_node* node,
		struct ici_pad_selection* ps);
	int (*node_get_pad_sel)(struct ici_isys_node* node,
		struct ici_pad_selection* ps);
};

enum ici_ext_sd_param_id {
	ICI_EXT_SD_PARAM_ID_LINK_FREQ = 1,
	ICI_EXT_SD_PARAM_ID_PIXEL_RATE,
	ICI_EXT_SD_PARAM_ID_HFLIP,
	ICI_EXT_SD_PARAM_ID_VFLIP,
	ICI_EXT_SD_PARAM_ID_FRAME_LENGTH_LINES,
	ICI_EXT_SD_PARAM_ID_LINE_LENGTH_PIXELS,
	ICI_EXT_SD_PARAM_ID_HBLANK,
	ICI_EXT_SD_PARAM_ID_VBLANK,
	ICI_EXT_SD_PARAM_ID_SENSOR_MODE,
	ICI_EXT_SD_PARAM_ID_ANALOGUE_GAIN,
	ICI_EXT_SD_PARAM_ID_EXPOSURE,
	ICI_EXT_SD_PARAM_ID_TEST_PATTERN,
	ICI_EXT_SD_PARAM_ID_GAIN,
	ICI_EXT_SD_PARAM_ID_THERMAL_DATA,
	ICI_EXT_SD_PARAM_ID_MIPI_LANES,
	ICI_EXT_SD_PARAM_ID_WDR_MODE,
};

enum ici_ext_sd_param_type {
	ICI_EXT_SD_PARAM_TYPE_INT32 = 0,
	ICI_EXT_SD_PARAM_TYPE_INT64,
	ICI_EXT_SD_PARAM_TYPE_STR,
};

struct ici_ext_sd_param_custom_data {
	unsigned size;
	char* data;
};

struct ici_ext_subdev;

struct ici_ext_sd_param {
	struct ici_ext_subdev* sd;
	int id;
	int type;
	union {
		s32 val;
		s64 s64val;
		struct ici_ext_sd_param_custom_data custom;
	};
};

struct ici_ext_subdev;

struct ici_ext_subdev_register {
	void* ipu_data;
	struct ici_ext_subdev* sd;
	int (*setup_node)(void* ipu_data,
		struct ici_ext_subdev* sd,
		const char* name);
	int (*create_link)(
		struct ici_isys_node* src,
		u16 src_pad,
		struct ici_isys_node* sink,
		u16 sink_pad,
		u32 flags);
};

struct ici_ext_subdev {
	struct ici_isys_node node;
	void* client;
	unsigned num_pads;
	struct node_pad pads[ICI_MAX_PADS];
	u16 src_pad;
	int (*do_register)(
		struct ici_ext_subdev_register* reg);
	void (*do_unregister)(struct ici_ext_subdev* sd);
	int (*set_param)(struct ici_ext_sd_param* param);
	int (*get_param)(struct ici_ext_sd_param* param);
	int (*get_menu_item)(struct ici_ext_sd_param* param,
		unsigned idx);
};

#endif // __ICI_PRIV_H__
