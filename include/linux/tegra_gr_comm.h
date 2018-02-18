/*
 * Tegra Graphics Virtualization Communication Framework
 *
 * Copyright (c) 2013-2014, NVIDIA Corporation. All rights reserved.
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

#ifndef __TEGRA_GR_COMM_H
#define __TEGRA_GR_COMM_H

#include <linux/platform_device.h>

enum {
	TEGRA_GR_COMM_CTX_CLIENT = 0,
	TEGRA_GR_COMM_CTX_SERVER
};

#define TEGRA_GR_COMM_ID_SELF (0xFF)

#ifdef CONFIG_TEGRA_GR_VIRTUALIZATION
int tegra_gr_comm_init(struct platform_device *pdev, u32 virt_ctx, u32 elems,
		const size_t *queue_sizes, u32 queue_start, u32 num_queues);
void tegra_gr_comm_deinit(u32 virt_ctx, u32 queue_start, u32 num_queues);
int tegra_gr_comm_send(u32 virt_ctx, u32 peer, u32 index, void *data,
		size_t size);
int tegra_gr_comm_recv(u32 virt_ctx, u32 index, void **handle, void **data,
		size_t *size, u32 *sender);
int tegra_gr_comm_sendrecv(u32 virt_ctx, u32 peer, u32 index, void **handle,
			void **data, size_t *size);
void tegra_gr_comm_release(void *handle);
u32 tegra_gr_comm_get_server_vmid(void);
void *tegra_gr_comm_oob_get_ptr(u32 virt_ctx, u32 peer, u32 index,
				void **ptr, size_t *size);
void tegra_gr_comm_oob_put_ptr(void *handle);
#else
static inline int tegra_gr_comm_init(struct platform_device *pdev,
				u32 virt_ctx, u32 elems,
				const size_t *queue_sizes, u32 queue_start,
				u32 num_queues)
{
	return -ENOSYS;
}

static inline void tegra_gr_comm_deinit(u32 virt_ctx, u32 queue_start,
					u32 num_queues)
{
}

static inline int tegra_gr_comm_send(u32 virt_ctx, u32 peer, u32 index,
				void *data, size_t size)
{
	return -ENOSYS;
}

static inline int tegra_gr_comm_recv(u32 virt_ctx, u32 index, void **handle,
				void **data, size_t *size, u32 *sender)
{
	return -ENOSYS;
}

static inline int tegra_gr_comm_sendrecv(u32 virt_ctx, u32 peer, u32 index,
					void **handle, void **data,
					size_t *size)
{
	return -ENOSYS;
}

static inline void tegra_gr_comm_release(void *handle) {}

static inline u32 tegra_gr_comm_get_server_vmid(void)
{
	return 0;
}

static inline void *tegra_gr_comm_oob_get_ptr(u32 virt_ctx, u32 peer,
					u32 index, void **ptr, size_t *size)
{
	return NULL;
}

static inline void tegra_gr_comm_oob_put_ptr(void *handle) {}
#endif

#endif
