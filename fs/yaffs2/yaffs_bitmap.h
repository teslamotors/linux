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

/*
 * Chunk bitmap manipulations
 */

#ifndef __YAFFS_BITMAP_H__
#define __YAFFS_BITMAP_H__

#include "yaffs_guts.h"

void yaffs_VerifyChunkBitId(yaffs_Device *dev, int blk, int chunk);
void yaffs_ClearChunkBits(yaffs_Device *dev, int blk);
void yaffs_ClearChunkBit(yaffs_Device *dev, int blk, int chunk);
void yaffs_SetChunkBit(yaffs_Device *dev, int blk, int chunk);
int yaffs_CheckChunkBit(yaffs_Device *dev, int blk, int chunk);
int yaffs_StillSomeChunkBits(yaffs_Device *dev, int blk);
int yaffs_CountChunkBits(yaffs_Device *dev, int blk);

#endif
