/*
 * drivers/video/tegra/host/nvhost_job.c
 *
 * Tegra Graphics Host Job
 *
 * Copyright (c) 2010-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/slab.h>
#include <linux/kref.h>
#include <linux/err.h>
#include <linux/vmalloc.h>
#include <linux/sort.h>
#include <linux/scatterlist.h>
#include <trace/events/nvhost.h>
#include "nvhost_channel.h"
#include "nvhost_vm.h"
#include "nvhost_job.h"
#include "nvhost_syncpt.h"
#include "dev.h"
#include "chip_support.h"

/* Magic to use to fill freed handle slots */
#define BAD_MAGIC 0xdeadbeef

static size_t job_size(u32 num_cmdbufs, u32 num_relocs, u32 num_waitchks,
			u32 num_syncpts)
{
	u64 num_unpins = (u64)num_cmdbufs + (u64)num_relocs;
	u64 total;

	total = sizeof(struct nvhost_job)
			+ (u64)num_relocs * sizeof(struct nvhost_reloc)
			+ (u64)num_relocs * sizeof(struct nvhost_reloc_shift)
			+ (u64)num_relocs * sizeof(struct nvhost_reloc_type)
			+ num_unpins * sizeof(struct nvhost_job_unpin)
			+ (u64)num_waitchks * sizeof(struct nvhost_waitchk)
			+ (u64)num_cmdbufs * sizeof(struct nvhost_job_gather)
			+ num_unpins * sizeof(dma_addr_t)
			+ num_unpins * sizeof(struct nvhost_pinid)
			+ (u64)num_syncpts * sizeof(struct nvhost_job_syncpt);

	if (total > UINT_MAX)
		return 0;

	return (size_t)total;
}


static void init_fields(struct nvhost_job *job,
		u32 num_cmdbufs, u32 num_relocs, u32 num_waitchks,
		u32 num_syncpts)
{
	int num_unpins = num_cmdbufs + num_relocs;
	void *mem = job;

	/* First init state to zero */

	/*
	 * Redistribute memory to the structs.
	 * Overflows and negative conditions have
	 * already been checked in job_alloc().
	 */
	mem += sizeof(struct nvhost_job);
	job->relocarray = num_relocs ? mem : NULL;
	mem += num_relocs * sizeof(struct nvhost_reloc);
	job->relocshiftarray = num_relocs ? mem : NULL;
	mem += num_relocs * sizeof(struct nvhost_reloc_shift);
	job->reloctypearray = num_relocs ? mem : NULL;
	mem += num_relocs * sizeof(struct nvhost_reloc_type);
	job->unpins = num_unpins ? mem : NULL;
	mem += num_unpins * sizeof(struct nvhost_job_unpin);
	job->waitchk = num_waitchks ? mem : NULL;
	mem += num_waitchks * sizeof(struct nvhost_waitchk);
	job->gathers = num_cmdbufs ? mem : NULL;
	mem += num_cmdbufs * sizeof(struct nvhost_job_gather);
	job->addr_phys = num_unpins ? mem : NULL;
	mem += num_unpins * sizeof(dma_addr_t);
	job->pin_ids = num_unpins ? mem : NULL;
	mem += num_unpins * sizeof(struct nvhost_pinid);
	job->sp = num_syncpts ? mem : NULL;

	job->reloc_addr_phys = job->addr_phys;
	job->gather_addr_phys = &job->addr_phys[num_relocs];
}

struct nvhost_job *nvhost_job_alloc(struct nvhost_channel *ch,
		int num_cmdbufs, int num_relocs, int num_waitchks,
		int num_syncpts)
{
	struct nvhost_job *job = NULL;
	size_t size =
		job_size(num_cmdbufs, num_relocs, num_waitchks, num_syncpts);

	if(!size)
		return NULL;
	if(size <= PAGE_SIZE)
		job = kzalloc(size, GFP_KERNEL);
	else
		job = vzalloc(size);
	if (!job)
		return NULL;

	kref_init(&job->ref);
	job->ch = ch;
	job->size = size;

	init_fields(job, num_cmdbufs, num_relocs, num_waitchks, num_syncpts);

	return job;
}
EXPORT_SYMBOL(nvhost_job_alloc);

void nvhost_job_get(struct nvhost_job *job)
{
	kref_get(&job->ref);
}

static void job_free(struct kref *ref)
{
	struct nvhost_job *job = container_of(ref, struct nvhost_job, ref);

	if (job->error_notifier_ref)
		dma_buf_put(job->error_notifier_ref);
	if (job->size <= PAGE_SIZE)
		kfree(job);
	else
		vfree(job);
}

void nvhost_job_put(struct nvhost_job *job)
{
	kref_put(&job->ref, job_free);
}
EXPORT_SYMBOL(nvhost_job_put);

int nvhost_job_add_client_gather_address(struct nvhost_job *job,
		u32 num_words, u32 class_id, dma_addr_t gather_address)
{
	nvhost_job_add_gather(job, 0, num_words, 0, class_id, 0);

	job->gathers[0].mem_base = gather_address;

	return 0;
}
EXPORT_SYMBOL(nvhost_job_add_client_gather_address);

void nvhost_job_add_gather(struct nvhost_job *job,
		u32 mem_id, u32 words, u32 offset, u32 class_id, int pre_fence)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(job->ch->dev);
	struct nvhost_job_gather *cur_gather =
			&job->gathers[job->num_gathers];

	cur_gather->words = words;
	cur_gather->mem_id = mem_id;
	cur_gather->offset = offset;
	cur_gather->class_id = class_id ? class_id : pdata->class;
	cur_gather->pre_fence = pre_fence;
	job->num_gathers += 1;
}

void nvhost_job_set_notifier(struct nvhost_job *job, u32 error)
{
	struct nvhost_notification *error_notifier;
	struct timespec time_data;
	void *va;
	u64 nsec;

	if (!job->error_notifier_ref)
		return;

	/* map handle and clear error notifier struct */
	va = dma_buf_vmap(job->error_notifier_ref);
	if (!va) {
		dma_buf_put(job->error_notifier_ref);
		dev_err(&job->ch->dev->dev, "Cannot map notifier handle\n");
		return;
	}

	error_notifier = va + job->error_notifier_offset;

	getnstimeofday(&time_data);
	nsec = ((u64)time_data.tv_sec) * 1000000000u +
		(u64)time_data.tv_nsec;
	error_notifier->time_stamp.nanoseconds[0] =
		(u32)nsec;
	error_notifier->time_stamp.nanoseconds[1] =
		(u32)(nsec >> 32);
	error_notifier->info32 = error;
	error_notifier->status = 0xffff;
	dev_err(&job->ch->dev->dev, "error notifier set to %d\n", error);

	dma_buf_vunmap(job->error_notifier_ref, va);
}

/*
 * Check driver supplied waitchk structs for syncpt thresholds
 * that have already been satisfied and NULL the comparison (to
 * avoid a wrap condition in the HW).
 */
static int do_waitchks(struct nvhost_job *job, struct nvhost_syncpt *sp,
		u32 patch_mem, struct dma_buf *buf)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(job->ch->dev);
	int i;

	/* compare syncpt vs wait threshold */
	for (i = 0; i < job->num_waitchk; i++) {
		struct nvhost_waitchk *wait = &job->waitchk[i];

		/* validate syncpt id */
		if (!nvhost_syncpt_is_valid_hw_pt(sp, wait->syncpt_id))
			continue;

		if (!wait->mem)
			continue;

		/* skip all other gathers */
		if (patch_mem != wait->mem)
			continue;

		trace_nvhost_syncpt_wait_check(wait->mem, wait->offset,
				wait->syncpt_id, wait->thresh,
				nvhost_syncpt_read(sp, wait->syncpt_id));
		if (nvhost_syncpt_is_expired(sp,
		    wait->syncpt_id, wait->thresh) ||
		     pdata->resource_policy == RESOURCE_PER_CHANNEL_INSTANCE) {
			void *patch_addr = NULL;

			/*
			 * NULL an already satisfied WAIT_SYNCPT host method,
			 * by patching its args in the command stream. The
			 * method data is changed to reference a reserved
			 * (never given out or incr) graphics host syncpt
			 * with a matching threshold value of 0, so is
			 * guaranteed to be popped by the host HW.
			 */
			dev_dbg(&syncpt_to_dev(sp)->dev->dev,
			    "drop WAIT id %d (%s) thresh 0x%x, min 0x%x\n",
			    wait->syncpt_id,
			    syncpt_op().name(sp, wait->syncpt_id),
			    wait->thresh,
			    nvhost_syncpt_read_min(sp, wait->syncpt_id));

			/* patch the wait */
			patch_addr = dma_buf_kmap(buf,
					wait->offset >> PAGE_SHIFT);
			if (patch_addr) {
				nvhost_syncpt_patch_wait(sp,
					(patch_addr +
					 (wait->offset & ~PAGE_MASK)));
				dma_buf_kunmap(buf,
						wait->offset >> PAGE_SHIFT,
						patch_addr);
			} else {
				pr_err("Couldn't map cmdbuf for wait check\n");
			}
		}

		wait->mem = 0;
	}
	return 0;
}

static int id_cmp(const void *_id1, const void *_id2)
{
	u32 id1 = ((struct nvhost_pinid *)_id1)->id;
	u32 id2 = ((struct nvhost_pinid *)_id2)->id;

	if (id1 < id2)
		return -1;
	if (id1 > id2)
		return 1;

	return 0;
}

static int pin_array_ids(struct platform_device *dev,
		struct nvhost_pinid *ids,
		dma_addr_t *phys_addr,
		u32 count,
		struct nvhost_job_unpin *unpin_data)
{
	int i, pin_count = 0;
	struct sg_table *sgt;
	struct dma_buf *buf;
	struct dma_buf_attachment *attach;
	u32 prev_id = 0;
	dma_addr_t prev_addr = 0;

	for (i = 0; i < count; i++)
		ids[i].index = i;

	sort(ids, count, sizeof(*ids), id_cmp, NULL);

	for (i = 0; i < count; i++) {
		if (ids[i].id == prev_id) {
			phys_addr[ids[i].index] = prev_addr;
			continue;
		}

		buf = dma_buf_get(ids[i].id);
		if (IS_ERR(buf))
			return -EINVAL;

		attach = dma_buf_attach(buf, &dev->dev);
		if (IS_ERR(attach))
			return PTR_ERR(attach);

		sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
		if (IS_ERR(sgt))
			return PTR_ERR(sgt);

		if (!sg_dma_address(sgt->sgl))
			sg_dma_address(sgt->sgl) = sg_phys(sgt->sgl);

		phys_addr[ids[i].index] = sg_dma_address(sgt->sgl);
		unpin_data[pin_count].buf = buf;
		unpin_data[pin_count].attach = attach;
		unpin_data[pin_count++].sgt = sgt;

		prev_id = ids[i].id;
		prev_addr = phys_addr[ids[i].index];
	}
	return pin_count;
}

static int pin_job_mem(struct nvhost_job *job)
{
	int i;
	int count = 0;
	int result;

	for (i = 0; i < job->num_relocs; i++) {
		struct nvhost_reloc *reloc = &job->relocarray[i];

		job->pin_ids[count].id = reloc->target;
		count++;
	}

	/* validate array and pin unique ids, get refs for reloc unpinning */
	result = pin_array_ids(job->ch->vm->pdev,
		job->pin_ids, job->addr_phys,
		job->num_relocs,
		job->unpins);
	if (result < 0)
		return result;

	job->num_unpins = result;

	for (i = 0; i < job->num_gathers; i++) {
		struct nvhost_job_gather *g = &job->gathers[i];

		job->pin_ids[count].id = g->mem_id;
		count++;
	}

	/* validate array and pin unique ids, get refs for gather unpinning */
	result = pin_array_ids(nvhost_get_host(job->ch->dev)->dev,
		&job->pin_ids[job->num_relocs],
		&job->addr_phys[job->num_relocs],
		job->num_gathers,
		&job->unpins[job->num_unpins]);
	if (result < 0) {
		nvhost_job_unpin(job);
		return result;
	}

	job->num_unpins += result;

	return result;
}

static int do_relocs(struct nvhost_job *job,
		u32 cmdbuf_mem, struct dma_buf *buf)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(job->ch->dev);
	int i = 0;
	int last_page = -1;
	void *cmdbuf_page_addr = NULL;
	dma_addr_t phys_addr;

	/* pin & patch the relocs for one gather */
	while (i < job->num_relocs) {
		struct nvhost_reloc *reloc = &job->relocarray[i];
		struct nvhost_reloc_shift *shift = &job->relocshiftarray[i];
		struct nvhost_reloc_type *type = &job->reloctypearray[i];

		/* skip all other gathers */
		if (cmdbuf_mem != reloc->cmdbuf_mem) {
			i++;
			continue;
		}

		if (last_page != reloc->cmdbuf_offset >> PAGE_SHIFT) {
			if (cmdbuf_page_addr)
				dma_buf_kunmap(buf, last_page,
						cmdbuf_page_addr);

			cmdbuf_page_addr = dma_buf_kmap(buf,
					reloc->cmdbuf_offset >> PAGE_SHIFT);
			last_page = reloc->cmdbuf_offset >> PAGE_SHIFT;

			if (unlikely(!cmdbuf_page_addr)) {
				pr_err("Couldn't map cmdbuf for relocation\n");
				return -ENOMEM;
			}
		}

		if (pdata->get_reloc_phys_addr)
			phys_addr = pdata->get_reloc_phys_addr(
						job->reloc_addr_phys[i],
						type->reloc_type);
		else
			phys_addr = job->reloc_addr_phys[i];

		__raw_writel(
			(phys_addr +
				reloc->target_offset) >> shift->shift,
			(void __iomem *)(cmdbuf_page_addr +
				(reloc->cmdbuf_offset & ~PAGE_MASK)));

		/* remove completed reloc from the job */
		if (i != job->num_relocs - 1) {
			struct nvhost_reloc *reloc_last =
				&job->relocarray[job->num_relocs - 1];
			struct nvhost_reloc_shift *shift_last =
				&job->relocshiftarray[job->num_relocs - 1];
			struct nvhost_reloc_type *type_last =
				&job->reloctypearray[job->num_relocs - 1];
			reloc->cmdbuf_mem	= reloc_last->cmdbuf_mem;
			reloc->cmdbuf_offset	= reloc_last->cmdbuf_offset;
			reloc->target		= reloc_last->target;
			reloc->target_offset	= reloc_last->target_offset;
			shift->shift		= shift_last->shift;
			type->reloc_type	= type_last->reloc_type;
			job->reloc_addr_phys[i] =
				job->reloc_addr_phys[job->num_relocs - 1];
			job->num_relocs--;
		} else {
			break;
		}
	}

	if (cmdbuf_page_addr)
		dma_buf_kunmap(buf, last_page, cmdbuf_page_addr);

	return 0;
}


int nvhost_job_pin(struct nvhost_job *job, struct nvhost_syncpt *sp)
{
	int err = 0, i = 0, j = 0;
	int nb_hw_pts = nvhost_syncpt_nb_hw_pts(sp);
	DECLARE_BITMAP(waitchk_mask, nb_hw_pts);

	bitmap_zero(waitchk_mask, nb_hw_pts);
	for (i = 0; i < job->num_waitchk; i++) {
		u32 syncpt_id = job->waitchk[i].syncpt_id;
		if (nvhost_syncpt_is_valid_hw_pt(sp, syncpt_id))
			set_bit(syncpt_id, waitchk_mask);
	}

	/* get current syncpt values for waitchk */
	for_each_set_bit(i, waitchk_mask, nb_hw_pts)
		nvhost_syncpt_update_min(sp, i);

	/* pin memory */
	err = pin_job_mem(job);
	if (err <= 0)
		goto fail;

	/* patch gathers */
	for (i = 0; i < job->num_gathers; i++) {
		struct nvhost_job_gather *g = &job->gathers[i];

		/* process each gather mem only once */
		if (!g->buf) {
			g->buf = dma_buf_get(g->mem_id);
			if (IS_ERR(g->buf)) {
				err = PTR_ERR(g->buf);
				g->buf = NULL;
				break;
			}

			g->mem_base = job->gather_addr_phys[i];

			for (j = 0; j < job->num_gathers; j++) {
				struct nvhost_job_gather *tmp =
					&job->gathers[j];
				if (!tmp->buf && tmp->mem_id == g->mem_id) {
					tmp->buf = g->buf;
					tmp->mem_base = g->mem_base;
				}
			}
			err = do_relocs(job, g->mem_id,  g->buf);
			if (!err)
				err = do_waitchks(job, sp,
						g->mem_id, g->buf);
			dma_buf_put(g->buf);
			if (err)
				break;
		}
	}
fail:
	return err;
}

void nvhost_job_unpin(struct nvhost_job *job)
{
	int i;

	for (i = 0; i < job->num_unpins; i++) {
		struct nvhost_job_unpin *unpin = &job->unpins[i];

		dma_buf_unmap_attachment(unpin->attach, unpin->sgt,
						DMA_BIDIRECTIONAL);
		dma_buf_detach(unpin->buf, unpin->attach);
		dma_buf_put(unpin->buf);
	}
	job->num_unpins = 0;
}

/**
 * Debug routine used to dump job entries
 */
void nvhost_job_dump(struct device *dev, struct nvhost_job *job)
{
	dev_info(dev, "    SYNCPT_ID   %d\n",
		job->sp->id);
	dev_info(dev, "    SYNCPT_VAL  %d\n",
		job->sp->fence);
	dev_info(dev, "    FIRST_GET   0x%x\n",
		job->first_get);
	dev_info(dev, "    TIMEOUT     %d\n",
		job->timeout);
	dev_info(dev, "    NUM_SLOTS   %d\n",
		job->num_slots);
	dev_info(dev, "    NUM_HANDLES %d\n",
		job->num_unpins);
}
