/*
 * shared buffer
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Contact Information: Li Fei <fei1.li@intel.com>
 *
 * BSD LICENSE
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Li Fei <fei1.li@intel.com>
 *
 */

#ifndef SHARED_BUF_H
#define SHARED_BUF_H

#include <linux/types.h>


#define SBUF_MAGIC	0x5aa57aa71aa13aa3
#define SBUF_MAX_SIZE	(1ULL << 22)
#define SBUF_HEAD_SIZE	64

/* sbuf flags */
#define OVERRUN_CNT_EN	(1ULL << 0) /* whether overrun counting is enabled */
#define OVERWRITE_EN	(1ULL << 1) /* whether overwrite is enabled */

/**
 * (sbuf) head + buf (store (ele_num - 1) elements at most)
 * buffer empty: tail == head
 * buffer full:  (tail + ele_size) % size == head
 *
 *             Base of memory for elements
 *                |
 *                |
 * ---------------------------------------------------------------------------------------
 * | shared_buf_t | raw data (ele_size)| raw date (ele_size) | ... | raw data (ele_size) |
 * ---------------------------------------------------------------------------------------
 * |
 * |
 * shared_buf_t *buf
 */

/* Make sure sizeof(shared_buf_t) == SBUF_HEAD_SIZE */
typedef struct shared_buf {
	uint64_t magic;
	uint32_t ele_num;	/* number of elements */
	uint32_t ele_size;	/* sizeof of elements */
	uint32_t head;		/* offset from base, to read */
	uint32_t tail;		/* offset from base, to write */
	uint64_t flags;
	uint32_t overrun_cnt;	/* count of overrun */
	uint32_t size;		/* ele_num * ele_size */
	uint32_t padding[6];
} ____cacheline_aligned shared_buf_t;

static inline void sbuf_clear_flags(shared_buf_t *sbuf, uint64_t flags)
{
	sbuf->flags &= ~flags;
}

static inline void sbuf_set_flags(shared_buf_t *sbuf, uint64_t flags)
{
	sbuf->flags = flags;
}

static inline void sbuf_add_flags(shared_buf_t *sbuf, uint64_t flags)
{
	sbuf->flags |= flags;
}

shared_buf_t *sbuf_allocate(uint32_t ele_num, uint32_t ele_size);
void sbuf_free(shared_buf_t *sbuf);
int sbuf_get(shared_buf_t *sbuf, uint8_t *data);
int sbuf_share_setup(uint32_t pcpu_id, uint32_t sbuf_id, shared_buf_t *sbuf);
shared_buf_t *sbuf_construct(uint32_t ele_num, uint32_t ele_size, uint64_t gpa);

#endif /* SHARED_BUF_H */
