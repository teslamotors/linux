// SPDX-License-Identifier: GPL-2.0-only
/*
 * exceptions.c: Everything to do with XPin exceptions.
 */
#define pr_fmt(fmt) KBUILD_MODNAME "-exceptions: " fmt

#include "xpin_internal.h"
#include <linux/atomic.h>
#include <linux/dcache.h>
#include <linux/err.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/path.h>
#include <linux/slab.h>
#include <linux/string.h>


struct xpin_lazy_exception {
	/* String path, needs kfree() */
	char *path;

	/* Entry in `xpin_lazy_exceptions_list` */
	struct list_head node;

	/* Bit flags from `XPIN_EXCEPTION_*` */
	int flags;
};


struct xpin_handled_superblock {
	/* Superblock whose lazy exceptions have been applied */
	struct super_block *sb;
	struct list_head node;
};


/* Protected by mutex, not RCU */
static LIST_HEAD(xpin_lazy_exceptions_list);
static DEFINE_MUTEX(xpin_lazy_exceptions_lock);
static unsigned long xpin_lazy_exceptions_count;


static bool xpin_exception_matches(
	const struct xpin_exception *exception,
	const struct path *path)
{
	const struct dentry *dentry = path->dentry;

	/* Fast path for inodes w/o hard links, just compare inode pointers */
	if (likely(exception->inode_mark->inode->i_nlink == 1))
		return dentry->d_inode == exception->inode_mark->inode;

	/*
	 * Only safe to access exception's dir_inode_number and basename fields
	 * for exceptions whose inode has link count > 1.
	 */

	/* Check if this dentry is in the same directory as the exception */
	if (dentry->d_parent->d_inode->i_ino != exception->dir_inode_number)
		return false;

	/*
	 * If in same directory and has same basename, must be target dentry.
	 *
	 * Don't need to worry about race conditions from renames because this
	 * is on an immutable dm-verity mount.
	 */
	return strcmp(exception->basename, dentry->d_name.name) == 0;
}


static struct xpin_exception *xpin_exception_create(
	const struct path *path,
	int flags)
{
	int err;
	struct xpin_exception *exception;
	const struct dentry *dentry = path->dentry;
	unsigned int nlink = dentry->d_inode->i_nlink;
	size_t size;

	if (nlink == 1)
		/*
		 * Optimization for inodes without hard links: no need to
		 * allocate space for the basename and parent's inode number.
		 */
		size = XPIN_EXCEPTION_BASE_SIZE;
	else
		/*
		 * Since exceptions can only be granted on dm-verity protected
		 * files, we don't have to worry about race conditions from
		 * renames causing the dentry's name length to change.
		 */
		size = XPIN_EXCEPTION_WITH_PATH_SIZE(dentry->d_name.len);

	/* Allocate variable-sized exception object */
	exception = kzalloc(size, GFP_KERNEL);
	if (!exception)
		return ERR_PTR(-ENOMEM);

	atomic_set(&exception->flags, flags & XPIN_FILE_EXCEPTION_MASK);

	if (nlink != 1) {
		/* Grab inode number of containing directory */
		exception->dir_inode_number = dentry->d_parent->d_inode->i_ino;

		/* Grab basename of this dentry */
		memcpy(
			exception->basename,
			dentry->d_name.name,
			dentry->d_name.len
		);
	}

	/* Get a reference to the inode mark. Will create it if necessary. */
	exception->inode_mark = xpin_inode_mark_grab(dentry->d_inode);
	if (IS_ERR(exception->inode_mark)) {
		err = PTR_ERR(exception->inode_mark);
		goto out;
	}

out:
	if (err && !IS_ERR(exception)) {
		kfree(exception);
		exception = ERR_PTR(err);
	}

	return exception;
}


static void xpin_exception_remove(struct xpin_exception *exception)
{
	struct xpin_inode_ctx *xinode;

	/* Step 1: Lock xinode */
	xinode = xpin_inode(exception->inode_mark->inode);
	spin_lock(&xinode->lock);

	/* Step 2: Remove from xinode's list */
	hlist_del_rcu(&exception->node);

	/* Step 3: Unlock xinode */
	spin_unlock(&xinode->lock);

	/* Step 4: Wait for RCU grace period */
	synchronize_rcu();

	/* Step 5: Drop reference to fsnotify mark wrapper */
	xpin_inode_mark_put(exception->inode_mark);

	/* Step 6: Free exception */
	kfree(exception);
}


static int xpin_validate_exception_file(struct path *path, const char *filename)
{
	struct inode *inode;

	if (!xpin_is_verity_sb(path->mnt->mnt_sb)) {
		if (xpin_lockdown) {
			pr_warn(
				"Refusing to add exception for non-verity program. path=\"%s\"\n",
				filename
			);
			return -EPERM;
		}

		pr_info(
			"Adding exception to non-verity program due to lockdown mode being disabled: \"%s\"\n",
			filename
		);
	}

	/* Make sure the path refers to a regular file */
	inode = path->dentry->d_inode;
	if (!(S_ISREG(inode->i_mode))) {
		if (S_ISDIR(inode->i_mode))
			return -EISDIR;
		return -EINVAL;
	}

	/* Is path executable by any user? */
	if (!(inode->i_mode & 0111))
		return -EACCES;

	return 0;
}


static int xpin_add_exception_now(
	const char *filename,
	int flags)
{
	int err;
	struct path path = {};
	struct inode *inode;
	struct xpin_exception *exception = NULL;
	struct xpin_inode_ctx *xinode;

	err = kern_path(filename, O_NOFOLLOW, &path);
	if (err)
		goto out;

	err = xpin_validate_exception_file(&path, filename);
	if (err)
		goto out;

	/* Create the exception, not yet added to lists */
	exception = xpin_exception_create(&path, flags);
	if (!exception) {
		err = -ENOMEM;
		goto out;
	}

	/* Check if the exception has already been added */
	inode = path.dentry->d_inode;
	xinode = xpin_inode(inode);
	spin_lock(&xinode->lock);
	if (!hlist_empty(&xinode->exceptions)) {
		struct xpin_exception *cur;

		/* Safe to use non-RCU traversal when lock is held */
		hlist_for_each_entry(cur, &xinode->exceptions, node) {
			if (xpin_exception_matches(cur, &path)) {
				spin_unlock(&xinode->lock);
				pr_info(
					"Exception already exists for \"%s\"\n",
					filename
				);
				err = -EALREADY;
				xpin_exception_free(exception);
				goto out;
			}
		}
	}

	/* Add exception to the list */
	hlist_add_head_rcu(&exception->node, &xinode->exceptions);
	spin_unlock(&xinode->lock);
	err = 0;

out:
	if (path.dentry)
		path_put(&path);

	/* Log at the end, after the lock has been dropped (if taken) */
	if (!err)
		pr_debug("Added exception: path=\"%s\"\n", filename);
	return err;
}


static int xpin_add_lazy_exception(const char *filename, int flags)
{
	int ret;
	struct xpin_lazy_exception *xlazy;

	/*
	 * Lazy case: Add filename to a list.
	 *
	 * Lazy paths are re-checked on first exec from a new superblock.
	 */
	xlazy = kzalloc(sizeof(*xlazy), GFP_KERNEL);
	if (!xlazy) {
		ret = -ENOMEM;
		goto err;
	}

	xlazy->path = kstrdup(filename, GFP_KERNEL);
	if (!xlazy->path) {
		ret = -ENOMEM;
		goto err;
	}

	xlazy->flags = flags;

	mutex_lock(&xpin_lazy_exceptions_lock);
	list_add_tail(&xlazy->node, &xpin_lazy_exceptions_list);
	++xpin_lazy_exceptions_count;
	mutex_unlock(&xpin_lazy_exceptions_lock);

	pr_debug("Added lazy exception: path=\"%s\"\n", filename);
	return 0;

err:
	kfree(xlazy);
	return ret;
}


int xpin_add_exception(const char *filename, int flags, bool lazy)
{
	int err;

	if (lazy) {
		/*
		 * Even if the file already exists, a lazy exception should
		 * still be created. That way, if the file gets unmounted and is
		 * then later re-mounted, it will still have the lazy exception
		 * applied.
		 */
		err = xpin_add_lazy_exception(filename, flags);
		if (err)
			return err;
	}

	/* Always attempt to directly add the exception, even if it's lazy */
	err = xpin_add_exception_now(filename, flags);
	if (err == -ENOENT && lazy)
		err = 0;
	return err;
}


void xpin_exception_free(struct xpin_exception *exception)
{
	WARN_ON(!hlist_unhashed(&exception->node));
	xpin_inode_mark_put(exception->inode_mark);
	kfree(exception);
}


static int xpin_apply_lazy_exceptions(struct xpin_superblock_ctx *xsb)
{
	int ret = 0;
	int err;
	struct xpin_lazy_exception *cur;

	/*
	 * Check whether each lazy entry applies now. There's no need to
	 * restrict this to only the current superblock.
	 */
	list_for_each_entry(cur, &xpin_lazy_exceptions_list, node) {
		err = xpin_add_exception_now(cur->path, cur->flags);
		if (likely(err == -ENOENT || err == -EALREADY))
			/*
			 * Ignore lazy exceptions for paths that still don't
			 * exist or lazy exceptions that have already been
			 * applied (to another superblock).
			 */
			continue;

		if (unlikely(err)) {
			pr_warn(
				"Failed to apply lazy exception: path=\"%s\" err=%d\n",
				cur->path, err
			);

			/* Return error overall but keep going */
			ret = err;
		} else {
			pr_debug(
				"Lazy exception realized: path=\"%s\"\n",
				cur->path
			);
		}
	}

	/* Mark this sb as having the lazy exceptions applied */
	xsb->flags |= XPIN_SB_LAZY_EXCEPTIONS_CHECKED;
	return ret;
}


/*
 * If this file's superblock hasn't been checked yet, then run through the
 * lazy exceptions list. Note that if multiple dm-verity superblocks with lazy
 * exceptions have been mounted but not yet checked, lazy exceptions will be
 * applied to each of them.
 */
static int xpin_check_superblock_exceptions(struct super_block *sb)
{
	int ret;
	struct xpin_superblock_ctx *xsb = xpin_superblock(sb);

	/* Fast check to see if we need to take the exceptions lock */
	if (likely(READ_ONCE(xsb->flags) & XPIN_SB_LAZY_EXCEPTIONS_CHECKED))
		return 0;

	/* Slow path: process the lazy list or wait for another task to do so */
	mutex_lock(&xpin_lazy_exceptions_lock);

	/*
	 * Check if there was a race condition where another task was checking
	 * exceptions for this superblock. If so, there's nothing left to do.
	 */
	if (unlikely(xsb->flags & XPIN_SB_LAZY_EXCEPTIONS_CHECKED)) {
		mutex_unlock(&xpin_lazy_exceptions_lock);
		return 0;
	}

	/* We are now the only task to apply the lazy exceptions list */
	ret = xpin_apply_lazy_exceptions(xsb);
	mutex_unlock(&xpin_lazy_exceptions_lock);
	return ret;
}


static struct xpin_exception *xpin_file_get_exception(struct file *file)
{
	struct xpin_inode_ctx *xinode;
	struct xpin_exception *cur;

	if (!file)
		return NULL;

	/*
	 * Ensure lazy exceptions have already been applied for this superblock.
	 * Note that this will take a mutex and is therefore illegal to call
	 * while holding a spinlock or RCU read lock.
	 *
	 * If this fails for any reason, ignore the error and proceed with the
	 * normal exception check.
	 */
	(void)xpin_check_superblock_exceptions(file->f_path.mnt->mnt_sb);

	/*
	 * Check all exceptions for this inode. This block correctly handles
	 * both cases, where the inode has multiple hard links or not.
	 */
	xinode = xpin_inode(file->f_inode);
	rcu_read_lock();
	hlist_for_each_entry_rcu(cur, &xinode->exceptions, node) {
		if (xpin_exception_matches(cur, &file->f_path)) {
			rcu_read_unlock();
			return cur;
		}
	}
	rcu_read_unlock();
	return NULL;
}


void xpin_file_remove_exception(struct file *file)
{
	struct xpin_exception *exception = xpin_file_get_exception(file);

	if (!exception)
		return;
	xpin_exception_remove(exception);
}


static void xpin_clear_exceptions_super_cb(
	struct super_block *sb,
	__always_unused void *cookie)
{
	xpin_sb_destroy(xpin_superblock(sb));
}


void xpin_clear_all_exceptions(void)
{
	struct xpin_lazy_exception *cur, *next;

	/*
	 * Clear lazy exceptions first, in case any get applied while clearning
	 * normal exceptions.
	 */
	mutex_lock(&xpin_lazy_exceptions_lock);
	list_for_each_entry_safe(cur, next, &xpin_lazy_exceptions_list, node) {
		list_del(&cur->node);
		kfree(cur->path);
	}
	mutex_unlock(&xpin_lazy_exceptions_lock);

	iterate_supers(xpin_clear_exceptions_super_cb, NULL);
}


struct xpin_collect_cookie {
	struct xpin_exceptions_list *elist;
	gfp_t gfp;
	int err;
};


static void xpin_collect_active_cb(struct super_block *sb, void *ptr)
{
	struct xpin_collect_cookie *cookie = ptr;
	struct xpin_exceptions_list local_elist;

	if (cookie->err)
		return;

	cookie->err = xpin_sb_collect_exceptions(sb, &local_elist, cookie->gfp);

	if (!cookie->err) {
		list_splice_tail(&local_elist.head, &cookie->elist->head);
		cookie->elist->count += local_elist.count;
	}
}


static int xpin_collect_active(
	struct xpin_exceptions_list *elist,
	gfp_t gfp)
{
	struct xpin_collect_cookie cookie = {};

	INIT_LIST_HEAD(&elist->head);
	elist->count = 0;
	cookie.elist = elist;
	cookie.gfp = gfp;

	iterate_supers(xpin_collect_active_cb, &cookie);
	if (cookie.err)
		xpin_clear_exceptions_list(elist);
	return cookie.err;
}


static int xpin_collect_lazy(struct xpin_exceptions_list *elist, gfp_t gfp)
{
	int err = 0;
	struct xpin_lazy_exception *lazy;

	INIT_LIST_HEAD(&elist->head);
	elist->count = 0;

	mutex_lock(&xpin_lazy_exceptions_lock);
	list_for_each_entry(lazy, &xpin_lazy_exceptions_list, node) {
		size_t pathlen = strlen(lazy->path);
		struct xpin_exception_info *info = kzalloc(
			XPIN_EXCEPTION_INFO_SIZE(pathlen), gfp
		);

		if (!info) {
			err = -ENOMEM;
			break;
		}

		info->flags = lazy->flags;
		info->lazy = true;
		memcpy(info->path, lazy->path, pathlen);

		list_add_tail(&info->node, &elist->head);
		++elist->count;
	}
	mutex_unlock(&xpin_lazy_exceptions_lock);

	if (err)
		xpin_clear_exceptions_list(elist);
	return err;
}


int xpin_collect_all_exceptions(struct xpin_exceptions_list *elist, gfp_t gfp)
{
	int err;
	struct xpin_exceptions_list local_elist;
	struct xpin_exception_info *fake;

	INIT_LIST_HEAD(&elist->head);
	elist->count = 0;

	fake = kzalloc(
		XPIN_EXCEPTION_INFO_SIZE(100), gfp
	);
	if (!fake) {
		err = -ENOMEM;
		goto out;
	}

	snprintf(fake->path, 100, "# Active exceptions:");
	list_add_tail(&fake->node, &elist->head);

	err = xpin_collect_active(&local_elist, gfp);
	if (err)
		goto out;

	list_splice_tail(&local_elist.head, &elist->head);
	elist->count += local_elist.count;

	fake = kzalloc(
		XPIN_EXCEPTION_INFO_SIZE(100), gfp
	);
	if (!fake) {
		err = -ENOMEM;
		goto out;
	}

	snprintf(fake->path, 100, "\n# Lazy exceptions:");
	list_add_tail(&fake->node, &elist->head);

	err = xpin_collect_lazy(&local_elist, gfp);
	if (err)
		goto out;

	list_splice_tail(&local_elist.head, &elist->head);
	elist->count += local_elist.count;

out:
	if (err)
		xpin_clear_exceptions_list(elist);
	return err;
}


int xpin_file_get_exception_flags(struct file *file)
{
	struct xpin_exception *exception = xpin_file_get_exception(file);

	if (likely(!exception))
		return 0;

	return atomic_read(&exception->flags);
}
