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

#ifndef __YAFFS_YAFFS2_H__
#define __YAFFS_YAFFS2_H__

#include "yaffs_guts.h"

void yaffs2_CalcOldestDirtySequence(yaffs_Device *dev);
void yaffs2_FindOldestDirtySequence(yaffs_Device *dev);
void yaffs2_ClearOldestDirtySequence(yaffs_Device *dev, yaffs_BlockInfo *bi);
void yaffs2_UpdateOldestDirtySequence(yaffs_Device *dev, unsigned blockNo, yaffs_BlockInfo *bi);
int yaffs2_BlockNotDisqualifiedFromGC(yaffs_Device *dev, yaffs_BlockInfo *bi);
__u32 yaffs2_FindRefreshBlock(yaffs_Device *dev);
int yaffs2_CheckpointRequired(yaffs_Device *dev);
int yaffs2_CalcCheckpointBlocksRequired(yaffs_Device *dev);


void yaffs2_InvalidateCheckpoint(yaffs_Device *dev);
int yaffs2_CheckpointSave(yaffs_Device *dev);
int yaffs2_CheckpointRestore(yaffs_Device *dev);

int yaffs2_HandleHole(yaffs_Object *obj, loff_t newSize);
int yaffs2_ScanBackwards(yaffs_Device *dev);

#endif
