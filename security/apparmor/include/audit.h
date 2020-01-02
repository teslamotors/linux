/*
 * AppArmor security module
 *
 * This file contains AppArmor auditing function definitions.
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2010 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#ifndef __AA_AUDIT_H
#define __AA_AUDIT_H

#include <linux/audit.h>
#include <linux/fs.h>
#include <linux/lsm_audit.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include "file.h"
#include "label.h"

extern const char *const audit_mode_names[];
#define AUDIT_MAX_INDEX 5
enum audit_mode {
	AUDIT_NORMAL,		/* follow normal auditing of accesses */
	AUDIT_QUIET_DENIED,	/* quiet all denied access messages */
	AUDIT_QUIET,		/* quiet all messages */
	AUDIT_NOQUIET,		/* do not quiet audit messages */
	AUDIT_ALL		/* audit all accesses */
};

enum audit_type {
	AUDIT_APPARMOR_AUDIT,
	AUDIT_APPARMOR_ALLOWED,
	AUDIT_APPARMOR_DENIED,
	AUDIT_APPARMOR_HINT,
	AUDIT_APPARMOR_STATUS,
	AUDIT_APPARMOR_ERROR,
	AUDIT_APPARMOR_KILL,
	AUDIT_APPARMOR_AUTO
};

extern const char *const op_table[];
enum aa_ops {
	OP_NULL,

	OP_SYSCTL,
	OP_CAPABLE,

	OP_UNLINK,
	OP_MKDIR,
	OP_RMDIR,
	OP_MKNOD,
	OP_TRUNC,
	OP_LINK,
	OP_SYMLINK,
	OP_RENAME_SRC,
	OP_RENAME_DEST,
	OP_CHMOD,
	OP_CHOWN,
	OP_GETATTR,
	OP_OPEN,

	OP_FRECEIVE,
	OP_FPERM,
	OP_FLOCK,
	OP_FMMAP,
	OP_FMPROT,
	OP_INHERIT,

	OP_PIVOTROOT,
	OP_MOUNT,
	OP_UMOUNT,

	OP_CREATE,
	OP_POST_CREATE,
	OP_BIND,
	OP_CONNECT,
	OP_LISTEN,
	OP_ACCEPT,
	OP_SENDMSG,
	OP_RECVMSG,
	OP_GETSOCKNAME,
	OP_GETPEERNAME,
	OP_GETSOCKOPT,
	OP_SETSOCKOPT,
	OP_SHUTDOWN,

	OP_PTRACE,
	OP_SIGNAL,

	OP_EXEC,
	OP_CHANGE_HAT,
	OP_CHANGE_PROFILE,
	OP_CHANGE_ONEXEC,

	OP_SETPROCATTR,
	OP_SETRLIMIT,

	OP_PROF_REPL,
	OP_PROF_LOAD,
	OP_PROF_RM,
};


struct apparmor_audit_data {
	int error;
	int op;
	int type;
	struct aa_label *label;
	const char *name;
	const char *info;
	u32 request;
	u32 denied;
	union {
		struct {
			const void *target;
			union {
				struct {
					long pos;
				} iface;
				struct {
					kuid_t ouid;
				} fs;
				struct {
					int type, protocol;
					struct sock *peer_sk;
					void *addr;
					int addrlen;
				} net;
				int signal;
			};
		};
		struct {
			int rlim;
			unsigned long max;
		} rlim;
		struct {
			const char *src_name;
			const char *type;
			const char *trans;
			const char *data;
			unsigned long flags;
		} mnt;
	};
};

/* macros for dealing with  apparmor_audit_data structure */
#define aad(SA) (SA)->apparmor_audit_data
#define DEFINE_AUDIT_DATA(NAME, T, X)					\
	/* TODO: cleanup audit init so we don't need _aad = {0,} */	\
	struct apparmor_audit_data NAME ## _aad = { .op = (X), };	\
	struct common_audit_data NAME =					\
	{								\
	.type = (T),							\
	.u.tsk = NULL,							\
	};								\
	NAME.apparmor_audit_data = &(NAME ## _aad)

void aa_audit_msg(int type, struct common_audit_data *sa,
		  void (*cb) (struct audit_buffer *, void *));
int aa_audit(int type, struct aa_profile *profile, struct common_audit_data *sa,
	     void (*cb) (struct audit_buffer *, void *));

#define aa_audit_error(ERROR, SA, CB)				\
({								\
	aad((SA))->error = (ERROR);				\
	aa_audit_msg(AUDIT_APPARMOR_ERROR, (SA), (CB));		\
	aad((SA))->error;					\
})


static inline int complain_error(int error)
{
	if (error == -EPERM || error == -EACCES)
		return 0;
	return error;
}

#endif /* __AA_AUDIT_H */
