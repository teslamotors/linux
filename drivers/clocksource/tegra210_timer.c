/*
 * drivers/clocksource/tegra210_timer.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/clockchips.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/percpu.h>
#include <linux/syscore_ops.h>
#include <linux/tick.h>
#include <linux/clk.h>
#include <linux/tegra-timer.h>

static u32 tegra210_timer_freq;
static void __iomem *tegra210_timer_reg_base;
phys_addr_t timer_reg_base_pa = 0x60005000;
static u32 usec_config;
static u32 timer_us_mult, timer_us_shift;

#define TIMER_FOR_CPU(cpu) (TIMER10_OFFSET + (cpu) * 8)

#define TNAMELEN 20

struct tegra210_clockevent {
	struct clock_event_device evt;
	char name[TNAMELEN];
	void __iomem *reg_base;
	struct irqaction iact;
};

static DEFINE_PER_CPU(struct tegra210_clockevent, tegra210_evt);

int tegra210_timer_get_remain(unsigned int cpu, u64 *time)
{
	u64 tmr_pcr;
	struct tegra210_clockevent *tevt;

	tevt = &per_cpu(tegra210_evt, cpu);
	tmr_pcr = __raw_readl(tevt->reg_base + TIMER_PCR);
	if (tmr_pcr <= 0)
		return -ETIME;

	*time = (u64)((u64)tmr_pcr * timer_us_mult) >> timer_us_shift;

	return 0;
}

static int tegra210_timer_set_next_event(unsigned long cycles,
					 struct clock_event_device *evt)
{
	struct tegra210_clockevent *tevt;
	tevt = container_of(evt, struct tegra210_clockevent, evt);
	__raw_writel((1 << 31) /* EN=1, enable timer */
		     | ((cycles > 1) ? (cycles - 1) : 0), /* n+1 scheme */
		     tevt->reg_base + TIMER_PTV);
	return 0;
}

static void tegra210_timer_set_mode(enum clock_event_mode mode,
				    struct clock_event_device *evt)
{
	struct tegra210_clockevent *tevt;

	tevt = container_of(evt, struct tegra210_clockevent, evt);
	__raw_writel(0, tevt->reg_base + TIMER_PTV);

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		__raw_writel((1 << 31) /* EN=1, enable timer */
			     | (1 << 30) /* PER=1, periodic mode */
			     | ((tegra210_timer_freq / HZ) - 1),
			     tevt->reg_base + TIMER_PTV);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_RESUME:
		break;
	}
}

static irqreturn_t tegra210_timer_isr(int irq, void *dev_id)
{
	struct tegra210_clockevent *tevt;
	tevt = (struct tegra210_clockevent *)dev_id;
	__raw_writel(1 << 30, /* INTR_CLR */
		     tevt->reg_base + TIMER_PCR);
	tevt->evt.event_handler(&tevt->evt);
	return IRQ_HANDLED;
}

static void tegra210_timer_setup(struct tegra210_clockevent *tevt)
{
#ifdef CONFIG_SMP
	int cpu = smp_processor_id();
#endif

	clockevents_config_and_register(&tevt->evt, tegra210_timer_freq,
					1, /* min */
					0x1fffffff); /* 29 bits */
#ifdef CONFIG_SMP
	if (irq_force_affinity(tevt->evt.irq, cpumask_of(cpu))) {
		pr_err("%s: cannot set irq %d affinity to CPU%d\n",
		       __func__, tevt->evt.irq, cpu);
		BUG();
	}
#endif
	enable_irq(tevt->evt.irq);
}

static void tegra210_timer_stop(struct tegra210_clockevent *tevt)
{
	tevt->evt.set_mode(CLOCK_EVT_MODE_UNUSED, &tevt->evt);
	disable_irq_nosync(tevt->evt.irq);
}

static int tegra210_timer_cpu_notify(struct notifier_block *self,
				     unsigned long action, void *hcpu)
{
	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_STARTING:
		tegra210_timer_setup(this_cpu_ptr(&tegra210_evt));
		break;
	case CPU_DYING:
		tegra210_timer_stop(this_cpu_ptr(&tegra210_evt));
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block tegra210_timer_cpu_nb = {
	.notifier_call = tegra210_timer_cpu_notify,
};

u32 notrace tegra_read_usec_raw(void)
{
	if (tegra210_timer_reg_base)
		return __raw_readl(tegra210_timer_reg_base + TIMERUS_CNTR_1US);
	else
		return 0;
}

static void tegra_timer_resume(void)
{
	__raw_writel(usec_config, tegra210_timer_reg_base + TIMERUS_USEC_CFG);
}

static void tegra_timer_restore(void)
{
	int cpu;

	tegra_timer_resume();

	for_each_online_cpu(cpu) {
		struct tegra210_clockevent *tevt;

		tevt = &per_cpu(tegra210_evt, cpu);
		tick_program_event(tevt->evt.next_event, true);
	}
}

static struct syscore_ops tegra_timer_syscore_ops = {
	/* .suspend and .save are NULL because they aren't necessary */
	.resume = tegra_timer_resume,
	.restore = tegra_timer_restore,
};

static void __init tegra210_timer_init(struct device_node *np)
{
	int cpu;
	struct tegra210_clockevent *tevt;
	struct clk *clk;
	int ret;

	tegra210_timer_reg_base = of_iomap(np, 0);
	if (!tegra210_timer_reg_base) {
		pr_err("%s: can't map timer registers\n", __func__);
		BUG();
	}

	clk = clk_get_sys("timer", NULL);
	if (IS_ERR(clk)) {
		pr_warn("Unable to get timer clock. Assuming 12.8Mhz input clock.\n");
		tegra210_timer_freq = 12800000;
	} else {
		clk_prepare_enable(clk);
		tegra210_timer_freq = clk_get_rate(clk);
	}

	for_each_possible_cpu(cpu) {
		tevt = &per_cpu(tegra210_evt, cpu);
		tevt->reg_base = tegra210_timer_reg_base + TIMER_FOR_CPU(cpu);
		tevt->evt.irq = irq_of_parse_and_map(np, cpu);
		if (!tevt->evt.irq) {
			pr_err("%s: can't map IRQ for CPU%d\n",
			       __func__, cpu);
			BUG();
		}

		sprintf(tevt->name, "tegra210_timer%d", cpu);
		tevt->evt.name = tevt->name;
		tevt->evt.cpumask = cpumask_of(cpu);
		tevt->evt.set_next_event = tegra210_timer_set_next_event;
		tevt->evt.set_mode = tegra210_timer_set_mode;
		tevt->evt.features = CLOCK_EVT_FEAT_PERIODIC |
			CLOCK_EVT_FEAT_ONESHOT;

		/* want to be preferred over arch timers */
		tevt->evt.rating = 460;

		tevt->iact.name = tevt->name;
		tevt->iact.dev_id = tevt;
		tevt->iact.flags = IRQF_TIMER | IRQF_TRIGGER_HIGH |
			IRQF_NOBALANCING;
		tevt->iact.handler = tegra210_timer_isr;
		irq_set_status_flags(tevt->evt.irq, IRQ_NOAUTOEN | IRQ_PER_CPU);
		ret = setup_irq(tevt->evt.irq, &tevt->iact);
		if (ret) {
			pr_err("%s: cannot setup irq %d for CPU%d\n",
				__func__, tevt->evt.irq, cpu);
			BUG();
		}
	}

	/*
	 * Configure microsecond timers to have 1MHz clock
	 * Config register is 0xqqww, where qq is "dividend", ww is "divisor"
	 * Uses n+1 scheme
	 */
	switch (tegra210_timer_freq) {
	case 12000000:
		usec_config = 0x000b; /* (11+1)/(0+1) */
		break;
	case 12800000:
		usec_config = 0x043f; /* (63+1)/(4+1) */
		break;
	case 13000000:
		usec_config = 0x000c; /* (12+1)/(0+1) */
		break;
	case 19200000:
		usec_config = 0x045f; /* (95+1)/(4+1) */
		break;
	case 26000000:
		usec_config = 0x0019; /* (25+1)/(0+1) */
		break;
	case 16800000:
		usec_config = 0x0453; /* (83+1)/(4+1) */
		break;
	case 38400000:
		usec_config = 0x04bf; /* (191+1)/(4+1) */
		break;
	case 48000000:
		usec_config = 0x002f; /* (47+1)/(0+1) */
		break;
	default:
		BUG();
	}

	__raw_writel(usec_config, tegra210_timer_reg_base + TIMERUS_USEC_CFG);
	/* boot cpu is online */
	tevt = &per_cpu(tegra210_evt, 0);
	tegra210_timer_setup(tevt);

	clocks_calc_mult_shift(&timer_us_mult, &timer_us_shift,
				tegra210_timer_freq, USEC_PER_SEC, 0);

	if (register_cpu_notifier(&tegra210_timer_cpu_nb)) {
		pr_err("%s: cannot setup CPU notifier\n", __func__);
		BUG();
	}

	register_syscore_ops(&tegra_timer_syscore_ops);

	of_node_put(np);
}

CLOCKSOURCE_OF_DECLARE(tegra210_timer, "nvidia,tegra210-timer",
		       tegra210_timer_init);
