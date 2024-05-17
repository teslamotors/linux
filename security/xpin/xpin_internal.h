/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * xpin_internal.h: Defines common macros, types, and function declarations for
 * use internally by XPin.
 */
#ifndef _XPIN_INTERNAL_H
#define _XPIN_INTERNAL_H

#include <linux/compiler_types.h>
#include <linux/cred.h>
#include <linux/fs.h>
#include <linux/fsnotify_backend.h>
#include <linux/init.h>
#include <linux/rcupdate.h>
#include <linux/refcount.h>
#include <linux/sched.h>
#include <linux/shmem_fs.h>
#include <linux/types.h>

#include <linux/xpin.h>


#ifndef XPIN_PARANOID_CHECKS
#define XPIN_PARANOID_CHECKS 1
#endif

#if XPIN_PARANOID_CHECKS
#define PARANOID(cond) WARN_ONCE(cond, pr_fmt("PARANOID(%s)\n"), #cond)
#else
#define PARANOID(cond) 0
#endif


/* Whether violations should be blocked */
extern bool xpin_enforce;

/* Which audit messages are logged */
extern unsigned int xpin_logging;
#define XPIN_LOGGING_NONE     0
#define XPIN_LOGGING_NORMAL   1
#define XPIN_LOGGING_VERBOSE  2

/* Should be set to true at runtime on prod-fused devices */
extern bool xpin_lockdown;

enum xpin_file_kind {
	XPIN_FILE_ANON,
	XPIN_FILE_MEMFD,
	XPIN_FILE_SHMEM,
	XPIN_FILE_UNSIGNED,
	XPIN_FILE_VERITY,

	XPIN_FILE_COUNT
};

extern const char *xpin_event_names[XPIN_EVENT_COUNT];
extern const char *xpin_file_kinds[XPIN_FILE_COUNT];


/* Strings checked as exception line prefixes */
#define XPIN_TOKEN_LAZY        "lazy "
#define XPIN_TOKEN_DENY        "deny "
#define XPIN_TOKEN_JIT         "jit "
#define XPIN_TOKEN_UNINHERIT   "uninherit "
#define XPIN_TOKEN_INHERIT     "inherit "
#define XPIN_TOKEN_NON_ELF     "nonelf "
#define XPIN_TOKEN_QUIET       "quiet "
#define XPIN_TOKEN_SECURE      "+"


struct xpin_inode_mark {
	/*
	 * Trick to safely hold references to inode objects to keep them
	 * in-core, allowing for more performant exception lookup checks. This
	 * was inspired by the ChromiumOS LSM, specifically inode_mark.c:
	 *
	 * https://chromium.googlesource.com/chromiumos/third_party/kernel/+/refs/heads/chromeos-5.4/security/chromiumos/inode_mark.c
	 */
	struct fsnotify_mark mark;

	/*
	 * Inode for which this mark applies. The fsnotify mark already holds a
	 * reference to the inode, so no need to hold one ourselves.
	 */
	struct inode *inode;

	/* Each `struct xpin_exception` will hold a reference to this struct. */
	refcount_t refcount;
};

/*
 * This struct holds a strong reference to the inode via the fsnotify mark.
 * References to this struct are held by `(struct xpin_inode_ctx).exceptions`.
 */
struct xpin_exception {
	/* Hold a strong reference to the inode mark wrapper struct */
	struct xpin_inode_mark *inode_mark;

	/* RCU! Head of this list is `(struct xpin_inode_ctx).exceptions` */
	struct hlist_node node;

	/* Bit flags from `XPIN_EXCEPTION_MASK` */
	atomic_t flags;

	/* REMAINING FIELDS ONLY PRESENT IF INODE HAS MULTIPLE HARD LINKS */

	/* The inode number of this exception file's containing directory */
	ino_t dir_inode_number;

	/* Basename of this exception's specific dentry */
	char basename[];
};

#define XPIN_EXCEPTION_BASE_SIZE ( \
	offsetof(struct xpin_exception, flags) \
	+ sizeof(((struct xpin_exception *)0)->flags) \
)

#define XPIN_EXCEPTION_WITH_PATH_SIZE(pathlen) \
offsetof(struct xpin_exception, basename[(pathlen) + 1])


struct xpin_exception_info {
	/* List node */
	struct list_head node;

	/* Flags for the exception from `XPIN_EXCEPTION_MASK` */
	int flags;

	/* True if this is a lazy exception */
	bool lazy;

	/* String path to the exception */
	char path[];
};

#define XPIN_EXCEPTION_INFO_SIZE(pathlen) \
offsetof(struct xpin_exception_info, path[(pathlen) + 1])


struct xpin_exceptions_list {
	/* Head of list of type `struct xpin_exception_info` */
	struct list_head head;

	/* Number of entries in `list` */
	unsigned int count;
};


static inline void xpin_clear_exceptions_list(
	struct xpin_exceptions_list *elist)
{
	struct xpin_exception_info *cur, *next;

	list_for_each_entry_safe(cur, next, &elist->head, node) {
		list_del(&cur->node);
		kfree(cur);
	}
	elist->count = 0;
}


struct xpin_cred_ctx {
	/* Bit flags from `XPIN_EXCEPTION_MASK` */
	int flags;
};

struct xpin_inode_ctx {
	/*
	 * List of type `struct xpin_exception`. This is using hlist instead of
	 * list to reduce the memory overhead of inodes. Traversals of the list
	 * should use the `hlist_*_rcu()` helper functions. For inodes whose
	 * link count is exactly 1, this will have a single element.
	 */
	struct hlist_head exceptions;

	/* Lock for `exceptions` */
	spinlock_t lock;

	/* Combination of bit flags from `XPIN_INODE_MASK` */
	atomic_t flags;
};

struct xpin_superblock_ctx {
	/*
	 * Using fsnotify as a vehicle to hold references to inodes. To prevent
	 * any of the normal fsnotify behavior from happening, we create our own
	 * blank fsnotify group to add the marks to.
	 */
	struct fsnotify_group __rcu *fsn_group;

	/* Locks write access to `fsn_group` and `flags` */
	spinlock_t lock;

	/* Bit flags from `XPIN_SB_MASK`. Effectively protected by RCU */
	int flags;

	/* Reference count, mainly held by fsnotify marks */
	refcount_t refcount;
};

struct xpin_line_buffer {
	struct file *file;
	loff_t file_off;
	char *buffer;
	unsigned int buffer_size;
	unsigned int write_pos;
	unsigned int read_pos;
};


/* audit.c */

/* Classify a file by its underlying storage kind */
enum xpin_file_kind xpin_get_file_kind(struct file *file);

/* Generate an audit log message for some operation XPin mediates */
void xpin_report(
	/* Who... (current) */
	/* ...did what... */
	enum xpin_event event,
	bool mutable,
	/* ...to whom... */
	struct file *file,
	/* ...and what was the result? */
	int error,
	bool allow,
	/* And why? */
	bool enforce,
	bool exception);


/* exceptions.c */

/* Add an XPin exception for the given file */
int xpin_add_exception(const char *filename, int flags, bool lazy);

/* Free an exception now */
void xpin_exception_free(struct xpin_exception *exception);

/* Remove any exception from the given file */
void xpin_file_remove_exception(struct file *file);

/* Clear ALL exceptions on all superblocks */
void xpin_clear_all_exceptions(void);

/* Gather a list of `struct xpin_exception_info` for all XPin exceptions */
int xpin_collect_all_exceptions(struct xpin_exceptions_list *elist, gfp_t gfp);


/* fs_mark.c */

/* Create a new inode mark and get a reference to it */
struct xpin_inode_mark *xpin_inode_mark_create(
	struct inode *inode,
	struct fsnotify_group *fsg);

/* Get a reference to the given inode mark */
struct xpin_inode_mark *xpin_inode_mark_get(struct xpin_inode_mark *xmark);

/* Put back a reference to the given inode mark */
void xpin_inode_mark_put(struct xpin_inode_mark *xmark);

/* Get a reference to an inode's XPin mark */
struct xpin_inode_mark *xpin_inode_mark_grab(struct inode *inode);

/* Create XPin's extra context for a superblock */
struct xpin_superblock_ctx *xpin_sb_create(struct super_block *sb);

/* Destroy contents of XPin's superblock context */
void xpin_sb_destroy(struct xpin_superblock_ctx *xsb);

/* Get a reference to the given superblock context */
struct xpin_superblock_ctx *xpin_sb_get(struct xpin_superblock_ctx *xpin_sb);

/* Put back a reference to the given superblock context */
void xpin_sb_put(struct xpin_superblock_ctx *xpin_sb);

/* Collect all active XPin exceptions on a superblock */
int xpin_sb_collect_exceptions(
	struct super_block *sb,
	struct xpin_exceptions_list *elist,
	gfp_t gfp);


/* init.c */

/* Main entrypoint for the XPin LSM */
int __init xpin_init(void);


/* line_buffer.c */

/* Read a line of text from a file using a line buffer */
char *xpin_read_line(struct xpin_line_buffer *lb);


/* lsm.c */

/* Install XPin's LSM hooks */
void __init xpin_add_hooks(void);

/* Get XPin's data attached to a cred struct */
struct xpin_cred_ctx *xpin_cred(const struct cred *cred);

/* Get XPin's data attached to an inode */
struct xpin_inode_ctx *xpin_inode(struct inode *inode);

/* Get XPin's data attached to a superblock */
struct xpin_superblock_ctx *xpin_superblock(struct super_block *sb);


/* securityfs.c */

/* Populate securityfs with XPin's directory structure */
int __init xpin_init_securityfs(void);


#endif /* _XPIN_INTERNAL_H */
