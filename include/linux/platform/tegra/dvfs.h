/*
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *
 * Copyright (c) 2010-2015 NVIDIA CORPORATION. All rights reserved.
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

#include <linux/of.h>
#include <linux/tegra_throttle.h>

#define MAX_DVFS_FREQS	40
#define MAX_DVFS_TABLES	80
#define DVFS_RAIL_STATS_TOP_BIN	100
#define MAX_THERMAL_LIMITS	8
#define MAX_THERMAL_RANGES	(MAX_THERMAL_LIMITS + 1)

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
	bool solved_at_suspend;
};

struct rail_stats {
	ktime_t time_at_mv[DVFS_RAIL_STATS_TOP_BIN + 1];
	ktime_t last_update;
	int last_index;
	bool off;
	int bin_uV;
};

struct rail_alignment {
	int offset_uv;
	int step_uv; /* Step voltage */
};

struct dvfs_rail {
	const char *reg_id;
	int min_millivolts;
	int max_millivolts;
	int reg_max_millivolts;
	int nominal_millivolts; /* Max DVFS voltage */
	int fixed_millivolts;
	int override_millivolts;
	int min_override_millivolts;
	int override_unresolved;
	int (*resolve_override)(int mv);

	const int *therm_mv_floors;
	const int *therm_mv_dfll_floors;
	int therm_mv_floors_num;
	const int *therm_mv_caps;
	int therm_mv_caps_num;
	const int *simon_vmin_offsets;
	int simon_vmin_offs_num;
	int simon_domain;

	int step;
	int step_up;
	bool jmp_to_zero;
	bool in_band_pm;
	bool disabled;
	bool updating;
	bool resolving_to;
	bool rate_set;
	bool dt_reg_fixed;
	bool dt_reg_pwm;

	struct device_node *dt_node; /* device tree rail node */
	struct list_head node;  /* node in dvfs_rail_list */
	struct list_head dvfs;  /* list head of attached dvfs clocks */
	struct list_head relationships_to;
	struct list_head relationships_from;
	struct regulator *reg;
	int millivolts;
	int new_millivolts;
	int dbg_mv_offs;
	int boot_millivolts;
	int disable_millivolts;
	int suspend_millivolts; /* voltage setting set during suspend */

	bool suspended;
	bool dfll_mode; /* DFLL mode ON/OFF */
	bool dfll_mode_updating;

	int therm_floor_idx;	/* index of vmin thermal range */
	int therm_cap_idx;	/* index of vmax thermal range */
	int therm_scale_idx;	/* index of thermal DVFS or clk source range */

	/* Trips for vmin cooling device */
	struct tegra_cooling_device *vmin_cdev;

	/* Trips for vmax cooling device */
	struct tegra_cooling_device *vmax_cdev;

	/* Trips for thermal DVFS cooling device */
	struct tegra_cooling_device *vts_cdev;

	/* Trips for clock source switch cooling device */
	struct tegra_cooling_device *clk_switch_cdev;

	/* Vmax capping method */
	int (*apply_vmax_cap)(int *cap_idx, int new_idx, int cap_mv);

	struct rail_alignment alignment;
	struct rail_stats stats;
	const char *version;
};

/*
 * dfll_range -
 *	DFLL_RANGE_NONE       : DFLL is not used
 *	DFLL_RANGE_ALL_RATES  : DFLL is is used for all CPU rates
 *	DFLL_RANGE_HIGH_RATES : DFLL is used only for high rates
 *				above crossover with PLL dvfs curve
 */
enum dfll_range {
	DFLL_RANGE_NONE = 0,
	DFLL_RANGE_ALL_RATES,
	DFLL_RANGE_HIGH_RATES,
};

/* DFLL usage is under thermal cooling device control */
#define TEGRA_USE_DFLL_CDEV_CNTRL 3

extern int tegra_override_dfll_range;

/* DVFS settings specific for DFLL clock source */
struct dvfs_dfll_data {
	u32		tune0;
	u32		tune0_high_mv;
	u32		tune0_simon_mask;
	u32		tune1;
	bool		tune0_low_at_cold;

	unsigned long	droop_rate_min;
	unsigned long	use_dfll_rate_min;
	unsigned long	out_rate_min;
	unsigned long	max_rate_boost;
	/* Boot frequency if DFLL is enabled by boot-loader; zero otherwise */
	unsigned long	dfll_boot_khz;

	int tune_high_min_millivolts;
	int tune_high_margin_mv;
	int min_millivolts;
	enum dfll_range	range;
	void (*tune_trimmers)(bool trim_high);
	unsigned int (*is_bypass_down)(void);
};

/* DVFS settings specific for PLL clock source */
struct dvfs_pll_data {
	int min_millivolts;
};

struct dvfs {
	const char *clk_name;
	struct clk *clk;
	int speedo_id;
	int process_id;

	/* Must be initialized before tegra_dvfs_init */
	int freqs_mult;
	unsigned long freqs[MAX_DVFS_FREQS];
	const int *millivolts;
	const int *peak_millivolts;
	/* voltage settings as per DFLL clock source */
	const int *dfll_millivolts;
	struct dvfs_rail *dvfs_rail;
	bool auto_dvfs;
	bool can_override;
	bool defer_override;
	bool boost_table;

	/* Filled in by tegra_dvfs_init */
	int max_millivolts;
	int num_freqs;
	struct dvfs_dfll_data dfll_data;

	/* Set if clock rate is adapted by h/w to voltage noise (noise-aware) */
	bool na_dvfs;
	/* Indicates thermal DVFS on/off */
	bool therm_dvfs;
	/* Maximum rate safe at minimum voltage across all thermal ranges */
	unsigned long fmax_at_vmin_safe_t;

	/*
	 * Current dvfs point = { index into V/F arrays, voltage, rate request }
	 * cur_index is invalid - set to MAX_DVFS_FREQS - if cur_rate and
	 * cur_millivolts are set to zero (may happen when dvfs rate is not set
	 * initially, or clock is disabled).
	 */
	int cur_index;
	int cur_millivolts;
	unsigned long cur_rate;

	unsigned long *alt_freqs;
	bool use_alt_freqs;
	long dbg_hz_offs;
	struct list_head node;
	struct list_head debug_node;
	struct list_head reg_node;
	struct mutex *lock;
};

/* CVB coefficients */
struct cvb_dvfs_parameters {
	int	c0;
	int	c1;
	int	c2;
	int	c3;
	int	c4;
	int	c5;
};

struct cvb_dvfs_table {
	unsigned long freq;

	/* Coeffs for voltage calculation, when dfll clock source is selected */
	struct cvb_dvfs_parameters cvb_dfll_param;

	/* Coeffs for voltage calculation, when pll clock source is selected */
	struct cvb_dvfs_parameters cvb_pll_param;
};

struct cvb_dvfs {
	int speedo_id;
	int process_id;

	/* Tuning parameters for dfll */
	struct dvfs_dfll_data dfll_tune_data;

	/* tuning parameters for pll clock */
	struct dvfs_pll_data pll_tune_data;

	/* dvfs Max voltage */
	int max_mv;

	/* dvfs Max frequency */
	unsigned long max_freq;
	int freqs_mult;

	/* scaling values for voltage calculation */
	int speedo_scale;
	int voltage_scale;
	int thermal_scale;

	struct cvb_dvfs_table cvb_vmin;

	/* CVB table for various frequencies */
	struct cvb_dvfs_table cvb_table[MAX_DVFS_FREQS];

	/* Trips for minimal voltage settings per thermal ranges */
	int vmin_trips_table[MAX_THERMAL_LIMITS];
	int therm_floors_table[MAX_THERMAL_LIMITS];

	/* Trips for thermal DVFS per thermal ranges */
	int vts_trips_table[MAX_THERMAL_LIMITS];

	/* Trips for clock source change per thermal ranges */
	int clk_switch_trips[MAX_THERMAL_LIMITS];
};

#define cpu_cvb_dvfs	cvb_dvfs
#define gpu_cvb_dvfs	cvb_dvfs
#define core_cvb_dvfs	cvb_dvfs

extern struct dvfs_rail *tegra_cpu_rail;
extern struct dvfs_rail *tegra_gpu_rail;
extern struct dvfs_rail *tegra_core_rail;

struct dvfs_data {
	struct dvfs_rail *rail;
	struct dvfs *tables;
	int *millivolts;
	unsigned int num_tables;
	unsigned int num_voltages;
};

#ifdef CONFIG_OF
typedef int (*of_tegra_dvfs_init_cb_t)(struct device_node *);
int of_tegra_dvfs_init(const struct of_device_id *matches);
int of_tegra_dvfs_rail_node_parse(struct device_node *rail_dn,
				  struct dvfs_rail *rail);
int of_tegra_dvfs_rail_get_cdev_trips(
	struct tegra_cooling_device *tegra_cdev, int *therm_trips_table,
	int *therm_limits_table, struct rail_alignment *align, bool up);
#else
static inline int of_tegra_dvfs_init(const struct of_device_id *matches)
{ return -ENODATA; }
static inline int of_tegra_dvfs_rail_node_parse(struct device_node *rail_dn,
						struct dvfs_rail *rail)
{ return -ENODEV; }
static inline int of_tegra_dvfs_rail_get_cdev_trips(
	struct tegra_cooling_device *tegra_cdev, int *therm_trips_table,
	int *therm_limits_table, struct rail_alignment *align, bool up)
{ return -ENODEV; }
#endif

void tegra11x_init_dvfs(void);
void tegra12x_init_dvfs(void);
void tegra13x_init_dvfs(void);
void tegra21x_init_dvfs(void);
void tegra12x_vdd_cpu_align(int step_uv, int offset_uv);
void tegra13x_vdd_cpu_align(int step_uv, int offset_uv);
void tegra_init_dvfs_one(struct dvfs *d, int max_freq_index);
int dvfs_debugfs_init(struct dentry *clk_debugfs_root);
int tegra_dvfs_rail_connect_regulators(void);
int tegra_dvfs_rail_register_notifiers(void);
int tegra_dvfs_init_rails(struct dvfs_rail *dvfs_rails[], int n);
void tegra_dvfs_add_relationships(struct dvfs_relationship *rels, int n);

void tegra_dvfs_rail_enable(struct dvfs_rail *rail);
void tegra_dvfs_rail_disable(struct dvfs_rail *rail);
int tegra_dvfs_rail_power_up(struct dvfs_rail *rail);
int tegra_dvfs_rail_power_down(struct dvfs_rail *rail);
bool tegra_dvfs_is_rail_up(struct dvfs_rail *rail);
bool tegra_dvfs_rail_updating(struct clk *clk);
void tegra_dvfs_rail_off(struct dvfs_rail *rail, ktime_t now);
void tegra_dvfs_rail_on(struct dvfs_rail *rail, ktime_t now);
void tegra_dvfs_rail_pause(struct dvfs_rail *rail, ktime_t delta, bool on);
int tegra_dvfs_rail_set_mode(struct dvfs_rail *rail, unsigned int mode);
int tegra_dvfs_rail_register_notifier(struct dvfs_rail *rail,
				      struct notifier_block *nb);
int tegra_dvfs_rail_unregister_notifier(struct dvfs_rail *rail,
					struct notifier_block *nb);
struct dvfs_rail *tegra_dvfs_get_rail_by_name(const char *reg_id);
int tegra_dvfs_rail_get_current_millivolts(struct dvfs_rail *rail);

int tegra_dvfs_override_core_cap_apply(int level);
int tegra_dvfs_therm_vmax_core_cap_apply(int *cap_idx, int new_idx, int level);

int tegra_dvfs_alt_freqs_install(struct dvfs *d, unsigned long *alt_freqs);
void tegra_dvfs_alt_freqs_install_always(
	struct dvfs *d, unsigned long *alt_freqs);
int tegra_dvfs_replace_voltage_table(struct dvfs *d, const int *new_millivolts);

int tegra_dvfs_butterfly_throttle(struct clk *c1, unsigned long *rate1,
				  struct clk *c2, unsigned long *rate2);

int tegra_dvfs_dfll_mode_set(struct dvfs *d, unsigned long rate);
int tegra_dvfs_dfll_mode_clear(struct dvfs *d, unsigned long rate);
int tegra_clk_dfll_range_control(enum dfll_range use_dfll);
bool tegra_dvfs_is_dfll_scale(struct dvfs *d, unsigned long rate);
bool tegra_dvfs_is_dfll_range(struct dvfs *d, unsigned long rate);
int tegra_dvfs_swap_dfll_range(struct dvfs *d, int range, int *old_range);
int tegra_dvfs_set_dfll_range(struct dvfs *d, int range);
int tegra_dvfs_rail_set_reg_volatile(struct dvfs_rail *rail, bool set);

void tegra_clip_freqs(u32 *freqs, int *num_freqs, int freqs_mult,
		     const unsigned long *rates_ladder, int num_rates, bool up);

struct tegra_cooling_device *tegra_dvfs_get_cpu_vmax_cdev(void);
struct tegra_cooling_device *tegra_dvfs_get_cpu_vmin_cdev(void);
struct tegra_cooling_device *tegra_dvfs_get_core_vmax_cdev(void);
struct tegra_cooling_device *tegra_dvfs_get_core_vmin_cdev(void);
struct tegra_cooling_device *tegra_dvfs_get_gpu_vmin_cdev(void);
struct tegra_cooling_device *tegra_dvfs_get_gpu_vts_cdev(void);
struct tegra_cooling_device *tegra_dvfs_get_cpu_clk_switch_cdev(void);
#ifdef CONFIG_TEGRA_USE_SIMON
void tegra_dvfs_rail_init_simon_vmin_offsets(
	int *offsets, int offs_num, struct dvfs_rail *rail);
#else
static inline void tegra_dvfs_rail_init_simon_vmin_offsets(
	int *offsets, int offs_num, struct dvfs_rail *rail)
{ }
#endif
void tegra_dvfs_rail_init_vmin_thermal_profile(
	int *therm_trips_table, int *therm_floors_table,
	struct dvfs_rail *rail, struct dvfs_dfll_data *d);
void tegra_dvfs_rail_init_vmax_thermal_profile(
	int *therm_trips_table, int *therm_caps_table,
	struct dvfs_rail *rail, struct dvfs_dfll_data *d);
int tegra_dvfs_rail_of_init_vmin_thermal_profile(
	int *therm_trips_table, int *therm_floors_table,
	struct dvfs_rail *rail, struct dvfs_dfll_data *d);
int tegra_dvfs_rail_of_init_vmax_thermal_profile(
	int *therm_trips_table, int *therm_caps_table,
	struct dvfs_rail *rail, struct dvfs_dfll_data *d);
int tegra_dvfs_rail_init_clk_switch_thermal_profile(
	int *clk_switch_trips, struct dvfs_rail *rail);
int tegra_dvfs_rail_init_thermal_dvfs_trips(
	int *therm_trips_table, struct dvfs_rail *rail);
int tegra_dvfs_init_thermal_dvfs_voltages(int *millivolts,
	int *peak_millivolts, int freqs_num, int ranges_num, struct dvfs *d);
int tegra_dvfs_rail_dfll_mode_set_cold(struct dvfs_rail *rail,
				       struct clk *dfll_clk);
int tegra_dvfs_rail_get_thermal_floor(struct dvfs_rail *rail);
void tegra_dvfs_rail_register_vmax_cdev(struct dvfs_rail *rail);

#ifdef CONFIG_TEGRA_VDD_CORE_OVERRIDE
int tegra_dvfs_resolve_override(struct clk *c, unsigned long max_rate);
int tegra_dvfs_rail_get_override_floor(struct dvfs_rail *rail);
#else
static inline int tegra_dvfs_resolve_override(struct clk *c, unsigned long rate)
{ return 0; }
static inline int tegra_dvfs_rail_get_override_floor(struct dvfs_rail *rail)
{ return 0; }
#endif

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
int tegra_dvfs_rail_disable_prepare(struct dvfs_rail *rail);
int tegra_dvfs_rail_post_enable(struct dvfs_rail *rail);
int tegra_dvfs_set_rail_relations_dfll_vmin(struct clk *dfll_clk,
							int external_vmin);
#else
static inline int tegra_dvfs_rail_disable_prepare(struct dvfs_rail *rail)
{ return 0; }
static inline int tegra_dvfs_rail_post_enable(struct dvfs_rail *rail)
{ return 0; }
#endif

bool tegra_dvfs_is_dfll_bypass(void);

static inline bool tegra_dvfs_rail_is_dfll_mode(struct dvfs_rail *rail)
{
	return rail ? rail->dfll_mode : false;
}

static inline void tegra_dvfs_rail_mode_updating(struct dvfs_rail *rail,
						 bool updating)
{
	if (rail)
		rail->dfll_mode_updating = updating;
}

static inline void tegra_dvfs_set_dfll_tune_trimmers(
	struct dvfs *d, void (*tune_trimmers)(bool trim_high))
{
	if (d->dfll_data.tune_high_min_millivolts)
		d->dfll_data.tune_trimmers = tune_trimmers;
}

#endif
