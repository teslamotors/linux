/*
 * AppArmor security module
 *
 * This file contains AppArmor label definitions
 *
 * Copyright 2013 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#ifndef __AA_LABEL_H
#define __AA_LABEL_H

#include <linux/atomic.h>
#include <linux/audit.h>
#include <linux/rbtree.h>
#include <linux/rcupdate.h>

#include "apparmor.h"

struct aa_namespace;

struct labelset_stats {
	atomic_t sread;
	atomic_t fread;
	atomic_t msread;
	atomic_t mfread;

	atomic_t insert;
	atomic_t existing;
	atomic_t minsert;
	atomic_t mexisting;

	atomic_t invalid;		/* outstanding invalid */
};

struct label_stats {
	struct labelset_stats set_stats;

	atomic_t allocated;
	atomic_t failed;
	atomic_t freed;

	atomic_t printk_name_alloc;
	atomic_t printk_name_fail;
	atomic_t seq_print_name_alloc;
	atomic_t seq_print_name_fail;
	atomic_t audit_name_alloc;
	atomic_t audit_name_fail;
};


#ifdef AA_LABEL_STATS
#define labelstats_inc(X) atomic_inc(stats.(X))
#define labelstats_dec(X) atomic_dec(stats.(X))
#define labelsetstats_inc(LS, X)		\
	do {					\
		labelstats_inc(set_stats.##X);	\
		atomic_inc((LS)->stats.(X));	\
	} while (0)
#define labelsetstats_dec(LS, X)		\
	do {					\
		labelstats_dec(set_stats.##X);	\
		atomic_dec((LS)->stats.(X));	\
	} while (0)
#else
#define labelstats_inc(X)
#define labelstats_dec(X)
#define labelsetstats_inc(LS, X)
#define labelsetstats_dec(LS, X)
#endif
#define labelstats_init(X)

/* struct aa_labelset - set of labels for a namespace
 *
 * Labels are reference counted; aa_labelset does not contribute to label
 * reference counts. Once a label's last refcount is put it is removed from
 * the set.
 */
struct aa_labelset {
	rwlock_t lock;

	struct rb_root root;

	/* stats */
#ifdef APPARMOR_LABEL_STATS
	struct labelset_stats stats;
#endif

};

#define __labelset_for_each(LS, N) \
	for((N) = rb_first(&(LS)->root); (N); (N) = rb_next(N))

void aa_labelset_destroy(struct aa_labelset *ls);
void aa_labelset_init(struct aa_labelset *ls);


enum label_flags {
	FLAG_HAT = 1,			/* profile is a hat */
	FLAG_UNCONFINED = 2,		/* label unconfined only if all
					 * constituant profiles unconfined */
	FLAG_NULL = 4,			/* profile is null learning profile */
	FLAG_IX_ON_NAME_ERROR = 8,	/* fallback to ix on name lookup fail */
	FLAG_IMMUTIBLE = 0x10,		/* don't allow changes/replacement */
	FLAG_USER_DEFINED = 0x20,	/* user based profile - lower privs */
	FLAG_NO_LIST_REF = 0x40,	/* list doesn't keep profile ref */
	FLAG_NS_COUNT = 0x80,		/* carries NS ref count */
	FLAG_IN_TREE = 0x100,		/* label is in tree */
	FLAG_PROFILE = 0x200,		/* label is a profile */
	FALG_EXPLICIT = 0x400,		/* explict static label */
	FLAG_INVALID = 0x800,		/* replaced/removed */
	FLAG_RENAMED = 0x1000,		/* label has renaming in it */
	FLAG_REVOKED = 0x2000,		/* label has revocation in it */

	/* These flags must correspond with PATH_flags */
	/* TODO: add new path flags */
};

struct aa_label;
struct aa_replacedby {
	struct kref count;
	struct aa_label __rcu *label;
};

struct label_it {
	int i, j;
};

/* struct aa_label - lazy labeling struct
 * @count: ref count of active users
 * @node: rbtree position
 * @rcu: rcu callback struct
 * @replacedby: is set to the label that replaced this label
 * @hname: text representation of the label (MAYBE_NULL)
 * @flags: invalid and other flags - values may change under label set lock
 * @sid: sid that references this label
 * @size: number of entries in @ent[]
 * @ent: set of profiles for label, actual size determined by @size
 */
struct aa_label {
	struct kref count;
	struct rb_node node;
	struct rcu_head rcu;
	struct aa_replacedby *replacedby;
	__counted char *hname;
	long flags;
	u32 sid;
	int size;
	struct aa_profile *ent[2];
};

#define last_error(E, FN)				\
do {							\
	int __subE = (FN);				\
	if (__subE)					\
		(E) = __subE;				\
} while (0)

#define label_isprofile(X) ((X)->flags & FLAG_PROFILE)
#define label_unconfined(X) ((X)->flags & FLAG_UNCONFINED)
#define unconfined(X) label_unconfined(X)
#define label_invalid(X) ((X)->flags & FLAG_INVALID)
#define __label_invalidate(X) do {	   \
	labelsetstats_inc(labels_set(X), invalid); \
	((X)->flags |= FLAG_INVALID);	   \
} while (0)
#define labels_last(X) ((X)->ent[(X)->size - 1])
#define labels_ns(X) (labels_last(X)->ns)
#define labels_set(X) (&labels_ns(X)->labels)
#define labels_profile(X) ({				\
	AA_BUG(!label_isprofile(X));			\
	container_of((X), struct aa_profile, label);	\
})

int aa_label_next_confined(struct aa_label *l, int i);

/* for each profile in a label */
#define label_for_each(I, L, P)						\
	for ((I).i = 0; ((P) = (L)->ent[(I).i]); ++((I).i))

#define label_for_each_at(I, L, P)					\
	for (; ((P) = (L)->ent[(I).i]);	++((I).i))

#define next_comb(I, L1, L2)						\
do {									\
	(I).j++;							\
	if ((I).j >= (L2)->size) {					\
		(I).i++;						\
		(I).j = 0;						\
	}								\
} while (0)

/* TODO: label_for_each_ns_comb */

/* for each combination of P1 in L1, and P2 in L2 */
#define label_for_each_comb(I, L1, L2, P1, P2)				\
for ((I).i = (I).j = 0;							\
     ((P1) = (L1)->ent[(I).i]) && ((P2) = (L2)->ent[(I).j]);		\
     (I) = next_comb(I, L1, L2))

#define fn_for_each_comb(L1, L2, P1, P2, FN)				\
({									\
	struct label_it i;						\
	int __E = 0;							\
	label_for_each_comb(i, (L1), (L2), (P1), (P2)) {		\
		last_error(__E, (FN));					\
	}								\
	__E;								\
})

/* internal cross check */
//fn_for_each_comb(L1, L2, P1, P2, xcheck(...));

/* external cross check */
// xcheck(fn_for_each_comb(L1, L2, ...),
//        fn_for_each_comb(L2, L1, ...));

/* for each profile that is enforcing confinement in a label */
#define label_for_each_confined(I, L, P)				\
	for ((I).i = aa_label_next_confined((L), 0);			\
	     ((P) = (L)->ent[(I).i]);					\
	     (I).i = aa_label_next_confined((L), (I).i + 1))

#define label_for_each_in_merge(I, A, B, P)				\
	for ((I).i = (I).j = 0;						\
	     ((P) = aa_label_next_in_merge(&(I), (A), (B)));		\
	     )

#define label_for_each_not_in_set(I, SET, SUB, P)			\
	for ((I).i = (I).j = 0;						\
	     ((P) = aa_label_next_not_in_set(&(I), (SET), (SUB)));	\
	     )

#define fn_for_each_XXX(L, P, FN, ...)					\
({									\
	struct label_it i;						\
	int __E = 0;							\
	label_for_each ## __VA_ARGS__ (i, (L), (P)) {			\
		last_error(__E, (FN));					\
	}								\
	__E;								\
})

#define fn_for_each(L, P, FN) fn_for_each_XXX(L, P, FN)
#define fn_for_each_confined(L, P, FN) fn_for_each_XXX(L, P, FN, _confined)

#define fn_for_each2_XXX(L1, L2, P, FN, ...)				\
({									\
	struct label_it i;						\
	int __E = 0;							\
	label_for_each ## __VA_ARGS__(i, (L1), (L2), (P)) {		\
		last_error(__E, (FN));					\
	}								\
	__E;								\
})

#define fn_for_each_in_merge(L1, L2, P, FN)				\
	fn_for_each2_XXX((L1), (L2), P, FN, _in_merge)
#define fn_for_each_not_in_set(L1, L2, P, FN)				\
	fn_for_each2_XXX((L1), (L2), P, FN, _not_in_set)

#define LABEL_MEDIATES(L, C)						\
({									\
	struct aa_profile *profile;					\
	struct label_it i;						\
	int ret = 0;							\
	label_for_each(i, (L), profile) {				\
		if (PROFILE_MEDIATES(profile, (C))) {			\
			ret = 1;					\
			break;						\
		}							\
	}								\
	ret;								\
})

void aa_labelset_destroy(struct aa_labelset *ls);
void aa_labelset_init(struct aa_labelset *ls);
void __aa_labelset_update_all(struct aa_namespace *ns);

void aa_label_destroy(struct aa_label *label);
void aa_label_free(struct aa_label *label);
void aa_label_kref(struct kref *kref);
bool aa_label_init(struct aa_label *label, int size);
struct aa_label *aa_label_alloc(int size, gfp_t gfp);

bool aa_label_is_subset(struct aa_label *set, struct aa_label *sub);
struct aa_profile * aa_label_next_not_in_set(struct label_it *I,
					     struct aa_label *set,
					     struct aa_label *sub);
bool aa_label_remove(struct aa_labelset *ls, struct aa_label *label);
struct aa_label *aa_label_insert(struct aa_labelset *ls, struct aa_label *l);
struct aa_label *aa_label_remove_and_insert(struct aa_labelset *ls,
					    struct aa_label *remove,
					    struct aa_label *insert);
bool aa_label_replace(struct aa_labelset *ls, struct aa_label *old,
		      struct aa_label *new);
bool aa_label_make_newest(struct aa_labelset *ls, struct aa_label *old,
			  struct aa_label *new);

struct aa_label *aa_label_find(struct aa_labelset *ls, struct aa_label *l);
struct aa_label *aa_label_vec_find(struct aa_labelset *ls,
				   struct aa_profile **vec,
				   int n);
struct aa_label *aa_label_vec_merge(struct aa_profile **vec, int len,
				    gfp_t gfp);

struct aa_profile *aa_label_next_in_merge(struct label_it *I,
					  struct aa_label *a,
					  struct aa_label *b);
struct aa_label *aa_label_find_merge(struct aa_label *a, struct aa_label *b);
struct aa_label *aa_label_merge(struct aa_label *a, struct aa_label *b,
				gfp_t gfp);

bool aa_update_label_name(struct aa_namespace *ns, struct aa_label *label,
			  gfp_t gfp);

int aa_profile_snprint(char *str, size_t size, struct aa_namespace *ns,
		       struct aa_profile *profile, bool mode);
int aa_label_snprint(char *str, size_t size, struct aa_namespace *ns,
		     struct aa_label *label, bool mode);
int aa_label_asprint(char **strp, struct aa_namespace *ns,
		     struct aa_label *label, bool mode, gfp_t gfp);
int aa_label_acntsprint(char __counted **strp, struct aa_namespace *ns,
			struct aa_label *label, bool mode, gfp_t gfp);
void aa_label_audit(struct audit_buffer *ab, struct aa_namespace *ns,
		    struct aa_label *label, bool mode, gfp_t gfp);
void aa_label_seq_print(struct seq_file *f, struct aa_namespace *ns,
			struct aa_label *label, bool mode, gfp_t gfp);
void aa_label_printk(struct aa_namespace *ns, struct aa_label *label,
		     bool mode, gfp_t gfp);
struct aa_label *aa_label_parse(struct aa_label *base, char *str,
				gfp_t gfp, bool create);

static inline struct aa_label *aa_get_label(struct aa_label *l)
{
	if (l)
		kref_get(&(l->count));

	return l;
}

static inline struct aa_label *aa_get_label_not0(struct aa_label *l)
{
	if (l && kref_get_not0(&l->count))
		return l;

	return NULL;
}

/**
 * aa_get_label_rcu - increment refcount on a label that can be replaced
 * @l: pointer to label that can be replaced (NOT NULL)
 *
 * Returns: pointer to a refcounted label.
 *     else NULL if no label
 */
static inline struct aa_label *aa_get_label_rcu(struct aa_label __rcu **l)
{
	struct aa_label *c;

	rcu_read_lock();
	do {
		c = rcu_dereference(*l);
	} while (c && !kref_get_not0(&c->count));
	rcu_read_unlock();

	return c;
}

/**
 * aa_get_newest_label - find the newest version of @l
 * @l: the label to check for newer versions of
 *
 * Returns: refcounted newest version of @l taking into account
 *          replacement, renames and removals
 *          return @l.
 */
static inline struct aa_label *aa_get_newest_label(struct aa_label *l)
{
	if (!l)
		return NULL;

	if (label_invalid(l))
		return aa_get_label_rcu(&l->replacedby->label);

	return aa_get_label(l);
}

static inline void aa_put_label(struct aa_label *l)
{
	if (l)
		kref_put(&l->count, aa_label_kref);
}


struct aa_replacedby *aa_alloc_replacedby(struct aa_label *l);
void aa_free_replacedby_kref(struct kref *kref);

static inline struct aa_replacedby *aa_get_replacedby(struct aa_replacedby *r)
{
	if (r)
		kref_get(&(r->count));

	return r;
}

static inline void aa_put_replacedby(struct aa_replacedby *r)
{
	if (r)
		kref_put(&r->count, aa_free_replacedby_kref);
}

void __aa_update_replacedby(struct aa_label *orig, struct aa_label *new);

#endif /* __AA_LABEL_H */
