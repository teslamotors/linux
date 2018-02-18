/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __POWERGATE_PRIV_H__
#define __POWERGATE_PRIV_H__

struct powergate_ops {
	const char *soc_name;

	int num_powerdomains;
	int num_cpu_domains;
	u8 *cpu_domains;

	spinlock_t *(*get_powergate_lock)(void);

	const char *(*get_powergate_domain_name)(int id);

	int (*powergate_partition)(int);
	int (*unpowergate_partition)(int id);

	int (*powergate_partition_with_clk_off)(int);
	int (*unpowergate_partition_with_clk_on)(int);

	int (*powergate_mc_enable)(int id);
	int (*powergate_mc_disable)(int id);

	int (*powergate_mc_flush)(int id);
	int (*powergate_mc_flush_done)(int id);

	int (*powergate_init_refcount)(void);

	bool (*powergate_check_clamping)(int id);

	bool (*powergate_skip)(int id);

	bool (*powergate_is_powered)(int id);
};

#endif /* __POWERGATE_PRIV_H__ */
