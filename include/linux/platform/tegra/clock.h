/*
 * include/linux/platform/tegra/clock.h
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *
 * Copyright (c) 2010-2015, NVIDIA Corporation.  All rights reserved.
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

#ifndef __MACH_TEGRA_CLOCK_H
#define __MACH_TEGRA_CLOCK_H

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define USE_PLL_LOCK_BITS 0	/* Never use lock bits on Tegra2 */
#else
#define USE_PLL_LOCK_BITS 1	/* Use lock bits for PLL stabiliation */
#define USE_PLLE_SS 1		/* Use spread spectrum coefficients for PLLE */
#define PLL_PRE_LOCK_DELAY  2	/* Delay 1st lock bit read after pll enabled */
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
#define PLL_POST_LOCK_DELAY 50	/* Safety delay after lock is detected */
#else
#define USE_PLLE_SWCTL 0	/* Use s/w controls for PLLE */
#define PLL_POST_LOCK_DELAY 10	/* Safety delay after lock is detected */
#endif
#endif

#define RESET_PROPAGATION_DELAY	5

#ifndef __ASSEMBLY__

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/clk.h>
#include <linux/list.h>
#include <linux/lockdep.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/clk/tegra.h>
#include <trace/events/power.h>
#include <asm/cputime.h>

#define MAX_SAME_LIMIT_SKU_IDS	16
#define STATS_USERS_LIST_SIZE	16

struct clk;

#define DIV_BUS			(1 << 0)	/* system bus clock */
#define DIV_U71			(1 << 1)	/* divider is U7.1 (N/2 + 1) */
#define DIV_U71_FIXED		(1 << 2)	/* divider is U7.1 and fixed unless reprogrammed by SW */
#define DIV_2			(1 << 3)	/* clock output is divided by 2 */
#define DIV_U16			(1 << 4)	/* divider is (N + 1) */
#define PLL_FIXED		(1 << 5)	/* PLL runs at fixed rate */
#define PLL_HAS_CPCON		(1 << 6)	/* PLL has CPCON value in its configuration register */
#define MUX			(1 << 7)	/* this clock uses a mux to select one of the input sources */
#define PLLD			(1 << 8)	/* clock is PLLD */
#define PERIPH_NO_RESET		(1 << 9)	/* doesn't have separate reset bit in the register */
#define PERIPH_NO_ENB		(1 << 10)	/* does not have clock enable bit in the register */
#define PERIPH_EMC_ENB		(1 << 11)	/* this peripheral clock is EMC */
#define PERIPH_MANUAL_RESET	(1 << 12)	/* don't take module out of reset automatically, when enabling clock */
#define PLL_ALT_MISC_REG	(1 << 13)	/* use alternate offset for PLL misc registers */
#define PLLU			(1 << 14)	/* PLL is PLLU */
#define PLLX			(1 << 15)	/* PLL is PLLX */
#define MUX_PWM			(1 << 16)	/* deprecated for T124 onwards */
#define MUX8			(1 << 17)	/* used in simulator setting for T124 onwards */
#define DIV_U151_UART		(1 << 18)	/* 16-bit uart divider with dual CAR/UART control */
#define MUX_CLK_OUT		(1 << 19)	/* used to locate extperiph clock registers */
#define PLLM			(1 << 20)	/* PLL is PLLM */
#define DIV_U71_INT		(1 << 21)	/* fractional divider is not allowed for this clock */
#define DIV_U71_IDLE		(1 << 22)	/* clock has a  separate idle state divider that is automatically
						   engaged when module is idle */
#define DIV_U151		(1 << 23)	/* divider will be U15.1 for this clock */
#define DFLL			(1 << 24)	/* this clock has closed loop control in h/w */
#define ENABLE_ON_INIT		(1 << 28)	/* enable during initialization (deprecated) */
#define PERIPH_ON_APB		(1 << 29)	/* this peripheral is on APB */
#define PERIPH_ON_CBUS		(1 << 30)	/* this peripheral is on one of the PLLCx bus */
#define BUS_RATE_LIMIT		(1 << 31)	/* Clock used to apply constraints on shared buses
						   (don't propagate enable/disable to the bus) */

#define SHARED_BUS_RETENTION	(1 << 0)
#define SHARED_BUS_USE_SKIPPERS	(1 << 1)

#define PERIPH_DIV		(DIV_U71 | DIV_U151 | DIV_U16)
#define PERIPH_INIT_SAFE_RATE	12000000

#ifdef CONFIG_COMMON_CLK
struct clk_tegra;
#define to_clk_tegra(_hw) container_of(_hw, struct clk_tegra, hw)
#endif

struct clk_mux_sel {
	struct clk	*input;
	u32		value;
};

struct clk_backup {
	struct clk	*input;
	u32		value;
	unsigned long	bus_rate;
};

struct clk_pll_freq_table {
	unsigned long	input_rate;
	unsigned long	output_rate;
	u16		n;
	u16		m;
	u8		p;
	u8		cpcon;
	u16		sdm_data;
};

struct clk_pll_div_layout {
	/* base dividers */
	u32		ndiv_shift;
	u32		ndiv_mask;
	u32		mdiv_shift;
	u32		mdiv_mask;
	u32		pdiv_shift;
	u32		pdiv_mask;
	u8		*pdiv_to_p;
	u8		pdiv_max;

	/* misc dividers */
	u32		ndiv_new_shift;
	u32		ndiv_new_reg_idx;
	u32		sdm_din_mask;
	u32		sdm_din_shift;
	u32		sdm_din_reg_idx;
};

struct clk_pll_controls {
	u32		enable_mask;
	u32		bypass_mask;

	u32		reset_mask;
	u32		reset_reg_idx;
	u32		iddq_mask;
	u32		iddq_reg_idx;
	u32		lock_mask;
	u32		lock_reg_idx;

	u32		dramp_en_mask;
	u32		dramp_done_mask;
	u32		dramp_ctrl_reg_idx;
	u32		sdm_en_mask;
	u32		ssc_en_mask;
	u32		sdm_ctrl_reg_idx;
};

enum pll_reg_indx {
	PLL_BASE_IDX = 0,
	PLL_MISC0_IDX,
	PLL_MISC1_IDX,
	PLL_MISC2_IDX,
	PLL_MISC3_IDX,
	PLL_MISC4_IDX,
	PLL_MISC5_IDX,
};

enum clk_state {
	UNINITIALIZED = 0,
	ON,
	OFF,
};

struct tegra_pto_table {
	char *name;
	int divider;
	u32 pto_id;
	u32 presel_value;
	u32 presel_reg;
	u32 presel_mask;
};

#ifndef CONFIG_COMMON_CLK
struct clk_ops {
	void		(*init)(struct clk *);
	int		(*enable)(struct clk *);
	void		(*disable)(struct clk *);
	int		(*set_parent)(struct clk *, struct clk *);
	int		(*set_rate)(struct clk *, unsigned long);
	long		(*round_rate)(struct clk *, unsigned long);
	void		(*reset)(struct clk *, bool);
	int		(*shared_bus_update)(struct clk *);
	int		(*clk_cfg_ex)(struct clk *,
				enum tegra_clk_ex_param, u32);
	long		(*round_rate_updown)(struct clk *, unsigned long, bool);
};

struct bus_stats {
	cputime64_t	*time_table;
	struct clk	*users_list[STATS_USERS_LIST_SIZE];
	int		users_num;
	int		rates_num;
	int		new_cap_idx;
	int		new_floor_idx;
	int		new_rate_idx;
	int		last_user_idx;
	int		last_rate_idx;
	u64		last_update;
};

struct clk_stats {
	struct bus_stats *histogram;
	cputime64_t 	time_on;
	u64 		last_update;
};

enum cpu_mode {
	MODE_G = 0,
	MODE_LP,
};

enum shared_bus_users_mode {
	SHARED_FLOOR = 0,
	SHARED_BW,
	SHARED_CEILING,
	SHARED_AUTO,
	SHARED_OVERRIDE,
	SHARED_ISO_BW,
	SHARED_CEILING_BUT_ISO,
};

struct clk {
	/* node for master clocks list */
	struct list_head	node;		/* node for list of all clocks */
	struct dvfs 		*dvfs;
	struct clk_lookup	lookup;

#ifdef CONFIG_DEBUG_FS
	struct dentry		*dent;
	struct tegra_pto_table	*pto_entry;
#endif
	bool			set;
	struct clk_ops		*ops;
	unsigned long		dvfs_rate;
	unsigned long		rate;
	unsigned long		boot_rate;
	unsigned long		max_rate;
	unsigned long		min_rate;
	bool			auto_dvfs;
	bool			cansleep;
	bool			set_disabled_div;
	u32			flags;
	u32			clk_id;
	const char		*name;

	u32			refcnt;
	enum clk_state		state;
	struct clk		*parent;
	u32			div;
	u32			mul;
	struct clk		*skipper;
	struct clk_stats 	stats;

	const struct clk_mux_sel	*inputs;
	u32				reg;
	u32				reg_shift;

#ifdef CONFIG_LOCKDEP
	struct lock_class_key	lockdep_class;
#endif

	struct list_head		shared_bus_list;
	struct clk_backup		shared_bus_backup;
	struct clk			*child_bus;
	unsigned long			override_rate;
	u32				shared_bus_flags;

	union {
		struct {
			unsigned int			clk_num;
			u32				src_mask;
			u32				src_shift;
			struct clk			*pll_low;
			struct clk			*pll_high;
			struct clk			*pll_selected;
			unsigned long			threshold;
			int				min_div_low;
			int				min_div_high;
		} periph;
		struct {
			unsigned long			input_min;
			unsigned long			input_max;
			unsigned long			cf_min;
			unsigned long			cf_max;
			unsigned long			vco_min;
			unsigned long			vco_max;
			u16				mdiv_default;
			u8				cpcon_default;
			const struct clk_pll_freq_table	*freq_table;
			int				lock_delay;
			unsigned long			fixed_rate;
			u32				misc0;
			u32				misc1;
			u32				misc2;
			u32				misc3;
			u32				misc4;
			u32				misc5;
			bool				defaults_set;
			bool				vco_out;
			struct clk_pll_controls		*controls;
			struct clk_pll_div_layout	*div_layout;
			u32	(*round_p_to_pdiv)(u32 p, u32 *pdiv);
			int	(*dyn_ramp)(struct clk *c,
					    struct clk_pll_freq_table *cfg);
			void	(*set_defaults)(struct clk *c,
						unsigned long input_rate);
		} pll;
		struct {
			void				*cl_dvfs;
			struct clk			*consumer;
		} dfll;
		struct {
			unsigned long			default_rate;
		} pll_div;
		struct {
			u32				sel;
			u32				reg_mask;
		} mux;
		struct {
			struct clk			*main;
			struct clk			*backup;
			struct clk			*dynamic;
			unsigned long			edp_safe_rate;
			unsigned long			backup_rate;
			seqcount_t			backup_seqcnt;
			enum cpu_mode			mode;
		} cpu;
		struct {
			u32				div71;
		} cclk;
		struct {
			struct clk			*pclk;
			struct clk			*hclk;
			struct clk			*sclk_low;
			struct clk			*sclk_high;
			unsigned long			threshold;
		} system;
		struct {
			struct clk			*top_user;
			struct clk			*slow_user;
		} cbus;
		struct {
			struct list_head		node;
			bool				enabled;
			unsigned long			rate;
			const char			*client_id;
			struct clk			*client;
			u32				client_div;
			enum shared_bus_users_mode	mode;
			u32				usage_flag;
			int				stats_idx;
		} shared_bus_user;
		struct {
			struct tegra_clk_export_ops	*ops;
		} export_clk;
	} u;

	struct raw_notifier_head			*rate_change_nh;
	wait_queue_head_t				*debug_poll_qh;
	int						rate_changed;

	struct mutex *cross_clk_mutex;
	struct mutex mutex;
	spinlock_t spinlock;
	unsigned long fixed_target_rate;
};

#else

struct clk_tegra {
	/* node for master clocks list */
	struct list_head	node;	/* node for list of all clocks */
	struct clk_lookup	lookup;
	struct clk_hw		hw;

	bool			set;
	unsigned long		fixed_rate;
	unsigned long		max_rate;
	unsigned long		min_rate;
	u32			flags;
	const char		*name;

	enum clk_state		state;
	u32			div;
	u32			mul;

	u32				reg;
	u32				reg_shift;

	struct list_head		shared_bus_list;

	union {
		struct {
			unsigned int			clk_num;
		} periph;
		struct {
			unsigned long			input_min;
			unsigned long			input_max;
			unsigned long			cf_min;
			unsigned long			cf_max;
			unsigned long			vco_min;
			unsigned long			vco_max;
			const struct clk_pll_freq_table	*freq_table;
			int				lock_delay;
			unsigned long			fixed_rate;
		} pll;
		struct {
			u32				sel;
			u32				reg_mask;
		} mux;
		struct {
			struct clk			*main;
			struct clk			*backup;
		} cpu;
		struct {
			struct list_head		node;
			bool				enabled;
			unsigned long			rate;
		} shared_bus_user;
	} u;

	void (*reset)(struct clk_hw *, bool);
	int (*clk_cfg_ex)(struct clk_hw *, enum tegra_clk_ex_param, u32);
};
#endif /* !CONFIG_COMMON_CLK */

struct clk_duplicate {
	const char *name;
	struct clk_lookup lookup;
};

struct tegra_clk_init_table {
	const char *name;
	const char *parent;
	unsigned long rate;
	bool enabled;
	unsigned long platform;
	struct clk *c;		/* clock to be initialized */
	struct clk *p;		/* clock parent specified in the table */
};

#define TEGRA_CLK_INIT_PLATFORM_ALL	0
#define TEGRA_CLK_INIT_PLATFORM_SI	BIT(0)
#define TEGRA_CLK_INIT_PLATFORM_QT	BIT(1)
#define TEGRA_CLK_INIT_PLATFORM_FPGA	BIT(2)
#define TEGRA_CLK_INIT_PLATFORM_LINSIM	BIT(3)
#define TEGRA_CLK_INIT_CPU_ASIM		BIT(4)
#define TEGRA_CLK_INIT_PLATFORM_NON_SI	(BIT(1) | BIT(2) | BIT(3))

#ifndef CONFIG_COMMON_CLK
void tegra_init_max_rate(struct clk *c, unsigned long max_rate);
void tegra_init_min_rate(struct clk *c, unsigned long min_rate);
void tegra_clk_preset_emc_monitor(unsigned long rate);
void tegra_periph_clk_safe_rate_init(struct clk *c);
void tegra_clk_verify_parents(void);
void tegra_clk_set_disabled_div_all(void);
void clk_init(struct clk *clk);
unsigned long tegra_clk_measure_input_freq(void);
unsigned long clk_get_rate_locked(struct clk *c);
void tegra_clk_init_from_dt(const char *dt_table_name);
void tegra_clk_init_cbus_plls_from_table(struct tegra_clk_init_table *table);
void clk_set_cansleep(struct clk *c);
unsigned long clk_get_min_rate(struct clk *c);
unsigned long clk_get_max_rate(struct clk *c);
bool tegra_is_clk_initialized(struct clk *c);

int clk_set_rate_locked(struct clk *c, unsigned long rate);
int clk_rate_change_notify(struct clk *c, unsigned long rate);
int clk_set_parent_locked(struct clk *c, struct clk *parent);
int clk_reparent(struct clk *c, struct clk *parent);
long clk_round_rate_locked(struct clk *c, unsigned long rate);
int tegra_clk_shared_bus_update(struct clk *c);
int tegra_clk_shared_bus_update_locked(struct clk *c);
void tegra3_set_cpu_skipper_delay(int delay);
unsigned long tegra_clk_measure_input_freq(void);
int clk_enable_locked(struct clk *c);
void clk_disable_locked(struct clk *c);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static inline bool tegra_clk_is_parent_allowed(struct clk *c, struct clk *p)
{ return true; }
#else
bool tegra_clk_is_parent_allowed(struct clk *c, struct clk *p);
#endif
#endif /* !CONFIG_COMMON_CLK */

struct tegra_sku_rate_limit {
	const char *clk_name;
	unsigned long max_rate;
	int sku_ids[MAX_SAME_LIMIT_SKU_IDS];
};

void tegra2_init_clocks(void);
void tegra30_init_clocks(void);
void tegra11x_init_clocks(void);
void tegra12x_init_clocks(void);
void tegra21x_init_clocks(void);
void tegra11x_clk_init_la(void);
void tegra_common_init_clock(void);
struct clk *tegra_get_clock_by_name(const char *name);

void tegra_clk_init_from_table(struct tegra_clk_init_table *table);

/* Reset driver through Reset framework */
extern int tegra_non_dt_clock_reset_init(void);

/**
  * Returns dfll requested booting frequency, which
  * is configured by bootloader as cpu clock source.
  * If booting with different cpu clock source then,
  * returns 0.
  */
unsigned long int tegra_dfll_boot_req_khz(void);

#ifndef CONFIG_COMMON_CLK
void tegra_shared_bus_stats_allocate(struct clk *c, struct bus_stats *stats);
int tegra_shared_bus_stats_copy_to_buffer(
	struct clk *c, struct bus_stats *stats, char *buf, int len);
void tegra_shared_bus_stats_update(
	struct bus_stats *stats, int user_idx, int rate_idx);

int clk_config_cpu_edp_safe_rate(struct clk *cpu, struct clk *cluster_clk,
				 unsigned long rate);

static inline unsigned long clk_get_cpu_edp_safe_rate(struct clk *cluster_clk)
{
	return cluster_clk->u.cpu.edp_safe_rate;
}

static inline void tegra_dfll_set_cl_dvfs_data(struct clk *c, void *cld)
{
	if (c->flags & DFLL)
		c->u.dfll.cl_dvfs = cld;
}

static inline void *tegra_dfll_get_cl_dvfs_data(struct clk *c)
{
	return c->flags & DFLL ? c->u.dfll.cl_dvfs : ERR_PTR(-ENODATA);
}

static inline bool clk_is_auto_dvfs(struct clk *c)
{
	return c->auto_dvfs;
}

static inline bool clk_is_dvfs(struct clk *c)
{
	return (c->dvfs != NULL);
}

static inline bool clk_cansleep(struct clk *c)
{
	return c->cansleep;
}

static inline bool clk_can_set_disabled_div(struct clk *c)
{
	return c->set_disabled_div;
}

static inline void clk_lock_save(struct clk *c, unsigned long *flags)
{
	if (clk_cansleep(c)) {
		*flags = 0;
		mutex_lock(&c->mutex);
		if (c->cross_clk_mutex)
			mutex_lock(c->cross_clk_mutex);
	} else {
		spin_lock_irqsave(&c->spinlock, *flags);
	}
}

static inline void clk_unlock_restore(struct clk *c, unsigned long *flags)
{
	if (clk_cansleep(c)) {
		if (c->cross_clk_mutex)
			mutex_unlock(c->cross_clk_mutex);
		mutex_unlock(&c->mutex);
	} else {
		spin_unlock_irqrestore(&c->spinlock, *flags);
	}
}

static inline int tegra_clk_prepare_enable(struct clk *c)
{
	if (clk_cansleep(c))
		return clk_prepare_enable(c);
	return clk_enable(c);
}

static inline void tegra_clk_disable_unprepare(struct clk *c)
{
	if (clk_cansleep(c))
		clk_disable_unprepare(c);
	else
		clk_disable(c);
}

static inline void clk_lock_init(struct clk *c)
{
	mutex_init(&c->mutex);
	spin_lock_init(&c->spinlock);
}
#endif

#ifdef CONFIG_DEBUG_FS

/* Extended version of the standard macro with poll function setting */
#define DEFINE_SIMPLE_POLL_ATTRIBUTE(__fops, __get, __set, __poll, __fmt) \
static int __fops ## _open(struct inode *inode, struct file *file)	\
{									\
	__simple_attr_check_format(__fmt, 0ull);			\
	return simple_attr_open(inode, file, __get, __set, __fmt);	\
}									\
static const struct file_operations __fops = {				\
	.owner	 = THIS_MODULE,						\
	.open	 = __fops ## _open,					\
	.release = simple_attr_release,					\
	.read	 = simple_attr_read,					\
	.write	 = simple_attr_write,					\
	.llseek	 = generic_file_llseek,					\
	.poll    = __poll,						\
};

void tegra_clk_add_pto_entries(struct tegra_pto_table *pto_table);

#endif

#endif
#endif
