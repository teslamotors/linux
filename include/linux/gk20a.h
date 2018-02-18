/*
 * gk20a GPU driver
 *
 * Copyright (c) 2014-2016, NVIDIA Corporation. All rights reserved.
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

#ifndef __GK20A_H
#define __GK20A_H

#include <linux/errno.h>

struct channel_gk20a;
struct platform_device;

#if defined(CONFIG_GK20A) && defined(CONFIG_PM)
int gk20a_do_idle(void);
int gk20a_do_unidle(void);
#else
static inline int gk20a_do_idle(void) { return -ENOSYS; }
static inline int gk20a_do_unidle(void) { return -ENOSYS; }
#endif

#endif
