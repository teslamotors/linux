/*
 * AppArmor security module
 *
 * This file contains basic common functions used in AppArmor
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2013 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "include/apparmor.h"
#include "include/audit.h"
#include "include/label.h"
#include "include/perms.h"
#include "include/policy.h"

/**
 * aa_split_fqname - split a fqname into a profile and namespace name
 * @fqname: a full qualified name in namespace profile format (NOT NULL)
 * @ns_name: pointer to portion of the string containing the ns name (NOT NULL)
 *
 * Returns: profile name or NULL if one is not specified
 *
 * Split a namespace name from a profile name (see policy.c for naming
 * description).  If a portion of the name is missing it returns NULL for
 * that portion.
 *
 * NOTE: may modify the @fqname string.  The pointers returned point
 *       into the @fqname string.
 */
char *aa_split_fqname(char *fqname, char **ns_name)
{
	char *name = strim(fqname);

	*ns_name = NULL;
	if (name[0] == ':') {
		char *split = strchr(&name[1], ':');
		*ns_name = skip_spaces(&name[1]);
		if (split) {
			/* overwrite ':' with \0 */
			*split++ = 0;
			if (strncmp(split, "//", 2) == 0)
				split += 2;
			name = skip_spaces(split);
		} else
			/* a ns name without a following profile is allowed */
			name = NULL;
	}
	if (name && *name == 0)
		name = NULL;

	return name;
}

/**
 * skipn_spaces - Removes leading whitespace from @str.
 * @str: The string to be stripped.
 *
 * Returns a pointer to the first non-whitespace character in @str.
 * if all whitespace will return NULL
 */

static char *skipn_spaces(const char *str, size_t n)
{
	for (;n && isspace(*str); --n)
		++str;
	if (n)
		return (char *)str;
	return NULL;
}

char *aa_splitn_fqname(char *fqname, size_t n, char **ns_name, size_t *ns_len)
{
	char *end = fqname + n;
	char *name = skipn_spaces(fqname, n);
	if (!name)
		return NULL;
	*ns_name = NULL;
	*ns_len = 0;
	if (name[0] == ':') {
		char *split = strnchr(name + 1, end - name - 1, ':');
		*ns_name = skipn_spaces(&name[1], end - &name[1]);
		if (!*ns_name)
			return NULL;
		if (split) {
			*ns_len = split - *ns_name - 1;
			if (*ns_len == 0)
				*ns_name = NULL;
			split++;
			if (end - split > 1 && strncmp(split, "//", 2) == 0)
				split += 2;
			name = skipn_spaces(split, end - split);
		} else {
			/* a ns name without a following profile is allowed */
			name = NULL;
			*ns_len = end - *ns_name;
		}
	}
	if (name && *name == 0)
		name = NULL;

	return name;
}

/**
 * aa_info_message - log a none profile related status message
 * @str: message to log
 */
void aa_info_message(const char *str)
{
	if (audit_enabled) {
	  DEFINE_AUDIT_DATA(sa, LSM_AUDIT_DATA_NONE, 0);
		aad(&sa)->info = str;
		aa_audit_msg(AUDIT_APPARMOR_STATUS, &sa, NULL);
	}
	printk(KERN_INFO "AppArmor: %s\n", str);
}

/**
 * __aa_kvmalloc - do allocation preferring kmalloc but falling back to vmalloc
 * @size: how many bytes of memory are required
 * @flags: the type of memory to allocate (see kmalloc).
 *
 * Return: allocated buffer or NULL if failed
 *
 * It is possible that policy being loaded from the user is larger than
 * what can be allocated by kmalloc, in those cases fall back to vmalloc.
 */
void *__aa_kvmalloc(size_t size, gfp_t flags)
{
	void *buffer = NULL;

	if (size == 0)
		return NULL;

	/* do not attempt kmalloc if we need more than 16 pages at once */
	if (size <= (16*PAGE_SIZE))
		buffer = kmalloc(size, flags | GFP_NOIO | __GFP_NOWARN);
	if (!buffer) {
		if (flags & __GFP_ZERO)
			buffer = vzalloc(size);
		else
			buffer = vmalloc(size);
	}
	return buffer;
}


__counted char *aa_str_alloc(int size, gfp_t gfp)
{
	struct counted_str *str;
	str = kmalloc(sizeof(struct counted_str) + size, gfp);
	if (!str)
		return NULL;

	kref_init(&str->count);
	return str->name;
}

void aa_str_kref(struct kref *kref)
{
	kfree(container_of(kref, struct counted_str, count));
}


const char aa_file_perm_chrs[] = "xwracd         km l     ";
const char *aa_file_perm_names[] = {
	"exec",
	"write",
	"read",
	"append",

	"create",
	"delete",
	"open",
	"rename",

	"setattr",
	"getattr",
	"setcred",
	"getcred",

	"chmod",
	"chown",
	"chgrp",
	"lock",

	"mmap",
	"mprot",
	"link",
	"snapshot",

	"unknown",
	"unknown",
	"unknown",
	"unknown",

	"unknown",
	"unknown",
	"unknown",
	"unknown",

	"stack",
	"change_onexec",
	"change_profile",
	"change_hat",
};

/**
 * aa_perm_mask_to_str - convert a perm mask to its short string
 * @str: character buffer to store string in (at least 10 characters)
 * @mask: permission mask to convert
 */
void aa_perm_mask_to_str(char *str, const char *chrs, u32 mask)
{
	unsigned int i, perm = 1;
	for (i = 0; i < 32; perm <<= 1, i++) {
		if (mask & perm)
			*str++ = chrs[i];
	}
	*str = '\0';
}

void aa_audit_perm_names(struct audit_buffer *ab, const char **names, u32 mask)
{
	const char *fmt = "%s";
	unsigned int i, perm = 1;
	bool prev = false;
	for (i = 0; i < 32; perm <<= 1, i++) {
		if (mask & perm) {
			audit_log_format(ab, fmt, names[i]);
			if (!prev) {
				prev = true;
				fmt = " %s";
			}
		}
	}
}

void aa_audit_perm_mask(struct audit_buffer *ab, u32 mask, const char *chrs,
			u32 chrsmask, const char **names, u32 namesmask)
{
	char str[33];

	audit_log_format(ab, "\"");
	if ((mask & chrsmask) && chrs) {
		aa_perm_mask_to_str(str, chrs, mask & chrsmask);
		mask &= ~chrsmask;
		audit_log_format(ab, "%s", str);
		if (mask & namesmask)
			audit_log_format(ab, " ");
	}
	if ((mask & namesmask) && names)
		aa_audit_perm_names(ab, names, mask & namesmask);
	audit_log_format(ab, "\"");
}

/**
 * aa_audit_perms_cb - generic callback fn for auditing perms
 * @ab: audit buffer (NOT NULL)
 * @va: audit struct to audit values of (NOT NULL)
 */
static void aa_audit_perms_cb(struct audit_buffer *ab, void *va)
{
	struct common_audit_data *sa = va;

	if (aad(sa)->request) {
		audit_log_format(ab, " requested_mask=");
		aa_audit_perm_mask(ab, aad(sa)->request, aa_file_perm_chrs,
				   PERMS_CHRS_MASK, aa_file_perm_names,
				   PERMS_NAMES_MASK);
	}
	if (aad(sa)->denied) {
		audit_log_format(ab, "denied_mask=");
		aa_audit_perm_mask(ab, aad(sa)->denied, aa_file_perm_chrs,
				   PERMS_CHRS_MASK, aa_file_perm_names,
				   PERMS_NAMES_MASK);
	}
	audit_log_format(ab, " target=");
	audit_log_untrustedstring(ab, aad(sa)->target);
}

void map_old_policy_perms(struct aa_dfa *dfa, unsigned int state,
			  struct aa_perms *perms)
{

}

/**
 * aa_apply_modes_to_perms - apply namespace and profile flags to perms
 * @profile: that perms where computed from
 * @perms: perms to apply mode modifiers to
 *
 * TODO: split into profile and ns based flags for when accumulating perms
 */
void aa_apply_modes_to_perms(struct aa_profile *profile, struct aa_perms *perms)
{
	switch (AUDIT_MODE(profile)) {
	case AUDIT_ALL:
		perms->audit = ALL_PERMS_MASK;
		/* fall through */
	case AUDIT_NOQUIET:
		perms->quiet = 0;
		break;
	case AUDIT_QUIET:
		perms->audit = 0;
		/* fall through */
	case AUDIT_QUIET_DENIED:
		perms->quiet = ALL_PERMS_MASK;
		break;
	}

	if (KILL_MODE(profile))
		perms->kill = ALL_PERMS_MASK;
	else if (COMPLAIN_MODE(profile))
		perms->complain = ALL_PERMS_MASK;
/* TODO:
	else if (PROMPT_MODE(profile))
		perms->prompt = ALL_PERMS_MASK;
*/
}

static u32 map_other(u32 x)
{
	return ((x & 0x3) << 8) |	/* SETATTR/GETATTR */
		((x & 0x1c) << 18) |	/* ACCEPT/BIND/LISTEN */
		((x & 0x60) << 19);	/* SETOPT/GETOPT */
}

void aa_compute_perms(struct aa_dfa *dfa, unsigned int state,
		      struct aa_perms *perms)
{
	perms->deny = 0;
	perms->kill = perms->stop = 0;
	perms->complain = perms->cond = 0;
	perms->hide = 0;
	perms->prompt = 0;
	perms->allow = dfa_user_allow(dfa, state);
	perms->audit = dfa_user_audit(dfa, state);
	perms->quiet = dfa_user_quiet(dfa, state);

	/* for v5 perm mapping in the policydb, the other set is used
	 * to extend the general perm set
	 */
	perms->allow |= map_other(dfa_other_allow(dfa, state));
	perms->audit |= map_other(dfa_other_audit(dfa, state));
	perms->quiet |= map_other(dfa_other_quiet(dfa, state));
//	perms->xindex = dfa_user_xindex(dfa, state);
}

/**
 * aa_perms_accum_raw - accumulate perms with out masking off overlapping perms
 * @accum - perms struct to accumulate into
 * @addend - perms struct to add to @accum
 */
void aa_perms_accum_raw(struct aa_perms *accum, struct aa_perms *addend)
{
	accum->deny |= addend->deny;
	accum->allow &= addend->allow & ~addend->deny;
	accum->audit |= addend->audit & addend->allow;
	accum->quiet &= addend->quiet & ~addend->allow;
	accum->kill |= addend->kill & ~addend->allow;
	accum->stop |= addend->stop & ~addend->allow;
	accum->complain |= addend->complain & ~addend->allow & ~addend->deny;
	accum->cond |= addend->cond & ~addend->allow & ~addend->deny;
	accum->hide &= addend->hide & ~addend->allow;
	accum->prompt |= addend->prompt & ~addend->allow & ~addend->deny;
}

/**
 * aa_perms_accum - accumulate perms, masking off overlapping perms
 * @accum - perms struct to accumulate into
 * @addend - perms struct to add to @accum
 */
void aa_perms_accum(struct aa_perms *accum, struct aa_perms *addend)
{
	accum->deny |= addend->deny;
	accum->allow &= addend->allow & ~accum->deny;
	accum->audit |= addend->audit & accum->allow;
	accum->quiet &= addend->quiet & ~accum->allow;
	accum->kill |= addend->kill & ~accum->allow;
	accum->stop |= addend->stop & ~accum->allow;
	accum->complain |= addend->complain & ~accum->allow & ~accum->deny;
	accum->cond |= addend->cond & ~accum->allow & ~accum->deny;
	accum->hide &= addend->hide & ~accum->allow;
	accum->prompt |= addend->prompt & ~accum->allow & ~accum->deny;
}

void aa_profile_match_label(struct aa_profile *profile, const char *label,
			    int type, struct aa_perms *perms)
{
	/* TODO: doesn't yet handle extended types */
	unsigned int state;
	if (profile->policy.dfa) {
		state = aa_dfa_next(profile->policy.dfa,
				    profile->policy.start[AA_CLASS_LABEL],
				    type);
		state = aa_dfa_match(profile->policy.dfa, state, label);
		aa_compute_perms(profile->policy.dfa, state, perms);
	} else
		memset(perms, 0, sizeof(*perms));
}


int aa_profile_label_perm(struct aa_profile *profile, struct aa_profile *target,
			  u32 request, int type, u32 *deny,
			  struct common_audit_data *sa)
{
	struct aa_perms perms;
	aad(sa)->label = &profile->label;
	aad(sa)->target = target;
	aad(sa)->request = request;

	aa_profile_match_label(profile, target->base.hname, type, &perms);
	aa_apply_modes_to_perms(profile, &perms);
	*deny |= request & perms.deny;
	return aa_check_perms(profile, &perms, request, sa, aa_audit_perms_cb);
}

/**
 * aa_check_perms - do audit mode selection based on perms set
 * @profile: profile being checked
 * @perms: perms computed for the request
 * @request: requested perms
 * @deny: Returns: explicit deny set
 * @sa: initialized audit structure (MAY BE NULL if not auditing)
 * @cb: callback fn for tpye specific fields (MAY BE NULL)
 *
 * Returns: 0 if permission else error code
 *
 * Note: profile audit modes need to be set before calling by setting the
 *       perm masks appropriately.
 *
 *       If not auditing then complain mode is not enabled and the
 *       error code will indicate whether there was an explicit deny
 *	 with a positive value.
 */
int aa_check_perms(struct aa_profile *profile, struct aa_perms *perms,
		   u32 request, struct common_audit_data *sa,
		   void (*cb) (struct audit_buffer *, void *))
{
	int type, error;
	bool stop = false;
	u32 denied = request & (~perms->allow | perms->deny);
	if (likely(!denied)) {
		/* mask off perms that are not being force audited */
		request &= perms->audit;
		if (!request || !sa)
			return 0;

		type = AUDIT_APPARMOR_AUDIT;
		error = 0;
	} else {
		error = -EACCES;

		if (denied & perms->kill)
			type = AUDIT_APPARMOR_KILL;
		else if (denied == (denied & perms->complain))
			type = AUDIT_APPARMOR_ALLOWED;
		else
			type = AUDIT_APPARMOR_DENIED;

		if (denied & perms->stop)
			stop = true;
		if (denied == (denied & perms->hide))
			error = -ENOENT;

		denied &= ~perms->quiet;
		if (type != AUDIT_APPARMOR_ALLOWED && (!sa || !denied))
			return error;
	}

	if (sa) {
		aad(sa)->label = &profile->label;
		aad(sa)->request = request;
		aad(sa)->denied = denied;
		aad(sa)->error = error;
		aa_audit_msg(type, sa, cb);
	}

	if (type == AUDIT_APPARMOR_ALLOWED)
		error = 0;

	return error;
}

const char *aa_imode_name(umode_t mode)
{
	switch(mode & S_IFMT) {
	case S_IFSOCK:
		return "sock";
	case S_IFLNK:
		return "link";
	case S_IFREG:
		return "reg";
	case S_IFBLK:
		return "blkdev";
	case S_IFDIR:
		return "dir";
	case S_IFCHR:
		return "chrdev";
	case S_IFIFO:
		return "fifo";
	}
	return "unknown";
}

const char *aa_peer_name(struct aa_profile *peer)
{
	if (profile_unconfined(peer))
		return "unconfined";

	return peer->base.hname;
}
