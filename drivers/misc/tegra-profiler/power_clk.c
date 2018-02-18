/*
 * drivers/misc/tegra-profiler/power_clk.c
 *
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
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
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpufreq.h>
#include <linux/clk.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/timer.h>
#include <linux/err.h>

#include <linux/tegra_profiler.h>

#include "power_clk.h"
#include "quadd.h"
#include "hrt.h"
#include "comm.h"
#include "debug.h"

#define PCLK_MAX_VALUES	32

struct power_clk_data {
	unsigned long value;
	unsigned long prev;
};

#define PCLK_NB_GPU	0
#define PCLK_NB_EMC	0

enum {
	PCLK_NB_CPU_FREQ,
	PCLK_NB_CPU_HOTPLUG,
	PCLK_NB_CPU_MAX,
};

#define PCLK_NB_MAX	PCLK_NB_CPU_MAX

struct power_clk_source {
	int type;

	struct clk *clkp;
	struct notifier_block nb[PCLK_NB_MAX];

	int nr;
	struct power_clk_data data[PCLK_MAX_VALUES];

	atomic_t active;
	struct mutex lock;
};

struct power_clk_context_s {
	struct power_clk_source cpu;
	struct power_clk_source gpu;
	struct power_clk_source emc;

	struct timer_list timer;
	unsigned int period;

	struct quadd_ctx *quadd_ctx;
};

enum {
	QUADD_POWER_CLK_CPU = 1,
	QUADD_POWER_CLK_GPU,
	QUADD_POWER_CLK_EMC,
};

static struct power_clk_context_s power_ctx;

static void make_sample(void)
{
	int i;
	u32 extra_cpus[NR_CPUS];
	struct power_clk_source *s;
	struct quadd_iovec vec;

	struct quadd_record_data record;
	struct quadd_power_rate_data *power_rate = &record.power_rate;

	record.record_type = QUADD_RECORD_TYPE_POWER_RATE;

	power_rate->time = quadd_get_time();

	s = &power_ctx.cpu;
	mutex_lock(&s->lock);
	if (atomic_read(&s->active)) {
		power_rate->nr_cpus = s->nr;
		for (i = 0; i < s->nr; i++)
			extra_cpus[i] = s->data[i].value;
	} else {
		power_rate->nr_cpus = 0;
	}
	mutex_unlock(&s->lock);

	s = &power_ctx.gpu;
	mutex_lock(&s->lock);
	if (atomic_read(&s->active))
		power_rate->gpu = s->data[0].value;
	else
		power_rate->gpu = 0;

	mutex_unlock(&s->lock);

	s = &power_ctx.emc;
	mutex_lock(&s->lock);
	if (atomic_read(&s->active))
		power_rate->emc = s->data[0].value;
	else
		power_rate->emc = 0;

	mutex_unlock(&s->lock);
/*
	pr_debug("make_sample: cpu: %u/%u/%u/%u, gpu: %u, emc: %u\n",
		 extra_cpus[0], extra_cpus[1], extra_cpus[2], extra_cpus[3],
		 power_rate->gpu, power_rate->emc);
*/
	vec.base = extra_cpus;
	vec.len = power_rate->nr_cpus * sizeof(extra_cpus[0]);

	quadd_put_sample(&record, &vec, 1);
}

static void
make_sample_hotplug(int cpu, int is_online)
{
	struct quadd_record_data record;
	struct quadd_hotplug_data *s = &record.hotplug;

	record.record_type = QUADD_RECORD_TYPE_HOTPLUG;

	s->cpu = cpu;
	s->is_online = is_online ? 1 : 0;
	s->time = quadd_get_time();
	s->reserved = 0;

	quadd_put_sample(&record, NULL, 0);
}

static inline int
is_data_changed(struct power_clk_source *s)
{
	int i;

	for (i = 0; i < s->nr; i++) {
		if (s->data[i].value != s->data[i].prev)
			return 1;
	}

	return 0;
}

static inline void
update_data(struct power_clk_source *s)
{
	int i;

	for (i = 0; i < s->nr; i++)
		s->data[i].prev = s->data[i].value;
}

static void check_clks(void)
{
	struct power_clk_source *s;
	int changed = 0;

	s = &power_ctx.gpu;
	mutex_lock(&s->lock);
	if (is_data_changed(s)) {
		update_data(s);
		changed = 1;
	}
	mutex_unlock(&s->lock);

	s = &power_ctx.emc;
	mutex_lock(&s->lock);
	if (is_data_changed(s)) {
		update_data(s);
		changed = 1;
	}
	mutex_unlock(&s->lock);

	if (changed) {
		pr_debug("gpu: %lu, emc: %lu\n",
			 power_ctx.gpu.data[0].value,
			 power_ctx.emc.data[0].value);

		make_sample();
	}
}

static void check_source(struct power_clk_source *s)
{
	mutex_lock(&s->lock);

	if (!is_data_changed(s))
		goto out_unlock;

	pr_debug("cpu: %lu/%lu/%lu/%lu\n",
		 power_ctx.cpu.data[0].value,
		 power_ctx.cpu.data[1].value,
		 power_ctx.cpu.data[2].value,
		 power_ctx.cpu.data[3].value);

	update_data(s);
	mutex_unlock(&s->lock);

	make_sample();
	return;

out_unlock:
	mutex_unlock(&s->lock);
}

static void
read_source(struct power_clk_source *s, int cpu)
{
	mutex_lock(&s->lock);

	switch (s->type) {
	case QUADD_POWER_CLK_CPU:
		/* update cpu frequency */
		if (cpu < 0 || cpu >= max_t(int, s->nr, nr_cpu_ids)) {
			pr_err_once("error: cpu id: %d\n", cpu);
			break;
		}

		s->data[cpu].value = cpufreq_get(cpu);
		pr_debug("QUADD_POWER_CLK_CPU, cpu: %d, value: %lu\n",
			 cpu, s->data[cpu].value);
		break;

	case QUADD_POWER_CLK_GPU:
		/* update gpu frequency */
		s->clkp = clk_get_sys("3d", NULL);
		if (!IS_ERR_OR_NULL(s->clkp)) {
			s->data[0].value =
				clk_get_rate(s->clkp) / 1000;
			clk_put(s->clkp);
		}
		pr_debug("QUADD_POWER_CLK_GPU, value: %lu\n",
			 s->data[0].value);
		break;

	case QUADD_POWER_CLK_EMC:
		/* update emc frequency */
		s->clkp = clk_get_sys("cpu", "emc");
		if (!IS_ERR_OR_NULL(s->clkp)) {
			s->data[0].value =
				clk_get_rate(s->clkp) / 1000;
			clk_put(s->clkp);
		}
		pr_debug("QUADD_POWER_CLK_EMC, value: %lu\n",
			 s->data[0].value);
		break;

	default:
		pr_err_once("error: invalid power_clk type\n");
		break;
	}

	mutex_unlock(&s->lock);
}

static int
gpu_notifier_call(struct notifier_block *nb,
		  unsigned long action, void *data)
{
	read_source(&power_ctx.gpu, -1);
	check_clks();

	return NOTIFY_DONE;
}

static int
emc_notifier_call(struct notifier_block *nb,
		  unsigned long action, void *data)
{
	read_source(&power_ctx.emc, -1);
	check_clks();

	return NOTIFY_DONE;
}

static void
read_cpufreq(struct power_clk_source *s, struct cpufreq_freqs *freq)
{
	int cpu, cpufreq;

	mutex_lock(&s->lock);

	if (!atomic_read(&s->active))
		goto out_unlock;

	cpu = freq->cpu;
	cpufreq = freq->new;

	pr_debug("cpu: %d, cpufreq: %d\n", cpu, cpufreq);

	if (cpu >= s->nr) {
		pr_err_once("error: cpu id: %d\n", cpu);
		goto out_unlock;
	}

	s->data[cpu].value = cpufreq;

	pr_debug("[%d] cpufreq: %u --> %u\n",
		 cpu, freq->old, cpufreq);

	mutex_unlock(&s->lock);
	check_source(s);
	return;

out_unlock:
	mutex_unlock(&s->lock);
}

static int
cpufreq_notifier_call(struct notifier_block *nb,
		      unsigned long action, void *hcpu)
{
	struct cpufreq_freqs *freq;
	struct power_clk_source *s = &power_ctx.cpu;

	if (!atomic_read(&s->active))
		return 0;

	pr_debug("action: %lu\n", action);

	if (action == CPUFREQ_POSTCHANGE) {
		freq = hcpu;
		read_cpufreq(s, freq);
	}

	return 0;
}

static int
cpu_hotplug_notifier_call(struct notifier_block *nb,
			  unsigned long action, void *hcpu)
{
	int cpu;
	struct power_clk_source *s = &power_ctx.cpu;

	if (!atomic_read(&s->active))
		return NOTIFY_DONE;

	cpu = (long)hcpu;

	pr_debug("cpu: %d, action: %lu\n", cpu, action);

	if (cpu >= s->nr) {
		pr_err_once("error: cpu id: %d\n", cpu);
		return NOTIFY_DONE;
	}

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		make_sample_hotplug(cpu, 1);
		break;

	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		mutex_lock(&s->lock);
		if (atomic_read(&s->active))
			s->data[cpu].value = 0;
		mutex_unlock(&s->lock);

		make_sample_hotplug(cpu, 0);
		break;

	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static void reset_data(struct power_clk_source *s)
{
	int i;

	mutex_lock(&s->lock);
	for (i = 0; i < s->nr; i++) {
		s->data[i].value = 0;
		s->data[i].prev = 0;
	}
	mutex_unlock(&s->lock);
}

static void init_source(struct power_clk_source *s,
			int nr_values,
			int type)
{
	s->type = type;
	s->nr = min_t(int, nr_values, PCLK_MAX_VALUES);
	atomic_set(&s->active, 0);
	mutex_init(&s->lock);

	reset_data(s);
}

static void
power_clk_work_func(struct work_struct *work)
{
	read_source(&power_ctx.gpu, -1);
	read_source(&power_ctx.emc, -1);

	check_clks();
}

static DECLARE_WORK(power_clk_work, power_clk_work_func);

static void power_clk_timer(unsigned long data)
{
	struct timer_list *timer = &power_ctx.timer;

	schedule_work(&power_clk_work);
	timer->expires = jiffies + msecs_to_jiffies(power_ctx.period);
	add_timer(timer);
}

static void
read_all_sources_work_func(struct work_struct *work)
{
	int cpu_id;
	struct power_clk_source *s = &power_ctx.cpu;

	for_each_possible_cpu(cpu_id)
		read_source(s, cpu_id);

	read_source(&power_ctx.gpu, -1);
	read_source(&power_ctx.emc, -1);

	check_clks();
	check_source(s);
}

static DECLARE_WORK(read_all_sources_work, read_all_sources_work_func);

int quadd_power_clk_start(void)
{
	struct power_clk_source *s;
	struct timer_list *timer = &power_ctx.timer;
	struct quadd_parameters *param = &power_ctx.quadd_ctx->param;

	if (param->power_rate_freq == 0) {
		pr_info("power_clk is not started\n");
		return 0;
	}

#ifdef CONFIG_COMMON_CLK
	power_ctx.period = 0;
#else
	power_ctx.period = MSEC_PER_SEC / param->power_rate_freq;
	pr_info("clk: use timer\n");
#endif

	pr_info("power_clk: start, freq: %d\n",
		param->power_rate_freq);

	/* setup gpu frequency */
	s = &power_ctx.gpu;
	s->clkp = clk_get_sys("3d", NULL);
	if (!IS_ERR_OR_NULL(s->clkp)) {
#ifdef CONFIG_COMMON_CLK
		int status = clk_notifier_register(s->clkp, s->nb);

		if (status < 0) {
			pr_err("error: could not setup gpu freq\n");
			clk_put(s->clkp);
			return status;
		}
#endif
		clk_put(s->clkp);
		reset_data(s);
		atomic_set(&s->active, 1);
	} else {
		pr_warn("warning: could not setup gpu freq\n");
		atomic_set(&s->active, 0);
	}

	/* setup emc frequency */
	s = &power_ctx.emc;
	s->clkp = clk_get_sys("cpu", "emc");
	if (!IS_ERR_OR_NULL(s->clkp)) {
#ifdef CONFIG_COMMON_CLK
		int status = clk_notifier_register(s->clkp, s->nb);

		if (status < 0) {
			pr_err("error: could not setup emc freq\n");
			clk_put(s->clkp);
			return status;
		}
#endif
		clk_put(s->clkp);
		reset_data(s);
		atomic_set(&s->active, 1);
	} else {
		pr_warn("warning: could not setup emc freq\n");
		atomic_set(&s->active, 0);
	}

	/* setup cpu frequency notifier */
	s = &power_ctx.cpu;
	reset_data(s);
	atomic_set(&s->active, 1);

	if (power_ctx.period > 0) {
		init_timer(timer);
		timer->function = power_clk_timer;
		timer->expires = jiffies + msecs_to_jiffies(power_ctx.period);
		timer->data = 0;
		add_timer(timer);
	}

	schedule_work(&read_all_sources_work);

	return 0;
}

void quadd_power_clk_stop(void)
{
	struct power_clk_source *s;
	struct quadd_parameters *param = &power_ctx.quadd_ctx->param;

	if (param->power_rate_freq == 0)
		return;

	if (power_ctx.period > 0)
		del_timer_sync(&power_ctx.timer);

	s = &power_ctx.gpu;
	mutex_lock(&s->lock);
	if (atomic_cmpxchg(&s->active, 1, 0)) {
#ifdef CONFIG_COMMON_CLK
		if (s->clkp)
			clk_notifier_unregister(s->clkp, s->nb);
#endif
	}
	mutex_unlock(&s->lock);

	s = &power_ctx.emc;
	mutex_lock(&s->lock);
	if (atomic_cmpxchg(&s->active, 1, 0)) {
#ifdef CONFIG_COMMON_CLK
		if (s->clkp)
			clk_notifier_unregister(s->clkp, s->nb);
#endif
	}
	mutex_unlock(&s->lock);

	s = &power_ctx.cpu;
	mutex_lock(&s->lock);
	atomic_set(&s->active, 0);
	mutex_unlock(&s->lock);

	pr_info("power_clk: stop\n");
}

int quadd_power_clk_init(struct quadd_ctx *quadd_ctx)
{
	struct power_clk_source *s;

	s = &power_ctx.gpu;
	s->nb[PCLK_NB_GPU].notifier_call = gpu_notifier_call;
	init_source(s, 1, QUADD_POWER_CLK_GPU);

	s = &power_ctx.emc;
	s->nb[PCLK_NB_EMC].notifier_call = emc_notifier_call;
	init_source(s, 1, QUADD_POWER_CLK_EMC);

	s = &power_ctx.cpu;
	s->nb[PCLK_NB_CPU_FREQ].notifier_call = cpufreq_notifier_call;
	s->nb[PCLK_NB_CPU_HOTPLUG].notifier_call = cpu_hotplug_notifier_call;
	init_source(s, nr_cpu_ids, QUADD_POWER_CLK_CPU);

	power_ctx.quadd_ctx = quadd_ctx;

	cpufreq_register_notifier(&s->nb[PCLK_NB_CPU_FREQ],
				  CPUFREQ_TRANSITION_NOTIFIER);
	register_cpu_notifier(&s->nb[PCLK_NB_CPU_HOTPLUG]);

	return 0;
}

void quadd_power_clk_deinit(void)
{
	struct power_clk_source *s = &power_ctx.cpu;

	quadd_power_clk_stop();

	cpufreq_unregister_notifier(&s->nb[PCLK_NB_CPU_FREQ],
				    CPUFREQ_TRANSITION_NOTIFIER);
	unregister_cpu_notifier(&s->nb[PCLK_NB_CPU_HOTPLUG]);
}
