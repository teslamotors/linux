/* SPDX-License_Identifier: GPL-2.0 */
/* Copyright (C) 2013 - 2018 Intel Corporation */

#ifndef IPU_PSYS_H
#define IPU_PSYS_H

#include <linux/cdev.h>
#include <linux/workqueue.h>

#include "ipu.h"
#include "ipu-pdata.h"
#include "ipu-fw-psys.h"

#include <uapi/linux/ipu-psys.h>

#define IPU_PSYS_PG_POOL_SIZE 16
#define IPU_PSYS_PG_MAX_SIZE 2048
#define IPU_MAX_PSYS_CMD_BUFFERS 32
#define IPU_PSYS_EVENT_CMD_COMPLETE IPU_FW_PSYS_EVENT_TYPE_SUCCESS
#define IPU_PSYS_EVENT_FRAGMENT_COMPLETE IPU_FW_PSYS_EVENT_TYPE_SUCCESS
#define IPU_PSYS_CLOSE_TIMEOUT_US   50
#define IPU_PSYS_CLOSE_TIMEOUT (100000 / IPU_PSYS_CLOSE_TIMEOUT_US)
#define IPU_PSYS_BUF_SET_POOL_SIZE 16
#define IPU_PSYS_BUF_SET_MAX_SIZE 1024
#define IPU_PSYS_WORK_QUEUE		system_power_efficient_wq
#define IPU_MAX_RESOURCES 32

/* Opaque structure. Do not access fields. */
struct ipu_resource {
	u32 id;
	int elements;	/* Number of elements available to allocation */
	unsigned long *bitmap;	/* Allocation bitmap, a bit for each element */
};

enum ipu_resource_type {
	IPU_RESOURCE_DEV_CHN = 0,
	IPU_RESOURCE_EXT_MEM,
	IPU_RESOURCE_DFM
};

/* Allocation of resource(s) */
/* Opaque structure. Do not access fields. */
struct ipu_resource_alloc {
	enum ipu_resource_type type;
	struct ipu_resource *resource;
	int elements;
	int pos;
};

/*
 * This struct represents all of the currently allocated
 * resources from IPU model. It is used also for allocating
 * resources for the next set of PGs to be run on IPU
 * (ie. those PGs which are not yet being run and which don't
 * yet reserve real IPU resources).
 */
#define IPU_PSYS_RESOURCE_OVERALLOC 2	/* Some room for ABI / ext lib delta */
struct ipu_psys_resource_pool {
	u32 cells;	/* Bitmask of cells allocated */
	struct ipu_resource dev_channels[IPU_FW_PSYS_N_DEV_CHN_ID +
					 IPU_PSYS_RESOURCE_OVERALLOC];
	struct ipu_resource ext_memory[IPU_FW_PSYS_N_MEM_ID +
				       IPU_PSYS_RESOURCE_OVERALLOC];
	struct ipu_resource dfms[IPU_FW_PSYS_N_DEV_DFM_ID +
				 IPU_PSYS_RESOURCE_OVERALLOC];
};

/*
 * This struct keeps book of the resources allocated for a specific PG.
 * It is used for freeing up resources from struct ipu_psys_resources
 * when the PG is released from IPU4 (or model of IPU4).
 */
struct ipu_psys_resource_alloc {
	u32 cells;	/* Bitmask of cells needed */
	struct ipu_resource_alloc
	 resource_alloc[IPU_MAX_RESOURCES];
	int resources;
};

struct task_struct;
struct ipu_psys {
	struct cdev cdev;
	struct device dev;

	struct mutex mutex;	/* Psys various */
	int power;
	bool icache_prefetch_sp;
	bool icache_prefetch_isp;
	spinlock_t power_lock;	/* Serialize access to power */
	spinlock_t pgs_lock;	/* Protect pgs list access */
	struct list_head fhs;
	struct list_head pgs;
	struct list_head started_kcmds_list;
	struct ipu_psys_pdata *pdata;
	struct ipu_bus_device *adev;
	struct ia_css_syscom_context *dev_ctx;
	struct ia_css_syscom_config *syscom_config;
	struct ia_css_psys_server_init *server_init;
	struct task_struct *sched_cmd_thread;
	struct work_struct watchdog_work;
	wait_queue_head_t sched_cmd_wq;
	atomic_t wakeup_sched_thread_count;
	struct dentry *debugfsdir;

	/* Resources needed to be managed for process groups */
	struct ipu_psys_resource_pool resource_pool_running;
	struct ipu_psys_resource_pool resource_pool_started;

	const struct firmware *fw;
	struct sg_table fw_sgt;
	u64 *pkg_dir;
	dma_addr_t pkg_dir_dma_addr;
	unsigned int pkg_dir_size;
	unsigned long timeout;

	int active_kcmds, started_kcmds;
	void *fwcom;
};

struct ipu_psys_fh {
	struct ipu_psys *psys;
	struct mutex mutex;	/* Protects bufmap & kcmds fields */
	struct list_head list;
	struct list_head bufmap;
	struct list_head kcmds[IPU_PSYS_CMD_PRIORITY_NUM];
	struct ipu_psys_kcmd
	*new_kcmd_tail[IPU_PSYS_CMD_PRIORITY_NUM];
	wait_queue_head_t wait;
	struct mutex bs_mutex;	/* Protects buf_set field */
	struct list_head buf_sets;
};


struct ipu_psys_pg {
	struct ipu_fw_psys_process_group *pg;
	size_t size;
	size_t pg_size;
	dma_addr_t pg_dma_addr;
	struct list_head list;
	struct ipu_psys_resource_alloc resource_alloc;
};

enum ipu_psys_cmd_state {
	KCMD_STATE_NEW,
	KCMD_STATE_START_PREPARED,
	KCMD_STATE_STARTED,
	KCMD_STATE_RUN_PREPARED,
	KCMD_STATE_RUNNING,
	KCMD_STATE_COMPLETE
};

struct ipu_psys_buffer_set {
	struct list_head list;
	struct ipu_fw_psys_buffer_set *buf_set;
	size_t size;
	size_t buf_set_size;
	dma_addr_t dma_addr;
	void *kaddr;
	struct ipu_psys_kcmd *kcmd;
};

struct ipu_psys_kcmd {
	struct ipu_psys_fh *fh;
	struct list_head list;
	struct list_head started_list;
	enum ipu_psys_cmd_state state;
	void *pg_manifest;
	size_t pg_manifest_size;
	struct ipu_psys_kbuffer **kbufs;
	struct ipu_psys_buffer *buffers;
	size_t nbuffers;
	struct ipu_fw_psys_process_group *pg_user;
	struct ipu_psys_pg *kpg;
	u64 user_token;
	u64 issue_id;
	u32 priority;
	struct ipu_buttress_constraint constraint;
	struct ipu_psys_buffer_set *kbuf_set;

	struct ipu_psys_event ev;
	struct timer_list watchdog;
};

struct ipu_dma_buf_attach {
	struct device *dev;
	u64 len;
	void *userptr;
	struct sg_table *sgt;
	bool vma_is_io;
	struct page **pages;
	size_t npages;
};

struct ipu_psys_kbuffer {
	u64 len;
	void *userptr;
	u32 flags;
	int fd;
	void *kaddr;
	struct list_head list;
	dma_addr_t dma_addr;
	struct sg_table *sgt;
	struct dma_buf_attachment *db_attach;
	struct dma_buf *dbuf;
	struct ipu_psys *psys;
	struct ipu_psys_fh *fh;
	bool valid;	/* True when buffer is usable */
};

#define inode_to_ipu_psys(inode) \
	container_of((inode)->i_cdev, struct ipu_psys, cdev)


#ifdef CONFIG_COMPAT
long ipu_psys_compat_ioctl32(struct file *file, unsigned int cmd,
			     unsigned long arg);
#endif

void ipu_psys_setup_hw(struct ipu_psys *psys);
void ipu_psys_handle_events(struct ipu_psys *psys);
int ipu_psys_kcmd_new(struct ipu_psys_command *cmd, struct ipu_psys_fh *fh);
int ipu_psys_kcmd_queue(struct ipu_psys *psys, struct ipu_psys_kcmd *kcmd);
void ipu_psys_kcmd_complete(struct ipu_psys *psys,
			    struct ipu_psys_kcmd *kcmd, int error);
void ipu_psys_run_next(struct ipu_psys *psys);
void ipu_psys_watchdog_work(struct work_struct *work);
int ipu_psys_kcmd_abort(struct ipu_psys *psys,
			struct ipu_psys_kcmd *kcmd, int error);
void ipu_psys_kcmd_free(struct ipu_psys_kcmd *kcmd);
struct ipu_psys_pg *__get_pg_buf(struct ipu_psys *psys, size_t pg_size);
struct ipu_psys_kbuffer *
ipu_psys_lookup_kbuffer(struct ipu_psys_fh *fh, int fd);
struct ipu_psys_kbuffer *
ipu_psys_lookup_kbuffer_by_kaddr(struct ipu_psys_fh *fh, void *kaddr);
#ifdef IPU_PSYS_GPC
int ipu_psys_gpc_init_debugfs(struct ipu_psys *psys);
#endif
int ipu_psys_resource_pool_init(struct ipu_psys_resource_pool *pool);
void ipu_psys_resource_pool_cleanup(struct ipu_psys_resource_pool *pool);

#endif /* IPU_PSYS_H */
