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

#include <linux/audit.h>
#include <linux/seq_file.h>

#include "include/apparmor.h"
#include "include/label.h"
#include "include/policy.h"
#include "include/sid.h"


/*
 * the aa_label represents the set of profiles confining an object
 *
 * Labels maintain a reference count to the set of pointers they reference
 * Labels are ref counted by
 *   tasks and object via the security field/security context off the field
 *   code - will take a ref count on a label if it needs the label
 *          beyond what is possible with an rcu_read_lock.
 *   profiles - each profile is a label
 *   sids - a pinned sid will keep a refcount of the label it is
 *          referencing
 *   objects - inode, files, sockets, ...
 *
 * Labels are not ref counted by the label set, so they maybe removed and
 * freed when no longer in use.
 *
 */

static void free_replacedby(struct aa_replacedby *r)
{
	if (r) {
		/* r->label will not updated any more as r is dead */
		aa_put_label(rcu_dereference_protected(r->label, true));
		kzfree(r);
	}
}

void aa_free_replacedby_kref(struct kref *kref)
{
	struct aa_replacedby *r = container_of(kref, struct aa_replacedby,
					       count);
	free_replacedby(r);
}

struct aa_replacedby *aa_alloc_replacedby(struct aa_label *l)
{
	struct aa_replacedby *r;

	r = kzalloc(sizeof(struct aa_replacedby), GFP_KERNEL);
	if (r) {
		kref_init(&r->count);
		rcu_assign_pointer(r->label, aa_get_label(l));
	}
	return r;
}

/* requires profile list write lock held */
void __aa_update_replacedby(struct aa_label *orig, struct aa_label *new)
{
	struct aa_label *tmp;

	AA_BUG(!orig);
	AA_BUG(!new);
	AA_BUG(!mutex_is_locked(&labels_ns(orig)->lock));

	tmp = rcu_dereference_protected(orig->replacedby->label,
					&labels_ns(orig)->lock);
	rcu_assign_pointer(orig->replacedby->label, aa_get_label(new));
	orig->flags |= FLAG_INVALID;
	aa_put_label(tmp);
}

/* helper fn for label_for_each_confined */
int aa_label_next_confined(struct aa_label *l, int i)
{
	AA_BUG(!l);
	AA_BUG(i < 0);

	for (; i < l->size; i++) {
		if (!profile_unconfined(l->ent[i]))
			return i;
	}

	return i;
}

#if 0
static int label_profile_pos(struct aa_label *l, struct aa_profile *profile)
{
	struct aa_profile *p;
	struct label_it i;

	AA_BUG(!profile);
	AA_BUG(!l);

	label_for_each(i, l, p) {
		if (p == profile)
			return i.i;
	}

	return -1;
}
#endif

#if 0
static bool profile_in_label(struct aa_profile *profile, struct aa_label *l)
{
	return label_profile_pos(l, profile) != -1;
}
#endif

static bool label_profiles_unconfined(struct aa_label *label)
{
	struct aa_profile *profile;
	struct label_it i;

	AA_BUG(!label);

	label_for_each(i, label, profile) {
		if (!profile_unconfined(profile))
			return false;
	}

	return true;
}

static int profile_cmp(struct aa_profile *a, struct aa_profile *b);
/**
 * aa_label_next_not_in_set - return the next profile of @sub not in @set
 * @I: label iterator
 * @set: label to test against
 * @sub: label to if is subset of @set
 *
 * Returns: profile in @sub that is not in @set, with iterator set pos after
 *     else NULL if @sub is a subset of @set
 */
struct aa_profile * aa_label_next_not_in_set(struct label_it *I,
					     struct aa_label *set,
					     struct aa_label *sub)
{
	AA_BUG(!set);
	AA_BUG(!I);
	AA_BUG(I->i < 0);
	AA_BUG(I->i > set->size);
	AA_BUG(!sub);
	AA_BUG(I->j < 0);
	AA_BUG(I->j > sub->size);

	while (I->j < sub->size && I->i < set->size) {
		int res = profile_cmp(sub->ent[I->j], set->ent[I->i]);
		if (res == 0) {
			(I->j)++;
			(I->i)++;
		} else if (res > 0)
			(I->i)++;
		else
			return sub->ent[(I->j)++];
	}

	if (I->j < sub->size)
		return sub->ent[(I->j)++];

	return NULL;
}

/**
 * aa_label_is_subset - test if @sub is a subset of @set
 * @set: label to test against
 * @sub: label to test if is subset of @set
 *
 * Returns: true if @sub is subset of @set
 *     else false
 */
bool aa_label_is_subset(struct aa_label *set, struct aa_label *sub)
{
	struct label_it i = { };

	AA_BUG(!set);
	AA_BUG(!sub);

	if (sub == set)
		return true;

	return aa_label_next_not_in_set(&i, set, sub) == NULL;
}

void aa_label_destroy(struct aa_label *label)
{
	AA_BUG(!label);

	if (label_invalid(label))
		labelsetstats_dec(labels_set(label), invalid);

	if (!label_isprofile(label)) {
		struct aa_profile *profile;
		struct label_it i;

		aa_put_str(label->hname);

		label_for_each(i, label, profile)
			aa_put_profile(profile);
	}

	aa_free_sid(label->sid);
	aa_put_replacedby(label->replacedby);
}

void aa_label_free(struct aa_label *label)
{
	if (!label)
		return;

	aa_label_destroy(label);
	labelstats_inc(freed);
	kzfree(label);
}

static void label_free_rcu(struct rcu_head *head)
{
	struct aa_label *l = container_of(head, struct aa_label, rcu);

	if (l->flags & FLAG_NS_COUNT)
		aa_free_namespace(labels_ns(l));
	else if (label_isprofile(l))
		aa_free_profile(labels_profile(l));
	else
		aa_label_free(l);
}

bool aa_label_remove(struct aa_labelset *ls, struct aa_label *label);
void aa_label_kref(struct kref *kref)
{
	struct aa_label *l = container_of(kref, struct aa_label, count);
	struct aa_namespace *ns = labels_ns(l);

	if (!ns) {
		/* never live, no rcu callback needed, just using the fn */
		label_free_rcu(&l->rcu);
		return;
	}

	(void) aa_label_remove(&ns->labels, l);

	/* TODO: if compound label and not invalid add to reclaim cache */
	call_rcu(&l->rcu, label_free_rcu);
}

bool aa_label_init(struct aa_label *label, int size)
{
	AA_BUG(!label);
	AA_BUG(size < 1);

	label->sid = aa_alloc_sid();
	if (label->sid == AA_SID_INVALID)
		return false;

	label->size = size;			/* doesn't include null */
	label->ent[size] = NULL;		/* null terminate */
	kref_init(&label->count);
	RB_CLEAR_NODE(&label->node);

	return true;
}

/**
 * aa_label_alloc - allocate a label with a profile vector of @size length
 * @size: size of profile vector in the label
 * @gfp: memory allocation type
 *
 * Returns: new label
 *     else NULL if failed
 */
struct aa_label *aa_label_alloc(int size, gfp_t gfp)
{
	struct aa_label *label;

	AA_BUG(size < 1);

	/* vector: size - 2 (size of array in label struct) + 1 for null */
	label = kzalloc(sizeof(*label) + sizeof(struct aa_label *) * (size - 1),
			gfp);
	AA_DEBUG("%s (%p)\n", __func__, label);
	if (!label)
		goto fail;

	if (!aa_label_init(label, size))
		goto fail;

	labelstats_inc(allocated);

	return label;

fail:
	kfree(label);
	labelstats_inc(failed);

	return NULL;
}

static bool __aa_label_remove(struct aa_labelset *ls, struct aa_label *label)
{
	AA_BUG(!ls);
	AA_BUG(!label);
	lockdep_assert_held(&ls->lock);
	AA_BUG(labels_set(label) != ls);

	if (label_invalid(label))
		labelstats_dec(invalid_intree);
	else
		__label_invalidate(label);

	if (label->flags & FLAG_IN_TREE) {
		labelsetstats_dec(ls, intree);
		rb_erase(&label->node, &ls->root);
		label->flags &= ~FLAG_IN_TREE;
		return true;
	}

	return false;
}

/**
 * aa_label_remove - remove a label from the labelset
 * @ls: set to remove the label from
 * @l: label to remove
 *
 * Returns: true if @l was removed from the tree
 *     else @l was not in tree so it could not be removed
 */
bool aa_label_remove(struct aa_labelset *ls, struct aa_label *l)
{
	unsigned long flags;
	bool res;

	write_lock_irqsave(&ls->lock, flags);
	res = __aa_label_remove(ls, l);
	write_unlock_irqrestore(&ls->lock, flags);

	return res;
}

#if 0
/* don't use when using ptr comparisons because nodes should never be
 * the same
 */
static bool __aa_label_replace(struct aa_labelset *ls, struct aa_label *old,
			       struct aa_label *new)
{
	AA_BUG(!ls);
	AA_BUG(!old);
	AA_BUG(!new);
	lockdep_assert_held(&ls->lock);
	AA_BUG(labels_set(old) != ls);
	AA_BUG(new->flags & FLAG_IN_TREE);

	if (label_invalid(old))
		labelstats_dec(invalid_intree);
	else
		__label_invalidate(old);

	if (old->flags & FLAG_IN_TREE) {
		rb_replace_node(&old->node, &new->node, &ls->root);
		old->flags &= ~FLAG_IN_TREE;
		new->flags |= FLAG_IN_TREE;
		return true;
	}

	return false;
}
#endif

static struct aa_label *__aa_label_insert(struct aa_labelset *ls,
					  struct aa_label *l);

static struct aa_label *__aa_label_remove_and_insert(struct aa_labelset *ls,
						     struct aa_label *remove,
						     struct aa_label *insert)
{
	AA_BUG(!ls);
	AA_BUG(!remove);
	AA_BUG(!insert);
	lockdep_assert_held(&ls->lock);
	AA_BUG(labels_set(remove) != ls);
	AA_BUG(insert->flags & FLAG_IN_TREE);

	__aa_label_remove(ls, remove);
	return __aa_label_insert(ls, insert);
}

struct aa_label *aa_label_remove_and_insert(struct aa_labelset *ls,
					    struct aa_label *remove,
					    struct aa_label *insert)
{
	unsigned long flags;
	struct aa_label *l;

	write_lock_irqsave(&ls->lock, flags);
	l = aa_get_label(__aa_label_remove_and_insert(ls, remove, insert));
	write_unlock_irqrestore(&ls->lock, flags);

	return l;
}

/**
 * aa_label_replace - replace a label @old with a new version @new
 * @ls: labelset being manipulated
 * @old: label to replace
 * @new: label replacing @old
 *
 * Returns: true if @old was in tree and replaced
 *     else @old was not in tree, and @new was not inserted
 */
bool aa_label_replace(struct aa_labelset *ls, struct aa_label *old,
		      struct aa_label *new)
{
	struct aa_label *l;
	unsigned long flags;
	bool res;

	write_lock_irqsave(&ls->lock, flags);
	if (!(old->flags & FLAG_IN_TREE))
		l = __aa_label_insert(ls, new);
	else
		l = __aa_label_remove_and_insert(ls, old, new);
	res = (l == new);
	write_unlock_irqrestore(&ls->lock, flags);

	return res;
}

static int ns_cmp(struct aa_namespace *a, struct aa_namespace *b)
{
	int res;

	AA_BUG(!a);
	AA_BUG(!b);
	AA_BUG(!a->base.name);
	AA_BUG(!b->base.name);

	if (a == b)
		return 0;

	res = a->level - b->level;
	if (res)
		return res;

	return strcmp(a->base.name, b->base.name);
}

/**
 * profile_cmp - profile comparision for set ordering
 * @a: profile to compare (NOT NULL)
 * @b: profile to compare (NOT NULL)
 *
 * Returns: <0  if a < b
 *          ==0 if a == b
 *          >0  if a > b
 */
static int profile_cmp(struct aa_profile *a, struct aa_profile *b)
{
	int res;

	AA_BUG(!a);
	AA_BUG(!b);
	AA_BUG(!a->ns);
	AA_BUG(!b->ns);
	AA_BUG(!a->base.hname);
	AA_BUG(!b->base.hname);

	if (a == b || a->base.hname == b->base.hname)
		return 0;
	res = ns_cmp(a->ns, b->ns);
	if (res)
		return res;

	return strcmp(a->base.hname, b->base.hname);
}

/**
 * label_vec_cmp - label comparision for set ordering
 * @a: label to compare (NOT NULL)
 * @vec: vector of profiles to compare (NOT NULL)
 * @n: length of @vec
 *
 * Returns: <0  if a < vec
 *          ==0 if a == vec
 *          >0  if a > vec
 */
static int label_vec_cmp(struct aa_label *a, struct aa_profile **vec, int n)
{
	int i;

	AA_BUG(!a);
	AA_BUG(!vec);
	AA_BUG(!*vec);
	AA_BUG(n <= 0);

	for (i = 0; i < a->size && i < n; i++) {
		int res = profile_cmp(a->ent[i], vec[i]);
		if (res != 0)
			return res;
	}

	return a->size - n;
}

/**
 * label_cmp - label comparision for set ordering
 * @a: label to compare (NOT NULL)
 * @b: label to compare (NOT NULL)
 *
 * Returns: <0  if a < b
 *          ==0 if a == b
 *          >0  if a > b
 */
static int label_cmp(struct aa_label *a, struct aa_label *b)
{
	AA_BUG(!b);

	if (a == b)
		return 0;

	return label_vec_cmp(a, b->ent, b->size);
}

/**
 * __aa_label_vec_find - find label that matches @vec in label set
 * @ls: set of labels to search (NOT NULL)
 * @vec: vec of profiles to find matching label for (NOT NULL)
 * @n: length of @vec
 *
 * Requires: @ls lock held
 *           caller to hold a valid ref on l
 *
 * Returns: unref counted @label if matching label is in tree
 *     else NULL if @vec equiv is not in tree
 */
static struct aa_label *__aa_label_vec_find(struct aa_labelset *ls,
					    struct aa_profile **vec, int n)
{
	struct rb_node *node;

	AA_BUG(!ls);
	AA_BUG(!vec);
	AA_BUG(!*vec);
	AA_BUG(n <= 0);

	node = ls->root.rb_node;
	while (node) {
		struct aa_label *this = rb_entry(node, struct aa_label, node);
		int result = label_vec_cmp(this, vec, n);

		if (result > 0)
			node = node->rb_left;
		else if (result < 0)
			node = node->rb_right;
		else
			return this;
	}

	return NULL;
}

/**
 * __aa_label_find - find label @l in label set
 * @ls: set of labels to search (NOT NULL)
 * @l: label to find (NOT NULL)
 *
 * Requires: @ls lock held
 *           caller to hold a valid ref on l
 *
 * Returns: unref counted @l if @l is in tree
 *          unref counted label that is equiv to @l in tree
 *     else NULL if @l or equiv is not in tree
 */
static struct aa_label *__aa_label_find(struct aa_labelset *ls,
					struct aa_label *l)
{
	AA_BUG(!l);

	return __aa_label_vec_find(ls, l->ent, l->size);
}

/**
 * aa_label_vec_find - find label @l in label set
 * @ls: set of labels to search (NOT NULL)
 * @vec: array of profiles to find equiv label for (NOT NULL)
 * @n: length of @vec
 *
 * Returns: refcounted label if @vec equiv is in tree
 *     else NULL if @vec equiv is not in tree
 */
struct aa_label *aa_label_vec_find(struct aa_labelset *ls,
				   struct aa_profile **vec,
				   int n)
{
	struct aa_label *label;
	unsigned long flags;

	AA_BUG(!ls);
	AA_BUG(!vec);
	AA_BUG(!*vec);
	AA_BUG(n <= 0);

	read_lock_irqsave(&ls->lock, flags);
	label = aa_get_label(__aa_label_vec_find(ls, vec, n));
	labelstats_inc(sread);
	read_unlock_irqrestore(&ls->lock, flags);

	return label;
}

/**
 * aa_label_find - find label @l in label set
 * @ls: set of labels to search (NOT NULL)
 * @l: label to find (NOT NULL)
 *
 * Requires: caller to hold a valid ref on l
 *
 * Returns: refcounted @l if @l is in tree
 *          refcounted label that is equiv to @l in tree
 *     else NULL if @l or equiv is not in tree
 */
struct aa_label *aa_label_find(struct aa_labelset *ls, struct aa_label *l)
{
	AA_BUG(!l);

	return aa_label_vec_find(ls, l->ent, l->size);
}

/**
 * __aa_label_insert - attempt to insert @l into a label set
 * @ls: set of labels to insert @l into (NOT NULL)
 * @l: new label to insert (NOT NULL)
 *
 * Requires: @ls->lock
 *           caller to hold a valid ref on l
 *
 * Returns: @l if successful in inserting @l
 *          else ref counted equivalent label that is already in the set.
 */
static struct aa_label *__aa_label_insert(struct aa_labelset *ls,
					  struct aa_label *l)
{
	struct rb_node **new, *parent = NULL;

	AA_BUG(!ls);
	AA_BUG(!l);
	lockdep_assert_held(&ls->lock);
	AA_BUG(l->flags & FLAG_IN_TREE);

	/* Figure out where to put new node */
	new = &ls->root.rb_node;
	while (*new) {
		struct aa_label *this = rb_entry(*new, struct aa_label, node);
		int result = label_cmp(l, this);

		parent = *new;
		if (result == 0) {
			labelsetstats_inc(ls, existing);
			return this;
		} else if (result < 0)
			new = &((*new)->rb_left);
		else /* (result > 0) */
			new = &((*new)->rb_right);
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&l->node, parent, new);
	rb_insert_color(&l->node, &ls->root);
	l->flags |= FLAG_IN_TREE;
	labelsetstats_inc(ls, insert);
	labelsetstats_inc(ls, intree);

        return 	l;
}

/**
 * aa_label_insert - insert label @l into @ls or return existing label
 * @ls - labelset to insert @l into
 * @l - label to insert
 *
 * Requires: caller to hold a valid ref on l
 *
 * Returns: ref counted @l if successful in inserting @l
 *     else ref counted equivalent label that is already in the set
 */
struct aa_label *aa_label_insert(struct aa_labelset *ls, struct aa_label *l)
{
	struct aa_label *label;
	unsigned long flags;

	AA_BUG(!ls);
	AA_BUG(!l);

	/* check if label exists before taking lock */
	if (!label_invalid(l)) {
		read_lock_irqsave(&ls->lock, flags);
		label = aa_get_label(__aa_label_find(ls, l));
		read_unlock_irqrestore(&ls->lock, flags);
		labelstats_inc(fread);
		if (label)
			return label;
	}

	write_lock_irqsave(&ls->lock, flags);
	label = aa_get_label(__aa_label_insert(ls, l));
	write_unlock_irqrestore(&ls->lock, flags);

	return label;
}

struct aa_label *aa_label_vec_find_or_create(struct aa_labelset *ls,
					     struct aa_profile **vec, int len)
{
	struct aa_label *label = aa_label_vec_find(ls, vec, len);
	if (label)
		return label;

	return aa_label_vec_merge(vec, len, GFP_KERNEL);
}

/**
 * aa_label_next_in_merge - find the next profile when merging @a and @b
 * @I: label iterator
 * @a: label to merge
 * @b: label to merge
 *
 * Returns: next profile
 *     else null if no more profiles
 */
struct aa_profile *aa_label_next_in_merge(struct label_it *I,
					  struct aa_label *a,
					  struct aa_label *b)
{
	AA_BUG(!a);
	AA_BUG(!b);
	AA_BUG(!I);
	AA_BUG(I->i < 0);
	AA_BUG(I->i > a->size);
	AA_BUG(I->j < 0);
	AA_BUG(I->j > b->size);

	if (I->i < a->size) {
		if (I->j < b->size) {
			int res = profile_cmp(a->ent[I->i], b->ent[I->j]);
			if (res > 0)
				return b->ent[(I->j)++];
			if (res == 0)
				(I->j)++;
		}

		return a->ent[(I->i)++];
	}

	if (I->j < b->size)
		return b->ent[(I->j)++];

	return NULL;
}

/**
 * label_merge_cmp - cmp of @a merging with @b against @z for set ordering
 * @a: label to merge then compare (NOT NULL)
 * @b: label to merge then compare (NOT NULL)
 * @z: label to compare merge against (NOT NULL)
 *
 * Assumes: using the most recent versions of @a, @b, and @z
 *
 * Returns: <0  if a < b
 *          ==0 if a == b
 *          >0  if a > b
 */
static int label_merge_cmp(struct aa_label *a, struct aa_label *b,
                           struct aa_label *z)
{
	struct aa_profile *p = NULL;
	struct label_it i = { };
	int k;

	AA_BUG(!a);
	AA_BUG(!b);
	AA_BUG(!z);

	for (k = 0;
	     k < z->size && (p = aa_label_next_in_merge(&i, a, b));
	     k++) {
		int res = profile_cmp(p, z->ent[k]);

		if (res != 0)
			return res;
	}

	if (p)
		return 1;
	else if (k < z->size)
		return -1;
	return 0;
}

#if 0
/**
 * label_merge_len - find the length of the merge of @a and @b
 * @a: label to merge (NOT NULL)
 * @b: label to merge (NOT NULL)
 *
 * Assumes: using newest versions of labels @a and @b
 *
 * Returns: length of a label vector for merge of @a and @b
 */
static int label_merge_len(struct aa_label *a, struct aa_label *b)
{
	int len = a->size + b->size;
	int i, j;

	AA_BUG(!a);
	AA_BUG(!b);

	/* find entries in common and remove from count */
	for (i = j = 0; i < a->size && j < b->size; ) {
		int res = profile_cmp(a->ent[i], b->ent[j]);
		if (res == 0) {
			len--;
			i++;
			j++;
		} else if (res < 0)
			i++;
		else
			j++;
	}

	return len;
}
#endif

/**
 * aa_sort_and_merge_profiles - canonical sort and merge a list of profiles
 * @n: number of refcounted profiles in the list (@n > 0)
 * @ps: list of profiles to sort and merge
 *
 * Returns: the number of duplicates eliminated == references put
 */
static int aa_sort_and_merge_profiles(int n, struct aa_profile **ps)
{
	int i, dups = 0;

	AA_BUG(n < 1);
	AA_BUG(!ps);

	/* label lists are usually small so just use insertion sort */
	for (i = 1; i < n; i++) {
		struct aa_profile *tmp = ps[i];
		int pos, j;

		for (pos = i - 1 - dups; pos >= 0; pos--) {
			int res = profile_cmp(ps[pos], tmp);
			if (res == 0) {
				aa_put_profile(tmp);
				dups++;
				goto continue_outer;
			} else if (res < 0)
				break;
		}
		pos++;

		for (j = i - dups; j > pos; j--)
			ps[j] = ps[j - 1];
		ps[pos] = tmp;
	continue_outer:
		; /* sigh empty statement required after the label */
	}

	return dups;
}

/**
 * __label_merge - create a new label by merging @a and @b
 * @l: preallocated label to merge into (NOT NULL)
 * @a: label to merge with @b  (NOT NULL)
 * @b: label to merge with @a  (NOT NULL)
 *
 * Returns: ref counted label either l if merge is unique
 *          a if b is a subset of a
 *          b if a is a subset of b
 *
 * NOTE: will not use l if the merge results in l == a or b
 *
 *       Must be used within labelset write lock to avoid racing with
 *       label invalidation.
 */
static struct aa_label *__label_merge(struct aa_label *l, struct aa_label *a,
				      struct aa_label *b)
{
	struct aa_profile *next;
	struct label_it i;
	int k = 0, invcount = 0;

	AA_BUG(!a);
	AA_BUG(a->size < 0);
	AA_BUG(!b);
	AA_BUG(b->size < 0);
	AA_BUG(!l);
	AA_BUG(l->size < a->size + b->size);

	if (a == b)
		return aa_get_label(a);

	label_for_each_in_merge(i, a, b, next) {
		if (PROFILE_INVALID(next)) {
			l->ent[k] = aa_get_newest_profile(next);
			if (next->label.replacedby !=
			    l->ent[k]->label.replacedby)
				invcount++;
			k++;
		} else
			l->ent[k++] = aa_get_profile(next);
	}
	/* set to actual size which is <= allocated len */
	l->size = k;
	l->ent[k] = NULL;

	if (invcount) {
		l->size -= aa_sort_and_merge_profiles(l->size, &l->ent[0]);
		if (label_profiles_unconfined(l))
			l->flags |= FLAG_UNCONFINED;
	} else {
		/* merge is same as at least one of the labels */
		if (k == a->size)
			return aa_get_label(a);
		else if (k == b->size)
			return aa_get_label(b);

		l->flags |= a->flags & b->flags & FLAG_UNCONFINED;
	}

	return aa_get_label(l);
}

/**
 * labelset_of_merge - find into which labelset a merged label should be inserted
 * @a: label to merge and insert
 * @b: label to merge and insert
 *
 * Returns: labelset that the merged label should be inserted into
 */
static struct aa_labelset *labelset_of_merge(struct aa_label *a, struct aa_label *b)
{
	struct aa_namespace *nsa = labels_ns(a);
	struct aa_namespace *nsb = labels_ns(b);

	if (ns_cmp(nsa, nsb) <= 0)
		return &nsa->labels;
	return &nsb->labels;
}

/**
 * __aa_label_find_merge - find label that is equiv to merge of @a and @b
 * @ls: set of labels to search (NOT NULL)
 * @a: label to merge with @b  (NOT NULL)
 * @b: label to merge with @a  (NOT NULL)
 *
 * Requires: read_lock held
 *
 * Returns: unref counted label that is equiv to merge of @a and @b
 *     else NULL if merge of @a and @b is not in set
 */
static struct aa_label *__aa_label_find_merge(struct aa_labelset *ls,
					      struct aa_label *a,
					      struct aa_label *b)
{
	struct rb_node *node;

	AA_BUG(!ls);
	AA_BUG(!a);
	AA_BUG(!b);

	if (a == b)
		return __aa_label_find(ls, a);

	node  = ls->root.rb_node;
	while (node) {
		struct aa_label *this = container_of(node, struct aa_label,
						     node);
		int result = label_merge_cmp(a, b, this);

		if (result < 0)
			node = node->rb_left;
		else if (result > 0)
			node = node->rb_right;
		else
			return this;
	}

	return NULL;
}


/**
 * __aa_label_find_merge - find label that is equiv to merge of @a and @b
 * @a: label to merge with @b  (NOT NULL)
 * @b: label to merge with @a  (NOT NULL)
 *
 * Requires: labels be fully constructed with a valid ns
 *
 * Returns: ref counted label that is equiv to merge of @a and @b
 *     else NULL if merge of @a and @b is not in set
 */
struct aa_label *aa_label_find_merge(struct aa_label *a, struct aa_label *b)
{
	struct aa_labelset *ls;
	struct aa_label *label, *ar = NULL, *br = NULL;
	unsigned long flags;

	AA_BUG(!a);
	AA_BUG(!b);

	ls = labelset_of_merge(a, b);
	read_lock_irqsave(&ls->lock, flags);
	if (label_invalid(a))
		a = ar = aa_get_newest_label(a);
	if (label_invalid(b))
		b = br = aa_get_newest_label(b);
	label = aa_get_label(__aa_label_find_merge(ls, a, b));
	read_unlock_irqrestore(&ls->lock, flags);
	aa_put_label(ar);
	aa_put_label(br);
	labelsetstats_inc(ls, msread);

	return label;
}

/**
 * aa_label_merge - attempt to insert new merged label of @a and @b
 * @ls: set of labels to insert label into (NOT NULL)
 * @a: label to merge with @b  (NOT NULL)
 * @b: label to merge with @a  (NOT NULL)
 * @gfp: memory allocation type
 *
 * Requires: caller to hold valid refs on @a and @b
 *           labels be fully constructed with a valid ns
 *
 * Returns: ref counted new label if successful in inserting merge of a & b
 *     else ref counted equivalent label that is already in the set.
 *     else NULL if could not create label (-ENOMEM)
 */
struct aa_label *aa_label_merge(struct aa_label *a, struct aa_label *b,
				gfp_t gfp)
{
	struct aa_label *label = NULL;
	struct aa_labelset *ls;
	unsigned long flags;

	AA_BUG(!a);
	AA_BUG(!b);

	if (a == b)
		return aa_get_newest_label(a);

	ls = labelset_of_merge(a, b);

	/* TODO: enable when read side is lockless
	 * check if label exists before taking locks
	if (!label_invalid(a) && !label_invalid(b))
		label = aa_label_find_merge(a, b);
	*/

	if (!label) {
		struct aa_label *new, *l;

		a = aa_get_newest_label(a);
		b = aa_get_newest_label(b);

		/* could use label_merge_len(a, b), but requires double
		 * comparison for small savings
		 */
		new = aa_label_alloc(a->size + b->size, gfp);
		if (!new)
			return NULL;

		write_lock_irqsave(&ls->lock, flags);
		l = __label_merge(new, a, b);
		if (l != new) {
			/* new may not be fully setup so no put_label */
			aa_label_free(new);
			new = NULL;
		}
		if (!(l->flags & FLAG_IN_TREE))
			label = aa_get_label(__aa_label_insert(ls, l));
		write_unlock_irqrestore(&ls->lock, flags);
		aa_put_label(new);
		aa_put_label(l);
		aa_put_label(a);
		aa_put_label(b);
	}

	return label;
}

/* requires sort and merge done first */
struct aa_label *aa_label_vec_merge(struct aa_profile **vec, int len,
				    gfp_t gfp)
{
	struct aa_label *label = NULL;
	struct aa_labelset *ls;
	unsigned long flags;
	struct aa_label *new;
	int i;

	AA_BUG(!vec);

	if (len == 1)
		return aa_get_label(&vec[0]->label);

	ls = labels_set(&vec[len - 1]->label);

	/* TODO: enable when read side is lockless
	 * check if label exists before taking locks
	 */
	new = aa_label_alloc(len, gfp);
	if (!new)
		return NULL;

	write_lock_irqsave(&ls->lock, flags);
	for (i = 0; i < len; i++) {
		new->ent[i] = aa_get_profile(vec[i]);
		label = __aa_label_insert(ls, new);
		if (label != new) {
			aa_get_label(label);
			/* not fully constructed don't put */
			aa_label_free(new);
		}
	}
	write_unlock_irqrestore(&ls->lock, flags);

	return label;
}

/**
 * aa_update_label_name - update a label to have a stored name
 * @ns: ns being viewed from (NOT NULL)
 * @label: label to update (NOT NULL)
 * @gfp: type of memory allocation
 *
 * Requires: labels_set(label) not locked in caller
 *
 * note: only updates the label name if it does not have a name already
 *       and if it is in the labelset
 */
bool aa_update_label_name(struct aa_namespace *ns, struct aa_label *label,
			  gfp_t gfp)
{
	struct aa_labelset *ls;
	unsigned long flags;
	char __counted *name;
	bool res = false;

	AA_BUG(!ns);
	AA_BUG(!label);

	if (label->hname || labels_ns(label) != ns)
		return res;

	if (aa_label_acntsprint(&name, ns, label, false, gfp) == -1)
		return res;

	ls = labels_set(label);
	write_lock_irqsave(&ls->lock, flags);
	if (!label->hname && label->flags & FLAG_IN_TREE) {
		label->hname = name;
		res = true;
	} else
		aa_put_str(name);
	write_unlock_irqrestore(&ls->lock, flags);

	return res;
}

/* cached label name is present and visible
 * @label->hname only exists if label is namespace hierachical */
static inline bool label_name_visible(struct aa_namespace *ns,
				      struct aa_label *label)
{
	if (label->hname && labels_ns(label) == ns)
		return true;

	return false;
}

/* helper macro for snprint routines */
#define update_for_len(total, len, size, str)	\
do {					\
	AA_BUG(len < 0);		\
	total += len;			\
	len = min(len, size);		\
	size -= len;			\
	str += len;			\
} while (0)

/**
 * aa_modename_snprint - print the mode name of a profile or label to a buffer
 * @str: buffer to write to (MAY BE NULL if @size == 0)
 * @size: size of buffer
 * @ns: namespace profile is being viewed from (NOT NULL)
 * @label: label to print the mode of (NOT NULL)
 *
 * Returns: size of name written or would be written if larger than
 *          available buffer
 *
 * Note: will print every mode name visible (mode1)(mode2)(mode3)
 *       this is likely not what is desired for most interfaces
 *       use aa_mode_snprint to get the standard mode format
 */
static int aa_modename_snprint(char *str, size_t size, struct aa_namespace *ns,
			       struct aa_label *label)
{
	struct aa_profile *profile;
	struct label_it i;
	int total = 0;
	size_t len;

	label_for_each(i, label, profile) {
		const char *modestr;
		if (!aa_ns_visible(ns, profile->ns))
			continue;
		/* no mode for 'unconfined' */
		if (profile_unconfined(profile) &&
		    profile == profile->ns->unconfined)
			break;
		modestr = aa_profile_mode_names[profile->mode];
		len = snprintf(str, size, "(%s)", modestr);
		update_for_len(total, len, size, str);
	}

	return total;
}

/**
 * aa_modechr_snprint - print the mode chr of a profile or labels to a buffer
 * @str: buffer to write to (MAY BE NULL if @size == 0)
 * @size: size of buffer
 * @ns: namespace profile is being viewed from (NOT NULL)
 * @label: label to print the mode chr of (NOT NULL)
 *
 * Returns: size of mode string written or would be written if larger than
 *          available buffer
 *
 * Note: will print the chr of every visible profile (123)
 *       this is likely not what is desired for most interfaces
 *       use aa_mode_snprint to get the standard mode format
 */
static int aa_modechr_snprint(char *str, size_t size, struct aa_namespace *ns,
			      struct aa_label *label)
{
	struct aa_profile *profile;
	struct label_it i;
	int total = 0;
	size_t len;

	len = snprintf(str, size, "(");
	update_for_len(total, len, size, str);
	label_for_each(i, label, profile) {
		const char *modestr;
		if (!aa_ns_visible(ns, profile->ns))
			continue;
		modestr = aa_profile_mode_names[profile->mode];
		/* just the first char of the modestr */
		len = snprintf(str, size, "%c", *modestr);
		update_for_len(total, len, size, str);
	}
	len = snprintf(str, size, ")");
	update_for_len(total, len, size, str);

	return total;
}

/**
 * aa_mode_snprint - print the mode of a profile or label to a buffer
 * @str: buffer to write to (MAY BE NULL if @size == 0)
 * @size: size of buffer
 * @ns: namespace profile is being viewed from (NOT NULL)
 * @label: label to print the mode of (NOT NULL)
 * @count: number of label entries to be printed (<= 0 if unknown)
 *
 * Returns: size of name written or would be written if larger than
 *          available buffer
 *
 * Note: dynamically switches between mode name, and mode char format as
 *       appropriate
 *       will not print anything if the label is not visible
 */
static int aa_mode_snprint(char *str, size_t size, struct aa_namespace *ns,
			   struct aa_label *label, int count)
{
	struct aa_profile *profile;
	struct label_it i;

	if (count <= 0) {
		count = 0;
		label_for_each(i, label, profile) {
			if (aa_ns_visible(ns, profile->ns))
				count++;
		}
	}

	if (count == 0)
		return 0;

	if (count == 1)
		return aa_modename_snprint(str, size, ns, label);

	return aa_modechr_snprint(str, size, ns, label);
}

/**
 * aa_snprint_profile - print a profile name to a buffer
 * @str: buffer to write to. (MAY BE NULL if @size == 0)
 * @size: size of buffer
 * @ns: namespace profile is being viewed from (NOT NULL)
 * @profile: profile to view (NOT NULL)
 * @mode: whether to include the mode string
 *
 * Returns: size of name written or would be written if larger than
 *          available buffer
 *
 * Note: will not print anything if the profile is not visible
 */
int aa_profile_snprint(char *str, size_t size, struct aa_namespace *ns,
		       struct aa_profile *profile, bool mode)
{
	const char *ns_name = aa_ns_name(ns, profile->ns);

	AA_BUG(!str && size != 0);
	AA_BUG(!ns);
	AA_BUG(!profile);

	if (!ns_name)
		return 0;

	if (mode && profile != profile->ns->unconfined) {
		const char *modestr = aa_profile_mode_names[profile->mode];
		if (strlen(ns_name))
			return snprintf(str, size, ":%s://%s (%s)", ns_name,
					profile->base.hname, modestr);
		return snprintf(str, size, "%s (%s)", profile->base.hname,
				modestr);
	}

	if (strlen(ns_name))
		return snprintf(str, size, ":%s://%s", ns_name,
				profile->base.hname);
	return snprintf(str, size, "%s", profile->base.hname);
}

/**
 * aa_label_snprint - print a label name to a string buffer
 * @str: buffer to write to. (MAY BE NULL if @size == 0)
 * @size: size of buffer
 * @ns: namespace profile is being viewed from (NOT NULL)
 * @label: label to view (NOT NULL)
 * @mode: whether to include the mode string
 *
 * Returns: size of name written or would be written if larger than
 *          available buffer
 *
 * Note: labels do not have to be strictly hierarchical to the ns as
 *       objects may be shared across different namespaces and thus
 *       pickup labeling from each ns.  If a particular part of the
 *       label is not visible it will just be excluded.  And if none
 *       of the label is visible "---" will be used.
 */
int aa_label_snprint(char *str, size_t size, struct aa_namespace *ns,
		     struct aa_label *label, bool mode)
{
	struct aa_profile *profile;
	struct label_it i;
	int count = 0, total = 0;
	size_t len;

	AA_BUG(!str && size != 0);
	AA_BUG(!ns);
	AA_BUG(!label);

	label_for_each(i, label, profile) {
		if (aa_ns_visible(ns, profile->ns)) {
			if (count > 0) {
				len = snprintf(str, size, "//&");
				update_for_len(total, len, size, str);
			}
			len = aa_profile_snprint(str, size, ns, profile, false);
			update_for_len(total, len, size, str);
			count++;
		}
	}

	if (count == 0)
		return snprintf(str, size, aa_hidden_ns_name);

	/* count == 1 && ... is for backwards compat where the mode
	 * is not displayed for 'unconfined' in the current ns
	 */
	if (mode &&
	    !(count == 1 && labels_ns(label) == ns &&
	      labels_profile(label) == ns->unconfined)) {
		len = snprintf(str, size, " ");
		update_for_len(total, len, size, str);
		len = aa_mode_snprint(str, size, ns, label, count);
		update_for_len(total, len, size, str);
	}

	return total;
}
#undef update_for_len

/**
 * aa_label_asprint - allocate a string buffer and print label into it
 * @strp: Returns - the allocated buffer with the label name. (NOT NULL)
 * @ns: namespace profile is being viewed from (NOT NULL)
 * @label: label to view (NOT NULL)
 * @mode: whether to include the mode string
 * @gfp: kernel memory allocation type
 *
 * Returns: size of name written or would be written if larger than
 *          available buffer
 */
int aa_label_asprint(char **strp, struct aa_namespace *ns,
		     struct aa_label *label, bool mode, gfp_t gfp)
{
	int size;

	AA_BUG(!strp);
	AA_BUG(!ns);
	AA_BUG(!label);

	size = aa_label_snprint(NULL, 0, ns, label, mode);
	if (size < 0)
		return size;

	*strp = kmalloc(size + 1, gfp);
	if (!*strp)
		return -ENOMEM;
	return aa_label_snprint(*strp, size + 1, ns, label, mode);
}

/**
 * aa_label_acntsprint - allocate a __counted string buffer and print label
 * @strp: buffer to write to. (MAY BE NULL if @size == 0)
 * @ns: namespace profile is being viewed from (NOT NULL)
 * @label: label to view (NOT NULL)
 * @mode: whether to include the mode string
 * @gfp: kernel memory allocation type
 *
 * Returns: size of name written or would be written if larger than
 *          available buffer
 */
int aa_label_acntsprint(char __counted **strp, struct aa_namespace *ns,
			struct aa_label *label, bool mode, gfp_t gfp)
{
	int size;

	AA_BUG(!strp);
	AA_BUG(!ns);
	AA_BUG(!label);

	size = aa_label_snprint(NULL, 0, ns, label, mode);
	if (size < 0)
		return size;

	*strp = aa_str_alloc(size + 1, gfp);
	if (!*strp)
		return -ENOMEM;
	return aa_label_snprint(*strp, size + 1, ns, label, mode);
}


void aa_label_audit(struct audit_buffer *ab, struct aa_namespace *ns,
		    struct aa_label *label, bool mode, gfp_t gfp)
{
	const char *str;
	char *name = NULL;
	int len;

	AA_BUG(!ab);
	AA_BUG(!ns);
	AA_BUG(!label);

	if (label_name_visible(ns, label)) {
		str = (char *) label->hname;
		len = strlen(str);
	} else {
		labelstats_inc(audit_name_alloc);
		len  = aa_label_asprint(&name, ns, label, mode, gfp);
		if (len == -1) {
			labelstats_inc(audit_name_fail);
			AA_DEBUG("label print error");
			return;
		}
		str = name;
	}

	if (audit_string_contains_control(str, len))
		audit_log_n_hex(ab, str, len);
	else
		audit_log_n_string(ab, str, len);

	kfree(name);
}

void aa_label_seq_print(struct seq_file *f, struct aa_namespace *ns,
			struct aa_label *label, bool mode, gfp_t gfp)
{
	AA_BUG(!f);
	AA_BUG(!ns);
	AA_BUG(!label);

	if (!label_name_visible(ns, label)) {
		char *str;
		int len;

		labelstats_inc(seq_print_name_alloc);
		len = aa_label_asprint(&str, ns, label, mode, gfp);
		if (len == -1) {
			labelstats_inc(seq_print_name_fail);
			AA_DEBUG("label print error");
			return;
		}
		seq_printf(f, "%s", str);
		kfree(str);
	} else
		seq_printf(f, "%s", label->hname);
}

void aa_label_printk(struct aa_namespace *ns, struct aa_label *label, bool mode,
		     gfp_t gfp)
{
	char *str;
	int len;

	AA_BUG(!ns);
	AA_BUG(!label);

	if (!label_name_visible(ns, label)) {
		labelstats_inc(printk_name_alloc);
		len = aa_label_asprint(&str, ns, label, mode, gfp);
		if (len == -1) {
			labelstats_inc(printk_name_fail);
			AA_DEBUG("label print error");
			return;
		}
		printk("%s", str);
		kfree(str);
	} else
		printk("%s", label->hname);
}

static int label_count_str_entries(const char *str)
{
	const char *split;
	int count = 1;

	AA_BUG(!str);

	for (split = strstr(str, "//&"); split; split = strstr(str, "//&")) {
		count++;
		str = split + 3;
	}

	return count;
}

/**
 * aa_label_parse - parse, validate and convert a text string to a label
 * @base: base label to use for lookups (NOT NULL)
 * @str: null terminated text string (NOT NULL)
 * @gfp: allocation type
 * @create: true if should create compound labels if they don't exist
 *
 * Returns: the matching refcounted label if present
 *     else ERRPTR
 */
struct aa_label *aa_label_parse(struct aa_label *base, char *str, gfp_t gfp,
				bool create)
{
	DEFINE_PROFILE_VEC(vec, tmp);
	struct aa_label *l;
	int i, len, error;
	char *split;

	AA_BUG(!base);
	AA_BUG(!str);

	len = label_count_str_entries(str);
	error = aa_setup_profile_vec(vec, tmp, len);
	if (error)
		return ERR_PTR(error);

	for (split = strstr(str, "//&"), i = 0; split && i < len; i++) {
		vec[i] = aa_fqlookupn_profile(base, str, split - str);
		if (!vec[i])
			goto fail;
		str = split + 3;
		split = strstr(str, "//&");
	}
	vec[i] = aa_fqlookupn_profile(base, str, strlen(str));
	if (!vec[i])
		goto fail;
	if (len == 1)
		/* no need to free vec as len < LOCAL_VEC_ENTRIES */
		return &vec[0]->label;

	i = aa_sort_and_merge_profiles(len, vec);
	len -= i;

	if (create)
		l = aa_label_vec_find_or_create(labels_set(base), vec, len);
	else
		l = aa_label_vec_find(labels_set(base), vec, len);
	if (!l)
		l = ERR_PTR(-ENOENT);

out:
	/* use adjusted len from after sort_and_merge, not original */
	aa_cleanup_profile_vec(vec, tmp, len);
	return l;

fail:
	l = ERR_PTR(-ENOENT);
	goto out;
}


/**
 * aa_labelset_destroy - remove all labels from the label set
 * @ls: label set to cleanup (NOT NULL)
 *
 * Labels that are removed from the set may still exist beyond the set
 * being destroyed depending on their reference counting
 */
void aa_labelset_destroy(struct aa_labelset *ls)
{
	struct rb_node *node;
	unsigned long flags;

	AA_BUG(!ls);

	write_lock_irqsave(&ls->lock, flags);
	for (node = rb_first(&ls->root); node; node = rb_first(&ls->root)) {
		struct aa_label *this = rb_entry(node, struct aa_label, node);
		__aa_label_remove(ls, this);
	}
	write_unlock_irqrestore(&ls->lock, flags);
}

/*
 * @ls: labelset to init (NOT NULL)
 */
void aa_labelset_init(struct aa_labelset *ls)
{
	AA_BUG(!ls);

	rwlock_init(&ls->lock);
	ls->root = RB_ROOT;
	labelstats_init(&ls);
}

static struct aa_label *labelset_next_invalid(struct aa_labelset *ls)
{
	struct aa_label *label;
	struct rb_node *node;
	unsigned long flags;

	AA_BUG(!ls);

	read_lock_irqsave(&ls->lock, flags);

	__labelset_for_each(ls, node) {
		struct aa_profile *p;
		struct label_it i;

		label = rb_entry(node, struct aa_label, node);
		if (label_invalid(label))
			goto out;

		label_for_each(i, label, p) {
			if (PROFILE_INVALID(p))
				goto out;
		}
	}
	label = NULL;

out:
	aa_get_label(label);
	read_unlock_irqrestore(&ls->lock, flags);

	return label;
}

/**
 * __label_update - insert updated version of @label into labelset
 * @label - the label to update/repace
 *
 * Returns: new label that is up to date
 *     else NULL on failure
 *
 * Requires: @ns lock be held
 *
 * Note: worst case is the stale @label does not get updated and has
 *       to be updated at a later time.
 */
static struct aa_label *__label_update(struct aa_label *label)
{
	struct aa_label *l, *tmp;
	struct aa_labelset *ls;
	struct aa_profile *p;
	struct label_it i;
	unsigned long flags;
	int invcount = 0;

	AA_BUG(!label);
	AA_BUG(!mutex_is_locked(&labels_ns(label)->lock));

	l = aa_label_alloc(label->size, GFP_KERNEL);
	if (!l)
		return NULL;

	if (!label->replacedby) {
		struct aa_replacedby *r = aa_alloc_replacedby(l);
		if (!r) {
			aa_put_label(l);
			return NULL;
		}
		/* only label update will set replacedby so ns lock is enough */
		label->replacedby = r;
	}

	/* while holding the ns_lock will stop profile replacement, removal,
	 * and label updates, label merging and removal can be occuring
	 */

	ls = labels_set(label);
	write_lock_irqsave(&ls->lock, flags);
	/* circular ref only broken by replace or remove */
	l->replacedby = aa_get_replacedby(label->replacedby);
	__aa_update_replacedby(label, l);

	label_for_each(i, label, p) {
		l->ent[i.i] = aa_get_newest_profile(p);
		if (&l->ent[i.i]->label.replacedby != &p->label.replacedby)
			invcount++;
	}

	/* updated label invalidated by being removed/renamed from labelset */
	if (invcount) {
		l->size -= aa_sort_and_merge_profiles(l->size, &l->ent[0]);
		if (labels_set(label) == labels_set(l)) {
			AA_BUG(__aa_label_remove_and_insert(labels_set(label), label, l) != l);
		} else {
			aa_label_remove(labels_set(label), label);
			goto other_ls_insert;
		}
	} else {
		AA_BUG(labels_ns(label) != labels_ns(l));
		AA_BUG(__aa_label_remove_and_insert(labels_set(label), label, l) != l);
	}
	write_unlock_irqrestore(&ls->lock, flags);

	return l;

other_ls_insert:
	write_unlock_irqrestore(&ls->lock, flags);
	tmp = aa_label_insert(labels_set(l), l);
	if (tmp != l) {
		aa_put_label(l);
		l = tmp;
	}

	return l;
}

/**
 * __labelset_update - invalidate and update labels in @ns
 * @ns: namespace to update and invalidate labels in  (NOT NULL)
 *
 * Requires: @ns lock be held
 *
 * Walk the labelset ensuring that all labels are up to date and valid
 * Any label that is outdated is replaced and by an updated version
 * invalidated and removed from the tree.
 *
 * If failures happen due to memory pressures then stale labels will
 * be left in place until the next pass.
 */
static void __labelset_update(struct aa_namespace *ns)
{
	struct aa_label *label;

	AA_BUG(!ns);
	AA_BUG(!mutex_is_locked(&ns->lock));

	do {
		label = labelset_next_invalid(&ns->labels);
		if (label) {
			struct aa_label *l;
			l = __label_update(label);
			aa_put_label(l);
			aa_put_label(label);
		}
	} while (label);
}

/**
 * __aa_labelset_invalidate_all - invalidate labels in @ns and below
 * @ns: ns to start invalidation at (NOT NULL)
 *
 * Requires: @ns lock be held
 *
 * Invalidates labels based on @p in @ns and any children namespaces.
*/
void __aa_labelset_update_all(struct aa_namespace *ns)
{
	struct aa_namespace *child;

	AA_BUG(!ns);
	AA_BUG(!mutex_is_locked(&ns->lock));

	__labelset_update(ns);

	list_for_each_entry(child, &ns->sub_ns, base.list) {
		mutex_lock(&child->lock);
		__aa_labelset_update_all(child);
		mutex_unlock(&child->lock);
	}
}
