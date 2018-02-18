/*
 * Copyright (c) 2013-2015 NVIDIA Corporation. All rights reserved.
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
 */

#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/list.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>

#include "ote_protocol.h"

static DECLARE_COMPLETION(req_ready);
static DECLARE_COMPLETION(req_complete);

static bool ready_for_req;
static struct te_ss_op_legacy *ss_op_shmem_legacy;
static uint32_t ss_op_size;
static struct tlk_info *tlk_info;
static struct te_ss_op *ss_op_shmem;

int te_handle_ss_ioctl(struct file *file, unsigned int ioctl_num,
	unsigned long ioctl_param)
{
	int ss_cmd;
	int ret;
	struct tlk_context *context = file->private_data;

	if (ioctl_num != TE_IOCTL_SS_CMD)
		return -EINVAL;

	if (copy_from_user(&ss_cmd, (void __user *)ioctl_param,
				sizeof(ss_cmd))) {
		pr_err("%s: copy from user space failed\n", __func__);
		return -EFAULT;
	}

	switch (ss_cmd) {
	case TE_IOCTL_SS_CMD_GET_NEW_REQ:
		if (!context->is_ss_daemon) {
			mutex_lock(&smc_lock);
			reinit_completion(&req_ready);
			reinit_completion(&req_complete);
			context->is_ss_daemon = true;
			ready_for_req = true;
			mutex_unlock(&smc_lock);
		}
		/* wait for a new request */
		ret = wait_for_completion_interruptible(&req_ready);
		if (ret != 0)
			return ret;
		break;
	case TE_IOCTL_SS_CMD_REQ_COMPLETE:
		/* signal the producer */
		complete(&req_complete);
		break;
	default:
		pr_err("%s: unknown ss_cmd 0x%x\n", __func__, ss_cmd);
		return -EINVAL;
	}

	return 0;
}

int tlk_ss_init(struct tlk_info *info)
{
	dma_addr_t ss_op_shmem_dma;
	uint32_t ret;

	/* allocate shared memory buffer */
	ss_op_shmem = dma_alloc_coherent(NULL, sizeof(struct te_ss_op),
			&ss_op_shmem_dma, GFP_KERNEL);
	if (!ss_op_shmem) {
		pr_err("%s: no memory available for fs operations\n", __func__);
		return -ENOMEM;
	}

	tlk_info = info;

	ret = tlk_generic_smc(info, TE_SMC_SS_REGISTER_HANDLER,
			(uintptr_t)ss_op_shmem, 0, 0);
	if (ret != 0) {
		dma_free_coherent(NULL, sizeof(struct te_ss_op),
			(void *)ss_op_shmem, ss_op_shmem_dma);
		ss_op_shmem = NULL;
		return -ENOTSUPP;
	}

	return 0;
}

static void indicate_ss_op_complete(void)
{
	tlk_generic_smc(tlk_info, TE_SMC_SS_REQ_COMPLETE, 0, 0, 0);
}

int te_handle_ss_ioctl_legacy(struct file *file, unsigned int ioctl_num,
	unsigned long ioctl_param)
{
	switch (ioctl_num) {
	case TE_IOCTL_SS_NEW_REQ_LEGACY:
		/* wait for a new request */
		if (wait_for_completion_interruptible(&req_ready))
			return -ENODATA;

		/* transfer pending request to daemon's buffer */
		if (copy_to_user((void __user *)ioctl_param, ss_op_shmem_legacy,
					ss_op_size)) {
			pr_err("copy_to_user failed for new request\n");
			return -EFAULT;
		}
		break;

	case TE_IOCTL_SS_REQ_COMPLETE_LEGACY: /* request complete */
		if (copy_from_user(ss_op_shmem_legacy,
			(void __user *)ioctl_param, ss_op_size)) {
			pr_err("copy_from_user failed for request\n");
			return -EFAULT;
		}

		/* signal the producer */
		complete(&req_complete);
		break;
	}

	return 0;
}

void tlk_ss_op_legacy(uint32_t size)
{
	/* store size of request */
	ss_op_size = size;

	/* signal consumer */
	complete(&req_ready);

	/* wait for the consumer's signal */
	wait_for_completion(&req_complete);

	/* signal completion to the secure world */
	indicate_ss_op_complete();
}

int tlk_ss_init_legacy(struct tlk_info *info)
{
	dma_addr_t ss_op_shmem_dma;

	/* allocate shared memory buffer */
	ss_op_shmem_legacy = dma_alloc_coherent(NULL,
		sizeof(struct te_ss_op_legacy), &ss_op_shmem_dma, GFP_KERNEL);
	if (!ss_op_shmem_legacy) {
		pr_err("%s: no memory available for fs operations\n", __func__);
		return -ENOMEM;
	}

	tlk_generic_smc(info, TE_SMC_SS_REGISTER_HANDLER_LEGACY,
		(uintptr_t)tlk_ss_op_legacy, (uintptr_t)ss_op_shmem_legacy, 0);

	return 0;
}

void tlk_ss_close(void)
{
	ready_for_req = false;
	complete_all(&req_complete);
}

uint32_t tlk_ss_op(void)
{
	if (!ready_for_req) {
		pr_err("%s: daemon not ready\n", __func__);
		return OTE_ERROR_BAD_STATE;
	}
	/* signal consumer */
	complete(&req_ready);

	/* wait for the consumer's signal */
	wait_for_completion(&req_complete);

	if (!ready_for_req) {
		pr_err("%s: daemon done\n", __func__);
		return OTE_ERROR_BAD_STATE;
	}

	return OTE_SUCCESS;
}

