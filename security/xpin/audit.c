// SPDX-License-Identifier: GPL-2.0-only
/*
 * audit.c: Responsible for creating and emitting audit log entries for
 * operations mediated by XPin.
 */

#define pr_fmt(fmt) KBUILD_MODNAME "-audit: " fmt

#include "xpin_internal.h"
#include <linux/audit.h>
#include <linux/bug.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/namei.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <linux/string_helpers.h>
#include <linux/string.h>
#include <linux/task_work.h>

/*
 * Needed to detect bind-mounts. Specifically, the public API functions that
 * operate on `struct vfsmount` provide no way of accessing either the parent
 * mount or the root dentry for a given mount. Without these, there's no good
 * way to tell whether a file comes from a bind-mount.
 *
 * Alternatives considered:
 * 1. Add an LSM hook on mount/umount to track bind mounts.
 *    This idea could work, but there are a lot of potential issues. Firstly,
 *    If there's another LSM after XPin in the LSM order, XPin could see a
 *    bind-mount operation and assume it succeeded. However, a later LSM could
 *    block the bind-mount operation from occurring. This would then leave XPin
 *    in a state where its perceived mount state of the system does not match
 *    the actual mount state of the system. Also, the kernel already has the
 *    information we want (in struct mount), so why go to the extra effort of
 *    separately keeping track of it ourselves?
 * 2. Open, read, and parse /proc/self/mountinfo within the kernel.
 *    This is apparently the ONLY public interface that gives access to the
 *    level of information about mounts that would allow XPin to determine if
 *    a file comes from a bind-mount. However, the whole idea of opening and
 *    parsing /proc/self/mountinfo within the kernel itself is ultra disgusting.
 * 3. Add new public API functions for getting the parent of a `struct vfsmount`
 *    as well as the mountpoint `dentry` of a `struct vfsmount`.
 *    This was considered but ultimately discarded, as XPin would be the only
 *    consumer of this new API. A design-level goal of XPin is to avoid any core
 *    kernel modifications if at all possible.
 *
 * After considering all of these options, including an internal header seems
 * like the least-bad option.
 */
#include "../../fs/mount.h"


struct xpin_audit_subject {
	/* needs put_task_struct() */
	struct task_struct *caller;
};

struct xpin_audit_action {
	enum xpin_event event;
	bool mutable;
};

struct xpin_audit_object {
	/* needs fput() */
	struct file *file;
};

struct xpin_audit_result {
	int error;
	bool allow;
};

struct xpin_audit_context {
	bool enforce;
	bool exception;
};

struct xpin_audit_data {
	struct xpin_audit_subject who;
	struct xpin_audit_action what;
	struct xpin_audit_object whom;
	struct xpin_audit_result result;
	struct xpin_audit_context why;
};

struct xpin_report_info {
	struct callback_head work;
	struct xpin_audit_data data;
};


const char *xpin_event_names[XPIN_EVENT_COUNT] = {
	"mmap",
	"mprotect",
	"exec",
	"exec(exception)",
	"shmat",
	"procmem",
	"check",
};

const char *xpin_file_kinds[XPIN_FILE_COUNT] = {
	"anonymous",
	"memfd",
	"shmem",
	"unsigned",
	"verity",
};


enum xpin_file_kind xpin_get_file_kind(struct file *file)
{
	if (!file)
		return XPIN_FILE_ANON;

	if (shmem_file(file)) {
		if (file->f_inode->i_nlink == 0)
			return XPIN_FILE_MEMFD;
		return XPIN_FILE_SHMEM;
	}

	if (xpin_is_verity_file(file))
		return XPIN_FILE_VERITY;

	return XPIN_FILE_UNSIGNED;
}


static void xpin_report_lite(enum xpin_event event, struct file *file)
{
	char *buffer = NULL;
	char *file_str;
	size_t bufsize;
	char comm[TASK_COMM_LEN];

	if (file) {
		bufsize = PATH_MAX + sizeof(" (deleted)");
		buffer = kmalloc(bufsize, GFP_ATOMIC);
		if (buffer) {
			file_str = file_path(file, buffer, bufsize);
			if (IS_ERR(file_str))
				file_str = "<too_long>";
		} else {
			file_str = "<no_memory>";
		}
	} else {
		file_str = "<shm>";
	}

	memcpy(comm, current->comm, sizeof(comm));
	pr_notice_ratelimited(
		"%s code from \"%s\" (%s) was attempted by \"%s\"[%d]\n",
		xpin_event_names[event],
		file_str, xpin_file_kinds[xpin_get_file_kind(file)],
		comm, task_pid_nr(current)
	);

	kfree(buffer);
}


/*
 * This function works by checking the parent of the file's mount to see if it
 * contains a file at the matching path. Effectively, it finds the same result
 * as if you unmounted the volume that the file is on and then tried to lookup
 * the file path after that.
 */
static struct file *xpin_get_buried_file(
	struct file *file,
	struct path *mountpoint)
{
	int err = 0;
	struct mount *real = real_mount(file->f_path.mnt);
	struct mount *parent = real->mnt_parent;
	char *buf = NULL;
	char *subpath;
	struct file *buried = NULL;
	struct path mount_path;

	/* Not covered by a bind-mount? */
	if (!parent)
		return NULL;

	if (file->f_path.dentry == real->mnt.mnt_root) {
		/*
		 * This file is directly bind-mounted, not part of a
		 * bind-mounted directory.
		 */
		mount_path.mnt = &parent->mnt;
		mount_path.dentry = real->mnt_mountpoint;
		buried = dentry_open(
			&mount_path, O_PATH | O_NOATIME, current->cred
		);
		goto out;
	}

	buf = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!buf) {
		err = -ENOMEM;
		goto out;
	}

	/* Get path to file under its mount */
	mount_path.mnt = &real->mnt;
	mount_path.dentry = real->mnt.mnt_root;
	subpath = __d_path(&file->f_path, &mount_path, buf, PATH_MAX);
	if (IS_ERR_OR_NULL(subpath)) {
		err = subpath ? PTR_ERR(subpath) : -ENOENT;
		goto out;
	}

	buried = file_open_root(
		real->mnt_mountpoint, &parent->mnt,
		subpath, O_PATH | O_NOFOLLOW, 0
	);

	if (IS_ERR_OR_NULL(buried)) {
		err = PTR_ERR(buried);
		goto out;
	}

out:
	kfree(buf);
	if (err)
		return ERR_PTR(err);

	mountpoint->mnt = mntget(&parent->mnt);
	mountpoint->dentry = dget(real->mnt_mountpoint);
	return buried;
}


static void xpin_report_cb(struct callback_head *work)
{
	struct audit_buffer *ab;
	struct xpin_report_info *info = container_of(work, typeof(*info), work);
	struct xpin_audit_data *xd = &info->data;
	const char *verb;
	char comm[TASK_COMM_LEN];
	char *p;
	struct file *file = xd->whom.file;
	struct file *exe;
	enum xpin_file_kind file_kind;
	const char *file_kind_str;

	ab = audit_log_start(
		xd->who.caller->audit_context, GFP_KERNEL, AUDIT_AVC
	);
	if (!ab) {
		pr_notice_ratelimited(
			"Dropping audit message because audit_log_start failed\n"
		);
		goto out;
	}

	if (xd->result.allow) {
		if (xd->result.error == 0)
			verb = "ALLOWED";
		else if (xd->why.exception)
			verb = "EXCEPTION";
		else if (xd->why.enforce)
			verb = "AUDIT";
		else
			verb = "OBSERVED";
	} else {
		verb = "DENIED";
	}

	audit_log_format(ab, "xpin=");
	audit_log_string(ab, verb);

	/* Same starting point as lsm_audit */
	audit_log_format(ab, " pid=%d comm=", task_tgid_nr(xd->who.caller));
	get_task_comm(comm, xd->who.caller);
	for (p = comm; *p; p++) {
		/*
		 * `audit_log_untrustedstring()` will print the whole string
		 * as hex if the input contains spaces or double quotes.
		 * Replace those with safe characters.
		 */
		if (*p == ' ')
			*p = '_';
		else if (*p == '"')
			*p = '\'';
	}
	audit_log_untrustedstring(ab, comm);

	exe = get_task_exe_file(xd->who.caller);
	if (exe) {
		char *exe_str = kstrdup_quotable_file(exe, GFP_KERNEL);

		if (exe_str) {
			audit_log_format(ab, " exe=");
			audit_log_untrustedstring(ab, exe_str);
			kfree(exe_str);
		}

		fput(exe);
	}

	audit_log_format(ab, " action=");
	audit_log_string(ab, xpin_event_names[xd->what.event]);

	if (file)
		audit_log_d_path(ab, " file=", &file->f_path);

	file_kind = xpin_get_file_kind(file);
	if (file_kind == XPIN_FILE_VERITY && xd->what.mutable)
		file_kind_str = "verity(rwx)";
	else
		file_kind_str = xpin_file_kinds[file_kind];

	audit_log_format(ab, " kind=");
	audit_log_string(ab, file_kind_str);

	if (file) {
		struct file *buried;
		struct path bind_mountpoint = {};

		/* Check if the file is on a bind-mount */
		buried = xpin_get_buried_file(file, &bind_mountpoint);
		if (!IS_ERR_OR_NULL(buried)) {
			char *buf;
			dev_t bind_dev = file->f_path.mnt->mnt_sb->s_dev;

			audit_log_format(
				ab, " bind_dev=%u:%u",
				MAJOR(bind_dev), MINOR(bind_dev)
			);

			buf = kmalloc(PATH_MAX, GFP_KERNEL);
			if (!buf) {
				audit_log_format(ab, " bind_src=NOMEM");
				audit_log_format(ab, " bind_dst=NOMEM");
			} else {
				char *tmp = dentry_path(
					file->f_path.mnt->mnt_root,
					buf, PATH_MAX
				);

				if (!IS_ERR_OR_NULL(tmp)) {
					audit_log_format(ab, " bind_src=");
					audit_log_untrustedstring(ab, tmp);
				}

				tmp = d_path(&bind_mountpoint, buf, PATH_MAX);
				if (!IS_ERR_OR_NULL(tmp)) {
					audit_log_format(ab, " bind_dst=");
					audit_log_untrustedstring(ab, tmp);
				}

				kfree(buf);
			}

			audit_log_format(ab, " buried_kind=");
			audit_log_string(ab,
				xpin_file_kinds[xpin_get_file_kind(buried)]
			);

			path_put(&bind_mountpoint);
			fput(buried);
		}
	}

	/* Only log unusual error codes (which can't be inferred from verb) */
	if (xd->result.error && xd->result.error != -EPERM)
		audit_log_format(ab, " errno=%d", -xd->result.error);

	audit_log_end(ab);

out:
	put_task_struct(xd->who.caller);
	fput_light(file, !!file);
	kfree(info);
}


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
	bool exception)
{
	struct xpin_report_info *info;

	if (!xpin_logging)
		return;

	/*
	 * According to comment in yama's report_access(), kthreads don't call
	 * task_work_run() before exiting. This allows xpin to log at least
	 * something for kthreads.
	 */
	if (current->flags & PF_KTHREAD) {
		xpin_report_lite(event, file);
		return;
	}

	/* Use GFP_ATOMIC to avoid sleeping in a potentially hot path */
	info = kzalloc(sizeof(*info), GFP_ATOMIC);
	if (!info)
		return;

	init_task_work(&info->work, xpin_report_cb);
	info->data.who.caller = get_task_struct(current);
	info->data.what.event = event;
	info->data.what.mutable = mutable;

	if (file)
		get_file(file);

	info->data.whom.file = file;
	info->data.result.error = error;
	info->data.result.allow = allow;
	info->data.why.enforce = enforce;
	info->data.why.exception = exception;

	/*
	 * Defer logging of audit message until later when it's safe to take
	 * the task lock. This will run just before the task switches back to
	 * user-space.
	 */
	if (task_work_add(current, &info->work, true) == 0)
		return;

	WARN(1, "%s called from exiting task", __func__);

	fput_light(file, !!file);
	put_task_struct(current);
	kfree(info);
}
