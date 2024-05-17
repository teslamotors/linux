// SPDX-License-Identifier: GPL-2.0-only
/*
 * fs_mark.c: Responsible for managing fsnotify marks on inodes and XPin's
 * superblock context (which is a glorified wrapper around an fsnotify group).
 */

#define pr_fmt(fmt) KBUILD_MODNAME "-fsmark: " fmt

#include "xpin_internal.h"
#include <linux/spinlock.h>
#include <linux/lockdep.h>


static int xpin_fsnotify_handle_event(
	struct fsnotify_group *group,
	struct inode *inode,
	u32 mask,
	const void *data,
	int data_type,
	const struct qstr *file_name,
	u32 cookie,
	struct fsnotify_iter_info *iter_info)
{
	/* Should never be called because a zero mask is set on the marks */
	WARN_ON_ONCE(1);
	return -EINVAL;
}


/*
 * Called when the inode mark is trying to be removed, perhaps due to an unmount
 * operation. Free up resources and references to prevent the inode from getting
 * stuck (and therefore causing a panic).
 */
static void xpin_fsnotify_freeing_mark(
	struct fsnotify_mark *mark,
	struct fsnotify_group *fsg)
{
	struct xpin_inode_mark *xmark;
	struct xpin_inode_ctx *xinode;
	struct xpin_exception *cur;
	struct hlist_node *next;
	HLIST_HEAD(to_remove);

	xmark = container_of(mark, typeof(*xmark), mark);
	xinode = xpin_inode(xmark->inode);

	spin_lock(&xinode->lock);

	/* This function appears to be safe to use with racing RCU traversals */
	hlist_move_list(&xinode->exceptions, &to_remove);

	spin_unlock(&xinode->lock);

	/* Wait for RCU traversals of the exceptions list to end */
	synchronize_rcu();

	/* Free each exception, dropping references to the fsnotify mark */
	hlist_for_each_entry_safe(cur, next, &to_remove, node) {
		hlist_del_init(&cur->node);
		xpin_exception_free(cur);
	}
}


/* This will only be called once there's no more references to xmark */
static void xpin_fsnotify_free_mark(struct fsnotify_mark *mark)
{
	struct xpin_inode_mark *xmark;

	xmark = container_of(mark, typeof(*xmark), mark);
	kfree(xmark);
}


static void xpin_fsnotify_free_group(struct fsnotify_group *group)
{
	struct xpin_superblock_ctx *xsb = group->private;

	spin_lock(&xsb->lock);
	/* About to be freed by fsnotify_final_destroy_group() */
	rcu_assign_pointer(xsb->fsn_group, NULL);
	spin_unlock(&xsb->lock);
	xpin_sb_put(xsb);
}


static const struct fsnotify_ops xpin_fsn_ops = {
	.handle_event = xpin_fsnotify_handle_event,
	.freeing_mark = xpin_fsnotify_freeing_mark,
	.free_mark = xpin_fsnotify_free_mark,
	.free_group_priv = xpin_fsnotify_free_group,
};


static struct fsnotify_group *xpin_fsnotify_group_create(
	struct xpin_superblock_ctx *xsb)
{
	struct fsnotify_group *new_fsg;
	struct fsnotify_group *fsg;

	/* Create a new fsnotify group */
	new_fsg = fsnotify_alloc_group(&xpin_fsn_ops);
	if (IS_ERR(new_fsg))
		return new_fsg;

	spin_lock(&xsb->lock);

	/* Check for a race adding an exception to this superblock */
	fsg = rcu_dereference_protected(
		xsb->fsn_group,
		lockdep_is_held(&xsb->lock)
	);

	if (fsg) {
		/* We lost the race, so use their fsg instead of ours */
		fsnotify_get_group(fsg);
		spin_unlock(&xsb->lock);
		fsnotify_put_group(new_fsg);
	} else {
		/* We won the race, so use our fsg */
		fsg = new_fsg;

		/* Grab an extra reference (1 from xsb, 1 from return value) */
		fsnotify_get_group(fsg);
		fsg->private = xpin_sb_get(xsb);
		rcu_assign_pointer(xsb->fsn_group, fsg);
		spin_unlock(&xsb->lock);
	}

	return fsg;
}


static struct fsnotify_group *xpin_fsnotify_group_grab(
	struct xpin_superblock_ctx *xsb)
{
	struct fsnotify_group *fsg;

	/* Try to grab a reference to the superblock's XPin fsnotify group */
	rcu_read_lock();
	fsg = rcu_dereference(xsb->fsn_group);
	if (fsg)
		fsnotify_get_group(fsg);
	rcu_read_unlock();

	/*
	 * If the superblock doesn't yet have an XPin fsnotify group, create it.
	 * The returned fsg reference is an extra strong reference.
	 */
	return fsg ?: xpin_fsnotify_group_create(xsb);
}


struct xpin_inode_mark *xpin_inode_mark_create(
	struct inode *inode,
	struct fsnotify_group *fsg)
{
	int err;
	struct xpin_inode_mark *xmark;

	xmark = kzalloc(sizeof(*xmark), GFP_KERNEL);
	if (!xmark)
		return ERR_PTR(-ENOMEM);

	/* This reference is from `exception` */
	refcount_set(&xmark->refcount, 1);

	/* fsnotify mark will hold a strong reference to the inode */
	xmark->inode = inode;
	fsnotify_init_mark(&xmark->mark, fsg);
	err = fsnotify_add_inode_mark(&xmark->mark, xmark->inode, false);
	if (err) {
		kfree(xmark);
		xmark = ERR_PTR(err);
	}

	return xmark;
}


struct xpin_inode_mark *xpin_inode_mark_get(struct xpin_inode_mark *xmark)
{
	refcount_inc(&xmark->refcount);
	return xmark;
}


void xpin_inode_mark_put(struct xpin_inode_mark *xmark)
{
	/* Most inode marks will only have a single reference */
	if (!xmark || !refcount_dec_and_test(&xmark->refcount))
		return;

	/* Detach from group's list, mark as destroyed and not alive */
	fsnotify_destroy_mark(&xmark->mark, xmark->mark.group);

	/*
	 * Drop what should be the final reference, which adds the mark to a
	 * list of marks to be freed after some delay.
	 */
	fsnotify_put_mark(&xmark->mark);

	/*
	 * Actually wait for the mark to be destroyed. The mark's free callback
	 * function will actually do the kfree() of xmark.
	 */
	fsnotify_wait_marks_destroyed();
}


struct xpin_inode_mark *xpin_inode_mark_grab(struct inode *inode)
{
	struct xpin_inode_mark *xmark;
	struct xpin_superblock_ctx *xsb = xpin_superblock(inode->i_sb);
	struct fsnotify_group *fsg = xpin_fsnotify_group_grab(xsb);
	struct fsnotify_mark *mark;

	if (IS_ERR(fsg))
		return ERR_CAST(fsg);

	mark = fsnotify_find_mark(&inode->i_fsnotify_marks, fsg);
	if (mark) {
		xmark = container_of(mark, typeof(*xmark), mark);
		refcount_inc(&xmark->refcount);
	} else {
		xmark = xpin_inode_mark_create(inode, fsg);
	}

	fsnotify_put_group(fsg);
	return xmark;
}


struct xpin_superblock_ctx *xpin_sb_create(struct super_block *sb)
{
	struct xpin_superblock_ctx *xsb;

	xsb = kzalloc(sizeof(*xsb), GFP_ATOMIC);
	if (!xsb)
		return ERR_PTR(-ENOMEM);

	/* The initial reference is held by `xsb->fsn_group` */
	refcount_set(&xsb->refcount, 1);
	spin_lock_init(&xsb->lock);

	/*
	 * Flags are already 0 from kzalloc. The fsnotify group will be created
	 * on-demand later when it's needed.
	 */
	return xsb;
}


void xpin_sb_destroy(struct xpin_superblock_ctx *xsb)
{
	struct fsnotify_group *fsg;

	spin_lock(&xsb->lock);
	/* Steal xsb's reference to fsg */
	fsg = rcu_dereference_protected(
		xsb->fsn_group, lockdep_is_held(&xsb->lock)
	);
	rcu_assign_pointer(xsb->fsn_group, NULL);
	spin_unlock(&xsb->lock);

	if (fsg)
		/* Note: this internally does an `fsnotify_put_group(fsg)` */
		fsnotify_destroy_group(fsg);

}


struct xpin_superblock_ctx *xpin_sb_get(struct xpin_superblock_ctx *xsb)
{
	refcount_inc(&xsb->refcount);
	return xsb;
}


void xpin_sb_put(struct xpin_superblock_ctx *xsb)
{
	struct fsnotify_group *fsg;

	if (!xsb || !refcount_dec_and_test(&xsb->refcount))
		return;

	/* Guaranteed to already have no RCU readers */
	fsg = rcu_dereference_protected(xsb->fsn_group, true);
	if (fsg) {
		/* Create a temp ref for the fsnotify free group callback */
		refcount_set(&xsb->refcount, 1);
		fsnotify_put_group(fsg);

		/* Ensure there's only the temporary reference left */
		WARN_ON(!refcount_dec_and_test(&xsb->refcount));
	}

	kfree(xsb);
}


static int xpin_match_parent_inode_cb(
	struct inode *inode,
	unsigned long ino,
	void *data)
{
	if (inode->i_ino != ino)
		/* Keep searching for inode */
		return 0;

	if (!igrab(inode))
		/* Stop searching, did not find inode */
		return -1;

	/* Stop searching, found inode */
	return 1;
}


char *xpin_sb_root_path(struct super_block *sb, gfp_t gfp)
{
	int err = 0;
	char *sb_root_path = kmalloc(PATH_MAX + 1, gfp);
	char *tmp;

	if (!sb_root_path)
		goto out;

	tmp = dentry_path(sb->s_root, sb_root_path, PATH_MAX + 1);
	if (IS_ERR_OR_NULL(tmp)) {
		err = tmp ? PTR_ERR(tmp) : -ENOMEM;
		goto out;
	}

	tmp = kstrdup(tmp, gfp);
	kfree(sb_root_path);
	sb_root_path = tmp;

out:
	if (err) {
		kfree(sb_root_path);
		return ERR_PTR(err);
	}

	return sb_root_path ?: ERR_PTR(-ENOMEM);
}


static struct fsnotify_group *xpin_fsnotify_get_group(struct super_block *sb)
{
	struct fsnotify_group *fsg;
	struct xpin_superblock_ctx *xsb = xpin_superblock(sb);

	rcu_read_lock();
	fsg = rcu_dereference(xsb->fsn_group);
	if (fsg)
		fsnotify_get_group(fsg);
	rcu_read_unlock();

	return fsg;
}


static int xpin_append_sb_description(
	struct super_block *sb,
	struct list_head *list,
	gfp_t gfp)
{
	struct xpin_exception_info *fake;

	fake = kzalloc(XPIN_EXCEPTION_INFO_SIZE(100), gfp);
	if (!fake)
		return -ENOMEM;

	snprintf(
		fake->path, 100, "\n# SB(s_dev=%u:%u, s_id=\"%s\")",
		MAJOR(sb->s_dev), MINOR(sb->s_dev), sb->s_id
	);
	list_add_tail(&fake->node, list);
	return 0;
}


static int xpin_preallocate_exceptions_list(
	struct xpin_inode_mark *xmark,
	struct xpin_exceptions_list *preallocated,
	gfp_t gfp)
{
	struct xpin_exception_info *new_info;

	/*
	 * Use inode's link count as an overestimation of the number of
	 * exceptions on the inode.
	 */
	while (preallocated->count < xmark->inode->i_nlink) {
		new_info = kzalloc(XPIN_EXCEPTION_INFO_SIZE(PATH_MAX), gfp);
		if (!new_info)
			return -ENOMEM;

		list_add_tail(&new_info->node, &preallocated->head);
		++preallocated->count;
	}

	return 0;
}


static struct dentry *xpin_exception_find_dentry(
	struct super_block *sb,
	struct xpin_exception *exception)
{
	struct inode *parent_inode;
	struct dentry *parent_dentry;
	struct dentry *dentry;
	struct qstr basename = {};

	/*
	 * We hold the RCU read lock and thus can't sleep. Therefore,
	 * it is illegal to call ilookup() from this context. Instead,
	 * use find_inode_nowait() to hopefully find the inode which
	 * has the parent directory's inode number.
	 */
	parent_inode = find_inode_nowait(
		sb,
		exception->dir_inode_number,
		xpin_match_parent_inode_cb,
		NULL
	);
	if (!parent_inode) {
		pr_warn(
			"Failed to find parent inode: ino=%lu basename=\"%s\"\n",
			exception->dir_inode_number,
			exception->basename
		);
		return NULL;
	}

	/* Get dentry matching the parent's inode */
	parent_dentry = d_find_any_alias(parent_inode);
	iput(parent_inode);
	if (!parent_dentry) {
		pr_warn(
			"Failed to find dentry alias for parent: ino=%lu basename=\"%s\"\n",
			exception->dir_inode_number,
			exception->basename
		);
		return NULL;
	}

	/*
	 * Now lookup the dentry for the exception by looking for a
	 * dentry with `basename` in the parent directory.
	 */
	basename.name = exception->basename;
	basename.len = strlen(basename.name);
	dentry = d_hash_and_lookup(parent_dentry, &basename);
	dput(parent_dentry);

	if (!dentry) {
		pr_warn(
			"Failed to lookup dentry: ino=%lu basename=\"%s\"\n",
			exception->dir_inode_number, exception->basename
		);
		return NULL;
	}

	return dentry;
}


static int xpin_dentry_path(struct dentry *dentry, char *buf, size_t size)
{
	char *path = dentry_path(dentry, buf, size);

	if (IS_ERR(path))
		return PTR_ERR(path);

	/*
	 * dentry_path builds the string path starting from the end of
	 * the buffer, so it usually returns a pointer somewhere in the
	 * middle of the buffer. Move the path to the front of the
	 * buffer where it belongs. There's no need to clear the string
	 * data after the end of the string as long as we move the NUL.
	 */
	memmove(buf, path, strlen(path) + 1);
	return 0;
}


static int xpin_fill_exception_info(
	struct super_block *sb,
	struct xpin_exception *exception,
	struct xpin_exception_info *info)
{
	int err;
	struct inode *inode;
	struct dentry *dentry = NULL;

	info->flags = atomic_read(&exception->flags);
	inode = exception->inode_mark->inode;
	if (inode->i_nlink > 1)
		dentry = xpin_exception_find_dentry(sb, exception);
	else
		dentry = d_find_any_alias(inode);

	if (dentry) {
		err = xpin_dentry_path(dentry, info->path, PATH_MAX + 1);
		dput(dentry);
	} else {
		snprintf(
			info->path, PATH_MAX + 1,
			"[ino:%lu]", inode->i_ino
		);
	}

	return err;
}


static int xpin_shrink_exception_info_list(
	struct xpin_exception_info *info,
	struct list_head *list,
	gfp_t gfp)
{
	list_for_each_entry_from(info, list, node) {
		size_t pathlen;
		struct xpin_exception_info *smaller;

		pathlen = strlen(info->path);
		smaller = kzalloc(XPIN_EXCEPTION_INFO_SIZE(pathlen), gfp);
		if (!smaller)
			return -ENOMEM;

		smaller->flags = info->flags;
		memcpy(smaller->path, info->path, pathlen);

		/* Swap with minimally-sized info object */
		list_replace(&info->node, &smaller->node);
		kfree(info);
		info = smaller;
	}

	return 0;
}


static int xpin_append_inode_exceptions_info(
	struct fsnotify_mark *mark,
	struct super_block *sb,
	struct xpin_exceptions_list *preallocated,
	struct xpin_exceptions_list *elist,
	gfp_t gfp)
{
	int err;
	struct xpin_inode_mark *xmark;
	struct xpin_inode_ctx *xinode;
	struct xpin_exception *exception;
	struct xpin_exception_info *info, *next, *first = NULL;

	xmark = container_of(mark, typeof(*xmark), mark);
	xinode = xpin_inode(xmark->inode);

	/* Pre-allocate list nodes, as we can't allocate them under RCU lock */
	err = xpin_preallocate_exceptions_list(xmark, preallocated, gfp);
	if (err)
		return err;

	/* Save pointer to first exception info object for this iteration */
	first = list_first_entry(&preallocated->head, typeof(*first), node);
	next = first;

	/* Copy info from inode's exceptions to a new list */
	rcu_read_lock();
	hlist_for_each_entry_rcu(exception, &xinode->exceptions, node) {
		/*
		 * Shouldn't be possible, but ensure there aren't more
		 * exceptions than the inode's link count.
		 */
		if (PARANOID(!next)) {
			err = -EAGAIN;
			break;
		}

		info = next;
		if (list_is_last(&info->node, &preallocated->head))
			next = NULL;
		else
			next = list_next_entry(info, node);

		err = xpin_fill_exception_info(sb, exception, info);
		if (err)
			break;

		/* Move exception info node from preallocated list to results */
		list_move_tail(&info->node, &elist->head);
		--preallocated->count;
		++elist->count;
	}
	rcu_read_unlock();

	if (err)
		return err;

	/*
	 * Now that we've dropped the RCU lock, we can shrink the exception info
	 * objects down (they're currently over a page each).
	 */
	return xpin_shrink_exception_info_list(first, &elist->head, gfp);
}


static int xpin_fsg_collect_exceptions(
	struct fsnotify_group *fsg,
	struct super_block *sb,
	struct xpin_exceptions_list *preallocated,
	struct xpin_exceptions_list *elist,
	gfp_t gfp)
{
	int err = 0;
	struct fsnotify_mark *mark;

	mutex_lock(&fsg->mark_mutex);
	list_for_each_entry(mark, &fsg->marks_list, g_list) {
		err = xpin_append_inode_exceptions_info(
			mark, sb, preallocated, elist, gfp
		);
		if (err)
			break;
	}
	mutex_unlock(&fsg->mark_mutex);

	return err;
}


int xpin_sb_collect_exceptions(
	struct super_block *sb,
	struct xpin_exceptions_list *elist,
	gfp_t gfp)
{
	int err = 0;
	struct fsnotify_group *fsg = NULL;
	char *sb_root_path;
	struct xpin_exceptions_list preallocated;

	INIT_LIST_HEAD(&elist->head);
	INIT_LIST_HEAD(&preallocated.head);
	elist->count = 0;
	preallocated.count = 0;

	/*
	 * Get string path for root of superblock. If this allocation fails,
	 * later allocations will probably fail too.
	 */
	sb_root_path = xpin_sb_root_path(sb, gfp);
	if (IS_ERR_OR_NULL(sb_root_path)) {
		err = sb_root_path ? PTR_ERR(sb_root_path) : -ENOMEM;
		goto out;
	}

	fsg = xpin_fsnotify_get_group(sb);
	if (!fsg)
		goto out;

	err = xpin_append_sb_description(sb, &elist->head, gfp);
	if (err)
		goto out;

	err = xpin_fsg_collect_exceptions(fsg, sb, &preallocated, elist, gfp);

out:
	if (fsg)
		fsnotify_put_group(fsg);
	kfree(sb_root_path);
	xpin_clear_exceptions_list(&preallocated);
	if (err)
		xpin_clear_exceptions_list(elist);
	return err;
}
