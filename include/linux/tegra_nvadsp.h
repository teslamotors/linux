/*
 * A Header file for managing ADSP/APE
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __LINUX_TEGRA_NVADSP_H
#define __LINUX_TEGRA_NVADSP_H

#include <linux/types.h>
#include <linux/timer.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/list.h>
#include <linux/completion.h>

struct nvadsp_platform_data {
	phys_addr_t co_pa;
	unsigned long co_size;
};

typedef int status_t;

/*
 * Shared Semaphores
 */
typedef struct {
	int magic; /* 'ssem' */
	uint8_t id;
	wait_queue_head_t wait;
	struct timer_list timer;
} nvadsp_shared_sema_t;

nvadsp_shared_sema_t *
nvadsp_shared_sema_init(uint8_t nvadsp_shared_sema_id);
status_t nvadsp_shared_sema_destroy(nvadsp_shared_sema_t *);
status_t nvadsp_shared_sema_acquire(nvadsp_shared_sema_t *);
status_t nvadsp_shared_sema_release(nvadsp_shared_sema_t *);

/*
 * Arbitrated Semaphores
 */
typedef struct {
	int magic; /* 'asem' */
	uint8_t  id;
	wait_queue_head_t wait;
	struct completion comp;
} nvadsp_arb_sema_t;

nvadsp_arb_sema_t *nvadsp_arb_sema_init(uint8_t nvadsp_arb_sema_id);
status_t nvadsp_arb_sema_destroy(nvadsp_arb_sema_t *);
status_t nvadsp_arb_sema_acquire(nvadsp_arb_sema_t *);
status_t nvadsp_arb_sema_release(nvadsp_arb_sema_t *);

/*
 * Mailbox Queue
 */
#define NVADSP_MBOX_QUEUE_SIZE		32
#define NVADSP_MBOX_QUEUE_SIZE_MASK	(NVADSP_MBOX_QUEUE_SIZE - 1)
struct nvadsp_mbox_queue {
	uint32_t array[NVADSP_MBOX_QUEUE_SIZE];
	uint16_t head;
	uint16_t tail;
	uint16_t count;
	struct completion comp;
	spinlock_t lock;
};

status_t nvadsp_mboxq_enqueue(struct nvadsp_mbox_queue *, uint32_t);

/*
 * Mailbox
 */
#define NVADSP_MBOX_NAME_MAX		16
#define NVADSP_MBOX_NAME_MAX_STR	(NVADSP_MBOX_NAME_MAX + 1)

typedef status_t (*nvadsp_mbox_handler_t)(uint32_t, void *);

struct nvadsp_mbox {
	uint16_t id;
	char name[NVADSP_MBOX_NAME_MAX_STR];
	struct nvadsp_mbox_queue recv_queue;
	nvadsp_mbox_handler_t handler;
#ifdef CONFIG_MBOX_ACK_HANDLER
	nvadsp_mbox_handler_t ack_handler;
#endif
	void *hdata;
};

#define NVADSP_MBOX_SMSG       0x1
#define NVADSP_MBOX_LMSG       0x2

status_t nvadsp_mbox_open(struct nvadsp_mbox *mbox, uint16_t *mid,
			  const char *name, nvadsp_mbox_handler_t handler,
			  void *hdata);
status_t nvadsp_mbox_send(struct nvadsp_mbox *mbox, uint32_t data,
			  uint32_t flags, bool block, unsigned int timeout);
status_t nvadsp_mbox_recv(struct nvadsp_mbox *mbox, uint32_t *data, bool block,
			  unsigned int timeout);
status_t nvadsp_mbox_close(struct nvadsp_mbox *mbox);

#ifdef CONFIG_MBOX_ACK_HANDLER
static inline void register_ack_handler(struct nvadsp_mbox *mbox,
			nvadsp_mbox_handler_t handler)
{
	mbox->ack_handler = handler;
}
#endif

/*
 * Circular Message Queue
 */
typedef struct _msgq_message_t {
	int32_t size;		/* size of payload in words */
	int32_t payload[1];	/* variable length payload */
} msgq_message_t;

#define MSGQ_MESSAGE_HEADER_SIZE \
	(sizeof(msgq_message_t) - sizeof(((msgq_message_t *)0)->payload))
#define MSGQ_MESSAGE_HEADER_WSIZE \
	(MSGQ_MESSAGE_HEADER_SIZE / sizeof(int32_t))

typedef struct _msgq_t {
	int32_t size;		/* queue size in words */
	int32_t write_index;	/* queue write index */
	int32_t read_index;	/* queue read index */
	int32_t queue[1];	/* variable length queue */
} msgq_t;

#define MSGQ_HEADER_SIZE	(sizeof(msgq_t) - sizeof(((msgq_t *)0)->queue))
#define MSGQ_HEADER_WSIZE	(MSGQ_HEADER_SIZE / sizeof(int32_t))
#define MSGQ_MAX_QUEUE_WSIZE	(8192 - MSGQ_HEADER_WSIZE)
#define MSGQ_MSG_WSIZE(x) \
	(((sizeof(x) + sizeof(int32_t) - 1) & (~(sizeof(int32_t)-1))) >> 2)
#define MSGQ_MSG_PAYLOAD_WSIZE(x) \
	(MSGQ_MSG_WSIZE(x) - MSGQ_MESSAGE_HEADER_WSIZE)

void msgq_init(msgq_t *msgq, int32_t size);
int32_t msgq_queue_message(msgq_t *msgq, const msgq_message_t *message);
int32_t msgq_dequeue_message(msgq_t *msgq, msgq_message_t *message);
#define msgq_discard_message(msgq) msgq_dequeue_message(msgq, NULL)

/*
 * DRAM Sharing
 */
typedef dma_addr_t nvadsp_iova_addr_t;
typedef enum dma_data_direction nvadsp_data_direction_t;

nvadsp_iova_addr_t
nvadsp_dram_map_single(struct device *nvadsp_dev,
		       void *cpu_addr, size_t size,
		       nvadsp_data_direction_t direction);
void
nvadsp_dram_unmap_single(struct device *nvadsp_dev,
			 nvadsp_iova_addr_t iova_addr, size_t size,
			 nvadsp_data_direction_t direction);

nvadsp_iova_addr_t
nvadsp_dram_map_page(struct device *nvadsp_dev,
		     struct page *page, unsigned long offset, size_t size,
		     nvadsp_data_direction_t direction);
void
nvadsp_dram_unmap_page(struct device *nvadsp_dev,
		       nvadsp_iova_addr_t iova_addr, size_t size,
		       nvadsp_data_direction_t direction);

void
nvadsp_dram_sync_single_for_cpu(struct device *nvadsp_dev,
				nvadsp_iova_addr_t iova_addr, size_t size,
				nvadsp_data_direction_t direction);
void
nvadsp_dram_sync_single_for_device(struct device *nvadsp_dev,
				   nvadsp_iova_addr_t iova_addr, size_t size,
				   nvadsp_data_direction_t direction);

/*
 * ARAM Bookkeeping
 */
bool nvadsp_aram_request(char *start, size_t size, char *id);
void nvadsp_aram_release(char *start, size_t size);

/*
 * ADSP OS
 */

typedef const void *nvadsp_os_handle_t;

void nvadsp_adsp_init(void);
int __must_check nvadsp_os_load(void);
int __must_check nvadsp_os_start(void);
void nvadsp_os_stop(void);
int __must_check nvadsp_os_suspend(void);
void dump_adsp_sys(void);
void nvadsp_get_os_version(char *, int);

/*
 * ADSP TSC
 */
uint64_t nvadsp_get_timestamp_counter(void);

/*
 * ADSP OS App
 */
#define ARGV_SIZE_IN_WORDS         128

enum {
	NVADSP_APP_STATE_UNKNOWN,
	NVADSP_APP_STATE_INITIALIZED,
	NVADSP_APP_STATE_STARTED,
	NVADSP_APP_STATE_STOPPED
};

enum adsp_app_status_msg {
	OS_LOAD_ADSP_APPS,
	RUN_ADSP_APP,
	ADSP_APP_INIT,
	ADSP_APP_START,
	ADSP_APP_START_STATUS,
	ADSP_APP_COMPLETE_STATUS
};

struct nvadsp_app_info;
typedef const void *nvadsp_app_handle_t;
typedef void (*app_complete_status_notifier)(struct nvadsp_app_info *,
	enum adsp_app_status_msg, int32_t);

typedef struct adsp_app_mem {
	/* DRAM segment*/
	void      *dram;
	/* DRAM in shared memory segment. uncached */
	void      *shared;
	/* DRAM in shared memory segment. write combined */
	void      *shared_wc;
	/*  ARAM if available, DRAM OK */
	void      *aram;
	/* set to 1 if ARAM allocation succeeded else 0 meaning allocated from
	 * dram.
	 */
	uint32_t   aram_flag;
	/* ARAM Segment. exclusively */
	void      *aram_x;
	/* set to 1 if ARAM allocation succeeded */
	uint32_t   aram_x_flag;
} adsp_app_mem_t;

typedef struct adsp_app_iova_mem {
	/* DRAM segment*/
	uint32_t	dram;
	/* DRAM in shared memory segment. uncached */
	uint32_t	shared;
	/* DRAM in shared memory segment. write combined */
	uint32_t	shared_wc;
	/*  ARAM if available, DRAM OK */
	uint32_t	aram;
	/*
	 * set to 1 if ARAM allocation succeeded else 0 meaning allocated from
	 * dram.
	 */
	uint32_t	aram_flag;
	/* ARAM Segment. exclusively */
	uint32_t	aram_x;
	/* set to 1 if ARAM allocation succeeded */
	uint32_t	aram_x_flag;
} adsp_app_iova_mem_t;


typedef struct nvadsp_app_args {
	 /* number of arguments passed in */
	int32_t  argc;
	/* binary representation of arguments */
	int32_t  argv[ARGV_SIZE_IN_WORDS];
} nvadsp_app_args_t;

typedef struct nvadsp_app_info {
	const char *name;
	const int instance_id;
	const uint32_t token;
	const int state;
	adsp_app_mem_t mem;
	adsp_app_iova_mem_t iova_mem;
	struct list_head node;
	uint32_t stack_size;
	const void *handle;
	int return_status;
	struct completion wait_for_app_complete;
	struct completion wait_for_app_start;
	app_complete_status_notifier complete_status_notifier;
	struct work_struct complete_work;
	enum adsp_app_status_msg status_msg;
	void *priv;
} nvadsp_app_info_t;

nvadsp_app_handle_t __must_check nvadsp_app_load(const char *, const char *);
nvadsp_app_info_t __must_check *nvadsp_app_init(nvadsp_app_handle_t,
		nvadsp_app_args_t *);
void nvadsp_app_unload(nvadsp_app_handle_t);
int __must_check nvadsp_app_start(nvadsp_app_info_t *);
int nvadsp_app_stop(nvadsp_app_info_t *);
int nvadsp_app_deinit(nvadsp_app_info_t *);
void *nvadsp_alloc_coherent(size_t, dma_addr_t *, gfp_t);
void nvadsp_free_coherent(size_t, void *, dma_addr_t);
void *nvadsp_da_to_va_mappings(u64, int);
nvadsp_app_info_t __must_check *nvadsp_run_app(nvadsp_os_handle_t, const char *,
	nvadsp_app_args_t *, app_complete_status_notifier, uint32_t, bool);
void nvadsp_exit_app(nvadsp_app_info_t *app, bool terminate);

static inline void
set_app_complete_notifier(
		nvadsp_app_info_t *info, app_complete_status_notifier notifier)
{
	info->complete_status_notifier = notifier;
}

static inline void wait_for_nvadsp_app_complete(nvadsp_app_info_t *info)
{
	/*
	 * wait_for_complete must be called only after app has started
	 */
	if (info->state == NVADSP_APP_STATE_STARTED)
		wait_for_completion(&info->wait_for_app_complete);
}

/**
 * wait_for_nvadsp_app_complete_timeout:
 * @info:  pointer to nvadsp_app_info_t
 * @timeout:  timeout value in jiffies
 *
 * This waits for either a completion of a specific app to be signaled or for a
 * specified timeout to expire. It is interruptible. The timeout is in jiffies.
 *
 * The return value is -ERESTARTSYS if interrupted, 0 if timed out,
 * positive (at least 1, or number of jiffies left till timeout) if completed.
 */
static inline long wait_for_nvadsp_app_complete_timeout(nvadsp_app_info_t *info,
			unsigned long timeout)
{
	int ret = -EINVAL;
	/*
	 * wait_for_complete must be called only after app has started
	 */
	if (info->state == NVADSP_APP_STATE_STARTED)
		ret = wait_for_completion_interruptible_timeout(
			&info->wait_for_app_complete, timeout);

	return ret;
}

#ifdef CONFIG_TEGRA_ADSP_DFS
/*
 * Override adsp freq and reinit actmon counters
 *
 * @params:
 * freq: adsp freq in KHz
 * return - final freq got set.
 *		- 0, incase of error.
 *
 */
unsigned long adsp_override_freq(unsigned long freq);
void adsp_update_dfs_min_rate(unsigned long freq);

/* Enable / disable dynamic freq scaling */
void adsp_update_dfs(bool enable);
#else
static inline unsigned long adsp_override_freq(unsigned long freq)
{
	return 0;
}

static inline void adsp_update_dfs_min_rate(unsigned long freq)
{
	return;
}

static inline void adsp_update_dfs(bool enable)
{
	return;
}
#endif

#endif /* __LINUX_TEGRA_NVADSP_H */
