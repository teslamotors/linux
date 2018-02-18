/*
 * Copyright (c) 2013-2016 NVIDIA Corporation. All rights reserved.
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

#ifndef __LINUX_OTE_PROTOCOL_H__
#define __LINUX_OTE_PROTOCOL_H__

enum {
	SECOS_UNINIT = -1,

	SECOS_NONE = 0,
	SECOS_TLK,
	SECOS_TRUSTY
};

int tegra_set_vpr_params(void *vpr_base, size_t vpr_size);

#if defined(CONFIG_TRUSTED_LITTLE_KERNEL) || defined(CONFIG_OTE_TRUSTY)
void te_restore_keyslots(void);
int  te_vrr_set_buf(phys_addr_t addr);
void te_vrr_sec(void);
int te_is_secos_dev_enabled(void);
#else
static inline void te_restore_keyslots(void)
{
}

static inline int  te_vrr_set_buf(phys_addr_t addr)
{
	return -EINVAL;
}

static inline void te_vrr_sec(void)
{
}

static inline int te_is_secos_dev_enabled(void)
{
	return SECOS_NONE;
}
#endif

#endif /* !__LINUX_OTE_PROTOCOL_H__ */
