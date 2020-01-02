/*
 * ARM/ARM64 generic CPU idle driver.
 *
 * Copyright (C) 2014 ARM Ltd.
 * Author: Lorenzo Pieralisi <lorenzo.pieralisi@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "CPUidle arm: " fmt

#include <asm/io.h>
#include <linux/cpuidle.h>
#include <linux/cpumask.h>
#include <linux/cpu_pm.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/topology.h>

#include <asm/cpuidle.h>

#include "dt_idle_states.h"
#include "cpuidle_profiler.h"

#ifdef CONFIG_TRAV_CPUIDLE_PROFILER

void __iomem *vr_addr[12];

unsigned int num_online_cpus_in_cluster(unsigned long cpu)
{
        int i;
        uint32_t start, end, value, num_cpus = 0;

        if (cpu < 4) {
                start = 0;
                end = 4;
        } else if (cpu < 8) {
                start = 4;
                end = 8;
        } else {
                start = 8;
                end = 12;
        }

        for (i = start; i < end; i++) {
		value = ioread32(vr_addr[i]);
                value &= PMU_CPU_ON_STATUS;
                if (value != 0)
                        num_cpus++;
        }
        return num_cpus;
}
#endif

int cpu_pm_cpu_idle_enter(unsigned long cpu, int idx)
{
        int __ret;

	if (!idx) {
#ifdef CONFIG_TRAV_CPUIDLE_PROFILER
		cpuidle_profile_start(cpu, 0, 0);
#endif
		cpu_do_idle();
#ifdef CONFIG_TRAV_CPUIDLE_PROFILER
		cpuidle_profile_finish(cpu, 0);
#endif
		return idx;
	}

	__ret = cpu_pm_enter();
	if (!__ret) {
#ifdef CONFIG_TRAV_CPUIDLE_PROFILER
		if(num_online_cpus_in_cluster(cpu) == 1)
			cpuidle_profile_start(cpu, idx, PSCI_CLUSTER_SLEEP);
		else
			cpuidle_profile_start(cpu, idx, idx);
#endif
		__ret = arm_cpuidle_suspend(idx);
#ifdef CONFIG_TRAV_CPUIDLE_PROFILER
		cpuidle_profile_finish(cpu, __ret);
#endif
		cpu_pm_exit();
	}
	return(__ret ? -1 : idx);
}

/*
 * arm_enter_idle_state - Programs CPU to enter the specified state
 *
 * dev: cpuidle device
 * drv: cpuidle driver
 * idx: state index
 *
 * Called from the CPUidle framework to program the device to the
 * specified target state selected by the governor.
 */
static int arm_enter_idle_state(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int idx)
{
	/*
	 * Pass idle state index to arm_cpuidle_suspend which in turn
	 * will call the CPU ops suspend protocol with idle index as a
	 * parameter.
	 */
	unsigned long cpu = smp_processor_id();
	if (cpu == 0)
		return cpu_pm_cpu_idle_enter(cpu, 0);
	else
		return cpu_pm_cpu_idle_enter(cpu, idx);
}

static struct cpuidle_driver arm_idle_driver __initdata = {
	.name = "arm_idle",
	.owner = THIS_MODULE,
	/*
	 * State at index 0 is standby wfi and considered standard
	 * on all ARM platforms. If in some platforms simple wfi
	 * can't be used as "state 0", DT bindings must be implemented
	 * to work around this issue and allow installing a special
	 * handler for idle state index 0.
	 */
	.states[0] = {
		.enter                  = arm_enter_idle_state,
		.exit_latency           = 1,
		.target_residency       = 1,
		.power_usage		= UINT_MAX,
		.name                   = "WFI",
		.desc                   = "ARM WFI",
	}
};

static const struct of_device_id arm_idle_state_match[] __initconst = {
	{ .compatible = "arm,idle-state",
	  .data = arm_enter_idle_state },
	{ },
};

/*
 * arm_idle_init
 *
 * Registers the arm specific cpuidle driver with the cpuidle
 * framework. It relies on core code to parse the idle states
 * and initialize them using driver data structures accordingly.
 */
static int __init arm_idle_init(void)
{
	int cpu, ret;
	struct cpuidle_driver *drv;
	struct cpuidle_device *dev;

	for_each_possible_cpu(cpu) {

		drv = kmemdup(&arm_idle_driver, sizeof(*drv), GFP_KERNEL);
		if (!drv) {
			ret = -ENOMEM;
			goto out_fail;
		}

		drv->cpumask = (struct cpumask *)cpumask_of(cpu);

		/*
		 * Initialize idle states data, starting at index 1.  This
		 * driver is DT only, if no DT idle states are detected (ret
		 * == 0) let the driver initialization fail accordingly since
		 * there is no reason to initialize the idle driver if only
		 * wfi is supported.
		 */
		ret = dt_init_idle_driver(drv, arm_idle_state_match, 1);
		if (ret <= 0) {
			ret = ret ? : -ENODEV;
			goto out_kfree_drv;
		}

		ret = cpuidle_register_driver(drv);
		if (ret) {
			pr_err("Failed to register cpuidle driver\n");
			goto out_kfree_drv;
		}

		/*
		 * Call arch CPU operations in order to initialize
		 * idle states suspend back-end specific data
		 */
		ret = arm_cpuidle_init(cpu);

		/*
		 * Skip the cpuidle device initialization if the reported
		 * failure is a HW misconfiguration/breakage (-ENXIO).
		 */
		if (ret == -ENXIO)
			continue;

		if (ret) {
			pr_err("CPU %d failed to init idle CPU ops\n", cpu);
			goto out_unregister_drv;
		}

		dev = kzalloc(sizeof(*dev), GFP_KERNEL);
		if (!dev) {
			pr_err("Failed to allocate cpuidle device\n");
			ret = -ENOMEM;
			goto out_unregister_drv;
		}
		dev->cpu = cpu;

		ret = cpuidle_register_device(dev);
		if (ret) {
			pr_err("Failed to register cpuidle device for CPU %d\n",
			       cpu);
			goto out_kfree_dev;
		}
	}
#ifdef CONFIG_TRAV_CPUIDLE_PROFILER
	{
		int i;

		cpuidle_profile_register(drv);
		for (i = 0; i < 12; i++) {
			vr_addr[i] = ioremap(PMU_CPU0_STATUS +
						i * PMU_CPU_OFFSET , 4);
		}
	}
#endif
	return 0;

out_kfree_dev:
	kfree(dev);
out_unregister_drv:
	cpuidle_unregister_driver(drv);
out_kfree_drv:
	kfree(drv);
out_fail:
	while (--cpu >= 0) {
		dev = per_cpu(cpuidle_devices, cpu);
		drv = cpuidle_get_cpu_driver(dev);
		cpuidle_unregister_device(dev);
		cpuidle_unregister_driver(drv);
		kfree(dev);
		kfree(drv);
	}

	return ret;
}
device_initcall(arm_idle_init);
