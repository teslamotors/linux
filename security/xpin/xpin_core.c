// SPDX-License-Identifier: GPL-2.0-only
/*
 * XPin: LoadPin but for eXecutable code
 *
 * XPin is a Linux Security Module (LSM) which enforces that all executable
 * memory pages must be backed by a file on a dm-verity protected volume.
 *
 * xpin_core.c: Implements the core access checking logic of XPin.
 */

#define pr_fmt(fmt) KBUILD_MODNAME "-core: " fmt

#include "xpin_internal.h"
#include <linux/atomic.h>
#include <linux/dm-verity-xpin.h>
#include <linux/kernel.h>
#include <linux/mount.h>
#include <linux/shmem_fs.h>
#include <linux/string_helpers.h>
#include <linux/string.h>
#include <linux/types.h>


static const u8 ELF_MAGIC[4] = {0x7F, 'E', 'L', 'F'};


static bool xpin_is_verity_sb_nocache(struct super_block *sb)
{
	/* It seems like tmpfs mounts have their s_bdev set to NULL */
	if (!sb->s_bdev)
		return false;

	return dm_verity_xpin_is_verity_bdev(sb->s_bdev);
}


bool xpin_is_verity_sb(struct super_block *sb)
{
	struct xpin_superblock_ctx *xsb;
	int flags;

	if (PARANOID(!sb))
		return false;

	xsb = xpin_superblock(sb);

	/* Lock-free fast path? */
	flags = READ_ONCE(xsb->flags);
	if (likely(flags & XPIN_SB_CHECKED))
		goto out;

	spin_lock(&xsb->lock);

	/* Re-check now that we hold the lock */
	flags = xsb->flags;
	if (unlikely(flags & XPIN_SB_CHECKED)) {
		spin_unlock(&xsb->lock);
		goto out;
	}

	/*
	 * No locking needed here, it's fine if more than one thread run this
	 * in parallel. They'll all end up checking if the superblock is a
	 * verity superblock, and then they'll all attempt to atomically set
	 * the right bits. That is fine.
	 */
	if (xpin_is_verity_sb_nocache(sb))
		flags |= XPIN_SB_VERITY;

	flags |= XPIN_SB_CHECKED;
	xsb->flags = flags;
	spin_unlock(&xsb->lock);

out:
	return !!(flags & XPIN_SB_VERITY);
}


bool xpin_is_verity_file(struct file *file)
{
	if (!file || PARANOID(!file->f_path.mnt))
		return false;

	return xpin_is_verity_sb(file->f_path.mnt->mnt_sb);
}


static bool xpin_is_elf_file_nocache(struct file *file)
{
	u8 magic[sizeof(ELF_MAGIC)];
	loff_t off = 0;
	ssize_t bytes_read;

	if (!file || !(file->f_mode & FMODE_READ))
		return false;

	/* Read ELF header */
	bytes_read = kernel_read(file, magic, sizeof(magic), &off);
	if (bytes_read != sizeof(magic)) {
		const char *filepath = "<no_memory>";
		char *pathbuf = kstrdup_quotable_file(file, GFP_KERNEL);

		if (pathbuf)
			filepath = pathbuf;

		pr_warn(
			"Failed to read file header: path=\"%s\" err=%zd\n",
			filepath, bytes_read
		);

		kfree(pathbuf);

		/* Fail-open for now until this is figured out */
		return true;
	}

	return memcmp(magic, ELF_MAGIC, sizeof(ELF_MAGIC)) == 0;
}


bool xpin_is_elf_file(struct file *file)
{
	struct xpin_inode_ctx *xinode;
	int flags;

	if (!file)
		return false;

	xinode = xpin_inode(file->f_inode);

	/* Already checked if this inode is an ELF file? */
	flags = atomic_read(&xinode->flags);
	if (likely(flags & XPIN_INODE_FORMAT_CHECKED))
		return !!(flags & XPIN_INODE_IS_ELF);

	/*
	 * No locking needed, racing tasks will simply check whether the file
	 * is an ELF file multiple times, and they will all set the right bits
	 * atomically.
	 */
	if (xpin_is_elf_file_nocache(file)) {
		atomic_or(
			XPIN_INODE_FORMAT_CHECKED | XPIN_INODE_IS_ELF,
			&xinode->flags
		);
		return true;
	}

	atomic_or(XPIN_INODE_FORMAT_CHECKED, &xinode->flags);
	return false;
}


bool xpin_is_memfd_file(struct file *file)
{
	/* Only allow memfd files with no links */
	if (!shmem_file(file) || file->f_inode->i_nlink != 0)
		return false;

	return strncmp(file->f_path.dentry->d_name.name, "memfd:", 6) == 0;
}


bool xpin_is_sysv_shm_file(struct file *file)
{
	const char *dentry_name = file->f_path.dentry->d_name.name;

	/* Only allow SYSV SHM files with no links */
	if (!shmem_file(file) || file->f_inode->i_nlink != 0)
		return false;

	/* SYSV SHM files have names like "SYSV%08x" */
	return (
		strncmp(dentry_name, "SYSV", 4) == 0
		&& strlen(dentry_name + 4) == 8
	);
}


int xpin_task_get_exception_flags(const struct task_struct *task)
{
	return xpin_cred(task->cred)->flags;
}


static inline bool xpin_event_is_mm(enum xpin_event event)
{
	return event == XPIN_EVENT_MMAP || event == XPIN_EVENT_MPROTECT;
}


static inline bool xpin_operation_is_jit(
	enum xpin_event event,
	struct file *file,
	bool is_trusted_file,
	bool mutable)
{
	return (
		!file
		|| xpin_is_memfd_file(file)
		|| xpin_is_sysv_shm_file(file)
		|| (is_trusted_file && mutable)
	);
}


static inline bool xpin_operation_is_exception(
	enum xpin_event event,
	int task_flags,
	struct file *file,
	bool is_trusted_file,
	bool mutable)
{
	return (task_flags & XPIN_EXCEPTION_ALLOW) && (
		/*
		 * Task is mapping anonymous memory with PROT_EXEC, or
		 * it's mapping code from a dm-verity ELF file as RWX.
		 */
		xpin_operation_is_jit(event, file, is_trusted_file, mutable)

		/*
		 * Task is trying to use code from an unsigned file, so
		 * allow if it doesn't have the JIT-only flag.
		 */
		|| !(task_flags & XPIN_EXCEPTION_JIT_ONLY)
	);
}


int xpin_access_check(struct file *file, enum xpin_event event, bool mutable)
{
	int ret = -EPERM;
	bool enforce = READ_ONCE(xpin_enforce);
	unsigned int logging = READ_ONCE(xpin_logging);
	int task_flags;
	bool is_trusted_file;
	bool non_elf = false;
	bool allow = false;
	bool exception_allow = false;

	/* Nothing to do? */
	if (unlikely(!enforce && logging == XPIN_LOGGING_NONE))
		return 0;

	/*
	 * It is only possible for a non-verity file to have exception flags
	 * when XPin lockdown mode is not enabled, meaning we're running in
	 * development mode. It is useful for development to allow treating
	 * unsigned files as trusted if they have any exception flags (such
	 * as a "deny" exception).
	 */
	is_trusted_file = (
		xpin_is_verity_file(file)
		|| xpin_file_get_exception_flags(file) != 0
	);
	task_flags = xpin_task_get_exception_flags(current);

	/*
	 * Allow common case: immutable use of code from a dm-verity protected
	 * volume (along with the ELF format check for mmap/mprotect).
	 */
	if (likely(!mutable && is_trusted_file)) {
		/* Calls to mmap/mprotect with PROT_EXEC require an ELF file. */
		if (xpin_event_is_mm(event)) {
			/*
			 * Allow mmap/mprotect of non-ELF files if the task
			 * either has the NON_ELF flag, or if it has a full
			 * exception (ALLOW && !JIT_ONLY). Note that we check
			 * task flags before checking the file format, as it's
			 * way slower to check the file format.
			 */
			if (likely(
				(task_flags & XPIN_EXCEPTION_NON_ELF)
				|| (
					!(task_flags & XPIN_EXCEPTION_JIT_ONLY)
					&& (task_flags & XPIN_EXCEPTION_ALLOW)
				)

				/*
				 * Note: Checking if a file is an ELF file
				 * involves reading from the file (which might
				 * sleep), so this is illegal to call while
				 * holding a spinlock or RCU read lock.
				 */
				|| xpin_is_elf_file(file)
			))
				return 0;

			non_elf = true;
		} else {
			/* No format check needed for other events */
			return 0;
		}
	}

	/* At this point, in normal cases, the request should be blocked */

	/* Allow if enforcement is disabled */
	if (unlikely(!enforce))
		allow = true;

	/*
	 * non_elf can only be true if the ONLY reason the action would be
	 * blocked is because the file is not an ELF file. It otherwise must
	 * pass all of XPin's checks (dm-verity, !mutable, etc). The NON_ELF
	 * exception criteria has already been checked (and failed) at this
	 * point, so further checks for exceptions are unnecessary.
	 */
	if (unlikely(non_elf)) {
		ret = -ENOEXEC;
	} else if (likely(
		/*
		 * For development, a process without a JIT exception should be
		 * able to exec a non-verity file that has been granted a JIT
		 * exception. When lockdown mode is enabled, it should be
		 * impossible to grant an exception to a non-verity file.
		 * However, it's possible that a JIT exception was granted to a
		 * non-verity file before lockdown mode was activated. In this
		 * case, the previously granted exception should still be
		 * honored. The reasoning behind this is that in production
		 * devices, lockdown mode will be activated in very early boot.
		 * Therefore, there will normally be no exceptions granted
		 * before lockdown mode is activated for prod-fused devices.
		 * The only time when a non-verity exception will likely be
		 * granted before lockdown mode is activated is during
		 * development of XPin or its exceptions lists, so this should
		 * be allowed.
		 */
		event == XPIN_EVENT_EXEC_EXCEPTION

		/* Allowed by exception? */
		|| xpin_operation_is_exception(
			event,
			task_flags,
			file,
			is_trusted_file,
			mutable
		)
	)) {
		exception_allow = true;
		allow = true;
	}

	/*
	 * Log an audit report if the action is not allowed by an exception or
	 * if verbose logging is enabled. If the task has the QUIET flag, then
	 * nothing will be logged.
	 */
	if (unlikely(
		(!exception_allow || logging >= XPIN_LOGGING_VERBOSE)
		&& !(task_flags & XPIN_EXCEPTION_QUIET)
	))
		/* Fine to report on XPIN_EVENT_CHECK */
		xpin_report(
			event, mutable, file,
			ret, allow, enforce, exception_allow
		);

	return allow ? 0 : ret;
}
