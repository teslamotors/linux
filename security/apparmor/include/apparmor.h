/*
 * AppArmor security module
 *
 * This file contains AppArmor basic global and lib definitions
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2013 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#ifndef __APPARMOR_H
#define __APPARMOR_H

#include <linux/slab.h>
#include <linux/fs.h>

#include "match.h"

/*
 * Class of mediation types in the AppArmor policy db
 */
#define AA_CLASS_ENTRY		0
#define AA_CLASS_UNKNOWN	1
#define AA_CLASS_FILE		2
#define AA_CLASS_CAP		3
#define AA_CLASS_NET		4
#define AA_CLASS_RLIMITS	5
#define AA_CLASS_DOMAIN		6
#define AA_CLASS_MOUNT		7
#define AA_CLASS_PTRACE		9
#define AA_CLASS_SIGNAL		10
#define AA_CLASS_LABEL		16

#define AA_CLASS_LAST		AA_CLASS_LABEL

/* Control parameters settable through module/boot flags */
extern enum audit_mode aa_g_audit;
extern bool aa_g_audit_header;
extern bool aa_g_debug;
extern bool aa_g_hash_policy;
extern bool aa_g_lock_policy;
extern bool aa_g_logsyscall;
extern bool aa_g_paranoid_load;
extern unsigned int aa_g_path_max;
extern bool aa_g_unconfined_init;

/*
 * DEBUG remains global (no per profile flag) since it is mostly used in sysctl
 * which is not related to profile accesses.
 */

#define AA_DEBUG(fmt, args...)						\
	do {								\
		if (aa_g_debug && printk_ratelimit())			\
			printk(KERN_DEBUG "AppArmor: " fmt, ##args);	\
	} while (0)

#define AA_WARN(X) WARN((X), "APPARMOR WARN %s: %s\n", __FUNCTION__, #X)

#define AA_BUG(X, args...) AA_BUG_FMT((X), "" args )
#define AA_BUG_FMT(X, fmt, args...)					\
	WARN((X), "AppArmor WARN %s: (" #X "): " fmt, __FUNCTION__ , ##args )

#define AA_ERROR(fmt, args...)						\
	do {								\
		if (printk_ratelimit())					\
			printk(KERN_ERR "AppArmor: " fmt, ##args);	\
	} while (0)

/* Flag indicating whether initialization completed */
extern int apparmor_initialized __initdata;

/* fn's in lib */
char *aa_split_fqname(char *args, char **ns_name);
char *aa_splitn_fqname(char *fqname, size_t n, char **ns_name, size_t *ns_len);
void aa_info_message(const char *str);
void *__aa_kvmalloc(size_t size, gfp_t flags);

static inline void *kvmalloc(size_t size)
{
	return __aa_kvmalloc(size, 0);
}

static inline void *kvzalloc(size_t size)
{
	return __aa_kvmalloc(size, __GFP_ZERO);
}

/* returns 0 if kref not incremented */
static inline int kref_get_not0(struct kref *kref)
{
	return atomic_inc_not_zero(&kref->refcount);
}

/**
 * aa_strneq - compare null terminated @str to a non null terminated substring
 * @str: a null terminated string
 * @sub: a substring, not necessarily null terminated
 * @len: length of @sub to compare
 *
 * The @str string must be full consumed for this to be considered a match
 */
static inline bool aa_strneq(const char *str, const char *sub, int len)
{
	return !strncmp(str, sub, len) && !str[len];
}

/**
 * aa_dfa_null_transition - step to next state after null character
 * @dfa: the dfa to match against
 * @start: the state of the dfa to start matching in
 *
 * aa_dfa_null_transition transitions to the next state after a null
 * character which is not used in standard matching and is only
 * used to separate pairs.
 */
static inline unsigned int aa_dfa_null_transition(struct aa_dfa *dfa,
						  unsigned int start)
{
	/* the null transition only needs the string's null terminator byte */
	return aa_dfa_next(dfa, start, 0);
}

static inline bool path_mediated_fs(struct dentry *dentry)
{
	return !(dentry->d_sb->s_flags & MS_NOUSER);
}


struct counted_str {
	struct kref count;
	char name[];
};

#define str_to_counted(str) \
	((struct counted_str *)(str - offsetof(struct counted_str,name)))

#define __counted	/* atm just a notation */

void aa_str_kref(struct kref *kref);
char *aa_str_alloc(int size, gfp_t gfp);


static inline __counted char *aa_get_str(__counted char *str)
{
	if (str)
		kref_get(&(str_to_counted(str)->count));

	return str;
}

static inline void aa_put_str(__counted char *str)
{
	if (str)
		kref_put(&str_to_counted(str)->count, aa_str_kref);
}

const char *aa_imode_name(umode_t mode);

#endif /* __APPARMOR_H */
