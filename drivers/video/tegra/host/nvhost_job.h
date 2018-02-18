/*
 * drivers/video/tegra/host/nvhost_job.h
 *
 * Tegra Graphics Host Interrupt Management
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NVHOST_JOB_H
#define __NVHOST_JOB_H

#include <linux/nvhost_ioctl.h>
#include <linux/kref.h>
#include <linux/dma-buf.h>

struct nvhost_channel;
struct nvhost_waitchk;
struct nvhost_syncpt;
struct sg_table;

struct nvhost_job_gather {
	u32 words;
	struct sg_table *mem_sgt;
	dma_addr_t mem_base;
	ulong mem_id;
	u32 class_id;
	int offset;
	struct dma_buf *buf;
	int pre_fence;
};

struct nvhost_job_syncpt {
	u32 id;
	u32 incrs;
	u32 fence;
};

struct nvhost_pinid {
	u32 id;
	u32 index;
};

struct nvhost_job_unpin {
	struct sg_table *sgt;
	struct dma_buf *buf;
	struct dma_buf_attachment *attach;
};

/*
 * Each submit is tracked as a nvhost_job.
 */
struct nvhost_job {
	/* Total size of job */
	size_t size;

	/* When refcount goes to zero, job can be freed */
	struct kref ref;

	/* List entry */
	struct list_head list;

	/* Channel where job is submitted to */
	struct nvhost_channel *ch;

	/* Hardware context valid for this client */
	int clientid;

	/* Gathers and their memory */
	struct nvhost_job_gather *gathers;
	int num_gathers;

	/* Wait checks to be processed at submit time */
	struct nvhost_waitchk *waitchk;
	int num_waitchk;

	/* Array of handles to be pinned & unpinned */
	struct nvhost_reloc *relocarray;
	struct nvhost_reloc_shift *relocshiftarray;
	struct nvhost_reloc_type *reloctypearray;
	int num_relocs;
	struct nvhost_job_unpin *unpins;
	int num_unpins;

	struct nvhost_pinid *pin_ids;
	dma_addr_t *addr_phys;
	dma_addr_t *gather_addr_phys;
	dma_addr_t *reloc_addr_phys;

	/* Sync point id, number of increments and end related to the submit */
	u32 client_managed_syncpt;
	struct nvhost_job_syncpt *sp;
	int num_syncpts;

	/* Maximum time to wait for this job */
	int timeout;

	/* Do debug dump after timeout */
	bool timeout_debug_dump;

	/* Index and number of slots used in the push buffer */
	int first_get;
	int num_slots;

	/* Set to true to force an added wait-for-idle before the job */
	int serialize;

	/* error notifiers used channel submit timeout */
	struct dma_buf *error_notifier_ref;
	u64 error_notifier_offset;
};

/*
 * Add a gather to a job.
 */
void nvhost_job_add_gather(struct nvhost_job *job,
		u32 mem_id, u32 words, u32 offset, u32 class_id, int pre_fence);

/*
 * Increment reference going to nvhost_job.
 */
void nvhost_job_get(struct nvhost_job *job);

/*
 * Pin memory related to job. This handles relocation of addresses to the
 * host1x address space. Handles both the gather memory and any other memory
 * referred to from the gather buffers.
 *
 * Handles also patching out host waits that would wait for an expired sync
 * point value.
 */
int nvhost_job_pin(struct nvhost_job *job, struct nvhost_syncpt *sp);

/*
 * Unpin memory related to job.
 */
void nvhost_job_unpin(struct nvhost_job *job);

/*
 * Dump contents of job to debug output.
 */
void nvhost_job_dump(struct device *dev, struct nvhost_job *job);

/*
 * Set error notifier for the job owner to informa userspace about
 * an error.
 */
void nvhost_job_set_notifier(struct nvhost_job *job, u32 error);

#endif
