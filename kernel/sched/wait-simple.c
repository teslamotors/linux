/*
 * Simple waitqueues without fancy flags and callbacks
 *
 * (C) 2011 Thomas Gleixner <tglx@linutronix.de>
 *
 * Based on kernel/wait.c
 *
 * For licencing details see kernel-base/COPYING
 */
#include <linux/init.h>
#include <linux/export.h>
#include <linux/sched.h>
#include <linux/wait-simple.h>

void __init_swait_head(struct swait_head *head, struct lock_class_key *key)
{
	raw_spin_lock_init(&head->lock);
	lockdep_set_class(&head->lock, key);
	INIT_LIST_HEAD(&head->list);
}
EXPORT_SYMBOL(__init_swait_head);

void swait_prepare(struct swait_head *head, struct swaiter *w, int state)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&head->lock, flags);
	w->task = current;
	if (list_empty(&w->node))
		__swait_enqueue(head, w);
	set_current_state(state);
	raw_spin_unlock_irqrestore(&head->lock, flags);
}
EXPORT_SYMBOL(swait_prepare);

void swait_finish(struct swait_head *head, struct swaiter *w)
{
	unsigned long flags;

	__set_current_state(TASK_RUNNING);
	if (w->task) {
		raw_spin_lock_irqsave(&head->lock, flags);
		__swait_dequeue(w);
		raw_spin_unlock_irqrestore(&head->lock, flags);
	}
}
EXPORT_SYMBOL(swait_finish);

int __swait_wake(struct swait_head *head, unsigned int state)
{
	struct swaiter *curr, *next;
	unsigned long flags;
	int woken = 0;

	raw_spin_lock_irqsave(&head->lock, flags);

	list_for_each_entry_safe(curr, next, &head->list, node) {
		if (wake_up_state(curr->task, state)) {
			__swait_dequeue(curr);
			curr->task = NULL;
			woken++;
		}
	}

	raw_spin_unlock_irqrestore(&head->lock, flags);
	return woken;
}
EXPORT_SYMBOL(__swait_wake);
