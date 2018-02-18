/*
 *  linux/drivers/mmc/card/queue.h
 *
 *  Copyright (C) 2003 Russell King, All Rights Reserved.
 *  Copyright 2006-2007 Pierre Ossman
 * Copyright (c) 2015 NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef MMC_QUEUE_H
#define MMC_QUEUE_H

#define MMC_REQ_SPECIAL_MASK   (REQ_DISCARD | REQ_FLUSH)

struct request;
struct task_struct;

struct mmc_blk_request {
	struct mmc_request	mrq_que;
	struct mmc_request	mrq;
	struct mmc_command	sbc;
	struct mmc_command	que;
	struct mmc_command	cmd;
	struct mmc_command	stop;
	struct mmc_data		data;
};

enum mmc_packed_type {
	MMC_PACKED_NONE = 0,
	MMC_PACKED_WRITE,
};

#define mmc_packed_cmd(type)	((type) != MMC_PACKED_NONE)
#define mmc_packed_wr(type)	((type) == MMC_PACKED_WRITE)

struct mmc_packed {
	struct list_head	list;
	__le32			cmd_hdr[1024];
	unsigned int		blocks;
	u8			nr_entries;
	u8			retries;
	s16			idx_failure;
};

struct mmc_queue_req {
	struct request		*req;
	struct mmc_blk_request	brq;
	struct scatterlist	*sg;
	char			*bounce_buf;
	struct scatterlist	*bounce_sg;
	unsigned int		bounce_sg_len;
	struct mmc_async_req	mmc_active;
	enum mmc_packed_type	cmd_type;
	struct mmc_packed	*packed;
	struct mmc_cmdq_req	mmc_cmdq_req;
	atomic_t		index;
};

#define INIT_ADMA3_FREE_INDEX 1 /* mqrq_prev points here */

struct mmc_queue {
	bool is_queue_end_req;
	int			adma3_free_index;
	struct mmc_card		*card;
	struct task_struct	*thread;
	struct task_struct	*cq_thread;
	struct semaphore	thread_sem;
	spinlock_t			cmdq_lock;
	unsigned int		flags;
#define MMC_QUEUE_SUSPENDED	(1 << 0)
#define MMC_QUEUE_NEW_REQUEST	(1 << 1)

	int			(*issue_fn)(struct mmc_queue *, struct request *);
	int	(*cmdq_issue_fn)(struct mmc_queue *, struct request *);
	int	(*cmdq_complete_fn)(struct request *);
	int			(*mmc_flush_queue_fn)(struct mmc_queue *);
	void			*data;
	struct request_queue	*queue;
	struct mmc_queue_req	mqrq[EMMC_MAX_QUEUE_DEPTH];
	struct mmc_queue_req	mqrq_adma3[ADMA3_MAX_QUEUE_DEPTH];
	struct mmc_queue_req	*mqrq_cur;
	struct mmc_queue_req	*mqrq_prev;
	struct mmc_queue_req	*mqrq_cmdq;
};

#define MMC_QUEUE_NORMAL_REQ	0
#define MMC_QUEUE_RT_REQ		(1 << 0)
#define MMC_QUEUE_SYNC_REQ		(1 << 1)

#define IS_SYNC_REQ(x)	(rq_is_sync(x))
#define IS_RT_CLASS_REQ(x)						\
		(IOPRIO_PRIO_CLASS(req_get_ioprio(x)) == IOPRIO_CLASS_RT)

extern int mmc_init_queue(struct mmc_queue *, struct mmc_card *, spinlock_t *,
			  const char *, int);
extern void mmc_cleanup_queue(struct mmc_queue *);
extern void mmc_queue_suspend(struct mmc_queue *);
extern void mmc_queue_resume(struct mmc_queue *);

extern unsigned int mmc_queue_map_sg(struct mmc_queue *,
				     struct mmc_queue_req *);
extern void mmc_queue_bounce_pre(struct mmc_queue_req *);
extern void mmc_queue_bounce_post(struct mmc_queue_req *);

extern void mmc_wait_cmdq_empty(struct mmc_host *);
extern int mmc_packed_init(struct mmc_queue *, struct mmc_card *);
extern void mmc_packed_clean(struct mmc_queue *);
extern int mmc_cmdq_init(struct mmc_queue *mq, struct mmc_card *card);
extern void mmc_cmdq_clean(struct mmc_queue *mq, struct mmc_card *card);

extern int mmc_access_rpmb(struct mmc_queue *);

#endif
