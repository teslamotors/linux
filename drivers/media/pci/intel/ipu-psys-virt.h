/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */
#ifndef IPU_PSYS_VIRT_H
#define IPU_PSYS_VIRT_H

#include "virtio/intel-ipu4-virtio-be-request-queue.h"

struct ipu_psys_fh;

struct psys_fops_virt {
	int (*get_manifest)(struct ipu_psys_fh *fh,
			struct ipu4_virtio_req_info *req_info);
	int (*map_buf)(struct ipu_psys_fh *fh,
			struct ipu4_virtio_req_info *req_info);
	int (*unmap_buf)(struct ipu_psys_fh *fh,
			struct ipu4_virtio_req_info *req_info);
	int (*qcmd)(struct ipu_psys_fh *fh,
			struct ipu4_virtio_req_info *req_info);
	int (*dqevent)(struct ipu_psys_fh *fh,
			struct ipu4_virtio_req_info *req_info,
			unsigned int f_flags);
	int (*get_buf)(struct ipu_psys_fh *fh,
			struct ipu4_virtio_req_info *req_info);
	int (*poll)(struct ipu_psys_fh *fh,
			struct ipu4_virtio_req_info *req_info);
};

//Function define in ipu-psys.c
long ipu_psys_unmapbuf(int fd, struct ipu_psys_fh *fh);
//Function define in ipu4-psys.c
void ipu_psys_kcmd_free(struct ipu_psys_kcmd *kcmd);
struct ipu_psys_kcmd *__ipu_get_completed_kcmd(struct ipu_psys_fh *fh);

extern struct psys_fops_virt psys_vfops;

#endif