/*
 *  linux/drivers/mmc/card/queue.c
 *
 *  Copyright (C) 2003 Russell King, All Rights Reserved.
 *  Copyright 2006-2007 Pierre Ossman
 * Copyright (c) 2015-2016 NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include "queue.h"

#define MMC_QUEUE_BOUNCESZ	65536

static inline bool mmc_cmdq_should_pull_reqs(struct mmc_host *host,
			struct mmc_cmdq_context_info *ctx);

static int mmc_cmdq_thread(void *d)
{
	struct mmc_queue *mq = d;
	struct request_queue *q = mq->queue;
	struct mmc_card *card = mq->card;
	struct mmc_host *host = card->host;
	struct mmc_cmdq_context_info *ctx = &host->cmdq_ctx;
	struct request *req;

	current->flags |= PF_MEMALLOC;

	down(&mq->thread_sem);
	do {
		set_current_state(TASK_INTERRUPTIBLE);
		if (!mmc_cmdq_should_pull_reqs(host, ctx)) {
			test_and_set_bit(0, &ctx->req_starved);
			continue;
		}

		spin_lock_irq(q->queue_lock);
		req = blk_peek_request(q);
		spin_unlock_irq(q->queue_lock);
		if (!req) {
			if (kthread_should_stop()) {
				set_current_state(TASK_RUNNING);
				break;
			}
			up(&mq->thread_sem);
			schedule();
			down(&mq->thread_sem);
			continue;
		}

		spin_lock_irq(q->queue_lock);
		if (blk_queue_start_tag(q, req)) {
			test_and_set_bit(0, &ctx->req_starved);
			spin_unlock_irq(q->queue_lock);
			continue;
		}

		spin_unlock_irq(q->queue_lock);
		set_current_state(TASK_RUNNING);
		mq->cmdq_issue_fn(mq, req);
		cond_resched();
	} while (1);

	up(&mq->thread_sem);
	return 0;
}

/*
 * Prepare a MMC request. This just filters out odd stuff.
 */
static int mmc_prep_request(struct request_queue *q, struct request *req)
{
	struct mmc_queue *mq = q->queuedata;

	/*
	 * We only like normal block requests and discards.
	 */
	if (req->cmd_type != REQ_TYPE_FS && !(req->cmd_flags & REQ_DISCARD)) {
		blk_dump_rq_flags(req, "MMC bad request");
		return BLKPREP_KILL;
	}

	if (mq && (mmc_card_removed(mq->card) || mmc_access_rpmb(mq)))
		return BLKPREP_KILL;

	req->cmd_flags |= REQ_DONTPREP;

	return BLKPREP_OK;
}

/*
 * mmc_cmdq_cnt - count using queue
 * @host: MMC host associated with command queue
 *
 * Return using queue count
 */
int mmc_cmdq_cnt(struct mmc_host *host)
{
	if (!host->card->ext_csd.cmdq_mode_en)
		return 0;
	return atomic_read(&host->areq_cnt);
}

static void mmc_queue_softirq_done(struct request *req)
{
	struct request_queue	*q = req->q;
	struct mmc_queue	*mq = (struct mmc_queue *)q->queuedata;

	/* this function always called after request is successfully handled */
	if (mq->card->ext_csd.cmdq_mode_en)
		blk_end_request_all(req, 0);
}

/*
 * mmc_is_cmdq_empty - check queue is empty
 * @host: MMC host associated with command queue
 *
 * Return true if queue is empty
 */
int mmc_is_cmdq_empty(struct mmc_host *host)
{
	if (!host->card->ext_csd.cmdq_mode_en)
		return 1;
	return !atomic_read(&host->areq_cnt);
}

/*
 * mmc_is_cmdq_full - check queue is full
 * @host: MMC host associated with command queue
 * @rt: priority to check
 *
 * Return true if queue is full
 * We reserved several slots for realtime request
 * if priority is normal we check only normal slot
 * if priority is high (realtime) we check all slot including reserved slot
 */
int mmc_is_cmdq_full(struct mmc_host *host, int iosche_sync_exist)
{
	int cnt;
	if (!host->card->ext_csd.cmdq_mode_en)
		return 0;
	cnt = atomic_read(&host->areq_cnt);
	if (iosche_sync_exist) {
		if (cnt >= host->card->ext_csd.cmdq_depth)
			return 1;
		else
			return 0;
	} else {
		if (cnt >= host->card->ext_csd.cmdq_depth -
				EMMC_MIN_RT_CLASS_TAG_COUNT)
			return 1;
		else
			return 0;
	}
}

static struct mmc_queue_req *get_mqrq_free(struct mmc_queue *mq)
{
	struct mmc_queue_req *mqrq_prev = NULL;

	if (mq->card->host->caps2 & MMC_CAP2_ADMA3) {
		/* adma3_free_index initialized as 1
		 * adma3_free_index can be reset once transfer done
		 */
		if (mq->adma3_free_index < (ARRAY_SIZE(mq->mqrq_adma3))) {
			mqrq_prev = &mq->mqrq_adma3[mq->adma3_free_index];
			mq->adma3_free_index++;
		} else {
			pr_err("Error: OOM free index=%d mqrq_adma3 array limit=%ld line=%d %s\n",
				mq->adma3_free_index,
				ARRAY_SIZE(mq->mqrq_adma3),
				__LINE__, __func__);
		}
		return mqrq_prev;
	}
	return mq->mqrq_prev;
}

/*
 * ADMA3 allows issue of multiple data commands to the host.
 * Modified mmc_queue_thread routine to support ADMA3 as follows:
 * 1. The request is fetched from the block layer and
 *    issue_fn is called. In ADMA3 mode the host holds the
 *    data command till a flush call is made from the block
 *    layer.
 *    Once flush call is sent from the block layer the host
 *    will issue the list of accumulated data commands
 *    together for execution. Once all the data commands
 *    in the list are completed interrupt is generated
 *    and software does post processing of all the data commands.
 * 2. ADMA3 Flush function mmc_flush_queue_fn is called
 *    in the following conditions:
 *    a) NULL req is returned by blk_fetch_request with non-zero
 *       qdepth
 *    b) queue depth reaches limit MMC_QUEUE_FLUSH_ADMA3_DEPTH
 *       i.e. 16 deep queue.
 *    c) if MMC_REQ_SPECIAL_MASK is set e.g. for cache flush
 *       (CMD6) or erase commands (CMD35 through CMD37).
 */
static int mmc_adma3_queue_thread(void *d)
{
	struct mmc_queue *mq = d;
	struct request_queue *q = mq->queue;
	int issue;
	int qdepth = 0;
	int flush_status = 0;

	current->flags |= PF_MEMALLOC;

	down(&mq->thread_sem);
	do {
		struct request *req = NULL;
		struct mmc_queue_req *tmp;
		unsigned int cmd_flags = 0;

		spin_lock_irq(q->queue_lock);
		set_current_state(TASK_INTERRUPTIBLE);


		req = blk_fetch_request(q);
		if (req) {
			if (mq->is_queue_end_req)
				pr_err("%s %s line=%d new request found before finishing processing queue flush, qdepth=%d\n",
					mmc_hostname(mq->card->host),
					__func__, __LINE__, qdepth);
			qdepth++;
		} else if (!req) {
			if (qdepth) {
				if (mq->card->host->caps2 & MMC_CAP2_ADMA3)
					mq->is_queue_end_req = true;
			}
		}
		mq->mqrq_cur->req = req;
		spin_unlock_irq(q->queue_lock);
		if (req || mq->mqrq_prev->req)
			issue = 1;
		else
			issue = 0;

		if (issue) {
			set_current_state(TASK_RUNNING);
			cmd_flags = req ? req->cmd_flags : 0;
			if ((mq->is_queue_end_req) &&
				qdepth &&
				(!flush_status)) {
				/* flush adma3 queue when queue end
				 * reached */

				if (!mq->card->host->flush_adma3_started) {
					mq->card->host->flush_adma3_started =
						true;
					flush_status =
						mq->mmc_flush_queue_fn(
							mq);
					qdepth = 0;
				}
			}
			if (!flush_status) {
				/* we flush data commands before
				 * mmc_req_special commands
				 */
				if (cmd_flags & MMC_REQ_SPECIAL_MASK) {
					if (qdepth > 1) {
						/* flush data requests */
						mq->card->host->
							flush_adma3_started =
							true;
						flush_status = mq->
							mmc_flush_queue_fn(
							mq);
					}
					if ((qdepth > 1) &&
						(!flush_status))
						mq->issue_fn(mq, NULL);
					qdepth = 0;
				}
				mq->issue_fn(mq, req);
				if (cmd_flags & MMC_REQ_SPECIAL_MASK)
					/* The issue_fn should have
					 * finished special req hence
					 * reset adma3_free_index
					 */
					mq->adma3_free_index = INIT_ADMA3_FREE_INDEX;
			}
			if (mq->flags & MMC_QUEUE_NEW_REQUEST) {
				mq->flags &= ~MMC_QUEUE_NEW_REQUEST;
				continue; /* fetch again */
			}

			/*
			 * Current request becomes previous request
			 * and vice versa.
			 * In case of special requests, current request
			 * has been finished. Do not assign it to previous
			 * request.
			 */
			if (cmd_flags & MMC_REQ_SPECIAL_MASK) {
				mq->mqrq_cur->req = NULL;
			}
			if ((qdepth >=
				MMC_QUEUE_FLUSH_ADMA3_DEPTH) &&
				(!flush_status)) {
				mq->card->host->flush_adma3_started =
					true;
				flush_status = mq->mmc_flush_queue_fn(
					mq);
				if (!flush_status)
					mq->issue_fn(mq, NULL);
				qdepth -= MMC_QUEUE_FLUSH_ADMA3_DEPTH;
			}
			tmp = get_mqrq_free(mq);
			if (!tmp) {
				/* We are trying to flush more
				 * data commands than allowed
				 */
				pr_err("%s %s line=%d ... error seen hence ignore commands and continue ...\n",
					mmc_hostname(
					mq->card->host),
					__func__, __LINE__);
				continue;
			}
			tmp->brq.mrq.data = NULL;
			tmp->req = NULL;
			mq->mqrq_prev = mq->mqrq_cur;
			mq->mqrq_cur = tmp;
			if (mq->is_queue_end_req)
				mq->is_queue_end_req = false;
		} else {
			if (kthread_should_stop()) {
				set_current_state(TASK_RUNNING);
				break;
			}
			up(&mq->thread_sem);
			schedule();
			down(&mq->thread_sem);
		}
	} while (1);
	up(&mq->thread_sem);

	return 0;
}

static int mmc_queue_thread(void *d)
{
	struct mmc_queue *mq = d;
	struct request_queue *q = mq->queue;
	struct request_list *rl = &q->root_rl;
	int iosche_sync_exist, issue;

	if (mq->card->host->caps2 & MMC_CAP2_ADMA3)
		return mmc_adma3_queue_thread(d);

	current->flags |= PF_MEMALLOC;

	down(&mq->thread_sem);
	do {
		struct request *req = NULL;
		struct mmc_queue_req *tmp;
		unsigned int cmd_flags = 0;

		spin_lock_irq(q->queue_lock);
		set_current_state(TASK_INTERRUPTIBLE);

		/*
		 * The criteria for queue full is different depend on
		 * priority of request, because we reserved several slots
		 * for high priority request.
		 * So, we need to know the priority of next request first.
		 *
		 * TODO: blk_peek_request drains a request to dispatch queue.
		 *       so, next time if we peek, we just get same request.
		 *       we have to do something to see request without drain.
		 */

		if (rl->count[BLK_RW_SYNC] > 0)
			iosche_sync_exist = 1;
		else
			iosche_sync_exist = 0;

		if (mmc_is_cmdq_full(mq->card->host, iosche_sync_exist)) {
			req = NULL;
			goto fetch_done;
		}

		req = blk_fetch_request(q);
fetch_done:
		/*
		 * If queue mode is enabled,
		 * assign queue and set mqrq_cur in issue_fa.
		 */
		if (!mq->card->ext_csd.cmdq_mode_en)
			mq->mqrq_cur->req = req;
		spin_unlock_irq(q->queue_lock);
		/*
		 * If queue mode is enabled,
		 * mmcqd only enqueues request. (tasklet issues command)
		 * so, mmcqd is scheduled if current request is NULL.
		 */
		if (mq->card->ext_csd.cmdq_mode_en) {
			if (req)
				issue = 1;
			else
				issue = 0;
		} else {
			if (req || mq->mqrq_prev->req)
				issue = 1;
			else
				issue = 0;
		}

		if (issue) {
			set_current_state(TASK_RUNNING);
			cmd_flags = req ? req->cmd_flags : 0;
			mq->issue_fn(mq, req);
			if (mq->flags & MMC_QUEUE_NEW_REQUEST) {
				mq->flags &= ~MMC_QUEUE_NEW_REQUEST;
				continue; /* fetch again */
			}

			/*
			 * Current request becomes previous request
			 * and vice versa.
			 * In case of special requests, current request
			 * has been finished. Do not assign it to previous
			 * request.
			 */
			if (!mq->card->ext_csd.cmdq_mode_en) {
				if (cmd_flags & MMC_REQ_SPECIAL_MASK)
					mq->mqrq_cur->req = NULL;

				mq->mqrq_prev->brq.mrq.data = NULL;
				mq->mqrq_prev->req = NULL;
				tmp = mq->mqrq_prev;
				mq->mqrq_prev = mq->mqrq_cur;
				mq->mqrq_cur = tmp;
			}
		} else {
			if (kthread_should_stop()) {
				set_current_state(TASK_RUNNING);
				break;
			}
			up(&mq->thread_sem);
			schedule();
			down(&mq->thread_sem);
		}
	} while (1);
	up(&mq->thread_sem);

	return 0;
}

static inline bool mmc_cmdq_should_pull_reqs(struct mmc_host *host,
			struct mmc_cmdq_context_info *ctx)
{
	int blocking_dcmd = 1;

	if (mmc_cmdq_support_qbr(host))
		blocking_dcmd = 0;

	spin_lock_bh(&ctx->cmdq_ctx_lock);
	if ((blocking_dcmd && ctx->active_dcmd) || ctx->rpmb_in_wait ||
			ctx->active_ncqcmd) {
		if ((ctx->curr_state != CMDQ_STATE_HALT) ||
			(ctx->curr_state != CMDQ_STATE_ERR)) {
			pr_debug("%s: skip pulling reqs: dcmd: %d rpmb: %d ncq: %d state: %d\n",
				 mmc_hostname(host), ctx->active_dcmd,
				 ctx->rpmb_in_wait, ctx->active_ncqcmd,
				 ctx->curr_state);
		}
		spin_unlock_bh(&ctx->cmdq_ctx_lock);
		return false;
	} else {
		spin_unlock_bh(&ctx->cmdq_ctx_lock);
		return true;
	}
}

static void mmc_cmdq_dispatch_req(struct request_queue *q)
{
	struct mmc_queue *mq = q->queuedata;

	wake_up_process(mq->cq_thread);
}

/*
 * Generic MMC request handler.  This is called for any queue on a
 * particular host.  When the host is not busy, we look for a request
 * on any queue on this host, and attempt to issue it.  This may
 * not be the queue we were asked to process.
 */
static void mmc_request_fn(struct request_queue *q)
{
	struct mmc_queue *mq = q->queuedata;
	struct request *req;
	unsigned long flags;
	struct mmc_context_info *cntx;

	if (!mq) {
		while ((req = blk_fetch_request(q)) != NULL) {
			req->cmd_flags |= REQ_QUIET;
			__blk_end_request_all(req, -EIO);
		}
		return;
	}

	cntx = &mq->card->host->context_info;

	/*
	 * mmcqd sleeps when current request is NULL. (mqrq_cur is not updated)
	 * so, we always wake up mmcqd when request is inserted.
	 *
	 * TODO: wake up mmcqd only when mmcqd sleeps.
	 */
	if (mq->card->ext_csd.cmdq_mode_en)
		wake_up_process(mq->thread);
	else {
		if (!mq->mqrq_cur->req && mq->mqrq_prev->req) {
			/*
			 * New MMC request arrived when MMC thread may be
			 * blocked on the previous request to be complete
			 * with no current request fetched
			 */
			spin_lock_irqsave(&cntx->lock, flags);
			if (cntx->is_waiting_last_req) {
				cntx->is_new_req = true;
				wake_up_interruptible(&cntx->wait);
			}
			spin_unlock_irqrestore(&cntx->lock, flags);
		} else if (!mq->mqrq_cur->req && !mq->mqrq_prev->req)
			wake_up_process(mq->thread);
	}
}

static struct scatterlist *mmc_alloc_sg(int sg_len, int *err)
{
	struct scatterlist *sg;

	sg = kmalloc(sizeof(struct scatterlist)*sg_len, GFP_KERNEL);
	if (!sg)
		*err = -ENOMEM;
	else {
		*err = 0;
		sg_init_table(sg, sg_len);
	}

	return sg;
}

static void mmc_queue_setup_discard(struct request_queue *q,
				    struct mmc_card *card)
{
	unsigned max_discard;

	max_discard = mmc_calc_max_discard(card);
	if (!max_discard)
		return;

	queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, q);
	q->limits.max_discard_sectors = max_discard;
	if (card->erased_byte == 0 && !mmc_can_discard(card))
		q->limits.discard_zeroes_data = 1;
	q->limits.discard_granularity = card->pref_erase << 9;
	/* granularity must not be greater than max. discard */
	if (card->pref_erase > max_discard)
		q->limits.discard_granularity = 0;
	if (mmc_can_secure_erase_trim(card))
		queue_flag_set_unlocked(QUEUE_FLAG_SECDISCARD, q);
}

/**
 * mmc_blk_cmdq_setup_queue
 * @mq: mmc queue
 * @card: card to attach to this queue
 *
 * Setup queue for CMDQ supporting MMC card
 */
void mmc_blk_cmdq_setup_queue(struct mmc_queue *mq, struct mmc_card *card)
{
	u64 limit = BLK_BOUNCE_HIGH;
	struct mmc_host *host = card->host;

	queue_flag_set_unlocked(QUEUE_FLAG_NONROT, mq->queue);
	queue_flag_clear_unlocked(QUEUE_FLAG_ADD_RANDOM, mq->queue);
	if (mmc_can_erase(card))
		mmc_queue_setup_discard(mq->queue, card);

	blk_queue_bounce_limit(mq->queue, limit);
	blk_queue_max_hw_sectors(mq->queue, min(host->max_blk_count,
					host->max_req_size / 512));
	blk_queue_max_segment_size(mq->queue, host->max_seg_size);
	blk_queue_max_segments(mq->queue, host->max_segs);
}

/**
 * mmc_init_queue - initialise a queue structure.
 * @mq: mmc queue
 * @card: mmc card to attach this queue
 * @lock: queue lock
 * @subname: partition subname
 *
 * Initialise a MMC card request queue.
 */
int mmc_init_queue(struct mmc_queue *mq, struct mmc_card *card,
		   spinlock_t *lock, const char *subname, int area_type)
{
	struct mmc_host *host = card->host;
	u64 limit = BLK_BOUNCE_HIGH;
	int i, ret;
	struct mmc_queue_req *mqrq_cur = &mq->mqrq[0];
	struct mmc_queue_req *mqrq_prev = &mq->mqrq[1];

	if (mmc_dev(host)->dma_mask && *mmc_dev(host)->dma_mask)
		limit = (u64)dma_max_pfn(mmc_dev(host)) << PAGE_SHIFT;

	mq->card = card;
	if ((card->host->caps2 & MMC_CAP2_HW_CQ) &&
		card->ext_csd.cmdq_support &&
		(area_type == MMC_BLK_DATA_AREA_MAIN)) {
		mq->queue = blk_init_queue(mmc_cmdq_dispatch_req, lock);
		if (!mq->queue)
			return -ENOMEM;
		mq->queue->queuedata = mq;
		blk_queue_prep_rq(mq->queue, mmc_prep_request);
		mmc_blk_cmdq_setup_queue(mq, card);
		ret = mmc_cmdq_init(mq, card);
		if (ret) {
			pr_err("%s: %d: cmdq: unable to set-up\n",
				       mmc_hostname(card->host), ret);
			blk_cleanup_queue(mq->queue);
		} else {
			sema_init(&mq->thread_sem, 1);
			mq->cq_thread = kthread_run(mmc_cmdq_thread, mq,
				"mmc_cmdq_d/%d%s", host->index,
				subname ? subname : "");
			if (IS_ERR(mq->cq_thread)) {
				ret = PTR_ERR(mq->cq_thread);
				pr_err("%s: %d: cmdq: unable to create thread\n",
						mmc_hostname(card->host), ret);
			}
			return ret;
		}
	}

	mq->queue = blk_init_queue(mmc_request_fn, lock);
	if (!mq->queue)
		return -ENOMEM;

	if (card->ext_csd.cmdq_support)
		for (i = 0; i < card->ext_csd.cmdq_depth; i++)
			atomic_set(&mq->mqrq[i].index, 0);
	spin_lock_init(&mq->cmdq_lock);
	if (card->host->caps2 & MMC_CAP2_ADMA3) {
		mq->adma3_free_index = INIT_ADMA3_FREE_INDEX;
		mqrq_cur = &mq->mqrq_adma3[0];
		mqrq_prev = &mq->mqrq_adma3[1];
	}
	mq->mqrq_cur = mqrq_cur;
	mq->mqrq_prev = mqrq_prev;
	mq->queue->queuedata = mq;

	blk_queue_prep_rq(mq->queue, mmc_prep_request);
	queue_flag_set_unlocked(QUEUE_FLAG_NONROT, mq->queue);
	queue_flag_clear_unlocked(QUEUE_FLAG_ADD_RANDOM, mq->queue);
	if (mmc_can_erase(card))
		mmc_queue_setup_discard(mq->queue, card);
	blk_queue_softirq_done(mq->queue, mmc_queue_softirq_done);

#ifdef CONFIG_MMC_BLOCK_BOUNCE
	if (host->max_segs == 1) {
		unsigned int bouncesz;

		bouncesz = MMC_QUEUE_BOUNCESZ;

		if (bouncesz > host->max_req_size)
			bouncesz = host->max_req_size;
		if (bouncesz > host->max_seg_size)
			bouncesz = host->max_seg_size;
		if (bouncesz > (host->max_blk_count * 512))
			bouncesz = host->max_blk_count * 512;

		if (bouncesz > 512) {
			if (card->ext_csd.cmdq_support) {
				for (i = 0; i < card->ext_csd.cmdq_depth; i++) {
					mq->mqrq[i].bounce_buf =
						kmalloc(bouncesz, GFP_KERNEL);
					if (!mq->mqrq[i].bounce_buf)
						pr_warn("%s: unable to "
							"allocate bounce "
							"cur buffer [%d]\n",
							mmc_card_name(card), i);
				}
			} else {
				mqrq_cur->bounce_buf = kmalloc(bouncesz,
					GFP_KERNEL);
				if (!mqrq_cur->bounce_buf) {
					pr_warn("%s: unable to allocate bounce"
						"cur buffer\n",
						mmc_card_name(card));
				}
				mqrq_prev->bounce_buf = kmalloc(bouncesz,
					GFP_KERNEL);
				if (!mqrq_prev->bounce_buf) {
					pr_warn("%s: unable to allocate bounce"
						      "prev buffer\n",
							mmc_card_name(card));
					kfree(mqrq_cur->bounce_buf);
					mqrq_cur->bounce_buf = NULL;
				}
			}
		}

		if (mqrq_cur->bounce_buf && mqrq_prev->bounce_buf) {
			blk_queue_bounce_limit(mq->queue, BLK_BOUNCE_ANY);
			blk_queue_max_hw_sectors(mq->queue, bouncesz / 512);
			blk_queue_max_segments(mq->queue, bouncesz / 512);
			blk_queue_max_segment_size(mq->queue, bouncesz);

			if (card->ext_csd.cmdq_support) {
				for (i = 0; i < card->ext_csd.cmdq_depth; i++) {
					mq->mqrq[i].sg = mmc_alloc_sg(1, &ret);
					if (ret)
						goto cleanup_queue;

					mq->mqrq[i].bounce_sg =
						mmc_alloc_sg(bouncesz / 512, &ret);
					if (ret)
						goto cleanup_queue;
				}
			} else {
				mqrq_cur->sg = mmc_alloc_sg(1, &ret);
				if (ret)
					goto cleanup_queue;

				mqrq_cur->bounce_sg =
					mmc_alloc_sg(bouncesz / 512, &ret);
				if (ret)
					goto cleanup_queue;

				mqrq_prev->sg = mmc_alloc_sg(1, &ret);
				if (ret)
					goto cleanup_queue;

				mqrq_prev->bounce_sg =
					mmc_alloc_sg(bouncesz / 512, &ret);
				if (ret)
					goto cleanup_queue;

			}
		}
	}
#endif

	if (!mqrq_cur->bounce_buf && !mqrq_prev->bounce_buf) {
		blk_queue_bounce_limit(mq->queue, limit);
		blk_queue_max_hw_sectors(mq->queue,
			min(host->max_blk_count, host->max_req_size / 512));
		blk_queue_max_segments(mq->queue, host->max_segs);
		blk_queue_max_segment_size(mq->queue, host->max_seg_size);

		if (card->ext_csd.cmdq_support) {
			for (i = 0; i < card->ext_csd.cmdq_depth; i++) {
				mq->mqrq[i].sg = mmc_alloc_sg(host->max_segs,
						&ret);
				if (ret)
					goto cleanup_queue;
			}
		} else if (mq->card->host->caps2 & MMC_CAP2_ADMA3) {
			/* used to support ADMA3 multiple case for SD card
			 * or if cmdq_support is disabled through DT on
			 * CMDQ capable EMMC parts or not CMDQ capable EMMC
			 */
			for (i = 0; i < ARRAY_SIZE(mq->mqrq_adma3); i++) {
				mq->mqrq_adma3[i].sg = mmc_alloc_sg(
					host->max_segs, &ret);
				if (ret)
					goto cleanup_queue;
			}
		} else {
			mqrq_cur->sg = mmc_alloc_sg(host->max_segs, &ret);
			if (ret)
				goto cleanup_queue;

			mqrq_prev->sg = mmc_alloc_sg(host->max_segs, &ret);
			if (ret)
				goto cleanup_queue;

		}
	}

	sema_init(&mq->thread_sem, 1);

	mq->thread = kthread_run(mmc_queue_thread, mq, "mmcqd/%d%s",
		host->index, subname ? subname : "");

	if (IS_ERR(mq->thread)) {
		ret = PTR_ERR(mq->thread);
		goto free_bounce_sg;
	}

	return 0;
 free_bounce_sg:
	if (card->ext_csd.cmdq_support) {
		for (i = 0; i < card->ext_csd.cmdq_depth; i++) {
			kfree(mq->mqrq[i].bounce_sg);
			mq->mqrq[i].bounce_sg = NULL;
		}
	} else {
		kfree(mqrq_cur->bounce_sg);
		mqrq_cur->bounce_sg = NULL;
		kfree(mqrq_prev->bounce_sg);
		mqrq_prev->bounce_sg = NULL;
	}

 cleanup_queue:
	if (card->ext_csd.cmdq_support) {
		for (i = 0; i < card->ext_csd.cmdq_depth; i++) {
			kfree(mq->mqrq[i].sg);
			mq->mqrq[i].sg = NULL;
			kfree(mq->mqrq[i].bounce_buf);
			mq->mqrq[i].bounce_buf = NULL;
		}
	} else {
		kfree(mqrq_cur->sg);
		mqrq_cur->sg = NULL;
		kfree(mqrq_cur->bounce_buf);
		mqrq_cur->bounce_buf = NULL;

		kfree(mqrq_prev->sg);
		mqrq_prev->sg = NULL;
		kfree(mqrq_prev->bounce_buf);
		mqrq_prev->bounce_buf = NULL;

	}

	blk_cleanup_queue(mq->queue);
	return ret;
}

void mmc_cleanup_queue(struct mmc_queue *mq)
{
	struct request_queue *q = mq->queue;
	unsigned long flags;
	struct mmc_queue_req *mqrq_cur = mq->mqrq_cur;
	struct mmc_queue_req *mqrq_prev = mq->mqrq_prev;

	/* Make sure the queue isn't suspended, as that will deadlock */
	mmc_queue_resume(mq);

	/* Then terminate our worker thread */
	kthread_stop(mq->thread);

	/* Empty the queue */
	spin_lock_irqsave(q->queue_lock, flags);
	q->queuedata = NULL;
	blk_start_queue(q);
	spin_unlock_irqrestore(q->queue_lock, flags);

	kfree(mqrq_cur->bounce_sg);
	mqrq_cur->bounce_sg = NULL;

	kfree(mqrq_cur->sg);
	mqrq_cur->sg = NULL;

	kfree(mqrq_cur->bounce_buf);
	mqrq_cur->bounce_buf = NULL;

	kfree(mqrq_prev->bounce_sg);
	mqrq_prev->bounce_sg = NULL;

	kfree(mqrq_prev->sg);
	mqrq_prev->sg = NULL;

	kfree(mqrq_prev->bounce_buf);
	mqrq_prev->bounce_buf = NULL;

	mq->card = NULL;
}
EXPORT_SYMBOL(mmc_cleanup_queue);

int mmc_packed_init(struct mmc_queue *mq, struct mmc_card *card)
{
	struct mmc_queue_req *mqrq_cur = &mq->mqrq[0];
	struct mmc_queue_req *mqrq_prev = &mq->mqrq[1];
	int ret = 0;


	mqrq_cur->packed = kzalloc(sizeof(struct mmc_packed), GFP_KERNEL);
	if (!mqrq_cur->packed) {
		pr_warn("%s: unable to allocate packed cmd for mqrq_cur\n",
			mmc_card_name(card));
		ret = -ENOMEM;
		goto out;
	}

	mqrq_prev->packed = kzalloc(sizeof(struct mmc_packed), GFP_KERNEL);
	if (!mqrq_prev->packed) {
		pr_warn("%s: unable to allocate packed cmd for mqrq_prev\n",
			mmc_card_name(card));
		kfree(mqrq_cur->packed);
		mqrq_cur->packed = NULL;
		ret = -ENOMEM;
		goto out;
	}

	INIT_LIST_HEAD(&mqrq_cur->packed->list);
	INIT_LIST_HEAD(&mqrq_prev->packed->list);

out:
	return ret;
}

void mmc_packed_clean(struct mmc_queue *mq)
{
	struct mmc_queue_req *mqrq_cur = &mq->mqrq[0];
	struct mmc_queue_req *mqrq_prev = &mq->mqrq[1];

	kfree(mqrq_cur->packed);
	mqrq_cur->packed = NULL;
	kfree(mqrq_prev->packed);
	mqrq_prev->packed = NULL;
}

static void mmc_cmdq_softirq_done(struct request *rq)
{
	struct mmc_queue *mq = rq->q->queuedata;

	mq->cmdq_complete_fn(rq);
}

int mmc_cmdq_init(struct mmc_queue *mq, struct mmc_card *card)
{
	int i, ret = 0;
	/* one slot is reserved for dcmd requests */
	int q_depth = card->ext_csd.cmdq_depth - 1;

	card->cmdq_init = false;
	spin_lock_init(&card->host->cmdq_ctx.cmdq_ctx_lock);
	sema_init(&card->host->cmdq_ctx.thread_sem, 1);

	mq->mqrq_cmdq = kzalloc(
		sizeof(struct mmc_queue_req) * q_depth, GFP_KERNEL);
	if (!mq->mqrq_cmdq) {
		pr_warn("%s: unable to allocate mqrq's for q_depth %d\n",
			mmc_card_name(card), q_depth);
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < q_depth; i++) {
		/* TODO: reduce the sg allocation by delaying them */
		mq->mqrq_cmdq[i].sg = mmc_alloc_sg(card->host->max_segs, &ret);
		if (ret) {
			pr_warn("%s: unable to allocate cmdq sg of size %d\n",
				mmc_card_name(card), card->host->max_segs);
			goto free_mqrq_sg;
		}
	}

	ret = blk_queue_init_tags(mq->queue, q_depth, NULL);
	if (ret) {
		pr_warn("%s: unable to allocate cmdq tags %d\n",
				mmc_card_name(card), q_depth);
		goto free_mqrq_sg;
	}

	blk_queue_softirq_done(mq->queue, mmc_cmdq_softirq_done);
	card->cmdq_init = true;
	goto out;

free_mqrq_sg:
	for (i = 0; i < q_depth; i++)
		kfree(mq->mqrq_cmdq[i].sg);
	kfree(mq->mqrq_cmdq);
	mq->mqrq_cmdq = NULL;
out:
	return ret;
}

void mmc_cmdq_clean(struct mmc_queue *mq, struct mmc_card *card)
{
	int i;
	int q_depth = card->ext_csd.cmdq_depth - 1;

	blk_free_tags(mq->queue->queue_tags);
	mq->queue->queue_tags = NULL;
	blk_queue_free_tags(mq->queue);

	for (i = 0; i < q_depth; i++)
		kfree(mq->mqrq_cmdq[i].sg);
	kfree(mq->mqrq_cmdq);
	mq->mqrq_cmdq = NULL;
}

/**
 * mmc_queue_suspend - suspend a MMC request queue
 * @mq: MMC queue to suspend
 *
 * Stop the block request queue, and wait for our thread to
 * complete any outstanding requests.  This ensures that we
 * won't suspend while a request is being processed.
 */
void mmc_queue_suspend(struct mmc_queue *mq)
{
	struct request_queue *q = mq->queue;
	unsigned long flags;

	if (!(mq->flags & MMC_QUEUE_SUSPENDED)) {
		mq->flags |= MMC_QUEUE_SUSPENDED;

		spin_lock_irqsave(q->queue_lock, flags);
		blk_stop_queue(q);
		spin_unlock_irqrestore(q->queue_lock, flags);

		down(&mq->thread_sem);
	}
}

/**
 * mmc_queue_resume - resume a previously suspended MMC request queue
 * @mq: MMC queue to resume
 */
void mmc_queue_resume(struct mmc_queue *mq)
{
	struct request_queue *q = mq->queue;
	unsigned long flags;

	if (mq->flags & MMC_QUEUE_SUSPENDED) {
		mq->flags &= ~MMC_QUEUE_SUSPENDED;

		up(&mq->thread_sem);

		spin_lock_irqsave(q->queue_lock, flags);
		blk_start_queue(q);
		spin_unlock_irqrestore(q->queue_lock, flags);
	}
}

static unsigned int mmc_queue_packed_map_sg(struct mmc_queue *mq,
					    struct mmc_packed *packed,
					    struct scatterlist *sg,
					    enum mmc_packed_type cmd_type)
{
	struct scatterlist *__sg = sg;
	unsigned int sg_len = 0;
	struct request *req;

	if (mmc_packed_wr(cmd_type)) {
		unsigned int hdr_sz = mmc_large_sector(mq->card) ? 4096 : 512;
		unsigned int max_seg_sz = queue_max_segment_size(mq->queue);
		unsigned int len, remain, offset = 0;
		u8 *buf = (u8 *)packed->cmd_hdr;

		remain = hdr_sz;
		do {
			len = min(remain, max_seg_sz);
			sg_set_buf(__sg, buf + offset, len);
			offset += len;
			remain -= len;
			(__sg++)->page_link &= ~0x02;
			sg_len++;
		} while (remain);
	}

	list_for_each_entry(req, &packed->list, queuelist) {
		sg_len += blk_rq_map_sg(mq->queue, req, __sg);
		__sg = sg + (sg_len - 1);
		(__sg++)->page_link &= ~0x02;
	}
	sg_mark_end(sg + (sg_len - 1));
	return sg_len;
}

/*
 * Prepare the sg list(s) to be handed of to the host driver
 */
unsigned int mmc_queue_map_sg(struct mmc_queue *mq, struct mmc_queue_req *mqrq)
{
	unsigned int sg_len;
	size_t buflen;
	struct scatterlist *sg;
	enum mmc_packed_type cmd_type;
	int i;

	cmd_type = mqrq->cmd_type;

	if (!mqrq->bounce_buf) {
		if (mmc_packed_cmd(cmd_type))
			return mmc_queue_packed_map_sg(mq, mqrq->packed,
						       mqrq->sg, cmd_type);
		else
			return blk_rq_map_sg(mq->queue, mqrq->req, mqrq->sg);
	}

	BUG_ON(!mqrq->bounce_sg);

	if (mmc_packed_cmd(cmd_type))
		sg_len = mmc_queue_packed_map_sg(mq, mqrq->packed,
						 mqrq->bounce_sg, cmd_type);
	else
		sg_len = blk_rq_map_sg(mq->queue, mqrq->req, mqrq->bounce_sg);

	mqrq->bounce_sg_len = sg_len;

	buflen = 0;
	for_each_sg(mqrq->bounce_sg, sg, sg_len, i)
		buflen += sg->length;

	sg_init_one(mqrq->sg, mqrq->bounce_buf, buflen);

	return 1;
}

/*
 * If writing, bounce the data to the buffer before the request
 * is sent to the host driver
 */
void mmc_queue_bounce_pre(struct mmc_queue_req *mqrq)
{
	if (!mqrq->bounce_buf)
		return;

	if (rq_data_dir(mqrq->req) != WRITE)
		return;

	sg_copy_to_buffer(mqrq->bounce_sg, mqrq->bounce_sg_len,
		mqrq->bounce_buf, mqrq->sg[0].length);
}

/*
 * If reading, bounce the data from the buffer after the request
 * has been handled by the host driver
 */
void mmc_queue_bounce_post(struct mmc_queue_req *mqrq)
{
	if (!mqrq->bounce_buf)
		return;

	if (rq_data_dir(mqrq->req) != READ)
		return;

	sg_copy_from_buffer(mqrq->bounce_sg, mqrq->bounce_sg_len,
		mqrq->bounce_buf, mqrq->sg[0].length);
}
