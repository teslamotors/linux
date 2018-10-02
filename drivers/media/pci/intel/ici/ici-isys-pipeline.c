// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include "./ici/ici-isys.h"

#ifdef ICI_ENABLED

#include "./ici/ici-isys-pipeline.h"

int ici_isys_pipeline_node_init(
	struct ici_isys *isys,
	struct ici_isys_node *node,
	const char* name,
	unsigned num_pads,
	struct node_pad *node_pads)
{
	unsigned int pad_id;

	mutex_lock(&isys->pipeline_dev.mutex);
	node->parent = &isys->pipeline_dev;
	snprintf(node->name, sizeof(node->name), "%s", name);
	if (num_pads > ICI_MAX_PADS) {
		dev_warn(&isys->adev->dev,
			"Too many external pads %d\n", num_pads);
		num_pads = ICI_MAX_PADS;
	}
	node->nr_pads = num_pads;
	node->node_pad = node_pads;
	node->nr_pipes = 0;
	node->node_id = isys->pipeline_dev.next_node_id++;

	INIT_LIST_HEAD(&node->node_entry);
	INIT_LIST_HEAD(&node->iterate_node);
	INIT_LIST_HEAD(&node->node_pipes);

	for (pad_id = 0; pad_id < num_pads; pad_id++) {
		node->node_pad[pad_id].node = node;
		node->node_pad[pad_id].pad_id = pad_id;
	}

	list_add_tail(&node->node_entry,
		&node->parent->nodes);
	dev_info(&isys->adev->dev,
		"Setup node \"%s\" with %d pads\n",
		node->name,
		node->nr_pads);
	mutex_unlock(&isys->pipeline_dev.mutex);
	return 0;
}

void node_pads_cleanup(struct ici_isys_node *node)
{
	struct node_pipe *tmp, *q, *np;
	list_for_each_entry_safe(np, q, &node->node_pipes, list_entry) {
		tmp = np;
		list_del(&np->list_entry);
		kfree(tmp);
	}
}

static struct node_pipe* node_pad_add_link(struct ici_isys_node *node)
{
	struct node_pipe *np;
	np = kzalloc(sizeof(*np), GFP_KERNEL);
	if (!np)
		return NULL;

	list_add_tail(&np->list_entry, &node->node_pipes);
	node->nr_pipes++;
	return np;
}

int node_pad_create_link(struct ici_isys_node *src,
		u16 src_pad, struct ici_isys_node *sink,
		u16 sink_pad, u32 flags )
{
	int rval = 0;
	struct node_pipe *np;
	struct node_pipe *rnp;
	if(!src || !sink || !src->parent)
		return -EINVAL;

	mutex_lock(&src->parent->mutex);
	np = node_pad_add_link(src);
	if(!np) {
		rval = -ENOMEM;
		goto cleanup_mutex;
	}

	np->src_pad = &src->node_pad[src_pad];
	np->sink_pad = &sink->node_pad[sink_pad];
	np->flags = flags;
	np->rev_pipe = NULL;

	rnp = node_pad_add_link(sink);
	if(!rnp) {
		rval = -ENOMEM;
		goto cleanup_mutex;
	}

	rnp->src_pad = &src->node_pad[src_pad];
	rnp->sink_pad = &sink->node_pad[sink_pad];
	rnp->flags = flags | ICI_LINK_FLAG_BACKLINK;
	rnp->rev_pipe = np;
	np->rev_pipe = rnp;

cleanup_mutex:
	mutex_unlock(&src->parent->mutex);
	return rval;
}

static int __ici_isys_pipeline_for_each_node(
	ici_isys_pipeline_node_cb_fn cb_fn,
	void* cb_data,
	struct ici_isys_node* start_node,
	struct ici_isys_pipeline *ip_active,
	bool backwards)
{
	struct node_pipe *pipe;
	struct ici_isys_node* node;
	struct ici_isys_node* next_node = NULL;
	int rval;
	LIST_HEAD(node_list);

	if (!cb_fn || !start_node || !start_node->parent)
		return -EINVAL;

	rval = cb_fn(cb_data, start_node, NULL);
	if (rval)
		return rval;
	list_add_tail(&start_node->iterate_node, &node_list);
	while (!list_empty(&node_list)) {
		node = list_entry(node_list.next,
			struct ici_isys_node,
			iterate_node);
		list_del(&node->iterate_node);
		list_for_each_entry(pipe, &node->node_pipes,
			list_entry) {
			if (backwards && !(pipe->flags & ICI_LINK_FLAG_BACKLINK))
				continue;
			else if (!backwards && (pipe->flags & ICI_LINK_FLAG_BACKLINK))
				continue;
			if (ip_active && !(pipe->flags & ICI_LINK_FLAG_ENABLED))
				continue;
			next_node = (backwards ? pipe->src_pad->node :
				pipe->sink_pad->node);
			rval = cb_fn(cb_data, next_node, pipe);
			if (rval)
				return rval;
			list_add_tail(&next_node->iterate_node,
				&node_list);
		}
	}
	return 0;
}

int ici_isys_pipeline_for_each_node(
	ici_isys_pipeline_node_cb_fn cb_fn,
	void* cb_data,
	struct ici_isys_node* start_node,
	struct ici_isys_pipeline *ip_active,
	bool backwards)
{
	int rval = 0;
	mutex_lock(&start_node->parent->mutex);
	rval = __ici_isys_pipeline_for_each_node(cb_fn,
		cb_data, start_node, ip_active, backwards);
	mutex_unlock(&start_node->parent->mutex);
	return rval;
}

#endif /* ICI_ENABLED */
