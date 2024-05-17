// SPDX-License-Identifier: GPL-2.0-only
/*
 * lsm.c: Defines LSM hooks and any extra data stored in the LSM "security"
 * fields of common kernel objects.
 */

#define pr_fmt(fmt) KBUILD_MODNAME "-lsm: " fmt

#include "xpin_internal.h"
#include <linux/binfmts.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/lsm_hooks.h>
#include <linux/mman.h>
#include <linux/sched.h>


/* Could add (alignof - 1), but then we'd be the bad one unaligning the blobs */
#define XPIN_BLOB_SIZE(obj) (sizeof(obj) + __alignof__(obj))

struct lsm_blob_sizes xpin_blob_sizes __lsm_ro_after_init = {
	.lbs_cred = XPIN_BLOB_SIZE(struct xpin_cred_ctx),
	.lbs_inode = XPIN_BLOB_SIZE(struct xpin_inode_ctx),
};


struct xpin_cred_ctx *xpin_cred(const struct cred *cred)
{
	return PTR_ALIGN(
		cred->security + xpin_blob_sizes.lbs_cred,
		__alignof__(struct xpin_cred_ctx)
	);
}


struct xpin_inode_ctx *xpin_inode(struct inode *inode)
{
	return PTR_ALIGN(
		inode->i_security + xpin_blob_sizes.lbs_inode,
		__alignof__(struct xpin_inode_ctx)
	);
}


struct xpin_superblock_ctx *xpin_superblock(struct super_block *sb)
{
	return sb->s_security;
}


static int xpin_sb_alloc_security(struct super_block *sb)
{
	struct xpin_superblock_ctx *xsb = xpin_sb_create(sb);

	/*
	 * In kernel versions before 5.13, the `s_security` field is a normal
	 * pointer that only one LSM can have control of. Luckily, the only LSMs
	 * that use this field (besides XPin) are SELinux and Smack. Therefore,
	 * systems which have AppArmor configured as their exclusive LSM of
	 * choice are safe to use with XPin.
	 *
	 * Linux 5.13 added a field to `struct lsm_blob_sizes` so that LSMs can
	 * request a certain number of bytes to stuff in the `s_security` field.
	 *
	 * If using this field ends up being a problem, the superblock context
	 * could be looked up in a hash table, using the superblock pointer as
	 * the key. ChromiumOS's LSM follows this pattern, presumably to support
	 * using it alongside SELinux.
	 */
#if IS_ENABLED(CONFIG_SECURITY_SELINUX)
#error "This will conflict with SELinux's usage of the s_security field"
#elif IS_ENABLED(CONFIG_SECURITY_SMACK)
#error "This will conflict with Smack's usage of the s_security field"
#endif
	WARN_ON(sb->s_security != NULL);
	sb->s_security = xsb;
	return 0;
}


static void xpin_sb_free_security(struct super_block *sb)
{
	/* At this point, xsb will have the following references:
	 *  1: [ALWAYS]    reference from sb->s_security
	 *  2: [SOMETIMES] reference from fsnotify group (if sb has exceptions)
	 *
	 * If there is a reference from the fsnotify group, that will be dropped
	 * in the call to `xpin_sb_destroy()` below.
	 */
	struct xpin_superblock_ctx *xsb = xpin_superblock(sb);

	/* Drop reference from fsnotify group (if exists) */
	xpin_sb_destroy(xsb);
	sb->s_security = NULL;

	/* Drop the final reference (from sb->s_security) */
	xpin_sb_put(xsb);
}


static int xpin_inode_alloc_security(struct inode *inode)
{
	struct xpin_inode_ctx *xinode = xpin_inode(inode);

	memset(xinode, 0, sizeof(*xinode));
	INIT_HLIST_HEAD(&xinode->exceptions);
	spin_lock_init(&xinode->lock);
	return 0;
}


static int xpin_cred_alloc_blank(struct cred *cred, gfp_t gfp)
{
	struct xpin_cred_ctx *xcred = xpin_cred(cred);

	memset(xcred, 0, sizeof(*xcred));
	return 0;
}


static inline void xpin_cred_copy(
	struct cred *new, const struct cred *old)
{
	struct xpin_cred_ctx *xold = xpin_cred(old);
	struct xpin_cred_ctx *xnew = xpin_cred(new);

	xnew->flags = xold->flags;
}

static void xpin_cred_transfer(struct cred *new, const struct cred *old)
{
	xpin_cred_copy(new, old);
}


static int xpin_cred_prepare(
	struct cred *new, const struct cred *old, gfp_t gfp)
{
	xpin_cred_copy(new, old);
	return 0;
}


static void xpin_inherit_exceptions(struct xpin_cred_ctx *xcred)
{
	int old_flags = xcred->flags;

	if (!old_flags || (old_flags & XPIN_EXCEPTION_INHERIT))
		return;

	/* Exception shouldn't inherit across exec, so clear it */
	xcred->flags = 0;
}


static int xpin_bprm_creds_for_exec(struct linux_binprm *bprm)
{
	int err;
	enum xpin_event event;
	int flags = xpin_file_get_exception_flags(bprm->file);
	struct xpin_cred_ctx *xcred;

	/*
	 * Check if exec target has an exception early, so that fact can be
	 * used by xpin_access_check() to decide whether or not to allow this
	 * execution.
	 */
	event = flags ? XPIN_EVENT_EXEC_EXCEPTION : XPIN_EVENT_EXEC;
	err = xpin_access_check(bprm->file, event, false);
	if (unlikely(err))
		return err;

	/* Clear non-inheritable exceptions */
	xcred = xpin_cred(bprm->cred);
	xpin_inherit_exceptions(xcred);

	/*
	 * Inheritable exceptions override any file exceptions, so only apply
	 * exceptions from the newly executed program if there are no exception
	 * flags being inherited.
	 *
	 * The one exception to this rule is if the file exception's flags has
	 * the XPIN_EXCEPTION_UNINHERIT flag set. In this case, the inherited
	 * exception flags are dropped and replaced with the file exception's
	 * flags.
	 */
	if (unlikely(
		flags && (
			!xcred->flags || (flags & XPIN_EXCEPTION_UNINHERIT)
		)
	)) {
		pr_debug(
			"Applying exception: exe=\"%s\" pid=%d flags=0x%x\n",
			bprm->filename,
			task_pid_nr(current),
			flags
		);
		xcred->flags = flags;
		bprm->secureexec = !!(flags & XPIN_EXCEPTION_SECURE);
	}
	return 0;
}


static int xpin_bprm_set_creds(struct linux_binprm *bprm)
{
	if (bprm->called_set_creds)
		return 0;

	return xpin_bprm_creds_for_exec(bprm);
}


static int xpin_mmap_file(
	struct file *file,
	unsigned long reqprot,
	unsigned long prot,
	unsigned long flags)
{
	bool mutable;
	enum xpin_event event = XPIN_EVENT_MMAP;

	/* Allow all mmap requests that don't ask for PROT_EXEC */
	if (likely(!(prot & PROT_EXEC)))
		return 0;

	/* Don't allow RWX mappings without an exception */
	mutable = !!(prot & PROT_WRITE);
	return xpin_access_check(file, event, mutable);
}


static int xpin_file_mprotect(
	struct vm_area_struct *vma,
	unsigned long reqprot,
	unsigned long prot)
{
	bool mutable;
	enum xpin_event event = XPIN_EVENT_MPROTECT;
	struct file *file;

	/* Allow all mprotect requests that don't ask for PROT_EXEC */
	if (likely(!(prot & PROT_EXEC)))
		return 0;

	/* Treat modified mappings as anonymous */
	file = vma->anon_vma ? NULL : vma->vm_file;

	/* Don't allow RWX mappings without an exception */
	mutable = !!(prot & PROT_WRITE);
	return xpin_access_check(file, event, mutable);
}


static int xpin_shm_shmat(
	struct kern_ipc_perm *perm,
	char __user *shmaddr,
	int shmflg)
{
	/* Allow all shmat requests that don't ask for SHM_EXEC */
	if (likely(!(shmflg & SHM_EXEC)))
		return 0;

	return xpin_access_check(NULL, XPIN_EVENT_MMAP, true);
}


static int xpin_getprocattr(struct task_struct *task, char *name, char **value)
{
	int task_flags;
	char *buf;
	char *p;
	int len = strlen("adjuieqs\n");

	if (strcmp(name, "current"))
		return -ENOENT;

	/* Don't need room for a NUL-terminator */
	buf = kmalloc(len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	task_flags = xpin_task_get_exception_flags(task);

	p = buf;
	*p++ = task_flags & XPIN_EXCEPTION_ALLOW     ? 'A' : 'a';
	*p++ = task_flags & XPIN_EXCEPTION_DENY      ? 'D' : 'd';
	*p++ = task_flags & XPIN_EXCEPTION_JIT_ONLY  ? 'J' : 'j';
	*p++ = task_flags & XPIN_EXCEPTION_UNINHERIT ? 'U' : 'u';
	*p++ = task_flags & XPIN_EXCEPTION_INHERIT   ? 'I' : 'i';
	*p++ = task_flags & XPIN_EXCEPTION_NON_ELF   ? 'E' : 'e';
	*p++ = task_flags & XPIN_EXCEPTION_QUIET     ? 'Q' : 'q';
	*p++ = task_flags & XPIN_EXCEPTION_SECURE    ? 'S' : 's';
	*p++ = '\n';

	*value = buf;
	return len;
}


static struct security_hook_list xpin_hooks[] __lsm_ro_after_init = {
	LSM_HOOK_INIT(sb_alloc_security, xpin_sb_alloc_security),
	LSM_HOOK_INIT(sb_free_security, xpin_sb_free_security),
	LSM_HOOK_INIT(inode_alloc_security, xpin_inode_alloc_security),
	LSM_HOOK_INIT(cred_alloc_blank, xpin_cred_alloc_blank),

	LSM_HOOK_INIT(cred_transfer, xpin_cred_transfer),
	LSM_HOOK_INIT(cred_prepare, xpin_cred_prepare),

	LSM_HOOK_INIT(bprm_set_creds, xpin_bprm_set_creds),
	LSM_HOOK_INIT(mmap_file, xpin_mmap_file),
	LSM_HOOK_INIT(file_mprotect, xpin_file_mprotect),
	LSM_HOOK_INIT(shm_shmat, xpin_shm_shmat),

	LSM_HOOK_INIT(getprocattr, xpin_getprocattr),
};


void __init xpin_add_hooks(void)
{
	security_add_hooks(xpin_hooks, ARRAY_SIZE(xpin_hooks), "xpin");
}


DEFINE_LSM(xpin) = {
	.name = "xpin",
	.init = xpin_init,
	.blobs = &xpin_blob_sizes,
};
