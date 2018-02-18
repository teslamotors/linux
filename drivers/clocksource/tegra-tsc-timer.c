/*
 * drivers/clocksource/tegra-tsc-timer.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *
 * Copyright (C) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/init.h>
#include <linux/err.h>
#include <linux/time.h>
#include <linux/clocksource.h>
#include <linux/cpu.h>
#include <linux/io.h>
#include <linux/cpu_pm.h>
#include <linux/of.h>
#include <linux/tegra-timer.h>
#include <linux/tegra-soc.h>
#include <linux/of_address.h>
#include <linux/sched_clock.h>

#include <asm/mach/time.h>
#include <asm/cputype.h>
#include <asm/system.h>
#include <asm/arch_timer.h>
#include <asm/delay.h>

#include <linux/platform/tegra/clock.h>
#include <linux/platform/tegra/common.h>

static u32 arch_timer_us_mult, arch_timer_us_shift;

bool arch_timer_initialized;
static struct delay_timer arch_delay_timer;

/* Is the optional system timer available? */
static int local_timer_is_architected(void)
{
	return (cpu_architecture() >= CPU_ARCH_ARMv7) &&
	       ((read_cpuid_ext(CPUID_EXT_PFR1) >> 16) & 0xf) == 1;
}

static unsigned long arch_timer_read_current_timer(void)
{
	cycle_t cval = 0;

	asm volatile("mrrc p15, 0, %Q0, %R0, c14" : "=r" (cval));
	return cval;
}

void __init tegra_cpu_timer_init(void)
{
	u32 tsc_ref_freq;

	if (!local_timer_is_architected())
		return;

	tsc_ref_freq = tegra_clk_measure_input_freq();
	if (tsc_ref_freq == 115200 || tsc_ref_freq == 230400) {
		/*
		 * OSC detection function will bug out if revision is not
		 * QT and the detected frequency is one of these two.
		 */
		tsc_ref_freq = 13000000;
		pr_info("fake tsc_ref_req=%d in QT\n", tsc_ref_freq);
	}

	clocks_calc_mult_shift(&arch_timer_us_mult, &arch_timer_us_shift,
				tsc_ref_freq, USEC_PER_SEC, 0);

	/* register arch timer as delay timer */
	arch_delay_timer.read_current_timer = &arch_timer_read_current_timer;
	arch_delay_timer.freq = tsc_ref_freq;
	register_current_timer_delay(&arch_delay_timer);

	return;
}

static void tegra_arch_timer_per_cpu_init(void)
{
	if (tegra_cpu_is_secure())
		return;

	if (arch_timer_initialized) {
		u32 tsc_ref_freq = tegra_clk_measure_input_freq();

		/*
		 * OSC detection function will bug out if revision is not QT and
		 * the detected frequency is one of these two.
		 */
		if (tsc_ref_freq == 115200 || tsc_ref_freq == 230400)
			tsc_ref_freq = 13000000;

		/* Program CNTFRQ to the input frequency.
		   NOTE: this is a write once (per CPU reset) register. */
		__asm__("mcr p15, 0, %0, c14, c0, 0\n" : : "r" (tsc_ref_freq));
	}
}

static int arch_timer_cpu_notify(struct notifier_block *self,
				    unsigned long action, void *data)
{
	switch (action) {
	case CPU_STARTING:
	case CPU_STARTING_FROZEN:
		tegra_arch_timer_per_cpu_init();
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block arch_timer_cpu_nb = {
	.notifier_call = arch_timer_cpu_notify,
};

static int arch_timer_cpu_pm_notify(struct notifier_block *self,
				    unsigned long action, void *data)
{
	switch (action) {
	case CPU_PM_EXIT:
		tegra_arch_timer_per_cpu_init();
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block arch_timer_cpu_pm_nb = {
	.notifier_call = arch_timer_cpu_pm_notify,
};

int __init tegra_init_arch_timer(void)
{
	if (!local_timer_is_architected())
		return -ENODEV;

	register_cpu_notifier(&arch_timer_cpu_nb);
	cpu_pm_register_notifier(&arch_timer_cpu_pm_nb);
	arch_timer_initialized = true;
	return 0;
}

void __init tegra_init_late_timer(void)
{}

int tegra_cpu_timer_get_remain(s64 *time)
{
	s32 cntp_tval;
	int ret = 0;

	asm volatile("mrc p15, 0, %0, c14, c2, 0" : "=r" (cntp_tval));

	if (cntp_tval <= 0)
		ret = -ETIME;
	else
		*time = (s64)((s64)cntp_tval * arch_timer_us_mult)
			>> arch_timer_us_shift;

	return ret;
}
