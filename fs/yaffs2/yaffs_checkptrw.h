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

#ifndef __YAFFS_CHECKPTRW_H__
#define __YAFFS_CHECKPTRW_H__

#include "yaffs_guts.h"

int yaffs2_CheckpointOpen(yaffs_Device *dev, int forWriting);

int yaffs2_CheckpointWrite(yaffs_Device *dev, const void *data, int nBytes);

int yaffs2_CheckpointRead(yaffs_Device *dev, void *data, int nBytes);

int yaffs2_GetCheckpointSum(yaffs_Device *dev, __u32 *sum);

int yaffs2_CheckpointClose(yaffs_Device *dev);

int yaffs2_CheckpointInvalidateStream(yaffs_Device *dev);


#endif
