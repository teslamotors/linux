/*
 * arch/arm/mach-tegra/nvdumper_config.c
 *
 * Copyright (c) 2011-2015, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqdesc.h>
#include <linux/idr.h>
#include <linux/workqueue.h>
#include <linux/hashtable.h>

/*
 * hack
 */
#include <../../../kernel/workqueue_internal.h>

/*
 * copied from kernel/kernel/workqueue.c
 */
#define BUSY_WORKER_HASH_ORDER 6

struct worker_pool {
	spinlock_t		lock;		/* the pool lock */
	int			cpu;		/* I: the associated cpu */
	int			node;		/* I: the associated node ID */
	int			id;		/* I: pool ID */
	unsigned int		flags;		/* X: flags */

	struct list_head	worklist;	/* L: list of pending works */
	int			nr_workers;	/* L: total number of workers */

	/* nr_idle includes the ones off idle_list for rebinding */
	int			nr_idle;	/* L: currently idle ones */

	struct list_head	idle_list;	/* X: list of idle workers */
	struct timer_list	idle_timer;	/* L: worker idle timeout */
	struct timer_list	mayday_timer;	/* L: SOS timer for workers */

	/* a workers is either on busy_hash or idle_list, or the manager */
	DECLARE_HASHTABLE(busy_hash, BUSY_WORKER_HASH_ORDER);
						/* L: hash of busy workers */

	/* see manage_workers() for details on the two manager mutexes */
	struct mutex		manager_arb;	/* manager arbitration */
	struct mutex		manager_mutex;	/* manager exclusion */
	struct idr		worker_idr;
		/* MG: worker IDs and iteration */

	struct workqueue_attrs	*attrs;		/* I: worker attributes */
	struct hlist_node	hash_node;	/* PL: unbound_pool_hash node */
	int			refcnt;
		/* PL: refcnt for unbound pools */

	/*
	 * The current concurrency level.  As it's likely to be accessed
	 * from other CPUs during try_to_wake_up(), put it in a separate
	 * cacheline.
	 */
	atomic_t		nr_running ____cacheline_aligned_in_smp;

	/*
	 * Destruction of pool is sched-RCU protected to allow dereferences
	 * from get_work_pool().
	 */
	struct rcu_head		rcu;
} ____cacheline_aligned_in_smp;


#define MAX_KEY_LENGTH 32
#define MAX_GROUP_LENGTH 32

struct config_option {
	char key[MAX_KEY_LENGTH];
	char group[MAX_GROUP_LENGTH];
	uint64_t value;
};

const struct config_option nvdumper_config[] = {
{
	"stack",
	"task_struct",
	(uint64_t)offsetof(struct task_struct, stack)
},
{
	"state",
	"task_struct",
	(uint64_t)offsetof(struct task_struct, state)
},
#if defined(CONFIG_SMP)
{
	"on_cpu",
	"task_struct",
	(uint64_t)offsetof(struct task_struct, on_cpu)
},
#endif
{
	"exit_state",
	"task_struct",
	(uint64_t)offsetof(struct task_struct, exit_state)
},
{
	"thread_group",
	"task_struct",
	(uint64_t)offsetof(struct task_struct, thread_group)
},
{
	"pid",
	"task_struct",
	(uint64_t)offsetof(struct task_struct, pid)
},
{
	"comm",
	"task_struct",
	(uint64_t)offsetof(struct task_struct, comm)
},
{
	"tasks",
	"task_struct",
	(uint64_t)offsetof(struct task_struct, tasks)
},
{
	"mm",
	"task_struct",
	(uint64_t)offsetof(struct task_struct, mm)
},
{
	"parent",
	"task_struct",
	(uint64_t)offsetof(struct task_struct, parent)
},
#if defined(CONFIG_ARM64)
{
	"thread",
	"task_struct",
	(uint64_t)offsetof(struct task_struct, thread)
},
{
	"cpu_context",
	"thread_struct",
	(uint64_t)offsetof(struct thread_struct, cpu_context)
},
{
	"on_cpu",
	"thread_info",
	(uint64_t)offsetof(struct thread_info, cpu)
},
{
	"fp",
	"cpu_context_save",
	(uint64_t)offsetof(struct cpu_context, fp)
},
{
	"sp",
	"cpu_context_save",
	(uint64_t)offsetof(struct cpu_context, sp)
},
{
	"pc",
	"cpu_context_save",
	(uint64_t)offsetof(struct cpu_context, pc)
},
#endif
#if defined(CONFIG_ARM)
{
	"cpu_context",
	"thread_info",
	(uint64_t)offsetof(struct thread_info, cpu_context)
},
{
	"fp",
	"cpu_context_save",
	(uint64_t)offsetof(struct cpu_context_save, fp)
},
{
	"sp",
	"cpu_context_save",
	(uint64_t)offsetof(struct cpu_context_save, sp)
},
{
	"pc",
	"cpu_context_save",
	(uint64_t)offsetof(struct cpu_context_save, pc)
},
#endif
{
	"mmap",
	"mm_struct",
	(uint64_t)offsetof(struct mm_struct, mmap)
},
{
	"pgd",
	"mm_struct",
	(uint64_t)offsetof(struct mm_struct, pgd)
},
{
	"vm_mm",
	"vm_area_struct",
	(uint64_t)offsetof(struct vm_area_struct, vm_mm)
},
{
	"vm_start",
	"vm_area_struct",
	(uint64_t)offsetof(struct vm_area_struct, vm_start)
},
{
	"vm_end",
	"vm_area_struct",
	(uint64_t)offsetof(struct vm_area_struct, vm_end)
},
{
	"vm_pgoff",
	"vm_area_struct",
	(uint64_t)offsetof(struct vm_area_struct, vm_pgoff)
},
{
	"vm_file",
	"vm_area_struct",
	(uint64_t)offsetof(struct vm_area_struct, vm_file)
},
{
	"vm_next",
	"vm_area_struct",
	(uint64_t)offsetof(struct vm_area_struct, vm_next)
},
{
	"vm_flags",
	"vm_area_struct",
	(uint64_t)offsetof(struct vm_area_struct, vm_flags)
},
{
	"f_dentry",
	"file",
	(uint64_t)offsetof(struct file, f_dentry)
},
{
	"d_name",
	"dentry",
	(uint64_t)offsetof(struct dentry, d_name)
},
{
	"name",
	"qstr",
	(uint64_t)offsetof(struct qstr, name)
},
{
	"irq_data",
	"irq_desc",
	(uint64_t)offsetof(struct irq_desc, irq_data)
},
{
	"kstat_irqs",
	"irq_desc",
	(uint64_t)offsetof(struct irq_desc, kstat_irqs)
},
{
	"action",
	"irq_desc",
	(uint64_t)offsetof(struct irq_desc, action)
},
{
	"name",
	"irq_desc",
	(uint64_t)offsetof(struct irq_desc, name)
},
{
	"chip",
	"irq_data",
	(uint64_t)offsetof(struct irq_data, chip)
},
{
	"name",
	"irq_chip",
	(uint64_t)offsetof(struct irq_chip, name)
},
{
	"name",
	"irq_action",
	(uint64_t)offsetof(struct irqaction, name)
},
{
	"idle_list",
	"worker_pool",
	(uint64_t)offsetof(struct worker_pool, idle_list)
},
{
	"busy_hash",
	"worker_pool",
	(uint64_t)offsetof(struct worker_pool, busy_hash)
},
{
	"entry",
	"worker",
	(uint64_t)offsetof(struct worker, entry)
},
{
	"hentry",
	"worker",
	(uint64_t)offsetof(struct worker, hentry)
},
{
	"current_work",
	"worker",
	(uint64_t)offsetof(struct worker, current_work)
},
{
	"current_func",
	"worker",
	(uint64_t)offsetof(struct worker, current_func)
},
{
	"task",
	"worker",
	(uint64_t)offsetof(struct worker, task)
}
};

EXPORT_SYMBOL(nvdumper_config);
