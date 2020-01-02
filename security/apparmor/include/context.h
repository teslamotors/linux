/*
 * AppArmor security module
 *
 * This file contains AppArmor contexts used to associate "labels" to objects.
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2010 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#ifndef __AA_CONTEXT_H
#define __AA_CONTEXT_H

#include <linux/cred.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include "label.h"
#include "policy.h"

#define cred_cxt(X) (X)->security
#define current_cxt() cred_cxt(current_cred())
#define current_ns() labels_ns(aa_current_raw_label())

/**
 * struct aa_task_cxt - primary label for confined tasks
 * @label: the current label   (NOT NULL)
 * @exec: label to transition to on next exec  (MAYBE NULL)
 * @previous: label the task may return to     (MAYBE NULL)
 * @token: magic value the task must know for returning to @previous
 *
 * Contains the task's current label (which could change due to
 * change_hat).  Plus the hat_magic needed during change_hat.
 *
 * TODO: make so a task can be confined by a stack of contexts
 */
struct aa_task_cxt {
	struct aa_label *label;
	struct aa_label *onexec;
	struct aa_label *previous;
	u64 token;
};

struct aa_task_cxt *aa_alloc_task_context(gfp_t flags);
void aa_free_task_context(struct aa_task_cxt *cxt);
void aa_dup_task_context(struct aa_task_cxt *new,
			 const struct aa_task_cxt *old);
int aa_replace_current_label(struct aa_label *label);
int aa_set_current_onexec(struct aa_label *label);
int aa_set_current_hat(struct aa_label *label, u64 token);
int aa_restore_previous_label(u64 cookie);
struct aa_label *aa_get_task_label(struct task_struct *task);


/**
 * aa_cred_raw_label - obtain cred's label
 * @cred: cred to obtain label from  (NOT NULL)
 *
 * Returns: confining label
 *
 * does NOT increment reference count
 */
static inline struct aa_label *aa_cred_raw_label(const struct cred *cred)
{
	struct aa_task_cxt *cxt = cred_cxt(cred);
	BUG_ON(!cxt || !cxt->label);
	return cxt->label;
}

/**
 * aa_get_newest_cred_label - obtain the newest version of the label on a cred
 * @cred: cred to obtain label from (NOT NULL)
 *
 * Returns: newest version of confining label
 */
static inline struct aa_label *aa_get_newest_cred_label(const struct cred *cred)
{
	return aa_get_newest_label(aa_cred_raw_label(cred));
}

/**
 * __aa_task_raw_label - retrieve another task's label
 * @task: task to query  (NOT NULL)
 *
 * Returns: @task's label without incrementing its ref count
 *
 * If @task != current needs to be called in RCU safe critical section
 */
static inline struct aa_label *__aa_task_raw_label(struct task_struct *task)
{
	return aa_cred_raw_label(__task_cred(task));
}

/**
 * __aa_task_is_confined - determine if @task has any confinement
 * @task: task to check confinement of  (NOT NULL)
 *
 * If @task != current needs to be called in RCU safe critical section
 */
static inline bool __aa_task_is_confined(struct task_struct *task)
{
	return !unconfined(__aa_task_raw_label(task));
}

/**
 * aa_current_raw_label - find the current tasks confining label
 *
 * Returns: up to date confining label or the ns unconfined label (NOT NULL)
 *
 * This fn will not update the tasks cred to the most up to date version
 * of the label so it is safe to call when inside of locks.
 */
static inline struct aa_label *aa_current_raw_label(void)
{
	return aa_cred_raw_label(current_cred());
}

/**
 * aa_get_current_label - get the newest version of the current tasks label
 *
 * Returns: newest version of confining label (NOT NULL)
 *
 * This fn will not update the tasks cred, so it is safe inside of locks
 *
 * The returned reference must be put with aa_put_label()
 */
static inline struct aa_label *aa_get_current_label(void)
{
	struct aa_label *l = aa_current_raw_label();

	if (label_invalid(l))
		return aa_get_newest_label(l);
	return aa_get_label(l);
}

/**
 * aa_begin_current_label - find newest version of the current tasks label
 *
 * Returns: newest version of confining label (NOT NULL)
 *
 * This fn will not update the tasks cred, so it is safe inside of locks
 *
 * The returned reference must be put with aa_end_current_label()
 */
static inline struct aa_label *aa_begin_current_label(void)
{
	struct aa_label *l = aa_current_raw_label();

	if (label_invalid(l))
		l = aa_get_newest_label(l);
	return l;
}

/**
 * aa_end_current_label - put a reference found with aa_begin_current_label
 * @label: label reference to put
 *
 * Should only be used with a reference obtained with aa_begin_current_label
 * and never used in situations where the task cred may be updated
 */
static inline void aa_end_current_label(struct aa_label *label)
{
	if (label != aa_current_raw_label())
		aa_put_label(label);
}

/**
 * aa_current_label - find the current tasks confining label and update it
 *
 * Returns: up to date confining label or the ns unconfined label (NOT NULL)
 *
 * This fn will update the tasks cred structure if the label has been
 * replaced.  Not safe to call inside locks
 */
static inline struct aa_label *aa_current_label(void)
{
	const struct aa_task_cxt *cxt = current_cxt();
	struct aa_label *label;
	BUG_ON(!cxt || !cxt->label);

	if (label_invalid(cxt->label)) {
		label = aa_get_newest_label(cxt->label);
		aa_replace_current_label(label);
		aa_put_label(label);
		cxt = current_cxt();
	}

	return cxt->label;
}

/**
 * aa_clear_task_cxt_trans - clear transition tracking info from the cxt
 * @cxt: task context to clear (NOT NULL)
 */
static inline void aa_clear_task_cxt_trans(struct aa_task_cxt *cxt)
{
	aa_put_label(cxt->previous);
	aa_put_label(cxt->onexec);
	cxt->previous = NULL;
	cxt->onexec = NULL;
	cxt->token = 0;
}

#endif /* __AA_CONTEXT_H */
