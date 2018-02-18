/*
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __APE_ACTMON_H
#define __APE_ACTMON_H
#include <linux/spinlock.h>

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
/* Units:
 * - frequency in kHz
 * - coefficients, and thresholds in %
 * - sampling period in ms
 * - window in sample periods (value = setting + 1)
 */
struct actmon_dev {
	u32	reg;
	int	irq;
	struct device *device;

	const char	*dev_id;
	const char	*con_id;
	const char *clk_name;
	struct clk	*clk;

	unsigned long	max_freq;
	unsigned long	target_freq;
	unsigned long	cur_freq;
	unsigned long	suspend_freq;

	unsigned long	avg_actv_freq;
	unsigned long	avg_band_freq;
	unsigned int	avg_sustain_coef;
	u32	avg_count;

	unsigned long	boost_freq;
	unsigned long	boost_freq_step;
	unsigned int	boost_up_coef;
	unsigned int	boost_down_coef;
	unsigned int	boost_up_threshold;
	unsigned int	boost_down_threshold;

	u8	up_wmark_window;
	u8	down_wmark_window;
	u8	avg_window_log2;
	u32	count_weight;

	enum actmon_type	type;
	enum actmon_state	state;
	enum actmon_state	saved_state;

	spinlock_t	lock;

};

struct actmon {
	struct clk *clk;
	unsigned long freq;
	unsigned long sampling_period;
	struct notifier_block clk_rc_nb;
	void __iomem *base;
};

int ape_actmon_init(struct platform_device *pdev);
int ape_actmon_exit(struct platform_device *pdev);
void actmon_rate_change(unsigned long freq, bool override);
#endif
