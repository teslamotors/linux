/*
 * AppArmor security module
 *
 * This file contains AppArmor LSM hooks.
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2010 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#include <linux/security.h>
#include <linux/moduleparam.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/ptrace.h>
#include <linux/ctype.h>
#include <linux/sysctl.h>
#include <linux/audit.h>
#include <linux/user_namespace.h>
#include <net/sock.h>

#include "include/af_unix.h"
#include "include/apparmor.h"
#include "include/apparmorfs.h"
#include "include/audit.h"
#include "include/capability.h"
#include "include/context.h"
#include "include/file.h"
#include "include/ipc.h"
#include "include/net.h"
#include "include/path.h"
#include "include/policy.h"
#include "include/procattr.h"
#include "include/mount.h"

/* Flag indicating whether initialization completed */
int apparmor_initialized __initdata;

DEFINE_PER_CPU(struct aa_buffers, aa_buffers);
DEFINE_LOCAL_IRQ_LOCK(aa_buffers_lock);

/*
 * LSM hook functions
 */

/*
 * free the associated aa_task_cxt and put its labels
 */
static void apparmor_cred_free(struct cred *cred)
{
	aa_free_task_context(cred_cxt(cred));
	cred_cxt(cred) = NULL;
}

/*
 * allocate the apparmor part of blank credentials
 */
static int apparmor_cred_alloc_blank(struct cred *cred, gfp_t gfp)
{
	/* freed by apparmor_cred_free */
	struct aa_task_cxt *cxt = aa_alloc_task_context(gfp);
	if (!cxt)
		return -ENOMEM;

	cred_cxt(cred) = cxt;
	return 0;
}

/*
 * prepare new aa_task_cxt for modification by prepare_cred block
 */
static int apparmor_cred_prepare(struct cred *new, const struct cred *old,
				 gfp_t gfp)
{
	/* freed by apparmor_cred_free */
	struct aa_task_cxt *cxt = aa_alloc_task_context(gfp);
	if (!cxt)
		return -ENOMEM;

	aa_dup_task_context(cxt, cred_cxt(old));
	cred_cxt(new) = cxt;
	return 0;
}

/*
 * transfer the apparmor data to a blank set of creds
 */
static void apparmor_cred_transfer(struct cred *new, const struct cred *old)
{
	const struct aa_task_cxt *old_cxt = cred_cxt(old);
	struct aa_task_cxt *new_cxt = cred_cxt(new);

	aa_dup_task_context(new_cxt, old_cxt);
}

static int apparmor_ptrace_access_check(struct task_struct *child,
					unsigned int mode)
{
	struct aa_label *tracer, *tracee;
	int error = cap_ptrace_access_check(child, mode);
	if (error)
		return error;

	tracer = aa_current_label();
	tracee = aa_get_task_label(child);
	error = aa_may_ptrace(tracer, tracee,
		  mode == PTRACE_MODE_READ ? AA_PTRACE_READ : AA_PTRACE_TRACE);
	aa_put_label(tracee);
	return error;
}

static int apparmor_ptrace_traceme(struct task_struct *parent)
{
	struct aa_label *tracer, *tracee;
	int error = cap_ptrace_traceme(parent);
	if (error)
		return error;

	tracee = aa_current_label();
	tracer = aa_get_task_label(parent);
	error = aa_may_ptrace(tracer, tracee, AA_PTRACE_TRACE);
	aa_put_label(tracer);
	return error;
}

/* Derived from security/commoncap.c:cap_capget */
static int apparmor_capget(struct task_struct *target, kernel_cap_t *effective,
			   kernel_cap_t *inheritable, kernel_cap_t *permitted)
{
	struct aa_label *label;
	const struct cred *cred;

	rcu_read_lock();
	cred = __task_cred(target);
	label = aa_get_newest_cred_label(cred);

	*effective = cred->cap_effective;
	*inheritable = cred->cap_inheritable;
	*permitted = cred->cap_permitted;

	if (!unconfined(label)) {
		struct aa_profile *profile;
		struct label_it i;
		label_for_each_confined(i, label, profile) {
			if (COMPLAIN_MODE(profile))
				continue;
			*effective = cap_intersect(*effective,
						   profile->caps.allow);
			*permitted = cap_intersect(*permitted,
						   profile->caps.allow);
		}
	}
	rcu_read_unlock();
	aa_put_label(label);

	return 0;
}

static int apparmor_capable(const struct cred *cred, struct user_namespace *ns,
			    int cap, int audit)
{
	struct aa_label *label;
	/* cap_capable returns 0 on success, else -EPERM */
	int error = cap_capable(cred, ns, cap, audit);
	if (error)
		return error;

	label = aa_get_newest_cred_label(cred);
	if (!unconfined(label))
		error = aa_capable(label, cap, audit);
	aa_put_label(label);

	return error;
}

/**
 * common_perm - basic common permission check wrapper fn for paths
 * @op: operation being checked
 * @path: path to check permission of  (NOT NULL)
 * @mask: requested permissions mask
 * @cond: conditional info for the permission request  (NOT NULL)
 *
 * Returns: %0 else error code if error or permission denied
 */
static int common_perm(int op, struct path *path, u32 mask,
		       struct path_cond *cond)
{
	struct aa_label *label;
	int error = 0;

	label = aa_begin_current_label();
	if (!unconfined(label))
		error = aa_path_perm(op, label, path, 0, mask, cond);
	aa_end_current_label(label);

	return error;
}

static int common_perm_cond(int op, struct path *path, u32 mask)
{
	struct path_cond cond = { path->dentry->d_inode->i_uid,
				  path->dentry->d_inode->i_mode
	};

	return common_perm(op, path, mask, &cond);
}

static void apparmor_inode_free_security(struct inode *inode)
{
	struct aa_label *cxt = inode_cxt(inode);

	if (cxt) {
		inode_cxt(inode) = NULL;
		aa_put_label(cxt);
	}
}

/**
 * common_perm_dir_dentry - common permission wrapper when path is dir, dentry
 * @op: operation being checked
 * @dir: directory of the dentry  (NOT NULL)
 * @dentry: dentry to check  (NOT NULL)
 * @mask: requested permissions mask
 * @cond: conditional info for the permission request  (NOT NULL)
 *
 * Returns: %0 else error code if error or permission denied
 */
static int common_perm_dir_dentry(int op, struct path *dir,
				  struct dentry *dentry, u32 mask,
				  struct path_cond *cond)
{
	struct path path = { dir->mnt, dentry };

	return common_perm(op, &path, mask, cond);
}

/**
 * common_perm_mnt_dentry - common permission wrapper when mnt, dentry
 * @op: operation being checked
 * @mnt: mount point of dentry (NOT NULL)
 * @dentry: dentry to check  (NOT NULL)
 * @mask: requested permissions mask
 *
 * Returns: %0 else error code if error or permission denied
 */
static int common_perm_mnt_dentry(int op, struct vfsmount *mnt,
				  struct dentry *dentry, u32 mask)
{
	struct path path = { mnt, dentry };

	return common_perm_cond(op, &path, mask);
}

/**
 * common_perm_rm - common permission wrapper for operations doing rm
 * @op: operation being checked
 * @dir: directory that the dentry is in  (NOT NULL)
 * @dentry: dentry being rm'd  (NOT NULL)
 * @mask: requested permission mask
 *
 * Returns: %0 else error code if error or permission denied
 */
static int common_perm_rm(int op, struct path *dir,
			  struct dentry *dentry, u32 mask)
{
	struct inode *inode = dentry->d_inode;
	struct path_cond cond = { };

	if (!inode || !dir->mnt || !path_mediated_fs(dentry))
		return 0;

	cond.uid = inode->i_uid;
	cond.mode = inode->i_mode;

	return common_perm_dir_dentry(op, dir, dentry, mask, &cond);
}

/**
 * common_perm_create - common permission wrapper for operations doing create
 * @op: operation being checked
 * @dir: directory that dentry will be created in  (NOT NULL)
 * @dentry: dentry to create   (NOT NULL)
 * @mask: request permission mask
 * @mode: created file mode
 *
 * Returns: %0 else error code if error or permission denied
 */
static int common_perm_create(int op, struct path *dir, struct dentry *dentry,
			      u32 mask, umode_t mode)
{
	struct path_cond cond = { current_fsuid(), mode };

	if (!dir->mnt || !path_mediated_fs(dir->dentry))
		return 0;

	return common_perm_dir_dentry(op, dir, dentry, mask, &cond);
}

static int apparmor_path_unlink(struct path *dir, struct dentry *dentry)
{
	return common_perm_rm(OP_UNLINK, dir, dentry, AA_MAY_DELETE);
}

static int apparmor_path_mkdir(struct path *dir, struct dentry *dentry,
			       umode_t mode)
{
	return common_perm_create(OP_MKDIR, dir, dentry, AA_MAY_CREATE,
				  S_IFDIR);
}

static int apparmor_path_rmdir(struct path *dir, struct dentry *dentry)
{
	return common_perm_rm(OP_RMDIR, dir, dentry, AA_MAY_DELETE);
}

static int apparmor_path_mknod(struct path *dir, struct dentry *dentry,
			       umode_t mode, unsigned int dev)
{
	return common_perm_create(OP_MKNOD, dir, dentry, AA_MAY_CREATE, mode);
}

static int apparmor_path_truncate(struct path *path)
{
	if (!path->mnt || !path_mediated_fs(path->dentry))
		return 0;

	return common_perm_cond(OP_TRUNC, path, MAY_WRITE | AA_MAY_SETATTR);
}

static int apparmor_path_symlink(struct path *dir, struct dentry *dentry,
				 const char *old_name)
{
	return common_perm_create(OP_SYMLINK, dir, dentry, AA_MAY_CREATE,
				  S_IFLNK);
}

static int apparmor_path_link(struct dentry *old_dentry, struct path *new_dir,
			      struct dentry *new_dentry)
{
	struct aa_label *label;
	int error = 0;

	if (!path_mediated_fs(old_dentry))
		return 0;

	label = aa_current_label();
	if (!unconfined(label))
		error = aa_path_link(label, old_dentry, new_dir, new_dentry);
	return error;
}

static int apparmor_path_rename(struct path *old_dir, struct dentry *old_dentry,
				struct path *new_dir, struct dentry *new_dentry)
{
	struct aa_label *label;
	int error = 0;

	if (!path_mediated_fs(old_dentry))
		return 0;

	label = aa_current_label();
	if (!unconfined(label)) {
		struct path old_path = { old_dir->mnt, old_dentry };
		struct path new_path = { new_dir->mnt, new_dentry };
		struct path_cond cond = { old_dentry->d_inode->i_uid,
					  old_dentry->d_inode->i_mode
		};

		error = aa_path_perm(OP_RENAME_SRC, label, &old_path, 0,
				     MAY_READ | AA_MAY_GETATTR | MAY_WRITE |
				     AA_MAY_SETATTR | AA_MAY_DELETE,
				     &cond);
		if (!error)
			error = aa_path_perm(OP_RENAME_DEST, label, &new_path,
					     0, MAY_WRITE | AA_MAY_SETATTR |
					     AA_MAY_CREATE, &cond);

	}
	return error;
}

static int apparmor_path_chmod(struct path *path, umode_t mode)
{
	if (!path_mediated_fs(path->dentry))
		return 0;

	return common_perm_cond(OP_CHMOD, path, AA_MAY_CHMOD);
}

static int apparmor_path_chown(struct path *path, kuid_t uid, kgid_t gid)
{
	if (!path_mediated_fs(path->dentry))
		return 0;

	return common_perm_cond(OP_CHOWN, path, AA_MAY_CHOWN);
}

static int apparmor_inode_getattr(struct vfsmount *mnt, struct dentry *dentry)
{
	if (!path_mediated_fs(dentry))
		return 0;

	return common_perm_mnt_dentry(OP_GETATTR, mnt, dentry,
				      AA_MAY_GETATTR);
}

static int apparmor_file_open(struct file *file, const struct cred *cred)
{
	struct aa_file_cxt *fcxt = file_cxt(file);
	struct aa_label *label;
	int error = 0;

	if (!path_mediated_fs(file->f_path.dentry))
		return 0;

	/* If in exec, permission is handled by bprm hooks.
	 * Cache permissions granted by the previous exec check, with
	 * implicit read and executable mmap which are required to
	 * actually execute the image.
	 */
	if (current->in_execve) {
		fcxt->allow = MAY_EXEC | MAY_READ | AA_EXEC_MMAP;
		return 0;
	}

	label = aa_get_newest_cred_label(cred);
	if (!unconfined(label)) {
		struct inode *inode = file_inode(file);
		struct path_cond cond = { inode->i_uid, inode->i_mode };

		error = aa_path_perm(OP_OPEN, label, &file->f_path, 0,
				     aa_map_file_to_perms(file), &cond);
		/* todo cache full allowed permissions set and state */
		fcxt->allow = aa_map_file_to_perms(file);
	}
	aa_put_label(label);

	return error;
}

static int apparmor_file_alloc_security(struct file *file)
{
	/* freed by apparmor_file_free_security */
	file->f_security = aa_alloc_file_cxt(aa_current_label(), GFP_KERNEL);
	if (!file_cxt(file))
		return -ENOMEM;
	return 0;

}

static void apparmor_file_free_security(struct file *file)
{
	aa_free_file_cxt(file_cxt(file));
}

static int common_file_perm(int op, struct file *file, u32 mask)
{
	struct aa_label *label;
	int error = 0;

	label = aa_begin_current_label();
	error = aa_file_perm(op, label, file, mask);
	aa_end_current_label(label);

	return error;
}

static int apparmor_file_receive(struct file *file)
{
	return common_file_perm(OP_FRECEIVE, file, aa_map_file_to_perms(file));
}

static int apparmor_file_permission(struct file *file, int mask)
{
	return common_file_perm(OP_FPERM, file, mask);
}

static int apparmor_file_lock(struct file *file, unsigned int cmd)
{
	u32 mask = AA_MAY_LOCK;

	if (cmd == F_WRLCK)
		mask |= MAY_WRITE;

	return common_file_perm(OP_FLOCK, file, mask);
}

static int common_mmap(int op, struct file *file, unsigned long prot,
		       unsigned long flags)
{
	int mask = 0;

	if (!file || !file_cxt(file))
		return 0;

	if (prot & PROT_READ)
		mask |= MAY_READ;
	/*
	 * Private mappings don't require write perms since they don't
	 * write back to the files
	 */
	if ((prot & PROT_WRITE) && !(flags & MAP_PRIVATE))
		mask |= MAY_WRITE;
	if (prot & PROT_EXEC)
		mask |= AA_EXEC_MMAP;

	return common_file_perm(op, file, mask);
}

static int apparmor_mmap_file(struct file *file, unsigned long reqprot,
			      unsigned long prot, unsigned long flags)
{
	return common_mmap(OP_FMMAP, file, prot, flags);
}

static int apparmor_file_mprotect(struct vm_area_struct *vma,
				  unsigned long reqprot, unsigned long prot)
{
	return common_mmap(OP_FMPROT, vma->vm_file, prot,
			   !(vma->vm_flags & VM_SHARED) ? MAP_PRIVATE : 0);
}

static int apparmor_sb_mount(const char *dev_name, struct path *path,
			     const char *type, unsigned long flags, void *data)
{
	struct aa_label *label;
	int error = 0;

	/* Discard magic */
	if ((flags & MS_MGC_MSK) == MS_MGC_VAL)
		flags &= ~MS_MGC_MSK;

	flags &= ~AA_MS_IGNORE_MASK;

	label = aa_begin_current_label();
	if (!unconfined(label)) {
		if (flags & MS_REMOUNT)
			error = aa_remount(label, path, flags, data);
		else if (flags & MS_BIND)
			error = aa_bind_mount(label, path, dev_name, flags);
		else if (flags & (MS_SHARED | MS_PRIVATE | MS_SLAVE |
				  MS_UNBINDABLE))
			error = aa_mount_change_type(label, path, flags);
		else if (flags & MS_MOVE)
			error = aa_move_mount(label, path, dev_name);
		else
			error = aa_new_mount(label, dev_name, path, type,
					     flags, data);
	}
	aa_end_current_label(label);

	return error;
}

static int apparmor_sb_umount(struct vfsmount *mnt, int flags)
{
	struct aa_label *label;
	int error = 0;

	label = aa_begin_current_label();
	if (!unconfined(label))
		error = aa_umount(label, mnt, flags);
	aa_end_current_label(label);

	return error;
}

static int apparmor_sb_pivotroot(struct path *old_path, struct path *new_path)
{
	struct aa_label *label;
	int error = 0;

	label = aa_begin_current_label();
	if (!unconfined(label))
		error = aa_pivotroot(label, old_path, new_path);
	aa_end_current_label(label);

	return error;
}

static int apparmor_getprocattr(struct task_struct *task, char *name,
				char **value)
{
	int error = -ENOENT;
	/* released below */
	const struct cred *cred = get_task_cred(task);
	struct aa_task_cxt *cxt = cred_cxt(cred);
	struct aa_label *label = NULL;

	if (strcmp(name, "current") == 0)
		label = aa_get_newest_label(cxt->label);
	else if (strcmp(name, "prev") == 0  && cxt->previous)
		label = aa_get_newest_label(cxt->previous);
	else if (strcmp(name, "exec") == 0 && cxt->onexec)
		label = aa_get_newest_label(cxt->onexec);
	else
		error = -EINVAL;

	if (label)
		error = aa_getprocattr(label, value);

	aa_put_label(label);
	put_cred(cred);

	return error;
}

static int apparmor_setprocattr(struct task_struct *task, char *name,
				void *value, size_t size)
{
	char *command, *args = value;
	size_t arg_size;
	int error;
	DEFINE_AUDIT_DATA(sa, LSM_AUDIT_DATA_NONE, OP_SETPROCATTR);

	if (size == 0)
		return -EINVAL;
	/* args points to a PAGE_SIZE buffer, AppArmor requires that
	 * the buffer must be null terminated or have size <= PAGE_SIZE -1
	 * so that AppArmor can null terminate them
	 */
	if (args[size - 1] != '\0') {
		if (size == PAGE_SIZE)
			return -EINVAL;
		args[size] = '\0';
	}

	/* task can only write its own attributes */
	if (current != task)
		return -EACCES;

	args = value;
	args = strim(args);
	command = strsep(&args, " ");
	if (!args)
		return -EINVAL;
	args = skip_spaces(args);
	if (!*args)
		return -EINVAL;

	arg_size = size - (args - (char *) value);
	if (strcmp(name, "current") == 0) {
		if (strcmp(command, "changehat") == 0) {
			error = aa_setprocattr_changehat(args, arg_size,
							 !AA_DO_TEST);
		} else if (strcmp(command, "permhat") == 0) {
			error = aa_setprocattr_changehat(args, arg_size,
							 AA_DO_TEST);
		} else if (strcmp(command, "changeprofile") == 0) {
			error = aa_setprocattr_changeprofile(args, !AA_ONEXEC,
							     !AA_DO_TEST);
		} else if (strcmp(command, "permprofile") == 0) {
			error = aa_setprocattr_changeprofile(args, !AA_ONEXEC,
							     AA_DO_TEST);
		} else
			goto fail;
	} else if (strcmp(name, "exec") == 0) {
		if (strcmp(command, "exec") == 0)
			error = aa_setprocattr_changeprofile(args, AA_ONEXEC,
							     !AA_DO_TEST);
		else
			goto fail;
	} else
		/* only support the "current" and "exec" process attributes */
		return -EINVAL;

	if (!error)
		error = size;
	return error;

fail:
	aad(&sa)->label = aa_current_label();
	aad(&sa)->info = name;
	aad(&sa)->error = -EINVAL;
	aa_audit_msg(AUDIT_APPARMOR_DENIED, &sa, NULL);
	return -EINVAL;
}

/**
 * apparmor_bprm_committing_creds - do task cleanup on committing new creds
 * @bprm: binprm for the exec  (NOT NULL)
 */
void apparmor_bprm_committing_creds(struct linux_binprm *bprm)
{
	struct aa_label *label = aa_current_raw_label();
	struct aa_task_cxt *new_cxt = cred_cxt(bprm->cred);

	/* bail out if unconfined or not changing profile */
	if ((new_cxt->label->replacedby == label->replacedby) ||
	    (unconfined(new_cxt->label)))
		return;

	aa_inherit_files(bprm->cred, current->files);

	current->pdeath_signal = 0;

	/* reset soft limits and set hard limits for the new label */
	__aa_transition_rlimits(label, new_cxt->label);
}

/**
 * apparmor_bprm_commited_cred - do cleanup after new creds committed
 * @bprm: binprm for the exec  (NOT NULL)
 */
void apparmor_bprm_committed_creds(struct linux_binprm *bprm)
{
	/* TODO: cleanup signals - ipc mediation */
	return;
}

static int apparmor_task_setrlimit(struct task_struct *task,
		unsigned int resource, struct rlimit *new_rlim)
{
	struct aa_label *label = aa_begin_current_label();
	int error = 0;

	if (!unconfined(label))
		error = aa_task_setrlimit(label, task, resource, new_rlim);
	aa_end_current_label(label);

	return error;
}

/**
 * apparmor_sk_alloc_security - allocate and attach the sk_security field
 */
static int apparmor_sk_alloc_security(struct sock *sk, int family, gfp_t flags)
{
	struct aa_sk_cxt *cxt;

	cxt = kzalloc(sizeof(*cxt), flags);
	if (!cxt)
		return -ENOMEM;

	SK_CXT(sk) = cxt;
	//??? set local too current???

	return 0;
}

/**
 * apparmor_sk_free_security - free the sk_security field
 */
static void apparmor_sk_free_security(struct sock *sk)
{
	struct aa_sk_cxt *cxt = SK_CXT(sk);

	SK_CXT(sk) = NULL;
	aa_put_label(cxt->label);
	aa_put_label(cxt->peer);
	kfree(cxt);
}

/**
 * apparmor_clone_security - clone the sk_security field
 */
static void apparmor_sk_clone_security(const struct sock *sk,
				       struct sock *newsk)
{
	struct aa_sk_cxt *cxt = SK_CXT(sk);
	struct aa_sk_cxt *new = SK_CXT(newsk);

	new->label = aa_get_label(cxt->label);
	new->peer = aa_get_label(cxt->peer);
}

/**
 * apparmor_unix_stream_connect - check perms before making unix domain conn
 *
 * peer is locked when this hook is called
 */
static int apparmor_unix_stream_connect(struct sock *sk, struct sock *peer_sk,
					struct sock *newsk)
{
	struct aa_sk_cxt *sk_cxt = SK_CXT(sk);
	struct aa_sk_cxt *peer_cxt = SK_CXT(peer_sk);
	struct aa_sk_cxt *new_cxt = SK_CXT(newsk);
	struct aa_label *label;
	int error;

	label = aa_begin_current_label();
	error = aa_unix_peer_perm(label, OP_CONNECT,
				(AA_MAY_CONNECT | AA_MAY_SEND | AA_MAY_RECEIVE),
				  sk, peer_sk, NULL);
	if (!UNIX_FS(peer_sk)) {
		last_error(error,
			aa_unix_peer_perm(peer_cxt->label, OP_CONNECT,
				(AA_MAY_ACCEPT | AA_MAY_SEND | AA_MAY_RECEIVE),
				peer_sk, sk, label));
	}
	aa_end_current_label(label);

	if (error)
		return error;

	/* label newsk if it wasn't labeled in post_create. Normally this
	 * would be done in sock_graft, but because we are directly looking
	 * at the peer_sk to obtain peer_labeling for unix socks this
	 * does not work
	 */
	if (!new_cxt->label)
		new_cxt->label = aa_get_label(peer_cxt->label);

	/* Cross reference the peer labels for SO_PEERSEC */
	if (new_cxt->peer)
		aa_put_label(new_cxt->peer);

	if (sk_cxt->peer)
		aa_put_label(sk_cxt->peer);

	new_cxt->peer = aa_get_label(sk_cxt->label);
	sk_cxt->peer = aa_get_label(peer_cxt->label);

	return 0;
}

/**
 * apparmor_unix_may_send - check perms before conn or sending unix dgrams
 *
 * other is locked when this hook is called
 *
 * dgram connect calls may_send, peer setup but path not copied?????
 */
static int apparmor_unix_may_send(struct socket *sock, struct socket *peer)
{
	struct aa_sk_cxt *peer_cxt = SK_CXT(peer->sk);
	struct aa_label *label = aa_begin_current_label();
	int error;

	error = xcheck(aa_unix_peer_perm(label, OP_SENDMSG, AA_MAY_SEND,
					 sock->sk, peer->sk, NULL),
		       aa_unix_peer_perm(peer_cxt->label, OP_SENDMSG, AA_MAY_RECEIVE,
					 peer->sk, sock->sk, label));
	aa_end_current_label(label);

	return error;
}

/**
 * apparmor_socket_create - check perms before creating a new socket
 */
static int apparmor_socket_create(int family, int type, int protocol, int kern)
{
	struct aa_label *label;

	label = aa_current_label();
	if (kern || unconfined(label))
		return 0;

	return aa_sock_create_perm(label, family, type, protocol);
}

/**
 * apparmor_socket_post_create - setup the per-socket security struct
 *
 * Note:
 * -   kernel sockets currently labeled unconfined but we may want to
 *     move to a special kernel label
 * -   socket may not have sk here if created with sock_create_lite or
 *     sock_alloc. These should be accept cases which will be handled in
 *     sock_graft.
 */
static int apparmor_socket_post_create(struct socket *sock, int family,
				       int type, int protocol, int kern)
{
	struct aa_label *label;

	if (kern)
		label = aa_get_label(&current_ns()->unconfined->label);
	else
		label = aa_get_label(aa_current_label());

	if (sock->sk) {
		struct aa_sk_cxt *cxt = SK_CXT(sock->sk);
		aa_put_label(cxt->label);
		cxt->label = aa_get_label(label);
	}
	aa_put_label(label);

	return 0;
}

/**
 * apparmor_socket_bind - check perms before bind addr to socket
 */
static int apparmor_socket_bind(struct socket *sock,
				struct sockaddr *address, int addrlen)
{
	return aa_sock_bind_perm(sock, address, addrlen);
}

/**
 * apparmor_socket_connect - check perms before connecting @sock to @address
 */
static int apparmor_socket_connect(struct socket *sock,
				   struct sockaddr *address, int addrlen)
{
	return aa_sock_connect_perm(sock, address, addrlen);
}

/**
 * apparmor_socket_list - check perms before allowing listen
 */
static int apparmor_socket_listen(struct socket *sock, int backlog)
{
	return aa_sock_listen_perm(sock, backlog);
}

/**
 * apparmor_socket_accept - check perms before accepting a new connection.
 *
 * Note: while @newsock is created and has some information, the accept
 *       has not been done.
 */
static int apparmor_socket_accept(struct socket *sock, struct socket *newsock)
{
	return aa_sock_accept_perm(sock, newsock);
}

/**
 * apparmor_socket_sendmsg - check perms before sending msg to another socket
 */
static int apparmor_socket_sendmsg(struct socket *sock,
				   struct msghdr *msg, int size)
{
	int error = aa_sock_msg_perm(OP_SENDMSG, AA_MAY_SEND, sock, msg, size);
	if (!error) {
		/* TODO: setup delegation on scm rights
		   see smack for AF_INET, AF_INET6 */
		;
	}

	return error;
}

/**
 * apparmor_socket_recvmsg - check perms before receiving a message
 */
static int apparmor_socket_recvmsg(struct socket *sock,
				   struct msghdr *msg, int size, int flags)
{
	return aa_sock_msg_perm(OP_RECVMSG, AA_MAY_RECEIVE, sock, msg, size);
}

/**
 * apparmor_socket_getsockname - check perms before getting the local address
 */
static int apparmor_socket_getsockname(struct socket *sock)
{
	return aa_sock_perm(OP_GETSOCKNAME, AA_MAY_GETATTR, sock);
}

/**
 * apparmor_socket_getpeername - check perms before getting remote address
 */
static int apparmor_socket_getpeername(struct socket *sock)
{
	return aa_sock_perm(OP_GETPEERNAME, AA_MAY_GETATTR, sock);
}

/**
 * apparmor_getsockopt - check perms before getting socket options
 */
static int apparmor_socket_getsockopt(struct socket *sock, int level,
				      int optname)
{
	return aa_sock_opt_perm(OP_GETSOCKOPT, AA_MAY_GETOPT, sock,
				level, optname);
}

/**
 * apparmor_setsockopt - check perms before setting socket options
 */
static int apparmor_socket_setsockopt(struct socket *sock, int level,
				      int optname)
{
	return aa_sock_opt_perm(OP_SETSOCKOPT, AA_MAY_SETOPT, sock,
				level, optname);
}

/**
 * apparmor_socket_shutdown - check perms before shutting down @sock conn
 */
static int apparmor_socket_shutdown(struct socket *sock, int how)
{
	return aa_sock_perm(OP_SHUTDOWN, AA_MAY_SHUTDOWN, sock);
}

/**
 * apparmor_socket_sock_recv_skb - check perms before associating skb to sk
 *
 * Note: can not sleep maybe called with locks held

dont want protocol specific in __skb_recv_datagram()
to deny an incoming connection  socket_sock_rcv_skb()

 */
static int apparmor_socket_sock_rcv_skb(struct sock *sk, struct sk_buff *skb)
{
	/* TODO: */
	return 0;
}


static struct aa_label *sk_peer_label(struct sock *sk)
{
	struct sock *peer_sk;
	struct aa_sk_cxt *cxt = SK_CXT(sk);

	if (cxt->peer)
		return cxt->peer;

	if (sk->sk_family != PF_UNIX)
		return ERR_PTR(-ENOPROTOOPT);

	/* check for sockpair peering which does not go through
	 * security_unix_stream_connect
	 */
	peer_sk = unix_peer(sk);
	if (peer_sk) {
		cxt = SK_CXT(peer_sk);
		if (cxt->label)
			return cxt->label;
	}

	return ERR_PTR(-ENOPROTOOPT);
}

/**
 * apparmor_socket_getpeersec_stream - get security context of peer
 *
 * Note: for tcp only valid if using ipsec or cipso on lan
 */
static int apparmor_socket_getpeersec_stream(struct socket *sock,
					     char __user *optval,
					     int __user *optlen, unsigned len)
{
	char *name;
	int slen, error = 0;
	struct aa_label *label = aa_current_label();
	struct aa_label *peer = sk_peer_label(sock->sk);

	if (IS_ERR(peer))
		return PTR_ERR(peer);

	slen = aa_label_asprint(&name, labels_ns(label), peer, true, GFP_KERNEL);
	/* don't include terminating \0 in slen, it breaks some apps */
	if (slen < 0) {
		error = -ENOMEM;
	} else {
		if (slen > len) {
			error = -ERANGE;
		} else if (copy_to_user(optval, name, slen)) {
			error = -EFAULT;
			goto out;
		}
		if (put_user(slen, optlen))
			error = -EFAULT;
	out:
		kfree(name);

	}

	return error;
}

/**
 * apparmor_socket_getpeersec_dgram - get security label of packet
 * @sock: the peer socket
 * @skb: packet data
 * @secid: pointer to where to put the secid of the packet
 *
 * Sets the netlabel socket state on sk from parent
 */
static int apparmor_socket_getpeersec_dgram(struct socket *sock,
					    struct sk_buff *skb, u32 *secid)

{
	/* TODO: requires secid support, and netlabel */
	return -ENOPROTOOPT;
}

/**
 * apparmor_sock_graft - Initialize newly created socket
 * @sk: child sock
 * @parent: parent socket
 *
 * Note: could set off of SOCK_CXT(parent) but need to track inode and we can
 *       just set sk security information off of current creating process label
 *       Labeling of sk for accept case - probably should be sock based
 *       instead of task, because of the case where an implicitly labeled
 *       socket is shared by different tasks.
 */
static void apparmor_sock_graft(struct sock *sk, struct socket *parent)
{
	struct aa_sk_cxt *cxt = SK_CXT(sk);
	if (!cxt->label)
		cxt->label = aa_get_current_label();
}

static int apparmor_task_kill(struct task_struct *target, struct siginfo *info,
			      int sig, u32 secid)
{
	struct aa_label *cl, *tl;
	int error;

	if (secid)
		/* TODO: after secid to label mapping is done.
		 *  Dealing with USB IO specific behavior
		 */
		return 0;
	cl = aa_begin_current_label();
	tl = aa_get_task_label(target);
	error = aa_may_signal(cl, tl, sig);
	aa_put_label(tl);
	aa_end_current_label(cl);

	return error;
}


static struct security_operations apparmor_ops = {
	.name =				"apparmor",

	.ptrace_access_check =		apparmor_ptrace_access_check,
	.ptrace_traceme =		apparmor_ptrace_traceme,
	.capget =			apparmor_capget,
	.capable =			apparmor_capable,

	.inode_free_security =		apparmor_inode_free_security,

	.sb_mount =			apparmor_sb_mount,
	.sb_umount =			apparmor_sb_umount,
	.sb_pivotroot =			apparmor_sb_pivotroot,

	.path_link =			apparmor_path_link,
	.path_unlink =			apparmor_path_unlink,
	.path_symlink =			apparmor_path_symlink,
	.path_mkdir =			apparmor_path_mkdir,
	.path_rmdir =			apparmor_path_rmdir,
	.path_mknod =			apparmor_path_mknod,
	.path_rename =			apparmor_path_rename,
	.path_chmod =			apparmor_path_chmod,
	.path_chown =			apparmor_path_chown,
	.path_truncate =		apparmor_path_truncate,
	.inode_getattr =                apparmor_inode_getattr,

	.file_open =			apparmor_file_open,
	.file_receive =			apparmor_file_receive,
	.file_permission =		apparmor_file_permission,
	.file_alloc_security =		apparmor_file_alloc_security,
	.file_free_security =		apparmor_file_free_security,
	.mmap_file =			apparmor_mmap_file,
	.mmap_addr =			cap_mmap_addr,
	.file_mprotect =		apparmor_file_mprotect,
	.file_lock =			apparmor_file_lock,

	.getprocattr =			apparmor_getprocattr,
	.setprocattr =			apparmor_setprocattr,

	.sk_alloc_security = 		apparmor_sk_alloc_security,
	.sk_free_security = 		apparmor_sk_free_security,
	.sk_clone_security =		apparmor_sk_clone_security,

	.unix_stream_connect = 		apparmor_unix_stream_connect,
	.unix_may_send = 		apparmor_unix_may_send,

	.socket_create =		apparmor_socket_create,
	.socket_post_create = 		apparmor_socket_post_create,
	.socket_bind =			apparmor_socket_bind,
	.socket_connect =		apparmor_socket_connect,
	.socket_listen =		apparmor_socket_listen,
	.socket_accept =		apparmor_socket_accept,
	.socket_sendmsg =		apparmor_socket_sendmsg,
	.socket_recvmsg =		apparmor_socket_recvmsg,
	.socket_getsockname =		apparmor_socket_getsockname,
	.socket_getpeername =		apparmor_socket_getpeername,
	.socket_getsockopt =		apparmor_socket_getsockopt,
	.socket_setsockopt =		apparmor_socket_setsockopt,
	.socket_shutdown =		apparmor_socket_shutdown,
	.socket_sock_rcv_skb =		apparmor_socket_sock_rcv_skb,
	.socket_getpeersec_stream =	apparmor_socket_getpeersec_stream,
	.socket_getpeersec_dgram =	apparmor_socket_getpeersec_dgram,
	.sock_graft = 			apparmor_sock_graft,

	.cred_alloc_blank =		apparmor_cred_alloc_blank,
	.cred_free =			apparmor_cred_free,
	.cred_prepare =			apparmor_cred_prepare,
	.cred_transfer =		apparmor_cred_transfer,

	.bprm_set_creds =		apparmor_bprm_set_creds,
	.bprm_committing_creds =	apparmor_bprm_committing_creds,
	.bprm_committed_creds =		apparmor_bprm_committed_creds,
	.bprm_secureexec =		apparmor_bprm_secureexec,

	.task_setrlimit =		apparmor_task_setrlimit,
	.task_kill =			apparmor_task_kill,
};

/*
 * AppArmor sysfs module parameters
 */

static int param_set_aabool(const char *val, const struct kernel_param *kp);
static int param_get_aabool(char *buffer, const struct kernel_param *kp);
#define param_check_aabool param_check_bool
static struct kernel_param_ops param_ops_aabool = {
	.flags = KERNEL_PARAM_OPS_FL_NOARG,
	.set = param_set_aabool,
	.get = param_get_aabool
};

static int param_set_aauint(const char *val, const struct kernel_param *kp);
static int param_get_aauint(char *buffer, const struct kernel_param *kp);
#define param_check_aauint param_check_uint
static struct kernel_param_ops param_ops_aauint = {
	.set = param_set_aauint,
	.get = param_get_aauint
};

static int param_set_aalockpolicy(const char *val, const struct kernel_param *kp);
static int param_get_aalockpolicy(char *buffer, const struct kernel_param *kp);
#define param_check_aalockpolicy param_check_bool
static struct kernel_param_ops param_ops_aalockpolicy = {
	.flags = KERNEL_PARAM_OPS_FL_NOARG,
	.set = param_set_aalockpolicy,
	.get = param_get_aalockpolicy
};

static int param_set_audit(const char *val, struct kernel_param *kp);
static int param_get_audit(char *buffer, struct kernel_param *kp);

static int param_set_mode(const char *val, struct kernel_param *kp);
static int param_get_mode(char *buffer, struct kernel_param *kp);

/* Flag values, also controllable via /sys/module/apparmor/parameters
 * We define special types as we want to do additional mediation.
 */

/* AppArmor global enforcement switch - complain, enforce, kill */
enum profile_mode aa_g_profile_mode = APPARMOR_ENFORCE;
module_param_call(mode, param_set_mode, param_get_mode,
		  &aa_g_profile_mode, S_IRUSR | S_IWUSR);

/* whether policy verification hashing is enabled */
bool aa_g_hash_policy = CONFIG_SECURITY_APPARMOR_HASH_DEFAULT;
module_param_named(hash_policy, aa_g_hash_policy, aabool, S_IRUSR | S_IWUSR);

/* Debug mode */
bool aa_g_debug;
module_param_named(debug, aa_g_debug, aabool, S_IRUSR | S_IWUSR);

/* Audit mode */
enum audit_mode aa_g_audit;
module_param_call(audit, param_set_audit, param_get_audit,
		  &aa_g_audit, S_IRUSR | S_IWUSR);

/* Determines if audit header is included in audited messages.  This
 * provides more context if the audit daemon is not running
 */
bool aa_g_audit_header = 1;
module_param_named(audit_header, aa_g_audit_header, aabool,
		   S_IRUSR | S_IWUSR);

/* lock out loading/removal of policy
 * TODO: add in at boot loading of policy, which is the only way to
 *       load policy, if lock_policy is set
 */
bool aa_g_lock_policy;
module_param_named(lock_policy, aa_g_lock_policy, aalockpolicy,
		   S_IRUSR | S_IWUSR);

/* Syscall logging mode */
bool aa_g_logsyscall;
module_param_named(logsyscall, aa_g_logsyscall, aabool, S_IRUSR | S_IWUSR);

/* Maximum pathname length before accesses will start getting rejected */
unsigned int aa_g_path_max = 2 * PATH_MAX;
module_param_named(path_max, aa_g_path_max, aauint, S_IRUSR);

/* Determines how paranoid loading of policy is and how much verification
 * on the loaded policy is done.
 */
bool aa_g_paranoid_load = 1;
module_param_named(paranoid_load, aa_g_paranoid_load, aabool,
		   S_IRUSR | S_IWUSR);

/* Boot time disable flag */
static bool apparmor_enabled = CONFIG_SECURITY_APPARMOR_BOOTPARAM_VALUE;
module_param_named(enabled, apparmor_enabled, bool, S_IRUGO);

/* Boot time to set use of default or unconfined as initial profile */
bool aa_g_unconfined_init = CONFIG_SECURITY_APPARMOR_UNCONFINED_INIT;
module_param_named(unconfined, aa_g_unconfined_init, bool, S_IRUSR);


static int __init apparmor_enabled_setup(char *str)
{
	unsigned long enabled;
	int error = kstrtoul(str, 0, &enabled);
	if (!error)
		apparmor_enabled = enabled ? 1 : 0;
	return 1;
}

__setup("apparmor=", apparmor_enabled_setup);

/* set global flag turning off the ability to load policy */
static int param_set_aalockpolicy(const char *val, const struct kernel_param *kp)
{
	if (!capable(CAP_MAC_ADMIN))
		return -EPERM;
	if (aa_g_lock_policy)
		return -EACCES;
	return param_set_bool(val, kp);
}

static int param_get_aalockpolicy(char *buffer, const struct kernel_param *kp)
{
	if (!capable(CAP_MAC_ADMIN))
		return -EPERM;
	return param_get_bool(buffer, kp);
}

static int param_set_aabool(const char *val, const struct kernel_param *kp)
{
	if (!capable(CAP_MAC_ADMIN))
		return -EPERM;
	return param_set_bool(val, kp);
}

static int param_get_aabool(char *buffer, const struct kernel_param *kp)
{
	if (!capable(CAP_MAC_ADMIN))
		return -EPERM;
	return param_get_bool(buffer, kp);
}

static int param_set_aauint(const char *val, const struct kernel_param *kp)
{
	if (!capable(CAP_MAC_ADMIN))
		return -EPERM;
	return param_set_uint(val, kp);
}

static int param_get_aauint(char *buffer, const struct kernel_param *kp)
{
	if (!capable(CAP_MAC_ADMIN))
		return -EPERM;
	return param_get_uint(buffer, kp);
}

static int param_get_audit(char *buffer, struct kernel_param *kp)
{
	if (!capable(CAP_MAC_ADMIN))
		return -EPERM;

	if (!apparmor_enabled)
		return -EINVAL;

	return sprintf(buffer, "%s", audit_mode_names[aa_g_audit]);
}

static int param_set_audit(const char *val, struct kernel_param *kp)
{
	int i;
	if (!capable(CAP_MAC_ADMIN))
		return -EPERM;

	if (!apparmor_enabled)
		return -EINVAL;

	if (!val)
		return -EINVAL;

	for (i = 0; i < AUDIT_MAX_INDEX; i++) {
		if (strcmp(val, audit_mode_names[i]) == 0) {
			aa_g_audit = i;
			return 0;
		}
	}

	return -EINVAL;
}

static int param_get_mode(char *buffer, struct kernel_param *kp)
{
	if (!capable(CAP_MAC_ADMIN))
		return -EPERM;

	if (!apparmor_enabled)
		return -EINVAL;

	return sprintf(buffer, "%s", aa_profile_mode_names[aa_g_profile_mode]);
}

static int param_set_mode(const char *val, struct kernel_param *kp)
{
	int i;
	if (!capable(CAP_MAC_ADMIN))
		return -EPERM;

	if (!apparmor_enabled)
		return -EINVAL;

	if (!val)
		return -EINVAL;

	for (i = 0; i < APPARMOR_MODE_NAMES_MAX_INDEX; i++) {
		if (strcmp(val, aa_profile_mode_names[i]) == 0) {
			aa_g_profile_mode = i;
			return 0;
		}
	}

	return -EINVAL;
}

/*
 * AppArmor init functions
 */

/**
 * set_init_cxt - set a task context and profile on the first task.
 */
static int __init set_init_cxt(void)
{
	struct cred *cred = (struct cred *)current->real_cred;
	struct aa_task_cxt *cxt;

	cxt = aa_alloc_task_context(GFP_KERNEL);
	if (!cxt)
		return -ENOMEM;

	if (!aa_g_unconfined_init) {
		cxt->label = aa_setup_default_label();
		if (!cxt->label) {
			aa_free_task_context(cxt);
			return -ENOMEM;
		}
		/* fs setup of default is done in aa_create_aafs() */
	} else
		cxt->label = aa_get_label(&root_ns->unconfined->label);
	cred_cxt(cred) = cxt;

	return 0;
}

static void destroy_buffers(void)
{
	u32 i, j;

	for_each_possible_cpu(i) {
		for_each_cpu_buffer(j) {
			kfree(per_cpu(aa_buffers, i).buf[j]);
			per_cpu(aa_buffers, i).buf[j] = NULL;
		}
	}
}

static int __init alloc_buffers(void)
{
	u32 i, j;

	for_each_possible_cpu(i) {
		for_each_cpu_buffer(j) {
			char *buffer;
			if (cpu_to_node(i) > num_online_nodes())
				/* fallback to kmalloc for offline nodes */
				buffer = kmalloc(aa_g_path_max, GFP_KERNEL);
			else
				buffer = kmalloc_node(aa_g_path_max, GFP_KERNEL,
						      cpu_to_node(i));
			if (!buffer) {
				destroy_buffers();
				return -ENOMEM;
			}
			per_cpu(aa_buffers, i).buf[j] = buffer;
		}
	}

	return 0;
}

static int __init apparmor_init(void)
{
	int error;

	if (!apparmor_enabled || !security_module_enable(&apparmor_ops)) {
		aa_info_message("AppArmor disabled by boot time parameter");
		apparmor_enabled = 0;
		return 0;
	}

	error = aa_alloc_root_ns();
	if (error) {
		AA_ERROR("Unable to allocate default profile namespace\n");
		goto alloc_out;
	}

	error = alloc_buffers();
	if (error) {
		AA_ERROR("Unable to allocate work buffers\n");
		goto buffers_out;
	}

	error = set_init_cxt();
	if (error) {
		AA_ERROR("Failed to set context on init task\n");
		goto register_security_out;
	}

	error = register_security(&apparmor_ops);
	if (error) {
		struct cred *cred = (struct cred *)current->real_cred;
		aa_free_task_context(cred_cxt(cred));
		cred_cxt(cred) = NULL;
		AA_ERROR("Unable to register AppArmor\n");
		goto register_security_out;
	}

	/* Report that AppArmor successfully initialized */
	apparmor_initialized = 1;
	if (aa_g_profile_mode == APPARMOR_COMPLAIN)
		aa_info_message("AppArmor initialized: complain mode enabled");
	else if (aa_g_profile_mode == APPARMOR_KILL)
		aa_info_message("AppArmor initialized: kill mode enabled");
	else
		aa_info_message("AppArmor initialized");

	return error;

register_security_out:
	aa_free_root_ns();

buffers_out:
	destroy_buffers();

alloc_out:
	aa_destroy_aafs();

	apparmor_enabled = 0;
	return error;
}

security_initcall(apparmor_init);
