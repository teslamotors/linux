/*
 * Copyright (c) 2013--2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _UAPI_INTEL_IPU4_PSYS_H
#define _UAPI_INTEL_IPU4_PSYS_H

#include <linux/types.h>

struct intel_ipu4_psys_capability {
	uint32_t version;
	uint8_t driver[20];
	uint32_t pg_count;
	uint8_t dev_model[32];
	uint32_t reserved[17];
} __attribute__ ((packed));

struct intel_ipu4_psys_event {
	uint32_t type;		/* INTEL_IPU4_PSYS_EVENT_TYPE_ */
	uint32_t id;
	uint64_t issue_id;
	uint32_t buffer_idx;
	uint32_t error;
	int32_t reserved[2];
} __attribute__ ((packed));

#define INTEL_IPU4_PSYS_EVENT_TYPE_CMD_COMPLETE	1
#define INTEL_IPU4_PSYS_EVENT_TYPE_BUFFER_COMPLETE	2

/**
 * @fd:		DMA-BUF handle
 * @data_offset:offset to valid data
 * @bytes_used:	amount of valid data including offset
 */
struct intel_ipu4_psys_dma_buf {
	int fd;
	uint32_t data_offset;
	uint32_t bytes_used;
	uint32_t flags;
	uint32_t reserved;
} __attribute__ ((packed));

/**
 * struct intel_ipu4_psys_buffer - for input/output terminals
 * @len:	buffer size
 * @userptr:	user pointer (NULL if not mapped to user space)
 * @fd:		DMA-BUF handle (filled by driver)
 * @flags:	flags
 */
struct intel_ipu4_psys_buffer {
	uint64_t len;
	void __user *userptr;
	int fd;
	uint32_t flags;
	uint32_t reserved[2];
} __attribute__ ((packed));

#define INTEL_IPU4_BUFFER_FLAG_INPUT	(1 << 0)
#define INTEL_IPU4_BUFFER_FLAG_OUTPUT	(1 << 1)
#define INTEL_IPU4_BUFFER_FLAG_MAPPED	(1 << 2)
#define INTEL_IPU4_BUFFER_FLAG_NO_FLUSH	(1 << 3)

#define	INTEL_IPU4_PSYS_CMD_PRIORITY_HIGH	0
#define	INTEL_IPU4_PSYS_CMD_PRIORITY_MED	1
#define	INTEL_IPU4_PSYS_CMD_PRIORITY_LOW	2
#define	INTEL_IPU4_PSYS_CMD_PRIORITY_NUM	3

/**
 * struct intel_ipu4_psys_command - processing command
 * @issue_id:		unique id for the command set by user
 * @id:			id of the command
 * @priority:		priority of the command
 * @pg_manifest:	userspace pointer to program group manifest
 * @buffers:		userspace pointers to array of psys dma buf structs
 * @pg:			process group DMA-BUF handle
 * @pg_manifest_size:	size of program group manifest
 * @bufcount:		number of buffers in buffers array
 * @min_psys_freq:	minimum psys frequency in MHz used for this cmd
 *
 * Specifies a processing command with input and output buffers.
 */
struct intel_ipu4_psys_command {
	uint64_t issue_id;
	uint32_t id;
	uint32_t priority;
	void __user *pg_manifest;
	struct intel_ipu4_psys_dma_buf __user *buffers;
	int pg;
	uint32_t pg_manifest_size;
	uint32_t bufcount;
	uint32_t min_psys_freq;
	uint32_t reserved[2];
} __attribute__ ((packed));

struct intel_ipu4_psys_manifest {
	uint32_t index;
	uint32_t size;
	void __user *manifest;
	uint32_t reserved[5];
} __attribute__ ((packed));

#define INTEL_IPU4_IOC_QUERYCAP _IOR('A', 1, struct intel_ipu4_psys_capability)
#define INTEL_IPU4_IOC_MAPBUF _IOWR('A', 2, int)
#define INTEL_IPU4_IOC_UNMAPBUF _IOWR('A', 3, int)
#define INTEL_IPU4_IOC_GETBUF _IOWR('A', 4, struct intel_ipu4_psys_buffer)
#define INTEL_IPU4_IOC_PUTBUF _IOWR('A', 5, struct intel_ipu4_psys_buffer)
#define INTEL_IPU4_IOC_QCMD _IOWR('A', 6, struct intel_ipu4_psys_command)
#define INTEL_IPU4_IOC_DQEVENT _IOWR('A', 7, struct intel_ipu4_psys_event)
#define INTEL_IPU4_IOC_CMD_CANCEL _IOWR('A', 8, struct intel_ipu4_psys_command)
#define INTEL_IPU4_IOC_GET_MANIFEST _IOWR('A', 9, struct intel_ipu4_psys_manifest)

#endif /* _UAPI_INTEL_IPU4_PSYS_H */
