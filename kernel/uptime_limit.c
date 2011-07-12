/*
 * uptime_limit.c
 *
 * This file contains the functions which can limit kernel uptime
 *
 * Copyright (C) 2011 Bruce Ashfield (bruce.ashfield@windriver.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * This functionality is somewhat close to the softdog watchdog
 * implementation, but it cannot be used directly for several reasons:
 *
 *   - The soft watchdog should be available while this functionality is active
 *   - The duration range is different between this and the softdog. The
 *     timeout available here is potentially quite a bit longer.
 *   - At expiration, there are different expiration requirements and actions.
 *   - This functionality is specific to a particular use case and should
 *     not impact mainline functionality
 *
 */
#include <linux/kernel.h>
#include <linux/reboot.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#define UPTIME_LIMIT_IN_SECONDS (CONFIG_UPTIME_LIMIT_DURATION * 60)
#define MIN(X, Y) ((X) <= (Y) ? (X) : (Y))
#define TEN_MINUTES_IN_SECONDS 600

enum uptime_expiration_type {
	uptime_no_action,
	uptime_reboot
};

static enum uptime_expiration_type uptime_expiration_action = uptime_no_action;
static struct timer_list timelimit_timer;
static struct task_struct *uptime_worker_task;

static void timelimit_expire(unsigned long timeout_seconds)
{
	char msg[128];
	int msglen = 127;

	if (timeout_seconds) {
		if (timeout_seconds >= 60)
			snprintf(msg, msglen,
				 "Uptime: kernel validity duration has %d %s remaining\n",
				 (int) timeout_seconds / 60, "minute(s)");
		else
			snprintf(msg, msglen,
				 "Uptime: kernel validity duration has %d %s remaining\n",
				 (int) timeout_seconds, "seconds");

		printk(KERN_CRIT "%s", msg);

		timelimit_timer.expires = jiffies + timeout_seconds * HZ;
		timelimit_timer.data = 0;
		add_timer_on(&timelimit_timer, cpumask_first(cpu_online_mask));
	} else {
		printk(KERN_CRIT "Uptime: Kernel validity timeout has expired\n");
#ifdef CONFIG_UPTIME_LIMIT_KERNEL_REBOOT
		uptime_expiration_action = uptime_reboot;
		wake_up_process(uptime_worker_task);
	}
#endif
}

/*
 * This thread starts and then immediately goes to sleep. When it is woken
 * up, it carries out the instructions left in uptime_expiration_action. If
 * no action was specified it simply goes back to sleep.
 */
static int uptime_worker(void *unused)
{
	set_current_state(TASK_INTERRUPTIBLE);

	while (!kthread_should_stop()) {
		schedule();

		if (kthread_should_stop())
			break;

		if (uptime_expiration_action == uptime_reboot) {
			printk(KERN_CRIT "Uptime: restarting machine\n");
			kernel_restart(NULL);
		}

		set_current_state(TASK_INTERRUPTIBLE);
	}
	__set_current_state(TASK_RUNNING);

	return 0;
}

static int timeout_enable(int cpu)
{
	int err = 0;
	int warning_limit;

	/*
	 * Create an uptime worker thread. This thread is required since the
	 * safe version of kernel restart cannot be called from a
	 * non-interruptible context. Which means we cannot call it directly
	 * from a timer callback.  So we arrange for the timer expiration to
	 * wakeup a thread, which performs the action.
	 */
	uptime_worker_task = kthread_create(uptime_worker,
					    (void *)(unsigned long)cpu,
					    "uptime_worker/%d", cpu);
	if (IS_ERR(uptime_worker_task)) {
		printk(KERN_ERR "Uptime: task for cpu %i failed\n", cpu);
		err = PTR_ERR(uptime_worker_task);
		goto out;
	}
	/* bind to cpu0 to avoid migration and hot plug nastiness */
	kthread_bind(uptime_worker_task, cpu);
	wake_up_process(uptime_worker_task);

	/* Create the timer that will wake the uptime thread at expiration */
	init_timer(&timelimit_timer);
	timelimit_timer.function = timelimit_expire;
	/*
	 * Fire two timers. One warning timeout and the final timer
	 * which will carry out the expiration action. The warning timer will
	 * expire at the minimum of half the original time or ten minutes.
	 */
	warning_limit = MIN(UPTIME_LIMIT_IN_SECONDS/2, TEN_MINUTES_IN_SECONDS);
	timelimit_timer.expires = jiffies + warning_limit * HZ;
	timelimit_timer.data = UPTIME_LIMIT_IN_SECONDS - warning_limit;

	add_timer_on(&timelimit_timer, cpumask_first(cpu_online_mask));
out:
	return err;
}

static int __init timelimit_init(void)
{
	int err = 0;

	printk(KERN_INFO "Uptime: system uptime restrictions enabled\n");

	/*
	 * Enable the timeout thread for cpu 0 only, assuming that the
	 * uptime limit is non-zero, to protect against any cpu
	 * migration issues.
	 */
	if (UPTIME_LIMIT_IN_SECONDS)
		err = timeout_enable(0);

	return err;
}
device_initcall(timelimit_init);
