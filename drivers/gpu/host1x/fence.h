/*
 * Copyright (C) 2015 NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __HOST1X_FENCE_H
#define __HOST1X_FENCE_H

struct host1x_sync_timeline;
struct host1x_sync_pt;

struct host1x;
struct host1x_syncpt;

#ifdef CONFIG_SYNC

struct host1x_sync_timeline *host1x_sync_timeline_create(
		struct host1x *host,
		struct host1x_syncpt *syncpt);
void host1x_sync_timeline_destroy(struct host1x_sync_timeline *timeline);
void host1x_sync_timeline_signal(struct host1x_sync_timeline *timeline);

#else

static inline struct host1x_sync_timeline *host1x_sync_timeline_create(
		struct host1x *host,
		struct host1x_syncpt *syncpt)
{
	return NULL;
}

static inline void host1x_sync_timeline_destroy(
		struct host1x_sync_timeline *timeline)
{
}

static inline void host1x_sync_timeline_signal(
		struct host1x_sync_timeline *timeline)
{
}

#endif

#endif
