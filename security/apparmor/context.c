/*
 * AppArmor security module
 *
 * This file contains AppArmor functions used to manipulate object security
 * contexts.
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2010 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 *
 * AppArmor sets confinement on every task, via the the aa_task_cxt and
 * the aa_task_cxt.label, both of which are required and are not allowed
 * to be NULL.  The aa_task_cxt is not reference counted and is unique
 * to each cred (which is reference count).  The label pointed to by
 * the task_cxt is reference counted.
 *
 * TODO
 * If a task uses change_hat it currently does not return to the old
 * cred or task context but instead creates a new one.  Ideally the task
 * should return to the previous cred if it has not been modified.
 *
 */

#include "include/context.h"
#include "include/policy.h"

/**
 * aa_alloc_task_context - allocate a new task_cxt
 * @flags: gfp flags for allocation
 *
 * Returns: allocated buffer or NULL on failure
 */
struct aa_task_cxt *aa_alloc_task_context(gfp_t flags)
{
	return kzalloc(sizeof(struct aa_task_cxt), flags);
}

/**
 * aa_free_task_context - free a task_cxt
 * @cxt: task_cxt to free (MAYBE NULL)
 */
void aa_free_task_context(struct aa_task_cxt *cxt)
{
	if (cxt) {
		aa_put_label(cxt->label);
		aa_put_label(cxt->previous);
		aa_put_label(cxt->onexec);

		kzfree(cxt);
	}
}

/**
 * aa_dup_task_context - duplicate a task context, incrementing reference counts
 * @new: a blank task context      (NOT NULL)
 * @old: the task context to copy  (NOT NULL)
 */
void aa_dup_task_context(struct aa_task_cxt *new, const struct aa_task_cxt *old)
{
	*new = *old;
	aa_get_label(new->label);
	aa_get_label(new->previous);
	aa_get_label(new->onexec);
}

/**
 * aa_get_task_label - Get another task's label
 * @task: task to query  (NOT NULL)
 *
 * Returns: counted reference to @task's label
 */
struct aa_label *aa_get_task_label(struct task_struct *task)
{
	struct aa_label *p;

	rcu_read_lock();
	p = aa_get_newest_label(__aa_task_raw_label(task));
	rcu_read_unlock();

	return p;
}

/**
 * aa_replace_current_label - replace the current tasks label
 * @label: new label  (NOT NULL)
 *
 * Returns: 0 or error on failure
 */
int aa_replace_current_label(struct aa_label *label)
{
	struct aa_task_cxt *cxt = current_cxt();
	struct cred *new;
	BUG_ON(!label);

	if (cxt->label == label)
		return 0;

	new  = prepare_creds();
	if (!new)
		return -ENOMEM;

	cxt = cred_cxt(new);
	if (unconfined(label) || (labels_ns(cxt->label) != labels_ns(label)))
		/* if switching to unconfined or a different label namespace
		 * clear out context state
		 */
		aa_clear_task_cxt_trans(cxt);

	aa_get_label(label);
	aa_put_label(cxt->label);
	cxt->label = label;

	commit_creds(new);
	return 0;
}

/**
 * aa_set_current_onexec - set the tasks change_profile to happen onexec
 * @label: system label to set at exec  (MAYBE NULL to clear value)
 *
 * Returns: 0 or error on failure
 */
int aa_set_current_onexec(struct aa_label *label)
{
	struct aa_task_cxt *cxt;
	struct cred *new = prepare_creds();
	if (!new)
		return -ENOMEM;

	cxt = cred_cxt(new);
	aa_get_label(label);
	aa_put_label(cxt->onexec);
	cxt->onexec = label;

	commit_creds(new);
	return 0;
}

/**
 * aa_set_current_hat - set the current tasks hat
 * @label: label to set as the current hat  (NOT NULL)
 * @token: token value that must be specified to change from the hat
 *
 * Do switch of tasks hat.  If the task is currently in a hat
 * validate the token to match.
 *
 * Returns: 0 or error on failure
 */
int aa_set_current_hat(struct aa_label *label, u64 token)
{
	struct aa_task_cxt *cxt;
	struct cred *new = prepare_creds();
	if (!new)
		return -ENOMEM;
	BUG_ON(!label);

	cxt = cred_cxt(new);
	if (!cxt->previous) {
		/* transfer refcount */
		cxt->previous = cxt->label;
		cxt->token = token;
	} else if (cxt->token == token) {
		aa_put_label(cxt->label);
	} else {
		/* previous_profile && cxt->token != token */
		abort_creds(new);
		return -EACCES;
	}
	cxt->label = aa_get_newest_label(label);
	/* clear exec on switching context */
	aa_put_label(cxt->onexec);
	cxt->onexec = NULL;

	commit_creds(new);
	return 0;
}

/**
 * aa_restore_previous_label - exit from hat context restoring previous label
 * @token: the token that must be matched to exit hat context
 *
 * Attempt to return out of a hat to the previous label.  The token
 * must match the stored token value.
 *
 * Returns: 0 or error of failure
 */
int aa_restore_previous_label(u64 token)
{
	struct aa_task_cxt *cxt;
	struct cred *new = prepare_creds();
	if (!new)
		return -ENOMEM;

	cxt = cred_cxt(new);
	if (cxt->token != token) {
		abort_creds(new);
		return -EACCES;
	}
	/* ignore restores when there is no saved label */
	if (!cxt->previous) {
		abort_creds(new);
		return 0;
	}

	aa_put_label(cxt->label);
	cxt->label = aa_get_newest_label(cxt->previous);
	BUG_ON(!cxt->label);
	/* clear exec && prev information when restoring to previous context */
	aa_clear_task_cxt_trans(cxt);

	commit_creds(new);
	return 0;
}
