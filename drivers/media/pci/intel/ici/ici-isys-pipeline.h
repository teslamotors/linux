/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef ICI_ISYS_PIPELINE_H
#define ICI_ISYS_PIPELINE_H

#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/completion.h>

#include <media/ici.h>

#define ICI_ISYS_OUTPUT_PINS 11
#define ICI_NUM_CAPTURE_DONE 2
#define ICI_ISYS_MAX_PARALLEL_SOF 2

struct ici_isys_node;
struct ici_isys_subdev;
struct ici_isys_csi2_be;
struct ici_isys_csi2;
struct ici_isys_tpg;
struct ia_css_isys_resp_info;
struct ici_isys_pipeline;
struct ici_isys_stream;
struct node_pad;

struct ici_sequence_info {
	unsigned int sequence;
	u64 timestamp;
};

struct ici_output_pin_data {
	void (*pin_ready)(struct ici_isys_pipeline *ip,
			  struct ia_css_isys_resp_info *info);
	struct ici_isys_frame_buf_list *buf_list;
};

struct ici_isys_pipeline {
	struct node_pipeline pipe;
	struct ici_isys_pipeline_device *pipeline_dev;
	int source;		/* SSI stream source */
	int stream_handle;	/* stream handle for CSS API */
	unsigned int nr_output_pins;	/* How many firmware pins? */
	struct ici_isys_csi2_be *csi2_be;
	struct ici_isys_csi2 *csi2;
	struct ici_isys_subdev *asd_source;
	int asd_source_pad_id;
	unsigned int streaming;
	struct completion stream_open_completion;
	struct completion stream_close_completion;
	struct completion stream_start_completion;
	struct completion stream_stop_completion;
	struct completion capture_ack_completion;
	struct ici_isys *isys;

	void (*capture_done[ICI_NUM_CAPTURE_DONE])
	(struct ici_isys_pipeline *ip,
	 struct ia_css_isys_resp_info *resp);
	struct ici_output_pin_data
			output_pins[ICI_ISYS_OUTPUT_PINS];
	bool interlaced;
	int error;
	int cur_field;
	unsigned int short_packet_source;
	unsigned int short_packet_trace_index;
	unsigned int vc;
};

int ici_isys_pipeline_node_init(
	struct ici_isys *isys,
	struct ici_isys_node *node,
	const char* name,
	unsigned num_pads,
	struct node_pad *node_pads);

int node_pad_create_link(struct ici_isys_node *src,
		u16 src_pad, struct ici_isys_node *sink,
		u16 sink_pad, u32 flags );

void node_pads_cleanup(struct ici_isys_node *node);

typedef int (*ici_isys_pipeline_node_cb_fn)(void* cb_data,
	struct ici_isys_node* node,
	struct node_pipe* pipe);

int ici_isys_pipeline_for_each_node(
	ici_isys_pipeline_node_cb_fn cb_fn,
	void* cb_data,
	struct ici_isys_node* start_node,
	struct ici_isys_pipeline* ip_active,
	bool backwards);

#define ici_nodepipe_to_pipeline(__np) \
        container_of(__np, struct ici_isys_pipeline, pipe)

#endif /* ICI_ISYS_PIPELINE_H */
