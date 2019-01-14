/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef __IPU4_VIRTIO_COMMON_PSYS_H__
#define __IPU4_VIRTIO_COMMON_PSYS_H__

struct ipu_psys_manifest_wrap {
	u64 psys_manifest;
	u64 manifest_data;
};

struct ipu_psys_usrptr_map {
	bool vma_is_io;
	u64 page_table_ref;
	size_t npages;
	u64 len;
	void *userptr;
};

struct ipu_psys_buffer_wrap {
	struct hlist_node node;
	u64 psys_buf;
	struct ipu_psys_usrptr_map map;
};

struct ipu_psys_command_wrap {
	u64 psys_command;
	u64 psys_manifest;
	u64 psys_buffer;
};

#endif
