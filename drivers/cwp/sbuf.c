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

#define pr_fmt(fmt) "SBuf: " fmt

#include <linux/gfp.h>
#include <asm/pgtable.h>
#include "sbuf.h"

static inline bool sbuf_is_empty(shared_buf_t *sbuf)
{
	return (sbuf->head == sbuf->tail);
}

static inline uint32_t sbuf_next_ptr(uint32_t pos,
		uint32_t span, uint32_t scope)
{
	pos += span;
	pos = (pos >= scope) ? (pos - scope) : pos;
	return pos;
}

static inline uint32_t sbuf_calculate_allocate_size(uint32_t ele_num,
						uint32_t ele_size)
{
	uint64_t sbuf_allocate_size;

	sbuf_allocate_size = ele_num * ele_size;
	sbuf_allocate_size +=  SBUF_HEAD_SIZE;
	if (sbuf_allocate_size > SBUF_MAX_SIZE) {
		pr_err("num=0x%x, size=0x%x exceed 0x%llx!\n",
			ele_num, ele_size, SBUF_MAX_SIZE);
		return 0;
	}

	/* align to PAGE_SIZE */
	return (sbuf_allocate_size + PAGE_SIZE - 1) & PAGE_MASK;
}

shared_buf_t *sbuf_allocate(uint32_t ele_num, uint32_t ele_size)
{
	shared_buf_t *sbuf;
	struct page *page;
	uint32_t sbuf_allocate_size;

	if (!ele_num || !ele_size) {
		pr_err("invalid parameter %s!\n", __func__);
		return NULL;
	}

	sbuf_allocate_size = sbuf_calculate_allocate_size(ele_num, ele_size);
	if (!sbuf_allocate_size)
		return NULL;

	page = alloc_pages(GFP_KERNEL | __GFP_ZERO,
					get_order(sbuf_allocate_size));
	if (page == NULL) {
		pr_err("failed to alloc pages!\n");
		return NULL;
	}

	sbuf = phys_to_virt(page_to_phys(page));
	sbuf->ele_num = ele_num;
	sbuf->ele_size = ele_size;
	sbuf->size = ele_num * ele_size;
	sbuf->magic = SBUF_MAGIC;
	pr_info("ele_num=0x%x, ele_size=0x%x allocated!\n",
		ele_num, ele_size);
	return sbuf;
}
EXPORT_SYMBOL(sbuf_allocate);

void sbuf_free(shared_buf_t *sbuf)
{
	uint32_t sbuf_allocate_size;

	if ((sbuf == NULL) || sbuf->magic != SBUF_MAGIC) {
		pr_err("invalid parameter %s\n", __func__);
		return;
	}

	sbuf_allocate_size = sbuf_calculate_allocate_size(sbuf->ele_num,
						sbuf->ele_size);
	if (!sbuf_allocate_size)
		return;

	sbuf->magic = 0;
	__free_pages((struct page *)virt_to_page(sbuf),
			get_order(sbuf_allocate_size));
}
EXPORT_SYMBOL(sbuf_free);

int sbuf_get(shared_buf_t *sbuf, uint8_t *data)
{
	const void *from;

	if ((sbuf == NULL) || (data == NULL))
		return -EINVAL;

	if (sbuf_is_empty(sbuf)) {
		/* no data available */
		return 0;
	}

	from = (void *)sbuf + SBUF_HEAD_SIZE + sbuf->head;

	memcpy(data, from, sbuf->ele_size);

	sbuf->head = sbuf_next_ptr(sbuf->head, sbuf->ele_size, sbuf->size);

	return sbuf->ele_size;
}
EXPORT_SYMBOL(sbuf_get);

shared_buf_t *sbuf_construct(uint32_t ele_num, uint32_t ele_size,
				uint64_t paddr)
{
	shared_buf_t *sbuf;

	if (!ele_num || !ele_size || !paddr)
		return NULL;

	sbuf = (shared_buf_t *)phys_to_virt(paddr);
	BUG_ON(!virt_addr_valid(sbuf));

	if ((sbuf->magic == SBUF_MAGIC) &&
		(sbuf->ele_num == ele_num) &&
		(sbuf->ele_size == ele_size)) {
		pr_info("construct sbuf at 0x%llx.\n", paddr);
		/* return sbuf for dump */
		return sbuf;
	}

	return NULL;
}
EXPORT_SYMBOL(sbuf_construct);
