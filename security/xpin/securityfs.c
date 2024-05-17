// SPDX-License-Identifier: GPL-2.0-only
/*
 * securityfs.c: Builds the securityfs directory structure for XPin, including
 * all of the logic for user interactions on those files. In a typical system,
 * this will create /sys/kernel/security/xpin and everything under it.
 */

#define pr_fmt(fmt) KBUILD_MODNAME "-securityfs: " fmt

#include "xpin_internal.h"
#include <linux/capability.h>
#include <linux/compiler_types.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/kernel.h>
#include <linux/mount.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/security.h>
#include <linux/seq_file.h>
#include <linux/shmem_fs.h>
#include <linux/sizes.h>
#include <linux/string_helpers.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <uapi/linux/xpin.h>


#define XPIN_EXCEPTIONS_LIST_HEADER "## XPin Exceptions List"


static ssize_t xpin_enforce_read(
	struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
	char buf[2];

	buf[0] = xpin_enforce ? '1' : '0';
	buf[1] = '\n';

	return simple_read_from_buffer(ubuf, count, ppos, buf, sizeof(buf));
}


static ssize_t xpin_enforce_write(
	struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	bool new_enforce;
	int err;

	if (!capable(CAP_MAC_ADMIN))
		return -EPERM;

	err = kstrtobool_from_user(ubuf, count, &new_enforce);
	if (err)
		return err;

	if (xpin_enforce == new_enforce)
		return count;

	if (xpin_lockdown && !new_enforce) {
		char comm_buf[TASK_COMM_LEN + 1];

		memcpy(comm_buf, current->comm, sizeof(comm_buf));
		comm_buf[TASK_COMM_LEN] = '\0';

		pr_warn(
			"Refusing to disable enforcement in lockdown mode: comm=\"%s\"\n",
			comm_buf
		);
		return -EPERM;
	}

	xpin_enforce = new_enforce;
	pr_info("Enforcement is now %sabled\n", xpin_enforce ? "en" : "dis");
	return count;
}


static const struct file_operations xpin_enforce_ops = {
	.read = xpin_enforce_read,
	.write = xpin_enforce_write,
};


static ssize_t xpin_logging_read(
	struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
	char buf[11];
	int available = scnprintf(buf, sizeof(buf), "%u\n", xpin_logging);

	return simple_read_from_buffer(ubuf, count, ppos, buf, available);
}


static ssize_t xpin_logging_write(
	struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	unsigned int new_logging;
	int err;

	if (!capable(CAP_MAC_ADMIN))
		return -EPERM;

	err = kstrtouint_from_user(ubuf, count, 10, &new_logging);
	if (err)
		return err;

	if (xpin_logging == new_logging)
		return count;

	if (xpin_lockdown && new_logging < xpin_logging) {
		char comm_buf[TASK_COMM_LEN + 1];

		memcpy(comm_buf, current->comm, sizeof(comm_buf));
		comm_buf[TASK_COMM_LEN] = '\0';

		pr_warn(
			"Refusing to disable logging in lockdown mode: comm=\"%s\"\n",
			comm_buf
		);
		return -EPERM;
	}

	xpin_logging = new_logging;
	pr_info(
		"Logging is now %sabled (%u)\n",
		xpin_logging ? "en" : "dis",
		xpin_logging
	);
	return count;
}


static const struct file_operations xpin_logging_ops = {
	.read = xpin_logging_read,
	.write = xpin_logging_write,
};


static ssize_t xpin_lockdown_read(
	struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
	char buf[2];

	buf[0] = xpin_lockdown ? '1' : '0';
	buf[1] = '\n';

	return simple_read_from_buffer(ubuf, count, ppos, buf, sizeof(buf));
}


static ssize_t xpin_lockdown_write(
	struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	bool new_lockdown;
	int err;

	if (!capable(CAP_MAC_ADMIN))
		return -EPERM;

	err = kstrtobool_from_user(ubuf, count, &new_lockdown);
	if (err)
		return err;

	if (xpin_lockdown == new_lockdown)
		return count;

	if (!new_lockdown) {
		char comm_buf[TASK_COMM_LEN + 1];

		memcpy(comm_buf, current->comm, sizeof(comm_buf));
		comm_buf[TASK_COMM_LEN] = '\0';

		pr_warn(
			"Refusing to disable lockdown mode: comm=\"%s\"\n",
			comm_buf
		);
		return -EPERM;
	}

	xpin_lockdown = true;
	pr_info("Lockdown mode is now enabled\n");
	return count;
}


static const struct file_operations xpin_lockdown_ops = {
	.read = xpin_lockdown_read,
	.write = xpin_lockdown_write,
};


static int xpin_check_file(struct xpin_check_file __user *uptr)
{
	int err;
	struct xpin_check_file local = {};
	struct fd f = {};
	u32 usize;
	int file_flags = 0;
	int task_flags;

	/* Read just the struct's size for now */
	err = get_user(usize, &uptr->size);
	if (err)
		return err;

	/* Size of 0 means the user is requesting the kernel's struct size */
	if (usize == 0)
		return put_user(sizeof(local), &uptr->size);

	/* Size must at least hold the smallest known version of the struct */
	if (usize < sizeof(struct xpin_check_file))
		return -EINVAL;

	/* Enforce a reasonable maximum size */
	if (usize > SZ_4K)
		return -E2BIG;

	/* Safely copy size-versioned struct into kernel */
	err = copy_struct_from_user(&local, sizeof(local), uptr, usize);
	if (err)
		return err;

	/* Is user trying some ToCToU shenanigans? */
	if (local.size != usize)
		return -EAGAIN;

	/* Tell user what the kernel's known size is */
	if (local.size > sizeof(local))
		local.size = sizeof(local);

	f = fdget(local.fd);
	if (!f.file)
		return -EBADF;

	/* Do dry-run access check and gather file and task flags */
	local.result = xpin_access_check(f.file, XPIN_EVENT_CHECK, false);
	file_flags = xpin_file_get_exception_flags(f.file);
	task_flags = xpin_task_get_exception_flags(current);

	/* Fill in some extra info */
	local.caller_has_exception = !!(task_flags & XPIN_EXCEPTION_ALLOW);
	local.is_verity = xpin_is_verity_file(f.file);
	local.is_shmem = shmem_file(f.file);
	local.is_memfd = xpin_is_memfd_file(f.file);
	local.is_elf = xpin_is_elf_file(f.file);
	local.is_exception = !!(file_flags & XPIN_EXCEPTION_ALLOW);
	local.is_deny = !!(file_flags & XPIN_EXCEPTION_DENY);
	local.is_jit_only = !!(file_flags & XPIN_EXCEPTION_JIT_ONLY);
	local.is_inheritable = !!(file_flags & XPIN_EXCEPTION_INHERIT);
	local.allows_non_elf = !!(file_flags & XPIN_EXCEPTION_NON_ELF);
	local.is_quiet = !!(file_flags & XPIN_EXCEPTION_QUIET);
	local.is_secure_exec = !!(file_flags & XPIN_EXCEPTION_SECURE);
	local.is_uninherit = !!(file_flags & XPIN_EXCEPTION_UNINHERIT);

	/*
	 * Be sure to use validated size here, to avoid corrupting userspace
	 * or sending more than the kernel's size of the struct (infoleak).
	 */
	if (copy_to_user(uptr, &local, local.size))
		err = -EFAULT;

	fdput(f);
	return err;
}


static bool xpin_line_skip_prefix(char **pline, const char *prefix)
{
	size_t prefix_len = strlen(prefix);

	if (strncmp(*pline, prefix, prefix_len))
		return false;

	*pline += prefix_len;
	return true;
}


static int xpin_process_exception_line(
	char *line,
	const char *filepath,
	unsigned int line_number)
{
	int flags = 0;
	bool lazy = false;

	/* Skip empty lines and comments */
	if (!line[0] || line[0] == '#')
		return 0;

	/*
	 * Parse zero or more leading attributes in the exception line. Example:
	 *     lazy jit nonelf inherit /path/to/file
	 */
	while (line[0] && line[0] != '/') {
		if (xpin_line_skip_prefix(&line, XPIN_TOKEN_LAZY))
			lazy = true;
		else if (xpin_line_skip_prefix(&line, XPIN_TOKEN_DENY))
			flags |= XPIN_EXCEPTION_DENY;
		else if (xpin_line_skip_prefix(&line, XPIN_TOKEN_JIT))
			flags |= XPIN_EXCEPTION_JIT_ONLY;
		else if (xpin_line_skip_prefix(&line, XPIN_TOKEN_UNINHERIT))
			flags |= XPIN_EXCEPTION_UNINHERIT;
		else if (xpin_line_skip_prefix(&line, XPIN_TOKEN_INHERIT))
			flags |= XPIN_EXCEPTION_INHERIT;
		else if (xpin_line_skip_prefix(&line, XPIN_TOKEN_NON_ELF))
			flags |= XPIN_EXCEPTION_NON_ELF;
		else if (xpin_line_skip_prefix(&line, XPIN_TOKEN_QUIET))
			flags |= XPIN_EXCEPTION_QUIET;
		else if (xpin_line_skip_prefix(&line, XPIN_TOKEN_SECURE))
			flags |= XPIN_EXCEPTION_SECURE;
		else
			break;

		line = skip_spaces(line);
	}

	/* Line must now only contain the path */
	if (line[0] != '/') {
		pr_warn(
			"Bad exception line format: path=\"%s\" line=%u\n",
			filepath, line_number
		);
		return -EINVAL;
	}

	/* Doesn't make sense to have an exception with "deny" and "jit" */
	if (
		(flags & (XPIN_EXCEPTION_DENY | XPIN_EXCEPTION_JIT_ONLY))
		== (XPIN_EXCEPTION_DENY | XPIN_EXCEPTION_JIT_ONLY)
	) {
		pr_warn(
			"Illegal modifier combination: path=\"%s\" line=%u\n",
			filepath, line_number
		);
		return -EINVAL;
	}

	/* Set the implied ALLOW flag unless the "deny" attribute is present */
	if (!(flags & XPIN_EXCEPTION_DENY))
		flags |= XPIN_EXCEPTION_ALLOW;

	return xpin_add_exception(line, flags, lazy);
}


static int xpin_parse_exceptions_file(struct file *file, const char *filepath)
{
	int err;
	char *line;
	unsigned int line_number;
	struct xpin_line_buffer lb = {};

	/* Setup line buffered reading structure */
	lb.file = file;
	lb.buffer_size = SZ_4K;
	lb.buffer = kzalloc(lb.buffer_size, GFP_KERNEL);
	if (!lb.buffer) {
		err = -ENOMEM;
		goto out;
	}

	/* Read and check first line */
	line = xpin_read_line(&lb);
	if (IS_ERR(line)) {
		err = PTR_ERR(line);
		pr_warn(
			"Error reading exceptions file header line: path=\"%s\" err=%d\n",
			filepath, err
		);
		goto out;
	} else if (!line || strcmp(line, XPIN_EXCEPTIONS_LIST_HEADER)) {
		pr_warn(
			"Exceptions file missing header line: path=\"%s\"\n",
			filepath
		);
		err = -EINVAL;
		goto out;
	}

	/* Continue reading from line 2 */
	line_number = 1;
	while ((line = xpin_read_line(&lb))) {
		++line_number;

		if (IS_ERR(line)) {
			err = PTR_ERR(line);
			pr_warn(
				"Error reading from exceptions file: path=\"%s\" line=%u err=%d\n",
				filepath, line_number, err
			);
			goto out;
		}

		/* Parse and add the line */
		err = xpin_process_exception_line(line, filepath, line_number);
		if (err)
			goto out;
	}

out:
	kfree(lb.buffer);
	return err;
}


static int xpin_add_exceptions(unsigned int __user *ufd)
{
	int err = 0;
	unsigned int fd;
	struct fd f = {};
	char *pathbuf = NULL;
	const char *filepath = "<no_memory>";

	if (!capable(CAP_MAC_ADMIN))
		return -EPERM;

	/* Read the file descriptor from user */
	err = get_user(fd, ufd);
	if (err)
		goto out;

	/* Lookup file object from file descriptor */
	f = fdget(fd);
	if (!f.file) {
		err = -EBADF;
		goto out;
	}

	/* File must be opened in read mode */
	if (!(f.file->f_mode & FMODE_READ)) {
		err = -EBADFD;
		goto out;
	}

	pathbuf = kstrdup_quotable_file(f.file, GFP_KERNEL);
	if (pathbuf)
		filepath = pathbuf;

	/* Ensure exceptions list file lives on a dm-verity protected volume */
	if (!xpin_is_verity_file(f.file)) {
		if (xpin_lockdown) {
			pr_warn(
				"Non-verity exceptions list ignored in lockdown mode: path=\"%s\"\n",
				filepath
			);
			err = -EPERM;
			goto out;
		} else {
			pr_warn(
				"Allowing non-verity exceptions file because lockdown mode is disabled: path=\"%s\"\n",
				filepath
			);
		}
	}

	if (pathbuf)
		pr_info("Importing exceptions list from \"%s\"\n", pathbuf);

	err = xpin_parse_exceptions_file(f.file, filepath);

out:
	kfree(pathbuf);
	fdput(f);
	return err;
}


static int xpin_clear_exceptions(int __user *ufd)
{
	int ret = 0;
	int fd;
	struct fd f = {};

	if (!capable(CAP_MAC_ADMIN) || xpin_lockdown)
		return -EPERM;

	/* Read the file descriptor from user */
	ret = get_user(fd, ufd);
	if (ret)
		goto out;

	if (fd == -1) {
		pr_info("Clearing all exceptions\n");
		xpin_clear_all_exceptions();
	} else {
		char *filepath;

		/* Lookup file object from file descriptor */
		f = fdget(fd);
		if (!f.file) {
			ret = -EBADF;
			goto out;
		}

		/* Any exception to remove? */
		if (!xpin_file_get_exception_flags(f.file)) {
			ret = 0;
			goto out;
		}

		filepath = kstrdup_quotable_file(f.file, GFP_KERNEL);
		pr_info(
			"Clearing exception from \"%s\"\n",
			filepath ?: "<no_memory>"
		);
		kfree(filepath);

		xpin_file_remove_exception(f.file);
	}

out:
	fdput(f);
	return ret;
}


static long xpin_control_ioctl(
	struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	switch (cmd) {
	case XPIN_IOC_ADD_EXCEPTIONS:
		return xpin_add_exceptions((unsigned int __user *)arg);

	case XPIN_IOC_CHECK_FILE:
		return xpin_check_file((struct xpin_check_file __user *)arg);

	case XPIN_IOC_CLEAR_EXCEPTIONS:
		return xpin_clear_exceptions((int __user *)arg);

	default:
		pr_warn("Unhandled ioctl value: cmd=%u\n", cmd);
		return -EINVAL;
	}
}


static const struct file_operations xpin_control_ops = {
	.unlocked_ioctl = xpin_control_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
};


static void *xpin_exceptions_seq_start(struct seq_file *m, loff_t *ppos)
{
	struct xpin_exceptions_list *elist = m->private;

	if (*ppos >= elist->count)
		return NULL;

	return seq_list_start(&elist->head, *ppos);
}

static void xpin_exceptions_seq_stop(struct seq_file *m, void *v)
{
	/* Nothing to do */
}

static void *xpin_exceptions_seq_next(struct seq_file *m, void *v, loff_t *ppos)
{
	struct xpin_exceptions_list *elist = m->private;

	return seq_list_next(v, &elist->head, ppos);
}

static int xpin_exceptions_seq_show(struct seq_file *m, void *v)
{
	struct list_head *iter = v;
	struct xpin_exception_info *info;

	info = list_entry(iter, typeof(*info), node);
	if (info->lazy)
		seq_printf(m, XPIN_TOKEN_LAZY);
	if (info->flags & XPIN_EXCEPTION_DENY)
		seq_printf(m, XPIN_TOKEN_DENY);
	if (info->flags & XPIN_EXCEPTION_JIT_ONLY)
		seq_printf(m, XPIN_TOKEN_JIT);
	if (info->flags & XPIN_EXCEPTION_UNINHERIT)
		seq_printf(m, XPIN_TOKEN_UNINHERIT);
	if (info->flags & XPIN_EXCEPTION_INHERIT)
		seq_printf(m, XPIN_TOKEN_INHERIT);
	if (info->flags & XPIN_EXCEPTION_NON_ELF)
		seq_printf(m, XPIN_TOKEN_NON_ELF);
	if (info->flags & XPIN_EXCEPTION_QUIET)
		seq_printf(m, XPIN_TOKEN_QUIET);
	if (info->flags & XPIN_EXCEPTION_SECURE)
		seq_printf(m, XPIN_TOKEN_SECURE);

	seq_printf(m, "%s\n", info->path[0] ? info->path : "<empty>");
	return 0;
}


static const struct seq_operations xpin_exceptions_seq_ops = {
	.start = xpin_exceptions_seq_start,
	.stop = xpin_exceptions_seq_stop,
	.next = xpin_exceptions_seq_next,
	.show = xpin_exceptions_seq_show,
};


static int xpin_exceptions_open(struct inode *inode, struct file *file)
{
	struct xpin_exceptions_list *elist;

	if (!capable(CAP_MAC_ADMIN) || xpin_lockdown)
		return -EPERM;

	elist = __seq_open_private(
		file, &xpin_exceptions_seq_ops, sizeof(*elist)
	);
	if (!elist)
		return -ENOMEM;

	return xpin_collect_all_exceptions(elist, GFP_KERNEL_ACCOUNT);
}


static int xpin_exceptions_release(struct inode *inode, struct file *file)
{
	struct seq_file *m = file->private_data;
	struct xpin_exceptions_list *elist;

	if (PARANOID(!m))
		return 0;

	elist = m->private;
	m->private = NULL;
	if (!elist)
		return 0;

	xpin_clear_exceptions_list(elist);
	kfree(elist);
	return seq_release(inode, file);
}


static const struct file_operations xpin_exceptions_ops = {
	.open = xpin_exceptions_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = xpin_exceptions_release,
};


int __init xpin_init_securityfs(void)
{
	int err;
	struct dentry *xpin_dir = NULL;
	struct dentry *xpin_enforce_file = NULL;
	struct dentry *xpin_logging_file = NULL;
	struct dentry *xpin_control_file = NULL;
	struct dentry *xpin_exceptions_file = NULL;
	struct dentry *xpin_lockdown_file = NULL;

	xpin_dir = securityfs_create_dir("xpin", NULL);
	if (IS_ERR(xpin_dir)) {
		err = PTR_ERR(xpin_dir);
		goto fail;
	}

	xpin_enforce_file = securityfs_create_file(
		"enforce", 0644, xpin_dir, NULL, &xpin_enforce_ops
	);
	if (IS_ERR(xpin_enforce_file)) {
		err = PTR_ERR(xpin_enforce_file);
		goto fail;
	}

	xpin_logging_file = securityfs_create_file(
		"logging", 0644, xpin_dir, NULL, &xpin_logging_ops
	);
	if (IS_ERR(xpin_logging_file)) {
		err = PTR_ERR(xpin_logging_file);
		goto fail;
	}

	xpin_control_file = securityfs_create_file(
		"control", 0666, xpin_dir, NULL, &xpin_control_ops
	);
	if (IS_ERR(xpin_control_file)) {
		err = PTR_ERR(xpin_control_file);
		goto fail;
	}

	xpin_exceptions_file = securityfs_create_file(
		"exceptions", 0400, xpin_dir, NULL, &xpin_exceptions_ops
	);
	if (IS_ERR(xpin_exceptions_file)) {
		err = PTR_ERR(xpin_exceptions_file);
		goto fail;
	}

	xpin_lockdown_file = securityfs_create_file(
		"lockdown", 0644, xpin_dir, NULL, &xpin_lockdown_ops
	);
	if (IS_ERR(xpin_lockdown_file)) {
		err = PTR_ERR(xpin_exceptions_file);
		goto fail;
	}

	pr_info("securityfs is ready\n");
	return 0;

fail:
	pr_crit("failed to create securityfs tree for XPin! err=%d\n", err);
	securityfs_remove(xpin_lockdown_file);
	securityfs_remove(xpin_exceptions_file);
	securityfs_remove(xpin_control_file);
	securityfs_remove(xpin_logging_file);
	securityfs_remove(xpin_enforce_file);
	securityfs_remove(xpin_dir);
	return err;
}
