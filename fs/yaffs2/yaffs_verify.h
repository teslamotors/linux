/*
 * YAFFS: Yet another Flash File System . A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2010 Aleph One Ltd.
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

#ifndef __YAFFS_VERIFY_H__
#define __YAFFS_VERIFY_H__

#include "yaffs_guts.h"

void yaffs_VerifyBlock(yaffs_Device *dev, yaffs_BlockInfo *bi, int n);
void yaffs_VerifyCollectedBlock(yaffs_Device *dev, yaffs_BlockInfo *bi, int n);
void yaffs_VerifyBlocks(yaffs_Device *dev);

void yaffs_VerifyObjectHeader(yaffs_Object *obj, yaffs_ObjectHeader *oh, yaffs_ExtendedTags *tags, int parentCheck);
void yaffs_VerifyFile(yaffs_Object *obj);
void yaffs_VerifyHardLink(yaffs_Object *obj);
void yaffs_VerifySymlink(yaffs_Object *obj);
void yaffs_VerifySpecial(yaffs_Object *obj);
void yaffs_VerifyObject(yaffs_Object *obj);
void yaffs_VerifyObjects(yaffs_Device *dev);
void yaffs_VerifyObjectInDirectory(yaffs_Object *obj);
void yaffs_VerifyDirectory(yaffs_Object *directory);
void yaffs_VerifyFreeChunks(yaffs_Device *dev);

int yaffs_VerifyFileSanity(yaffs_Object *obj);

int yaffs_SkipVerification(yaffs_Device *dev);

#endif

