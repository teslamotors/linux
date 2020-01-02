/*
 * AppArmor security module
 *
 * This file contains AppArmor mediation of files
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2012 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/namei.h>

#include "include/apparmor.h"
#include "include/audit.h"
#include "include/context.h"
#include "include/domain.h"
#include "include/file.h"
#include "include/match.h"
#include "include/mount.h"
#include "include/path.h"
#include "include/policy.h"


static void audit_mnt_flags(struct audit_buffer *ab, unsigned long flags)
{
	if (flags & MS_RDONLY)
		audit_log_format(ab, "ro");
	else
		audit_log_format(ab, "rw");
	if (flags & MS_NOSUID)
		audit_log_format(ab, ", nosuid");
	if (flags & MS_NODEV)
		audit_log_format(ab, ", nodev");
	if (flags & MS_NOEXEC)
		audit_log_format(ab, ", noexec");
	if (flags & MS_SYNCHRONOUS)
		audit_log_format(ab, ", sync");
	if (flags & MS_REMOUNT)
		audit_log_format(ab, ", remount");
	if (flags & MS_MANDLOCK)
		audit_log_format(ab, ", mand");
	if (flags & MS_DIRSYNC)
		audit_log_format(ab, ", dirsync");
	if (flags & MS_NOATIME)
		audit_log_format(ab, ", noatime");
	if (flags & MS_NODIRATIME)
		audit_log_format(ab, ", nodiratime");
	if (flags & MS_BIND)
		audit_log_format(ab, flags & MS_REC ? ", rbind" : ", bind");
	if (flags & MS_MOVE)
		audit_log_format(ab, ", move");
	if (flags & MS_SILENT)
		audit_log_format(ab, ", silent");
	if (flags & MS_POSIXACL)
		audit_log_format(ab, ", acl");
	if (flags & MS_UNBINDABLE)
		audit_log_format(ab, flags & MS_REC ? ", runbindable" :
				 ", unbindable");
	if (flags & MS_PRIVATE)
		audit_log_format(ab, flags & MS_REC ? ", rprivate" :
				 ", private");
	if (flags & MS_SLAVE)
		audit_log_format(ab, flags & MS_REC ? ", rslave" :
				 ", slave");
	if (flags & MS_SHARED)
		audit_log_format(ab, flags & MS_REC ? ", rshared" :
				 ", shared");
	if (flags & MS_RELATIME)
		audit_log_format(ab, ", relatime");
	if (flags & MS_I_VERSION)
		audit_log_format(ab, ", iversion");
	if (flags & MS_STRICTATIME)
		audit_log_format(ab, ", strictatime");
	if (flags & MS_NOUSER)
		audit_log_format(ab, ", nouser");
}

/**
 * audit_cb - call back for mount specific audit fields
 * @ab: audit_buffer  (NOT NULL)
 * @va: audit struct to audit values of  (NOT NULL)
 */
static void audit_cb(struct audit_buffer *ab, void *va)
{
	struct common_audit_data *sa = va;

	if (aad(sa)->mnt.type) {
		audit_log_format(ab, " fstype=");
		audit_log_untrustedstring(ab, aad(sa)->mnt.type);
	}
	if (aad(sa)->mnt.src_name) {
		audit_log_format(ab, " srcname=");
		audit_log_untrustedstring(ab, aad(sa)->mnt.src_name);
	}
	if (aad(sa)->mnt.trans) {
		audit_log_format(ab, " trans=");
		audit_log_untrustedstring(ab, aad(sa)->mnt.trans);
	}
	if (aad(sa)->mnt.flags || aad(sa)->op == OP_MOUNT) {
		audit_log_format(ab, " flags=\"");
		audit_mnt_flags(ab, aad(sa)->mnt.flags);
		audit_log_format(ab, "\"");
	}
	if (aad(sa)->mnt.data) {
		audit_log_format(ab, " options=");
		audit_log_untrustedstring(ab, aad(sa)->mnt.data);
	}
}

/**
 * audit_mount - handle the auditing of mount operations
 * @profile: the profile being enforced  (NOT NULL)
 * @op: operation being mediated (NOT NULL)
 * @name: name of object being mediated (MAYBE NULL)
 * @src_name: src_name of object being mediated (MAYBE_NULL)
 * @type: type of filesystem (MAYBE_NULL)
 * @trans: name of trans (MAYBE NULL)
 * @flags: filesystem idependent mount flags
 * @data: filesystem mount flags
 * @request: permissions requested
 * @perms: the permissions computed for the request (NOT NULL)
 * @info: extra information message (MAYBE NULL)
 * @error: 0 if operation allowed else failure error code
 *
 * Returns: %0 or error on failure
 */
static int audit_mount(struct aa_profile *profile, int op, const char *name,
		       const char *src_name, const char *type,
		       const char *trans, unsigned long flags,
		       const void *data, u32 request, struct file_perms *perms,
		       const char *info, int error)
{
	int audit_type = AUDIT_APPARMOR_AUTO;
	DEFINE_AUDIT_DATA(sa, LSM_AUDIT_DATA_NONE, op);

	if (likely(!error)) {
		u32 mask = perms->audit;

		if (unlikely(AUDIT_MODE(profile) == AUDIT_ALL))
			mask = 0xffff;

		/* mask off perms that are not being force audited */
		request &= mask;

		if (likely(!request))
			return 0;
		audit_type = AUDIT_APPARMOR_AUDIT;
	} else {
		/* only report permissions that were denied */
		request = request & ~perms->allow;

		if (request & perms->kill)
			audit_type = AUDIT_APPARMOR_KILL;

		/* quiet known rejects, assumes quiet and kill do not overlap */
		if ((request & perms->quiet) &&
		    AUDIT_MODE(profile) != AUDIT_NOQUIET &&
		    AUDIT_MODE(profile) != AUDIT_ALL)
			request &= ~perms->quiet;

		if (!request)
			return COMPLAIN_MODE(profile) ?
				complain_error(error) : error;
	}

	aad(&sa)->name = name;
	aad(&sa)->mnt.src_name = src_name;
	aad(&sa)->mnt.type = type;
	aad(&sa)->mnt.trans = trans;
	aad(&sa)->mnt.flags = flags;
	if (data && (perms->audit & AA_AUDIT_DATA))
		aad(&sa)->mnt.data = data;
	aad(&sa)->info = info;
	aad(&sa)->error = error;

	return aa_audit(audit_type, profile, &sa, audit_cb);
}

/**
 * match_mnt_flags - Do an ordered match on mount flags
 * @dfa: dfa to match against
 * @state: state to start in
 * @flags: mount flags to match against
 *
 * Mount flags are encoded as an ordered match. This is done instead of
 * checking against a simple bitmask, to allow for logical operations
 * on the flags.
 *
 * Returns: next state after flags match
 */
static unsigned int match_mnt_flags(struct aa_dfa *dfa, unsigned int state,
				    unsigned long flags)
{
	unsigned int i;

	for (i = 0; i <= 31 ; ++i) {
		if ((1 << i) & flags)
			state = aa_dfa_next(dfa, state, i + 1);
	}

	return state;
}

/**
 * compute_mnt_perms - compute mount permission associated with @state
 * @dfa: dfa to match against (NOT NULL)
 * @state: state match finished in
 *
 * Returns: mount permissions
 */
static struct file_perms compute_mnt_perms(struct aa_dfa *dfa,
					   unsigned int state)
{
	struct file_perms perms;

	perms.kill = 0;
	perms.allow = dfa_user_allow(dfa, state);
	perms.audit = dfa_user_audit(dfa, state);
	perms.quiet = dfa_user_quiet(dfa, state);
	perms.xindex = dfa_user_xindex(dfa, state);

	return perms;
}

static const char *mnt_info_table[] = {
	"match succeeded",
	"failed mntpnt match",
	"failed srcname match",
	"failed type match",
	"failed flags match",
	"failed data match"
};

/*
 * Returns 0 on success else element that match failed in, this is the
 * index into the mnt_info_table above
 */
static int do_match_mnt(struct aa_dfa *dfa, unsigned int start,
			const char *mntpnt, const char *devname,
			const char *type, unsigned long flags,
			void *data, bool binary, struct file_perms *perms)
{
	unsigned int state;

	state = aa_dfa_match(dfa, start, mntpnt);
	state = aa_dfa_null_transition(dfa, state);
	if (!state)
		return 1;

	if (devname)
		state = aa_dfa_match(dfa, state, devname);
	state = aa_dfa_null_transition(dfa, state);
	if (!state)
		return 2;

	if (type)
		state = aa_dfa_match(dfa, state, type);
	state = aa_dfa_null_transition(dfa, state);
	if (!state)
		return 3;

	state = match_mnt_flags(dfa, state, flags);
	if (!state)
		return 4;
	*perms = compute_mnt_perms(dfa, state);
	if (perms->allow & AA_MAY_MOUNT)
		return 0;

	/* only match data if not binary and the DFA flags data is expected */
	if (data && !binary && (perms->allow & AA_MNT_CONT_MATCH)) {
		state = aa_dfa_null_transition(dfa, state);
		if (!state)
			return 4;

		state = aa_dfa_match(dfa, state, data);
		if (!state)
			return 5;
		*perms = compute_mnt_perms(dfa, state);
		if (perms->allow & AA_MAY_MOUNT)
			return 0;
	}

	/* failed at end of flags match */
	return 4;
}

/**
 * match_mnt - handle path matching for mount
 * @profile: the confining profile
 * @mntpnt: string for the mntpnt (NOT NULL)
 * @devname: string for the devname/src_name (MAYBE NULL)
 * @type: string for the dev type (MAYBE NULL)
 * @flags: mount flags to match
 * @data: fs mount data (MAYBE NULL)
 * @binary: whether @data is binary
 * @perms: Returns: permission found by the match
 * @info: Returns: infomation string about the match for logging
 *
 * Returns: 0 on success else error
 */
static int match_mnt(struct aa_profile *profile, const char *mntpnt,
		     const char *devname, const char *type,
		     unsigned long flags, void *data, bool binary)
{
	struct file_perms perms = { };
	const char *info = NULL;
	int pos, error = -EACCES;

	if (!profile->policy.dfa)
		goto audit;

	pos = do_match_mnt(profile->policy.dfa,
			   profile->policy.start[AA_CLASS_MOUNT],
			   mntpnt, devname, type, flags, data, binary, &perms);
	if (pos) {
		info = mnt_info_table[pos];
		goto audit;
	}
	error = 0;

audit:
	return audit_mount(profile, OP_MOUNT, mntpnt, devname, type, NULL,
			   flags, data, AA_MAY_MOUNT, &perms, info, error);
}

static int path_flags(struct aa_profile *profile, struct path *path)
{
	return profile->path_flags |
		S_ISDIR(path->dentry->d_inode->i_mode) ? PATH_IS_DIR : 0;
}

int aa_remount(struct aa_label *label, struct path *path, unsigned long flags,
	       void *data)
{
	struct aa_profile *profile;
	const char *name, *info = NULL;
	char *buffer = NULL;
	bool binary;
	int error;

	binary = path->dentry->d_sb->s_type->fs_flags & FS_BINARY_MOUNTDATA;

	get_buffers(buffer);
	error = aa_path_name(path, path_flags(labels_profile(label), path),
			     buffer, &name, &info,
			     labels_profile(label)->disconnected);
	if (error) {
		error = audit_mount(labels_profile(label), OP_MOUNT, name, NULL,
				    NULL, NULL, flags, data, AA_MAY_MOUNT,
				    &nullperms, info, error);
		goto out;
	}

	error = fn_for_each_confined(label, profile,
			match_mnt(profile, name, NULL, NULL, flags, data,
				  binary));

out:
	put_buffers(buffer);

	return error;
}

int aa_bind_mount(struct aa_label *label, struct path *path,
		  const char *dev_name, unsigned long flags)
{
	struct aa_profile *profile;
	char *buffer = NULL, *old_buffer = NULL;
	const char *name, *old_name = NULL, *info = NULL;
	struct path old_path;
	int error;

	if (!dev_name || !*dev_name)
		return -EINVAL;

	flags &= MS_REC | MS_BIND;

	get_buffers(buffer, old_buffer);
	error = aa_path_name(path, path_flags(labels_profile(label), path), buffer, &name,
			     &info, labels_profile(label)->disconnected);
	if (error)
		goto error;

	error = kern_path(dev_name, LOOKUP_FOLLOW|LOOKUP_AUTOMOUNT, &old_path);
	if (error)
		goto error;

	error = aa_path_name(&old_path, path_flags(labels_profile(label),
						   &old_path),
			     old_buffer, &old_name, &info,
			     labels_profile(label)->disconnected);
	path_put(&old_path);
	if (error)
		goto error;

	error = fn_for_each_confined(label, profile,
			match_mnt(profile, name, old_name, NULL, flags, NULL,
				  false));

out:
	put_buffers(buffer, old_buffer);

	return error;

error:
	error = fn_for_each(label, profile,
			audit_mount(profile, OP_MOUNT, name, old_name, NULL,
				    NULL, flags, NULL, AA_MAY_MOUNT, &nullperms,
				    info, error));
	goto out;
}

int aa_mount_change_type(struct aa_label *label, struct path *path,
			 unsigned long flags)
{
	struct aa_profile *profile;
	char *buffer = NULL;
	const char *name, *info = NULL;
	int error;

	/* These are the flags allowed by do_change_type() */
	flags &= (MS_REC | MS_SILENT | MS_SHARED | MS_PRIVATE | MS_SLAVE |
		  MS_UNBINDABLE);

	get_buffers(buffer);
	error = aa_path_name(path, path_flags(labels_profile(label), path),
			     buffer, &name, &info,
			     labels_profile(label)->disconnected);
	if (error) {
		error = fn_for_each(label, profile,
				audit_mount(profile, OP_MOUNT, name, NULL,
					    NULL, NULL, flags, NULL,
					    AA_MAY_MOUNT, &nullperms, info,
					    error));
		goto out;
	}

	error = fn_for_each_confined(label, profile,
			match_mnt(profile, name, NULL, NULL, flags, NULL,
				  false));

out:
	put_buffers(buffer);

	return error;
}

int aa_move_mount(struct aa_label *label, struct path *path,
		  const char *orig_name)
{
	struct aa_profile *profile;
	char *buffer = NULL, *old_buffer = NULL;
	const char *name, *old_name = NULL, *info = NULL;
	struct path old_path;
	int error;

	if (!orig_name || !*orig_name)
		return -EINVAL;

	get_buffers(buffer, old_buffer);
	error = aa_path_name(path, path_flags(labels_profile(label), path),
			     buffer, &name, &info,
			     labels_profile(label)->disconnected);
	if (error)
		goto error;

	error = kern_path(orig_name, LOOKUP_FOLLOW, &old_path);
	if (error)
		goto error;

	error = aa_path_name(&old_path, path_flags(labels_profile(label),
						   &old_path),
			     old_buffer, &old_name, &info,
			     labels_profile(label)->disconnected);
	path_put(&old_path);
	if (error)
		goto error;

	error = fn_for_each_confined(label, profile,
			match_mnt(profile, name, old_name, NULL, MS_MOVE, NULL,
				  false));

out:
	put_buffers(buffer, old_buffer);

	return error;

error:
	error = fn_for_each(label, profile,
			audit_mount(profile, OP_MOUNT, name, old_name, NULL,
				    NULL, MS_MOVE, NULL, AA_MAY_MOUNT,
				    &nullperms, info, error));
	goto out;
}

int aa_new_mount(struct aa_label *label, const char *orig_dev_name,
		 struct path *path, const char *type, unsigned long flags,
		 void *data)
{
	struct aa_profile *profile;
	char *buffer = NULL, *dev_buffer = NULL;
	const char *name = NULL, *dev_name = NULL, *info = NULL;
	bool binary = true;
	int error;

	dev_name = orig_dev_name;
	get_buffers(buffer, dev_buffer);
	if (type) {
		int requires_dev;
		struct file_system_type *fstype = get_fs_type(type);
		if (!fstype)
			return -ENODEV;

		binary = fstype->fs_flags & FS_BINARY_MOUNTDATA;
		requires_dev = fstype->fs_flags & FS_REQUIRES_DEV;
		put_filesystem(fstype);

		if (requires_dev) {
			struct path dev_path;

			if (!dev_name || !*dev_name) {
				error = -ENOENT;
				goto out;
			}

			error = kern_path(dev_name, LOOKUP_FOLLOW, &dev_path);
			if (error)
				goto error;

			error = aa_path_name(&dev_path,
					     path_flags(labels_profile(label),
							&dev_path),
					     dev_buffer, &dev_name, &info,
					     labels_profile(label)->disconnected);
			path_put(&dev_path);
			if (error)
				goto error;
		}
	}

	error = aa_path_name(path, path_flags(labels_profile(label), path),
			     buffer, &name, &info,
			     labels_profile(label)->disconnected);
	if (error)
		goto error;

	error = fn_for_each_confined(label, profile,
			match_mnt(profile, name, dev_name, type, flags, data,
				  binary));

cleanup:
	put_buffers(buffer, dev_buffer);

out:
	return error;

error:
	error = fn_for_each(label, profile,
			audit_mount(labels_profile(label), OP_MOUNT, name,
				    dev_name, type, NULL, flags, data,
				    AA_MAY_MOUNT, &nullperms, info, error));
	goto cleanup;
}

static int profile_umount(struct aa_profile *profile, const char *name)
{
	struct file_perms perms = { };
	const char *info = NULL;
	int e = 0;

	if (profile->policy.dfa) {
		unsigned int state;
		state = aa_dfa_match(profile->policy.dfa,
				     profile->policy.start[AA_CLASS_MOUNT],
				     name);
		perms = compute_mnt_perms(profile->policy.dfa, state);
		if (AA_MAY_UMOUNT & ~perms.allow)
			e = -EACCES;
	} else
		e = -EACCES;
	return audit_mount(profile, OP_UMOUNT, name, NULL, NULL, NULL, 0, NULL,
			   AA_MAY_UMOUNT, &perms, info, e);
}

int aa_umount(struct aa_label *label, struct vfsmount *mnt, int flags)
{
	struct aa_profile *profile;
	char *buffer = NULL;
	const char *name, *info = NULL;
	int error;

	struct path path = { mnt, mnt->mnt_root };
	get_buffers(buffer);
	error = aa_path_name(&path, path_flags(labels_profile(label), &path),
			     buffer, &name, &info,
			     labels_profile(label)->disconnected);
	if (error) {
		error = fn_for_each(label, profile,
				audit_mount(profile, OP_UMOUNT, name, NULL,
					    NULL, NULL, 0, NULL, AA_MAY_UMOUNT,
					    &nullperms, info, error));
		goto out;
	}

	error = fn_for_each_confined(label, profile,
			profile_umount(profile, name));

out:
	put_buffers(buffer);

	return error;
}

static int profile_pivotroot(struct aa_profile *profile, const char *new_name,
			     const char *old_name, struct aa_profile **trans)
{
	struct aa_profile *target = NULL;
	struct file_perms perms = { };
	const char *info = NULL;
	int error = -EACCES;

	/* TODO: actual domain transition computation for multiple
	 *  profiles
	 */
	if (profile->policy.dfa) {
		unsigned int state;
		state = aa_dfa_match(profile->policy.dfa,
				     profile->policy.start[AA_CLASS_MOUNT],
				     new_name);
		state = aa_dfa_null_transition(profile->policy.dfa, state);
		state = aa_dfa_match(profile->policy.dfa, state, old_name);
		perms = compute_mnt_perms(profile->policy.dfa, state);

		if (AA_MAY_PIVOTROOT & perms.allow) {
			if ((perms.xindex & AA_X_TYPE_MASK) == AA_X_TABLE) {
				target = x_table_lookup(profile, perms.xindex);
				if (!target)
					error = -ENOENT;
				else
				  *trans = target;
			} else
				error = 0;
		}
	}

	error = audit_mount(profile, OP_PIVOTROOT, new_name, old_name,
			    NULL, target ? target->base.name : NULL,
			    0, NULL, AA_MAY_PIVOTROOT, &perms, info,
			    error);
	if (!*trans)
	aa_put_profile(target);

	return error;
}

int aa_pivotroot(struct aa_label *label, struct path *old_path,
		  struct path *new_path)
{
	struct aa_profile *profile, *target = NULL;
	char *old_buffer = NULL, *new_buffer = NULL;
	const char *old_name, *new_name = NULL, *info = NULL;
	int error;

	get_buffers(old_buffer, new_buffer);
	error = aa_path_name(old_path, path_flags(labels_profile(label),
						  old_path),
			     old_buffer, &old_name, &info,
			     labels_profile(label)->disconnected);
	if (error)
		goto error;

	error = aa_path_name(new_path, path_flags(labels_profile(label),
						  new_path),
			     new_buffer, &new_name, &info,
			     labels_profile(label)->disconnected);
	if (error)
		goto error;

	error = fn_for_each(label, profile,
			profile_pivotroot(profile, new_name, old_name,
					  &target));
out:
	put_buffers(old_buffer, new_buffer);

	if (target)
		error = aa_replace_current_label(&target->label);

	return error;

error:
	error = fn_for_each(label, profile,
			audit_mount(profile, OP_PIVOTROOT, new_name, old_name,
				    NULL, NULL,
				    0, NULL, AA_MAY_PIVOTROOT, &nullperms, info,
				    error));
	goto out;
}
