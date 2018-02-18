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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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

static void tlk_ss_reset(void)
{
	/* reset completion vars to default state */
	reinit_completion(&req_ready);
	reinit_completion(&req_complete);
}

int te_handle_ss_ioctl(struct file *file, unsigned int ioctl_num,
	unsigned long ioctl_param)
{
	int ss_cmd;

	if (ioctl_num != TE_IOCTL_SS_CMD)
		return -EINVAL;

	if (copy_from_user(&ss_cmd, (void __user *)ioctl_param,
				sizeof(ss_cmd))) {
		pr_err("%s: copy from user space failed\n", __func__);
		return -EFAULT;
	}

	switch (ss_cmd) {
	case TE_IOCTL_SS_CMD_GET_NEW_REQ:
		/* wait for a new request */
		if (wait_for_completion_interruptible(&req_ready))
			return -ENODATA;
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

int tlk_ss_op(void)
{
	/* signal consumer */
	complete(&req_ready);

	/* wait for the consumer's signal */
	if (!wait_for_completion_timeout(&req_complete,
						msecs_to_jiffies(5000))) {
		/* daemon didn't respond */
		tlk_ss_reset();
		return OTE_ERROR_CANCEL;
	}

	return OTE_SUCCESS;
}

static int __init tlk_ss_init(void)
{
	int ret;

	/* storage disabled? */
	ret = ote_property_is_disabled("storage");
	if (ret) {
		pr_err("%s: fail (%d)\n", __func__, ret);
		return ret;
	}

	return 0;
}

arch_initcall(tlk_ss_init);
