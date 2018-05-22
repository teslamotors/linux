// SPDX-License_Identifier: GPL-2.0
// Copyright (C) 2013 - 2018 Intel Corporation

#include <asm/cacheflush.h>
#include <linux/io.h>

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "ipu-bus.h"
#include "ipu-dma.h"
#include "ipu-mmu.h"
#include "ipu-wrapper.h"

struct wrapper_base {
	void __iomem *sys_base;
	const struct dma_map_ops *ops;
	/* Protect shared memory buffers */
	spinlock_t lock;
	struct list_head buffers;
	u32 css_map_done;
	struct device *dev;
};

struct wrapper_base isys;
struct wrapper_base psys;

struct my_css_memory_buffer_item {
	struct list_head list;
	dma_addr_t iova;
	unsigned long *addr;
	size_t bytes;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	struct dma_attrs attrs;
#else
	unsigned long attrs;
#endif
};

unsigned long long get_hrt_base_address(void)
{
	return 0;
}

static struct wrapper_base *get_mem_sub_system(int mmid)
{
	if (mmid == ISYS_MMID)
		return &isys;

	if (mmid == PSYS_MMID)
		return &psys;
	WARN(1, "Invalid mem subsystem");
	return NULL;
}

static struct wrapper_base *get_sub_system(int ssid)
{
	if (ssid == ISYS_SSID)
		return &isys;

	if (ssid == PSYS_SSID)
		return &psys;
	WARN(1, "Invalid subsystem");
	return NULL;
}

/*
 * Subsystem access functions to access IUNIT MMIO space
 */
static void *host_addr(int ssid, u32 addr)
{
	if (ssid == ISYS_SSID)
		return isys.sys_base + addr;
	else if (ssid == PSYS_SSID)
		return psys.sys_base + addr;
	/*
	 * Calling WARN_ON is a bit brutal but better to capture wrong register
	 * accesses immediately. We have no way to return an error here.
	 */
	WARN_ON(1);

	return NULL;
}

void vied_subsystem_store_32(int ssid, u32 addr, u32 data)
{
	writel(data, host_addr(ssid, addr));
}

void vied_subsystem_store_16(int ssid, u32 addr, u16 data)
{
	writew(data, host_addr(ssid, addr));
}

void vied_subsystem_store_8(int ssid, u32 addr, u8 data)
{
	writeb(data, host_addr(ssid, addr));
}

void vied_subsystem_store(int ssid,
			  u32 addr, const void *data, unsigned int size)
{
	void *dst = host_addr(ssid, addr);

	dev_dbg(get_sub_system(ssid)->dev, "access: %s 0x%x size: %d\n",
		__func__, addr, size);

	for (; size >= sizeof(u32); size -= sizeof(u32),
	     dst += sizeof(u32), data += sizeof(u32)) {
		writel(*(u32 *) data, dst);
	}
	if (size >= sizeof(u16)) {
		writew(*(u16 *) data, dst);
		size -= sizeof(u16), dst += sizeof(u16), data += sizeof(u16);
	}
	if (size)
		writeb(*(u8 *) data, dst);
}

u32 vied_subsystem_load_32(int ssid, u32 addr)
{
	return readl(host_addr(ssid, addr));
}

u16 vied_subsystem_load_16(int ssid, u32 addr)
{
	return readw(host_addr(ssid, addr));
}

u8 vied_subsystem_load_8(int ssid, u32 addr)
{
	return readb(host_addr(ssid, addr));
}

void vied_subsystem_load(int ssid, u32 addr, void *data, unsigned int size)
{
	void *src = host_addr(ssid, addr);

	dev_dbg(get_sub_system(ssid)->dev, "access: %s 0x%x size: %d\n",
		__func__, addr, size);

	for (; size >= sizeof(u32); size -= sizeof(u32),
	     src += sizeof(u32), data += sizeof(u32))
		*(u32 *) data = readl(src);
	if (size >= sizeof(u16)) {
		*(u16 *) data = readw(src);
		size -= sizeof(u16), src += sizeof(u16), data += sizeof(u16);
	}
	if (size)
		*(u8 *) data = readb(src);
}

/*
 * Initialize base address for subsystem
 */
void vied_subsystem_access_initialize(int system)
{
}

/*
 * Shared memory access codes written by Dash Biswait,
 * copied from FPGA environment
 */

/**
 * \brief Initialize the shared memory interface administration on the host.
 * \param mmid: id of ddr memory
 * \param host_ddr_addr: physical address of memory as seen from host
 * \param memory_size: size of ddr memory in bytes
 * \param ps: size of page in bytes (for instance 4096)
 */
int shared_memory_allocation_initialize(int mmid, u64 host_ddr_addr,
					size_t memory_size, size_t ps)
{
	return 0;
}

/**
 * \brief De-initialize the shared memory interface administration on the host.
 *
 */
void shared_memory_allocation_uninitialize(int mmid)
{
}

/**
 * \brief Initialize the shared memory interface administration on the host.
 * \param ssid: id of subsystem
 * \param mmid: id of ddr memory
 * \param mmu_ps: size of page in bits
 * \param mmu_pnrs: page numbers
 * \param ddr_addr: base address
 * \param inv_tlb: invalidate tbl
 * \param sbt: set l1 base address
 */
int shared_memory_map_initialize(int ssid, int mmid, size_t mmu_ps,
				 size_t mmu_pnrs, u64 ddr_addr,
				 int inv_tlb, int sbt)
{
	return 0;
}

/**
 * \brief De-initialize the shared memory interface administration on the host.
 */
void shared_memory_map_uninitialize(int ssid, int mmid)
{
}

static u8 alloc_cookie;

/**
 * \brief Allocate (DDR) shared memory space and return a host virtual address.
 * \Returns NULL when insufficient memory available
 */
u64 shared_memory_alloc(int mmid, size_t bytes)
{
	struct wrapper_base *mine = get_mem_sub_system(mmid);
	struct my_css_memory_buffer_item *buf;
	unsigned long flags;

	dev_dbg(mine->dev, "%s: in, size: %zu\n", __func__, bytes);

	if (!bytes)
		return (unsigned long)&alloc_cookie;

	might_sleep();

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return 0;

	/*alloc using ipu dma driver */
	buf->bytes = PAGE_ALIGN(bytes);

	buf->addr = dma_alloc_attrs(mine->dev, buf->bytes, &buf->iova,
				    GFP_KERNEL,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
				    NULL
#else
				    0
#endif
	    );
	if (!buf->addr) {
		kfree(buf);
		return 0;
	}

	spin_lock_irqsave(&mine->lock, flags);
	list_add(&buf->list, &mine->buffers);
	spin_unlock_irqrestore(&mine->lock, flags);

	return (unsigned long)buf->addr;
}

/**
 * \brief Free (DDR) shared memory space.
 */
void shared_memory_free(int mmid, u64 addr)
{
	struct wrapper_base *mine = get_mem_sub_system(mmid);
	struct my_css_memory_buffer_item *buf = NULL;
	unsigned long flags;

	if ((void *)(unsigned long)addr == &alloc_cookie)
		return;

	might_sleep();

	dev_dbg(mine->dev, "looking for iova %8.8llx\n", addr);

	spin_lock_irqsave(&mine->lock, flags);
	list_for_each_entry(buf, &mine->buffers, list) {
		dev_dbg(mine->dev, "buffer addr %8.8lx\n", (long)buf->addr);
		if ((long)buf->addr != addr)
			continue;

		dev_dbg(mine->dev, "found it!\n");
		list_del(&buf->list);
		spin_unlock_irqrestore(&mine->lock, flags);
		dma_free_attrs(mine->dev, buf->bytes, buf->addr, buf->iova,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			       &buf->attrs
#else
			       buf->attrs
#endif
		    );
		kfree(buf);
		return;
	}
	dev_warn(mine->dev, "Can't find mem object %8.8llx\n", addr);
	spin_unlock_irqrestore(&mine->lock, flags);
}

/**
 * \brief Convert a host virtual address to a CSS virtual address and
 * \update the MMU.
 */
u32 shared_memory_map(int ssid, int mmid, u64 addr)
{
	struct wrapper_base *mine = get_mem_sub_system(mmid);
	struct my_css_memory_buffer_item *buf = NULL;
	unsigned long flags;

	if ((void *)(unsigned long)addr == &alloc_cookie)
		return 0;

	spin_lock_irqsave(&mine->lock, flags);
	list_for_each_entry(buf, &mine->buffers, list) {
		dev_dbg(mine->dev, "%s %8.8lx\n", __func__, (long)buf->addr);
		if ((long)buf->addr != addr)
			continue;

		dev_dbg(mine->dev, "mapped!!\n");
		spin_unlock_irqrestore(&mine->lock, flags);
		return buf->iova;
	}
	dev_err(mine->dev, "Can't find mapped object %8.8llx\n", addr);
	spin_unlock_irqrestore(&mine->lock, flags);
	return 0;
}

/**
 * \brief Free a CSS virtual address and update the MMU.
 */
void shared_memory_unmap(int ssid, int mmid, u32 addr)
{
}

/**
 * \brief Store a byte into (DDR) shared memory space using a host
 * \virtual address
 */
void shared_memory_store_8(int mmid, u64 addr, u8 data)
{
	dev_dbg(get_mem_sub_system(mmid)->dev,
		"access: %s: Enter addr = 0x%llx data = 0x%x\n",
		__func__, addr, data);

	*((u8 *)(unsigned long) addr) = data;
	/*Invalidate the cache lines to flush the content to ddr. */
	clflush_cache_range((void *)(unsigned long)addr, sizeof(u8));
}

/**
 * \brief Store a 16-bit word into (DDR) shared memory space using a host
 * \virtual address
 */
void shared_memory_store_16(int mmid, u64 addr, u16 data)
{
	dev_dbg(get_mem_sub_system(mmid)->dev,
		"access: %s: Enter addr = 0x%llx data = 0x%x\n",
		__func__, addr, data);

	*((u16 *)(unsigned long) addr) = data;
	/*Invalidate the cache lines to flush the content to ddr. */
	clflush_cache_range((void *)(unsigned long) addr, sizeof(u16));
}

/**
 * \brief Store a 32-bit word into (DDR) shared memory space using a host
 * \virtual address
 */
void shared_memory_store_32(int mmid, u64 addr, u32 data)
{
	dev_dbg(get_mem_sub_system(mmid)->dev,
		"access: %s: Enter addr = 0x%llx data = 0x%x\n",
		__func__, addr, data);

	*((u32 *)(unsigned long) addr) = data;
	/* Invalidate the cache lines to flush the content to ddr. */
	clflush_cache_range((void *)(unsigned long) addr, sizeof(u32));
}

/**
 * \brief Store a number of bytes into (DDR) shared memory space using a host
 * \virtual address
 */
void shared_memory_store(int mmid, u64 addr, const void *data, size_t bytes)
{
	dev_dbg(get_mem_sub_system(mmid)->dev,
		"access: %s: Enter addr = 0x%lx bytes = 0x%zx\n", __func__,
		(unsigned long)addr, bytes);

	if (!data) {
		dev_err(get_mem_sub_system(mmid)->dev,
			"%s: data ptr is null\n", __func__);
	} else {
		const u8 *pdata = data;
		u8 *paddr = (u8 *)(unsigned long)addr;
		size_t i = 0;

		for (; i < bytes; ++i)
			*paddr++ = *pdata++;

		/* Invalidate the cache lines to flush the content to ddr. */
		clflush_cache_range((void *)(unsigned long) addr, bytes);
	}
}

/**
 * \brief Set a number of bytes of (DDR) shared memory space to 0 using a host
 * \virtual address
 */
void shared_memory_zero(int mmid, u64 addr, size_t bytes)
{
	dev_dbg(get_mem_sub_system(mmid)->dev,
		"access: %s: Enter addr = 0x%llx data = 0x%zx\n",
		__func__, (unsigned long long)addr, bytes);

	memset((void *)(unsigned long)addr, 0, bytes);
	clflush_cache_range((void *)(unsigned long)addr, bytes);
}

/**
 * \brief Load a byte from (DDR) shared memory space using a host
 * \virtual address
 */
u8 shared_memory_load_8(int mmid, u64 addr)
{
	u8 data = 0;

	dev_dbg(get_mem_sub_system(mmid)->dev,
		"access: %s: Enter addr = 0x%llx\n", __func__, addr);

	/* Invalidate the cache lines to flush the content to ddr. */
	clflush_cache_range((void *)(unsigned long)addr, sizeof(u8));
	data = *(u8 *)(unsigned long) addr;
	return data;
}

/**
 * \brief Load a 16-bit word from (DDR) shared memory space using a host
 * \virtual address
 */
u16 shared_memory_load_16(int mmid, u64 addr)
{
	u16 data = 0;

	dev_dbg(get_mem_sub_system(mmid)->dev,
		"access: %s: Enter addr = 0x%llx\n", __func__, addr);

	/* Invalidate the cache lines to flush the content to ddr. */
	clflush_cache_range((void *)(unsigned long)addr, sizeof(u16));
	data = *(u16 *)(unsigned long)addr;
	return data;
}

/**
 * \brief Load a 32-bit word from (DDR) shared memory space using a host
 * \virtual address
 */
u32 shared_memory_load_32(int mmid, u64 addr)
{
	u32 data = 0;

	dev_dbg(get_mem_sub_system(mmid)->dev,
		"access: %s: Enter addr = 0x%llx\n", __func__, addr);

	/* Invalidate the cache lines to flush the content to ddr. */
	clflush_cache_range((void *)(unsigned long)addr, sizeof(u32));
	data = *(u32 *)(unsigned long)addr;
	return data;
}

/**
 * \brief Load a number of bytes from (DDR) shared memory space using a host
 * \virtual address
 */
void shared_memory_load(int mmid, u64 addr, void *data, size_t bytes)
{
	dev_dbg(get_mem_sub_system(mmid)->dev,
		"access: %s: Enter addr = 0x%lx bytes = 0x%zx\n", __func__,
		(unsigned long)addr, bytes);

	if (!data) {
		dev_err(get_mem_sub_system(mmid)->dev,
			"%s: data ptr is null\n", __func__);

	} else {
		u8 *pdata = data;
		u8 *paddr = (u8 *)(unsigned long)addr;
		size_t i = 0;

		/* Invalidate the cache lines to flush the content to ddr. */
		clflush_cache_range((void *)(unsigned long)addr, bytes);
		for (; i < bytes; ++i)
			*pdata++ = *paddr++;
	}
}

static int init_wrapper(struct wrapper_base *sys)
{
	INIT_LIST_HEAD(&sys->buffers);
	spin_lock_init(&sys->lock);
	return 0;
}

/*
 * Wrapper driver set base address for library use
 */
void ipu_wrapper_init(int mmid, struct device *dev, void __iomem *base)
{
	struct wrapper_base *sys = get_mem_sub_system(mmid);

	init_wrapper(sys);
	sys->dev = dev;
	sys->sys_base = base;
}
