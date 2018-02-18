/*
 * include/linux/nvmap.h
 *
 * structure declarations for nvmem and nvmap user-space ioctls
 *
 * Copyright (c) 2009-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/ioctl.h>
#include <linux/types.h>
#if defined(__KERNEL__)
#include <linux/rbtree.h>
#include <linux/file.h>
#include <linux/dma-buf.h>
#include <linux/device.h>
#endif

#ifndef _LINUX_NVMAP_H
#define _LINUX_NVMAP_H

#define NVMAP_HEAP_IOVMM   (1ul<<30)

/* common carveout heaps */
#define NVMAP_HEAP_CARVEOUT_IRAM    (1ul<<29)
#define NVMAP_HEAP_CARVEOUT_VPR     (1ul<<28)
#define NVMAP_HEAP_CARVEOUT_TSEC    (1ul<<27)
#define NVMAP_HEAP_CARVEOUT_VIDMEM  (1ul<<26)
#define NVMAP_HEAP_CARVEOUT_IVM     (1ul<<1)
#define NVMAP_HEAP_CARVEOUT_GENERIC (1ul<<0)

#define NVMAP_HEAP_CARVEOUT_MASK    (NVMAP_HEAP_IOVMM - 1)

/* allocation flags */
#define NVMAP_HANDLE_UNCACHEABLE     (0x0ul << 0)
#define NVMAP_HANDLE_WRITE_COMBINE   (0x1ul << 0)
#define NVMAP_HANDLE_INNER_CACHEABLE (0x2ul << 0)
#define NVMAP_HANDLE_CACHEABLE       (0x3ul << 0)
#define NVMAP_HANDLE_CACHE_FLAG      (0x3ul << 0)

#define NVMAP_HANDLE_SECURE          (0x1ul << 2)
#define NVMAP_HANDLE_KIND_SPECIFIED  (0x1ul << 3)
#define NVMAP_HANDLE_COMPR_SPECIFIED (0x1ul << 4)
#define NVMAP_HANDLE_ZEROED_PAGES    (0x1ul << 5)
#define NVMAP_HANDLE_PHYS_CONTIG     (0x1ul << 6)
#define NVMAP_HANDLE_CACHE_SYNC      (0x1ul << 7)
#define NVMAP_HANDLE_CACHE_SYNC_AT_RESERVE      (0x1ul << 8)

#if defined(__KERNEL__)

#ifdef CONFIG_NVMAP_PAGE_POOLS
ulong nvmap_page_pool_get_unused_pages(void);
#else
static inline ulong nvmap_page_pool_get_unused_pages(void)
{
	return 0;
}
#endif

ulong nvmap_iovmm_get_used_pages(void);
int nvmap_register_vidmem_carveout(struct device *dma_dev,
		phys_addr_t base, size_t size);

struct nvmap_platform_carveout {
	const char *name;
	unsigned int usage_mask;
	phys_addr_t base;
	size_t size;
	struct device *cma_dev;
	bool resize;
	struct device *dma_dev;
	struct device dev;
	struct dma_declare_info *dma_info;
	bool is_ivm;
	int peer;
	int vmid;
	int can_alloc;
	bool enable_static_dma_map;
	bool disable_dynamic_dma_map;
	bool no_cpu_access; /* carveout can't be accessed from cpu at all */
	bool init_done;	/* FIXME: remove once all caveouts use reserved-memory */
};

struct nvmap_platform_data {
	const struct nvmap_platform_carveout *carveouts;
	unsigned int nr_carveouts;
};

#endif /* __KERNEL__ */

/*
 * DOC: NvMap Userspace API
 *
 * create a client by opening /dev/nvmap
 * most operations handled via following ioctls
 *
 */
enum {
	NVMAP_HANDLE_PARAM_SIZE = 1,
	NVMAP_HANDLE_PARAM_ALIGNMENT,
	NVMAP_HANDLE_PARAM_BASE,
	NVMAP_HANDLE_PARAM_HEAP,
	NVMAP_HANDLE_PARAM_KIND,
	NVMAP_HANDLE_PARAM_COMPR, /* ignored, to be removed */
};

enum {
	NVMAP_CACHE_OP_WB = 0,
	NVMAP_CACHE_OP_INV,
	NVMAP_CACHE_OP_WB_INV,
};

enum {
	NVMAP_PAGES_UNRESERVE = 0,
	NVMAP_PAGES_RESERVE,
	NVMAP_INSERT_PAGES_ON_UNRESERVE,
	NVMAP_PAGES_PROT_AND_CLEAN,
};

struct nvmap_create_handle {
	union {
		__u32 id;	/* FromId */
		__u32 size;	/* CreateHandle */
		__s32 fd;	/* DmaBufFd or FromFd */
	};
	__u32 handle;		/* returns nvmap handle */
};

struct nvmap_create_handle_from_va {
	__u64 va;		/* FromVA*/
	__u32 size;		/* non-zero for partial memory VMA. zero for end of VMA */
	__u32 flags;		/* wb/wc/uc/iwb, tag etc. */
	__u32 handle;		/* returns nvmap handle */
};

struct nvmap_gup_test {
	__u64 va;		/* FromVA*/
	__u32 handle;		/* returns nvmap handle */
	__u32 result;		/* result=1 for pass, result=-err for failure */
};

struct nvmap_alloc_handle {
	__u32 handle;		/* nvmap handle */
	__u32 heap_mask;	/* heaps to allocate from */
	__u32 flags;		/* wb/wc/uc/iwb etc. */
	__u32 align;		/* min alignment necessary */
};

struct nvmap_alloc_ivm_handle {
	__u32 handle;		/* nvmap handle */
	__u32 heap_mask;	/* heaps to allocate from */
	__u32 flags;		/* wb/wc/uc/iwb etc. */
	__u32 align;		/* min alignment necessary */
	__u32 peer;		/* peer with whom handle must be shared. Used
				 *  only for NVMAP_HEAP_CARVEOUT_IVM
				 */
};

struct nvmap_alloc_kind_handle {
	__u32 handle;		/* nvmap handle */
	__u32 heap_mask;
	__u32 flags;
	__u32 align;
	__u8  kind;
	__u8  comp_tags;
};

struct nvmap_map_caller {
	__u32 handle;		/* nvmap handle */
	__u32 offset;		/* offset into hmem; should be page-aligned */
	__u32 length;		/* number of bytes to map */
	__u32 flags;		/* maps as wb/iwb etc. */
	unsigned long addr;	/* user pointer */
};

#ifdef CONFIG_COMPAT
struct nvmap_map_caller_32 {
	__u32 handle;		/* nvmap handle */
	__u32 offset;		/* offset into hmem; should be page-aligned */
	__u32 length;		/* number of bytes to map */
	__u32 flags;		/* maps as wb/iwb etc. */
	__u32 addr;		/* user pointer*/
};
#endif

struct nvmap_rw_handle {
	unsigned long addr;	/* user pointer*/
	__u32 handle;		/* nvmap handle */
	__u32 offset;		/* offset into hmem */
	__u32 elem_size;	/* individual atom size */
	__u32 hmem_stride;	/* delta in bytes between atoms in hmem */
	__u32 user_stride;	/* delta in bytes between atoms in user */
	__u32 count;		/* number of atoms to copy */
};

#ifdef CONFIG_COMPAT
struct nvmap_rw_handle_32 {
	__u32 addr;		/* user pointer */
	__u32 handle;		/* nvmap handle */
	__u32 offset;		/* offset into hmem */
	__u32 elem_size;	/* individual atom size */
	__u32 hmem_stride;	/* delta in bytes between atoms in hmem */
	__u32 user_stride;	/* delta in bytes between atoms in user */
	__u32 count;		/* number of atoms to copy */
};
#endif

struct nvmap_pin_handle {
	__u32 *handles;		/* array of handles to pin/unpin */
	unsigned long *addr;	/* array of addresses to return */
	__u32 count;		/* number of entries in handles */
};

#ifdef CONFIG_COMPAT
struct nvmap_pin_handle_32 {
	__u32 handles;		/* array of handles to pin/unpin */
	__u32 addr;		/*  array of addresses to return */
	__u32 count;		/* number of entries in handles */
};
#endif

struct nvmap_handle_param {
	__u32 handle;		/* nvmap handle */
	__u32 param;		/* size/align/base/heap etc. */
	unsigned long result;	/* returns requested info*/
};

#ifdef CONFIG_COMPAT
struct nvmap_handle_param_32 {
	__u32 handle;		/* nvmap handle */
	__u32 param;		/* size/align/base/heap etc. */
	__u32 result;		/* returns requested info*/
};
#endif

struct nvmap_cache_op {
	unsigned long addr;	/* user pointer*/
	__u32 handle;		/* nvmap handle */
	__u32 len;		/* bytes to flush */
	__s32 op;		/* wb/wb_inv/inv */
};

#ifdef CONFIG_COMPAT
struct nvmap_cache_op_32 {
	__u32 addr;		/* user pointer*/
	__u32 handle;		/* nvmap handle */
	__u32 len;		/* bytes to flush */
	__s32 op;		/* wb/wb_inv/inv */
};
#endif

struct nvmap_cache_op_list {
	__u64 handles;		/* Ptr to u32 type array, holding handles */
	__u64 offsets;		/* Ptr to u32 type array, holding offsets
				 * into handle mem */
	__u64 sizes;		/* Ptr to u32 type array, holindg sizes of memory
				 * regions within each handle */
	__u32 nr;		/* Number of handles */
	__s32 op;		/* wb/wb_inv/inv */
};

struct nvmap_debugfs_handles_header {
	__u8 version;
};

struct nvmap_debugfs_handles_entry {
	__u64 base;
	__u64 size;
	__u32 flags;
	__u32 share_count;
	__u64 mapped_size;
};

struct nvmap_set_tag_label {
	__u32 tag;
	__u32 len;		/* in: label length
				   out: number of characters copied */
	__u64 addr;		/* in: pointer to label or NULL to remove */
};

#define NVMAP_IOC_MAGIC 'N'

/* Creates a new memory handle. On input, the argument is the size of the new
 * handle; on return, the argument is the name of the new handle
 */
#define NVMAP_IOC_CREATE  _IOWR(NVMAP_IOC_MAGIC, 0, struct nvmap_create_handle)
#define NVMAP_IOC_FROM_ID _IOWR(NVMAP_IOC_MAGIC, 2, struct nvmap_create_handle)

/* Actually allocates memory for the specified handle */
#define NVMAP_IOC_ALLOC    _IOW(NVMAP_IOC_MAGIC, 3, struct nvmap_alloc_handle)

/* Frees a memory handle, unpinning any pinned pages and unmapping any mappings
 */
#define NVMAP_IOC_FREE       _IO(NVMAP_IOC_MAGIC, 4)

/* Maps the region of the specified handle into a user-provided virtual address
 * that was previously created via an mmap syscall on this fd */
#define NVMAP_IOC_MMAP       _IOWR(NVMAP_IOC_MAGIC, 5, struct nvmap_map_caller)
#ifdef CONFIG_COMPAT
#define NVMAP_IOC_MMAP_32    _IOWR(NVMAP_IOC_MAGIC, 5, struct nvmap_map_caller_32)
#endif

/* Reads/writes data (possibly strided) from a user-provided buffer into the
 * hmem at the specified offset */
#define NVMAP_IOC_WRITE      _IOW(NVMAP_IOC_MAGIC, 6, struct nvmap_rw_handle)
#define NVMAP_IOC_READ       _IOW(NVMAP_IOC_MAGIC, 7, struct nvmap_rw_handle)
#ifdef CONFIG_COMPAT
#define NVMAP_IOC_WRITE_32   _IOW(NVMAP_IOC_MAGIC, 6, struct nvmap_rw_handle_32)
#define NVMAP_IOC_READ_32    _IOW(NVMAP_IOC_MAGIC, 7, struct nvmap_rw_handle_32)
#endif

#define NVMAP_IOC_PARAM _IOWR(NVMAP_IOC_MAGIC, 8, struct nvmap_handle_param)
#ifdef CONFIG_COMPAT
#define NVMAP_IOC_PARAM_32 _IOWR(NVMAP_IOC_MAGIC, 8, struct nvmap_handle_param_32)
#endif

/* Pins a list of memory handles into IO-addressable memory (either IOVMM
 * space or physical memory, depending on the allocation), and returns the
 * address. Handles may be pinned recursively. */
#define NVMAP_IOC_PIN_MULT      _IOWR(NVMAP_IOC_MAGIC, 10, struct nvmap_pin_handle)
#define NVMAP_IOC_UNPIN_MULT    _IOW(NVMAP_IOC_MAGIC, 11, struct nvmap_pin_handle)
#ifdef CONFIG_COMPAT
#define NVMAP_IOC_PIN_MULT_32   _IOWR(NVMAP_IOC_MAGIC, 10, struct nvmap_pin_handle_32)
#define NVMAP_IOC_UNPIN_MULT_32 _IOW(NVMAP_IOC_MAGIC, 11, struct nvmap_pin_handle_32)
#endif

#define NVMAP_IOC_CACHE      _IOW(NVMAP_IOC_MAGIC, 12, struct nvmap_cache_op)
#ifdef CONFIG_COMPAT
#define NVMAP_IOC_CACHE_32  _IOW(NVMAP_IOC_MAGIC, 12, struct nvmap_cache_op_32)
#endif

/* Returns a global ID usable to allow a remote process to create a handle
 * reference to the same handle */
#define NVMAP_IOC_GET_ID  _IOWR(NVMAP_IOC_MAGIC, 13, struct nvmap_create_handle)

/* Returns a dma-buf fd usable to allow a remote process to create a handle
 * reference to the same handle */
#define NVMAP_IOC_SHARE  _IOWR(NVMAP_IOC_MAGIC, 14, struct nvmap_create_handle)

/* Returns a file id that allows a remote process to create a handle
 * reference to the same handle */
#define NVMAP_IOC_GET_FD  _IOWR(NVMAP_IOC_MAGIC, 15, struct nvmap_create_handle)

/* Create a new memory handle from file id passed */
#define NVMAP_IOC_FROM_FD _IOWR(NVMAP_IOC_MAGIC, 16, struct nvmap_create_handle)

/* Perform cache maintenance on a list of handles. */
#define NVMAP_IOC_CACHE_LIST _IOW(NVMAP_IOC_MAGIC, 17,	\
				  struct nvmap_cache_op_list)
/* Perform reserve operation on a list of handles. */
#define NVMAP_IOC_RESERVE _IOW(NVMAP_IOC_MAGIC, 18,	\
				  struct nvmap_cache_op_list)

#define NVMAP_IOC_FROM_IVC_ID _IOWR(NVMAP_IOC_MAGIC, 19, struct nvmap_create_handle)
#define NVMAP_IOC_GET_IVC_ID _IOWR(NVMAP_IOC_MAGIC, 20, struct nvmap_create_handle)
#define NVMAP_IOC_GET_IVM_HEAPS _IOR(NVMAP_IOC_MAGIC, 21, unsigned int)

/* Create a new memory handle from VA passed */
#define NVMAP_IOC_FROM_VA _IOWR(NVMAP_IOC_MAGIC, 22, struct nvmap_create_handle_from_va)

#define NVMAP_IOC_GUP_TEST _IOWR(NVMAP_IOC_MAGIC, 23, struct nvmap_gup_test)

/* Define a label for allocation tag */
#define NVMAP_IOC_SET_TAG_LABEL	_IOW(NVMAP_IOC_MAGIC, 24, struct nvmap_set_tag_label)

/* START of T124 IOCTLS */
/* Actually allocates memory for the specified handle, with kind */
#define NVMAP_IOC_ALLOC_KIND _IOW(NVMAP_IOC_MAGIC, 100, struct nvmap_alloc_kind_handle)

/* Actually allocates memory from IVM heaps */
#define NVMAP_IOC_ALLOC_IVM _IOW(NVMAP_IOC_MAGIC, 101, struct nvmap_alloc_ivm_handle)

/* Allocate seperate memory for VPR */
#define NVMAP_IOC_VPR_FLOOR_SIZE _IOW(NVMAP_IOC_MAGIC, 102, __u32)

#define NVMAP_IOC_MAXNR (_IOC_NR(NVMAP_IOC_VPR_FLOOR_SIZE))

#endif /* _LINUX_NVMAP_H */
