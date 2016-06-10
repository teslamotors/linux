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

#ifndef INTEL_IPU4_PSYS_H
#define INTEL_IPU4_PSYS_H

#include <linux/cdev.h>
#include <linux/workqueue.h>

#include "intel-ipu4.h"
#include "intel-ipu4-pdata.h"
#include "intel-ipu4-resources.h"

#define INTEL_IPU4_PSYS_PG_POOL_SIZE 16
#define INTEL_IPU4_PSYS_PG_MAX_SIZE 2048
#define INTEL_IPU4_MAX_PSYS_CMD_BUFFERS 32
#define INTEL_IPU4_PSYS_CMD_TIMEOUT_MS_FPGA (60000*15)
#define INTEL_IPU4_PSYS_CMD_TIMEOUT_MS_SOC 2000
#define INTEL_IPU4_PSYS_OPEN_TIMEOUT_US	   50
#define INTEL_IPU4_PSYS_OPEN_RETRY (10000 / INTEL_IPU4_PSYS_OPEN_TIMEOUT_US)
#define INTEL_IPU4_PSYS_EVENT_CMD_COMPLETE IA_CSS_PSYS_EVENT_TYPE_SUCCESS
#define INTEL_IPU4_PSYS_EVENT_FRAGMENT_COMPLETE IA_CSS_PSYS_EVENT_TYPE_SUCCESS
#define INTEL_IPU4_PSYS_CLOSE_TIMEOUT_US   50
#define INTEL_IPU4_PSYS_CLOSE_TIMEOUT \
	(100000 / INTEL_IPU4_PSYS_CLOSE_TIMEOUT_US)

struct task_struct;

struct intel_ipu4_psys {
	struct cdev cdev;
	struct device dev;

	struct mutex mutex;
	int power;
	bool icache_prefetch_sp;
	bool icache_prefetch_isp;
	spinlock_t power_lock;
	spinlock_t pgs_lock;
	struct list_head fhs;
	struct list_head pgs;
	struct list_head started_kcmds_list;
	struct intel_ipu4_psys_pdata *pdata;
	struct intel_ipu4_bus_device *adev;
	struct ia_css_syscom_context *dev_ctx;
	struct ia_css_syscom_config *syscom_config;
	struct ia_css_psys_server_init *server_init;
	struct task_struct *isr_thread;
	struct task_struct *sched_cmd_thread;
	struct work_struct watchdog_work;
	wait_queue_head_t sched_cmd_wq;
	atomic_t wakeup_sched_thread_count;
	struct dentry *debugfsdir;

	/* Resources needed to be managed for process groups */
	struct intel_ipu4_psys_resource_pool resource_pool_running;
	struct intel_ipu4_psys_resource_pool resource_pool_started;

	const struct firmware *fw;
	struct sg_table fw_sgt;
	u64 *pkg_dir;
	dma_addr_t pkg_dir_dma_addr;
	unsigned pkg_dir_size;
	unsigned long timeout;

	int active_kcmds, started_kcmds;
	void *fwcom;
};

struct intel_ipu4_psys_fh {
	struct intel_ipu4_psys *psys;
	struct mutex mutex;  /* Protects bufmap & kcmds fields */
	struct list_head list;
	struct list_head bufmap;
	struct list_head kcmds[INTEL_IPU4_PSYS_CMD_PRIORITY_NUM];
	struct intel_ipu4_psys_kcmd
			*new_kcmd_tail[INTEL_IPU4_PSYS_CMD_PRIORITY_NUM];
	wait_queue_head_t wait;
};

struct intel_ipu4_psys_pg {
	struct ia_css_process_group *pg;
	size_t size;
	size_t pg_size;
	dma_addr_t pg_dma_addr;
	struct list_head list;
};

enum intel_ipu4_psys_cmd_state {
	KCMD_STATE_NEW,
	KCMD_STATE_START_PREPARED,
	KCMD_STATE_STARTED,
	KCMD_STATE_RUN_PREPARED,
	KCMD_STATE_RUNNING,
	KCMD_STATE_COMPLETE
};

struct intel_ipu4_psys_kcmd {
	struct intel_ipu4_psys_fh *fh;
	struct list_head list;
	struct list_head started_list;
	enum intel_ipu4_psys_cmd_state state;
	void *pg_manifest;
	size_t pg_manifest_size;
	struct intel_ipu4_psys_kbuffer **kbufs;
	struct intel_ipu4_psys_dma_buf *buffers;
	size_t nbuffers;
	struct ia_css_process_group *pg_user;
	struct intel_ipu4_psys_pg *kpg;
	uint32_t id;
	uint64_t issue_id;
	uint32_t priority;
	struct intel_ipu4_buttress_constraint constraint;

	struct intel_ipu4_psys_resource_alloc resource_alloc;
	struct intel_ipu4_psys_event ev;
	struct timer_list watchdog;
};

struct intel_ipu4_psys_kbuffer {
	uint64_t len;
	void *userptr;
	uint32_t flags;
	int fd;
	void *kaddr;
	struct list_head list;
	bool vma_is_io;
	dma_addr_t dma_addr;
	struct sg_table *sgt;
	struct page **pages;
	size_t npages;
	struct dma_buf_attachment *db_attach;
	struct dma_buf *dbuf;
	struct intel_ipu4_psys *psys;
	struct intel_ipu4_psys_fh *fh;
	bool valid; /* True when buffer is usable */
};

#define inode_to_intel_ipu4_psys(inode) \
	container_of((inode)->i_cdev, struct intel_ipu4_psys, cdev)

#ifdef CONFIG_COMPAT
extern long intel_ipu4_psys_compat_ioctl32(struct file *file, unsigned int cmd,
					unsigned long arg);
#endif
#endif /* INTEL_IPU4_PSYS_H */
