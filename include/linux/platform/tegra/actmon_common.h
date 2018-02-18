/*
 * Copyright (C) 2016, NVIDIA Corporation. All rights reserved.
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
#ifndef ACTMON_COMMON_H
/* START: These device register offsets have common value across socs */
#define ACTMON_CMN_DEV_CTRL				0x00
#define ACTMON_CMN_DEV_CTRL_ENB			(0x1 << 31)
#define ACTMON_CMN_DEV_CTRL_UP_WMARK_NUM_SHIFT	26
#define ACTMON_CMN_DEV_CTRL_UP_WMARK_NUM_MASK	(0x7 << 26)
#define ACTMON_CMN_DEV_CTRL_DOWN_WMARK_NUM_SHIFT	21
#define ACTMON_CMN_DEV_CTRL_DOWN_WMARK_NUM_MASK (0x7 << 21)

/* Common dev interrupt status bits across socs */
#define ACTMON_CMN_DEV_INTR_UP_WMARK		(0x1 << 31)
#define ACTMON_CMN_DEV_INTR_DOWN_WMARK		(0x1 << 30)
/* END: common device regs across socs */

#define ACTMON_DEFAULT_AVG_WINDOW_LOG2		7
#define ACTMON_DEFAULT_AVG_BAND			6	/* 1/10 of % */
#define ACTMON_DEFAULT_SAMPLING_PERIOD		7

#define DEFAULT_SUSPEND_FREQ		204000
#define DEFAULT_BOOST_UP_COEF	200
#define DEFAULT_BOOST_DOWN_COEF	50
#define DEFAULT_BOOST_UP_THRESHOLD	30
#define DEFAULT_BOOST_DOWN_THRESHOLD	20
#define DEFAULT_UP_WMARK_WINDOW	3
#define DEFAULT_DOWN_WMARK_WINDOW	2
#define DEFAULT_EWMA_COEF_K		6
#define DEFAULT_COUNT_WEIGHT		0x53
#define FREQ_SAMPLER 1
#define LOAD_SAMPLER 0
#define DEFAULT_ACTMON_TYPE	FREQ_SAMPLER
/* Maximum frequency EMC is running at when sourced from PLLP. This is
 * really a short-cut, but it is true for all Tegra3  platforms
 */
#define EMC_PLLP_FREQ_MAX			204000

enum actmon_devices {
	MC_ALL, /* Should match with device sequence in dt */
	MAX_DEVICES,
};

enum actmon_type {
	ACTMON_LOAD_SAMPLER,
	ACTMON_FREQ_SAMPLER,
};

enum actmon_state {
	ACTMON_UNINITIALIZED = -1,
	ACTMON_OFF = 0,
	ACTMON_ON  = 1,
	ACTMON_SUSPENDED = 2,
};
struct actmon_dev;
struct actmon_drv_data;
struct dev_reg_ops {
	void (*set_init_avg)(u32 value, void __iomem *base);
	void (*set_avg_up_wm)(u32 value, void __iomem *base);
	void (*set_avg_dn_wm)(u32 value, void __iomem *base);
	void (*set_dev_up_wm)(u32 value, void __iomem *base);
	void (*set_dev_dn_wm)(u32 value, void __iomem *base);
	void (*enb_dev_wm)(u32 *value);
	void (*disb_dev_up_wm)(u32 *value);
	void (*disb_dev_dn_wm)(u32 *value);
	void (*set_intr_st)(u32 value, void __iomem *base);
	void (*init_dev_cntrl)(struct actmon_dev *, void __iomem *base);
	void (*enb_dev_intr)(u32 value, void __iomem *base);
	void (*enb_dev_intr_all)(void __iomem *base);
	void (*set_cnt_wt)(u32 value, void __iomem *base);
	u32 (*get_intr_st)(void __iomem *base);
	u32 (*get_dev_intr_enb)(void __iomem *base);
	u32 (*get_dev_intr)(void __iomem *base);
	u32 (*get_raw_cnt)(void __iomem *base);
	u32 (*get_avg_cnt)(void __iomem *base);
	u32 (*get_cum_cnt)(void __iomem *base);
};

/* Units:
 * - frequency in kHz
 * - coefficients, and thresholds in %
 * - sampling period in ms
 * - window in sample periods (value = setting + 1)
 */
struct actmon_dev {
	struct device_node *dn;
	u32 reg_offs;
	u32 glb_status_irq_mask;
	const char *dev_name;
	const char *con_id;
	void *clnt;

	unsigned long max_freq;
	unsigned long target_freq;
	unsigned long cur_freq;
	unsigned long suspend_freq;

	unsigned long avg_actv_freq;
	unsigned long avg_band_freq;
	unsigned int avg_sustain_coef;
	u32 avg_count;
	u32 avg_dependency_threshold;

	unsigned long boost_freq;
	unsigned long boost_freq_step;
	unsigned int boost_up_coef;
	unsigned int boost_down_coef;
	unsigned int boost_up_threshold;
	unsigned int boost_down_threshold;

	u8 up_wmark_window;
	u8 down_wmark_window;
	u8 avg_window_log2;
	u32 count_weight;

	enum actmon_type type;
	enum actmon_state state;
	enum actmon_state saved_state;

	struct dev_reg_ops ops;
	void (*actmon_dev_set_rate)(struct actmon_dev *, unsigned long);
	unsigned long (*actmon_dev_get_rate)(struct actmon_dev *);
	unsigned long (*actmon_dev_post_change_rate)(struct actmon_dev *,
			void *v);
	void (*actmon_dev_clk_enable)(struct actmon_dev *);
	spinlock_t lock;
	struct notifier_block rate_change_nb;
	struct kobj_attribute avgact_attr;
};

struct actmon_reg_ops {
	void (*set_sample_prd)(u32 value, void __iomem *base);
	void (*set_glb_intr)(u32 value, void __iomem *base);
	u32 (*get_glb_intr_st)(void __iomem *base);
};
struct actmon_drv_data {
	void __iomem *base;
	struct platform_device *pdev;
	int virq;
	struct clk *actmon_clk;
	u8 sample_period;
	unsigned long freq;
	struct reset_control *actmon_rst;
	struct actmon_dev devices[MAX_DEVICES];
	int (*clock_init)(struct platform_device *pdev);
	int (*clock_deinit)(struct platform_device *pdev);
	int (*reset_init)(struct platform_device *pdev);
	int (*reset_deinit)(struct platform_device *pdev);
	void (*dev_free_resource)(struct actmon_dev *adev,
		struct platform_device *pdev);
	struct actmon_reg_ops ops;
#if CONFIG_DEBUG_FS
	struct dentry *dfs_root;
#endif
	struct kobject *actmon_kobj;
};

static inline void actmon_wmb(void)
{
	dsb(sy);
}

static inline u32 actmon_dev_readl(void __iomem *base,
			u32 offset)
{
	return __raw_readl(base + offset);
}

static inline void actmon_dev_writel(void __iomem *base, u32
				offset, u32 val)
{
	__raw_writel(val, base + offset);
}
int __init actmon_platform_register(struct platform_device *pdev);
int actmon_dev_platform_register(struct actmon_dev *adev,
		struct platform_device *pdev);
#endif /* ACTMON_COMMON_H */
