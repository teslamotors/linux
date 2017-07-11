/*
 * Copyright (c) 2016 Google Inc. All rights reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __LINUX_TRUSTY_SMWALL_H
#define __LINUX_TRUSTY_SMWALL_H

/**
 * DOC: Introduction
 *
 * SM Wall buffer is formatted by secure side to contain the location of
 * objects it exports:
 *
 * In general it starts with sm_wall_toc header struct followed
 * by array of sm_wall_toc_item objects describing location of
 * individual objects within SM Wall buffer.
 */

/* current version of TOC structure */
#define SM_WALL_TOC_VER   1

/**
 * struct sm_wall_toc_item - describes individual table of content item
 * @id:           item id
 * @offset:       item offset relative to appropriate base. For global items
 * it is relative to SM wall buffer base address. For per cpu item, this is an
 * offset within each individual per cpu region.
 * @size:         item size
 * @reserved:     reserved: must be set to zero
 */
struct sm_wall_toc_item {
	u32 id;
	u32 offset;
	u32 size;
	u32 reserved;
};

/**
 * struct sm_wall_toc - describes sm_wall table of content structure
 * @version:             current toc structure version
 * @cpu_num:             number of cpus supported
 * @per_cpu_toc_offset:  offset of the start of per_cpu item table relative to
 *                       SM wall buffer base address.
 * @per_cpu_num_items:   number of per cpu toc items located at position
 *                       specified by @per_cpu_toc_offset.
 * @per_cpu_base_offset: offset of the start of a sequence of per cpu data
 *                       regions (@cpu_num total) relative to SM wall buffer
 *                       base address.
 * @per_cpu_region_size: size of each per cpu data region.
 * @global_toc_offset:   offset of the start of global item table relative to
 *                       SM wall buffer base address.
 * @global_num_items:    number of items in global item table
 */
struct sm_wall_toc {
	u32 version;
	u32 cpu_num;
	u32 per_cpu_toc_offset;
	u32 per_cpu_num_items;
	u32 per_cpu_base_offset;
	u32 per_cpu_region_size;
	u32 global_toc_offset;
	u32 global_num_items;
};

struct trusty_wall_dev_state {
	struct device *dev;
	struct device *trusty_dev;
	void   *va;
	size_t sz;
};

#endif /* __LINUX_TRUSTY_SMWALL_H */
