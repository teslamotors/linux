/*
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *
 * Copyright (C) 2010-2011 NVIDIA Corporation.
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

#ifndef _TEGRA_DVFS_H_
#define _TEGRA_DVFS_H_

#define MAX_DVFS_FREQS	18
#define DVFS_RAIL_STATS_TOP_BIN	40

struct clk;
struct dvfs_rail;

/*
 * dvfs_relationship between to rails, "from" and "to"
 * when the rail changes, it will call dvfs_rail_update on the rails
 * in the relationship_to list.
 * when determining the voltage to set a rail to, it will consider each
 * rail in the relationship_from list.
 */
struct dvfs_relationship {
	struct dvfs_rail *to;
	struct dvfs_rail *from;
	int (*solve)(struct dvfs_rail *, struct dvfs_rail *);

	struct list_head to_node; /* node in relationship_to list */
	struct list_head from_node; /* node in relationship_from list */
	bool solved_at_nominal;
};

struct rail_stats {
	ktime_t time_at_mv[DVFS_RAIL_STATS_TOP_BIN + 1];
	ktime_t last_update;
	int last_index;
	bool off;
};

struct dvfs_rail {
	const char *reg_id;
	int min_millivolts;
	int max_millivolts;
	int nominal_millivolts;
	int step;
	bool jmp_to_zero;
	bool disabled;
	bool updating;
	bool resolving_to;

	struct list_head node;  /* node in dvfs_rail_list */
	struct list_head dvfs;  /* list head of attached dvfs clocks */
	struct list_head relationships_to;
	struct list_head relationships_from;
	struct regulator *reg;
	int millivolts;
	int new_millivolts;
	bool suspended;
	struct rail_stats stats;
};

struct dvfs {
	/* Used only by tegra2_clock.c */
	const char *clk_name;
	int speedo_id;
	int process_id;

	/* Must be initialized before tegra_dvfs_init */
	int freqs_mult;
	unsigned long freqs[MAX_DVFS_FREQS];
	const int *millivolts;
	struct dvfs_rail *dvfs_rail;
	bool auto_dvfs;

	/* Filled in by tegra_dvfs_init */
	int max_millivolts;
	int num_freqs;

	int cur_millivolts;
	unsigned long cur_rate;
	struct list_head node;
	struct list_head debug_node;
	struct list_head reg_node;
};

extern struct dvfs_rail *tegra_cpu_rail;

#ifdef CONFIG_TEGRA_SILICON_PLATFORM

void tegra_soc_init_dvfs(void);
int tegra_enable_dvfs_on_clk(struct clk *c, struct dvfs *d);
int dvfs_debugfs_init(struct dentry *clk_debugfs_root);
int tegra_dvfs_late_init(void);
int tegra_dvfs_init_rails(struct dvfs_rail *dvfs_rails[], int n);
void tegra_dvfs_add_relationships(struct dvfs_relationship *rels, int n);
void tegra_dvfs_rail_enable(struct dvfs_rail *rail);
void tegra_dvfs_rail_disable(struct dvfs_rail *rail);
bool tegra_dvfs_rail_updating(struct clk *clk);
void tegra_dvfs_rail_off(struct dvfs_rail *rail, ktime_t now);
void tegra_dvfs_rail_on(struct dvfs_rail *rail, ktime_t now);
void tegra_dvfs_rail_pause(struct dvfs_rail *rail, ktime_t delta, bool on);
struct dvfs_rail *tegra_dvfs_get_rail_by_name(const char *reg_id);
int tegra_dvfs_predict_millivolts(struct clk *c, unsigned long rate);
void tegra_dvfs_core_cap_enable(bool enable);
void tegra_dvfs_core_cap_level_set(int level);
#else
static inline void tegra_soc_init_dvfs(void)
{}
static inline int tegra_enable_dvfs_on_clk(struct clk *c, struct dvfs *d)
{ return 0; }
static inline int dvfs_debugfs_init(struct dentry *clk_debugfs_root)
{ return 0; }
static inline int tegra_dvfs_late_init(void)
{ return 0; }
static inline int tegra_dvfs_init_rails(struct dvfs_rail *dvfs_rails[], int n)
{ return 0; }
static inline void tegra_dvfs_add_relationships(struct dvfs_relationship *rels, int n)
{}
static inline void tegra_dvfs_rail_enable(struct dvfs_rail *rail)
{}
static inline void tegra_dvfs_rail_disable(struct dvfs_rail *rail)
{}
static inline bool tegra_dvfs_rail_updating(struct clk *clk)
{ return false; }
static inline void tegra_dvfs_rail_off(struct dvfs_rail *rail, ktime_t now)
{}
static inline void tegra_dvfs_rail_on(struct dvfs_rail *rail, ktime_t now)
{}
static inline void tegra_dvfs_rail_pause(
	struct dvfs_rail *rail, ktime_t delta, bool on)
{}
static inline struct dvfs_rail *tegra_dvfs_get_rail_by_name(const char *reg_id)
{ return NULL;}
static inline int tegra_dvfs_predict_millivolts(struct clk *c, unsigned long rate)
{ return 0; }
static inline void tegra_dvfs_core_cap_enable(bool enable)
{}
static inline void tegra_dvfs_core_cap_level_set(int level)
{}
#endif

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
int tegra_dvfs_rail_disable_prepare(struct dvfs_rail *rail);
int tegra_dvfs_rail_post_enable(struct dvfs_rail *rail);
#else
static inline int tegra_dvfs_rail_disable_prepare(struct dvfs_rail *rail)
{ return 0; }
static inline int tegra_dvfs_rail_post_enable(struct dvfs_rail *rail)
{ return 0; }
#endif

#endif
