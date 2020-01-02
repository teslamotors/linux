/*
 * AppArmor security module
 *
 * This file contains AppArmor file mediation function definitions.
 *
 * Copyright 2014 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 *
 * This is a file of helper macros, defines for backporting newer versions
 * of apparmor to older kernels
 */
#ifndef __AA_BACKPORT_H
#define __AA_BACKPORT_H

#include <linux/fs.h>
#include <linux/security.h>


/* 4.4 backport to supprt 5955102c9984fa081b2d570cfac75c97eecf8f3b */
static inline void inode_lock(struct inode *inode)
{
	mutex_lock(&inode->i_mutex);
}

static inline void inode_unlock(struct inode *inode)
{
	mutex_unlock(&inode->i_mutex);
}

static inline void inode_lock_nested(struct inode *inode, unsigned subclass)
{
	mutex_lock_nested(&inode->i_mutex, subclass);
}

/* 4.1 backport for b1d9e6b0646d0e5ee5d9050bd236b6c65d66faef */

#define LSM_HOOKS_NAME(X) LSM_HOOK_INIT(name, X),
#define security_add_hooks(X, Y) /* nothing */

#define security_module_enable(X)					\
({									\
	int ret = (security_module_enable)(&apparmor_ops);		\
	if (ret) {							\
		int error = register_security(&apparmor_ops);		\
		if (error) {						\
			AA_ERROR("Unable to register AppArmor\n");	\
			ret = 0;					\
		}							\
	}								\
	(ret);								\
})

/* 4.1 backport for e20b043a6902ecb61c2c84355c3bae5149f391db */
#define LSM_HOOK_INIT(N, FN)	.N = (FN)

#endif /* __AA_BACKPORT_H */
