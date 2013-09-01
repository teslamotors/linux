/*
 * YAFFS: Yet another Flash File System . A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2011 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1 as
 * published by the Free Software Foundation.
 *
 * Note: Only YAFFS headers are LGPL, YAFFS C code is covered by GPL.
 */

#ifndef __YAFFS_ATTRIBS_H__
#define __YAFFS_ATTRIBS_H__

#include "yaffs_guts.h"

void yaffs_load_attribs(struct yaffs_obj *obj, struct yaffs_obj_hdr *oh);
void yaffs_load_attribs_oh(struct yaffs_obj_hdr *oh, struct yaffs_obj *obj);
void yaffs_attribs_init(struct yaffs_obj *obj, u32 gid, u32 uid, u32 rdev);
void yaffs_load_current_time(struct yaffs_obj *obj, int do_a, int do_c);
int yaffs_set_attribs(struct yaffs_obj *obj, struct iattr *attr);
int yaffs_get_attribs(struct yaffs_obj *obj, struct iattr *attr);


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0))
static inline uid_t ia_uid_read(const struct iattr *iattr)
{
	return from_kuid(&init_user_ns, iattr->ia_uid);
}

static inline gid_t ia_gid_read(const struct iattr *iattr)
{
	return from_kgid(&init_user_ns, iattr->ia_gid);
}

static inline void ia_uid_write(struct iattr *iattr, uid_t uid)
{
	iattr->ia_uid = make_kuid(&init_user_ns, uid);
}

static inline void ia_gid_write(struct iattr *iattr, gid_t gid)
{
	iattr->ia_gid = make_kgid(&init_user_ns, gid);
}
#else
static inline uid_t ia_uid_read(const struct iattr *iattr)
{
	return iattr->ia_uid;
}

static inline gid_t ia_gid_read(const struct iattr *inode)
{
	return iattr->ia_gid;
}

static inline void ia_uid_write(struct iattr *iattr, uid_t uid)
{
	iattr->ia_uid = uid;
}

static inline void ia_gid_write(struct iattr *iattr, gid_t gid)
{
	iattr->ia_gid = gid;
}
#endif

#endif
