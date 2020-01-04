/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2010 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include "yaffs_guts.h"
#include "yaffs_trace.h"
#include "yaffs_yaffs2.h"
#include "yaffs_checkptrw.h"
#include "yaffs_bitmap.h"
#include "yaffs_qsort.h"
#include "yaffs_nand.h"
#include "yaffs_getblockinfo.h"
#include "yaffs_verify.h"

/*
 * Checkpoints are really no benefit on very small partitions.
 *
 * To save space on small partitions don't bother with checkpoints unless
 * the partition is at least this big.
 */
#define YAFFS_CHECKPOINT_MIN_BLOCKS 60

#define YAFFS_SMALL_HOLE_THRESHOLD 4


/*
 * Oldest Dirty Sequence Number handling.
 */
 
/* yaffs2_CalcOldestDirtySequence()
 * yaffs2_FindOldestDirtySequence()
 * Calculate the oldest dirty sequence number if we don't know it.
 */
void yaffs2_CalcOldestDirtySequence(yaffs_Device *dev)
{
	int i;
	unsigned seq;
	unsigned blockNo = 0;
	yaffs_BlockInfo *b;

	if(!dev->param.isYaffs2)
		return;

	/* Find the oldest dirty sequence number. */
	seq = dev->sequenceNumber + 1;
	b = dev->blockInfo;
	for (i = dev->internalStartBlock; i <= dev->internalEndBlock; i++) {
		if (b->blockState == YAFFS_BLOCK_STATE_FULL &&
			(b->pagesInUse - b->softDeletions) < dev->param.nChunksPerBlock &&
			b->sequenceNumber < seq) {
			seq = b->sequenceNumber;
			blockNo = i;
		}
		b++;
	}

	if(blockNo){
		dev->oldestDirtySequence = seq;
		dev->oldestDirtyBlock = blockNo;
	}

}


void yaffs2_FindOldestDirtySequence(yaffs_Device *dev)
{
	if(!dev->param.isYaffs2)
		return;

	if(!dev->oldestDirtySequence)
		yaffs2_CalcOldestDirtySequence(dev);
}

/*
 * yaffs_ClearOldestDirtySequence()
 * Called when a block is erased or marked bad. (ie. when its sequenceNumber
 * becomes invalid). If the value matches the oldest then we clear 
 * dev->oldestDirtySequence to force its recomputation.
 */
void yaffs2_ClearOldestDirtySequence(yaffs_Device *dev, yaffs_BlockInfo *bi)
{

	if(!dev->param.isYaffs2)
		return;

	if(!bi || bi->sequenceNumber == dev->oldestDirtySequence){
		dev->oldestDirtySequence = 0;
		dev->oldestDirtyBlock = 0;
	}
}

/*
 * yaffs2_UpdateOldestDirtySequence()
 * Update the oldest dirty sequence number whenever we dirty a block.
 * Only do this if the oldestDirtySequence is actually being tracked.
 */
void yaffs2_UpdateOldestDirtySequence(yaffs_Device *dev, unsigned blockNo, yaffs_BlockInfo *bi)
{
	if(!dev->param.isYaffs2)
		return;

	if(dev->oldestDirtySequence){
		if(dev->oldestDirtySequence > bi->sequenceNumber){
			dev->oldestDirtySequence = bi->sequenceNumber;
			dev->oldestDirtyBlock = blockNo;
		}
	}
}

int yaffs2_BlockNotDisqualifiedFromGC(yaffs_Device *dev,
					yaffs_BlockInfo *bi)
{

	if (!dev->param.isYaffs2)
		return 1;	/* disqualification only applies to yaffs2. */

	if (!bi->hasShrinkHeader)
		return 1;	/* can gc */

	yaffs2_FindOldestDirtySequence(dev);

	/* Can't do gc of this block if there are any blocks older than this one that have
	 * discarded pages.
	 */
	return (bi->sequenceNumber <= dev->oldestDirtySequence);
}

/*
 * yaffs2_FindRefreshBlock()
 * periodically finds the oldest full block by sequence number for refreshing.
 * Only for yaffs2.
 */
__u32 yaffs2_FindRefreshBlock(yaffs_Device *dev)
{
	__u32 b ;

	__u32 oldest = 0;
	__u32 oldestSequence = 0;

	yaffs_BlockInfo *bi;

	if(!dev->param.isYaffs2)
		return oldest;

	/*
	 * If refresh period < 10 then refreshing is disabled.
	 */
	if(dev->param.refreshPeriod < 10)
	        return oldest;

        /*
         * Fix broken values.
         */
        if(dev->refreshSkip > dev->param.refreshPeriod)
                dev->refreshSkip = dev->param.refreshPeriod;

	if(dev->refreshSkip > 0)
	        return oldest;

	/*
	 * Refresh skip is now zero.
	 * We'll do a refresh this time around....
	 * Update the refresh skip and find the oldest block.
	 */
	dev->refreshSkip = dev->param.refreshPeriod;
	dev->refreshCount++;
	bi = dev->blockInfo;
	for (b = dev->internalStartBlock; b <=dev->internalEndBlock; b++){

		if (bi->blockState == YAFFS_BLOCK_STATE_FULL){

			if(oldest < 1 ||
                                bi->sequenceNumber < oldestSequence){
                                oldest = b;
                                oldestSequence = bi->sequenceNumber;
                        }
		}
		bi++;
	}

	if (oldest > 0) {
		T(YAFFS_TRACE_GC,
		  (TSTR("GC refresh count %d selected block %d with sequenceNumber %d" TENDSTR),
		   dev->refreshCount, oldest, oldestSequence));
	}

	return oldest;
}

int yaffs2_CheckpointRequired(yaffs_Device *dev)
{
	int nblocks;
	
	if(!dev->param.isYaffs2)
		return 0;
	
	nblocks = dev->internalEndBlock - dev->internalStartBlock + 1 ;

	return 	!dev->param.skipCheckpointWrite &&
		!dev->readOnly &&
		(nblocks >= YAFFS_CHECKPOINT_MIN_BLOCKS);
}

int yaffs2_CalcCheckpointBlocksRequired(yaffs_Device *dev)
{
	int retval;

	if(!dev->param.isYaffs2)
		return 0;

	if (!dev->nCheckpointBlocksRequired &&
		yaffs2_CheckpointRequired(dev)){
		/* Not a valid value so recalculate */
		int nBytes = 0;
		int nBlocks;
		int devBlocks = (dev->param.endBlock - dev->param.startBlock + 1);

		nBytes += sizeof(yaffs_CheckpointValidity);
		nBytes += sizeof(yaffs_CheckpointDevice);
		nBytes += devBlocks * sizeof(yaffs_BlockInfo);
		nBytes += devBlocks * dev->chunkBitmapStride;
		nBytes += (sizeof(yaffs_CheckpointObject) + sizeof(__u32)) * (dev->nObjects);
		nBytes += (dev->tnodeSize + sizeof(__u32)) * (dev->nTnodes);
		nBytes += sizeof(yaffs_CheckpointValidity);
		nBytes += sizeof(__u32); /* checksum*/

		/* Round up and add 2 blocks to allow for some bad blocks, so add 3 */

		nBlocks = (nBytes/(dev->nDataBytesPerChunk * dev->param.nChunksPerBlock)) + 3;

		dev->nCheckpointBlocksRequired = nBlocks;
	}

	retval = dev->nCheckpointBlocksRequired - dev->blocksInCheckpoint;
	if(retval < 0)
		retval = 0;
	return retval;
}

/*--------------------- Checkpointing --------------------*/


static int yaffs2_WriteCheckpointValidityMarker(yaffs_Device *dev, int head)
{
	yaffs_CheckpointValidity cp;

	memset(&cp, 0, sizeof(cp));

	cp.structType = sizeof(cp);
	cp.magic = YAFFS_MAGIC;
	cp.version = YAFFS_CHECKPOINT_VERSION;
	cp.head = (head) ? 1 : 0;

	return (yaffs2_CheckpointWrite(dev, &cp, sizeof(cp)) == sizeof(cp)) ?
		1 : 0;
}

static int yaffs2_ReadCheckpointValidityMarker(yaffs_Device *dev, int head)
{
	yaffs_CheckpointValidity cp;
	int ok;

	ok = (yaffs2_CheckpointRead(dev, &cp, sizeof(cp)) == sizeof(cp));

	if (ok)
		ok = (cp.structType == sizeof(cp)) &&
		     (cp.magic == YAFFS_MAGIC) &&
		     (cp.version == YAFFS_CHECKPOINT_VERSION) &&
		     (cp.head == ((head) ? 1 : 0));
	return ok ? 1 : 0;
}

static void yaffs2_DeviceToCheckpointDevice(yaffs_CheckpointDevice *cp,
					   yaffs_Device *dev)
{
	cp->nErasedBlocks = dev->nErasedBlocks;
	cp->allocationBlock = dev->allocationBlock;
	cp->allocationPage = dev->allocationPage;
	cp->nFreeChunks = dev->nFreeChunks;

	cp->nDeletedFiles = dev->nDeletedFiles;
	cp->nUnlinkedFiles = dev->nUnlinkedFiles;
	cp->nBackgroundDeletions = dev->nBackgroundDeletions;
	cp->sequenceNumber = dev->sequenceNumber;

}

static void yaffs2_CheckpointDeviceToDevice(yaffs_Device *dev,
					   yaffs_CheckpointDevice *cp)
{
	dev->nErasedBlocks = cp->nErasedBlocks;
	dev->allocationBlock = cp->allocationBlock;
	dev->allocationPage = cp->allocationPage;
	dev->nFreeChunks = cp->nFreeChunks;

	dev->nDeletedFiles = cp->nDeletedFiles;
	dev->nUnlinkedFiles = cp->nUnlinkedFiles;
	dev->nBackgroundDeletions = cp->nBackgroundDeletions;
	dev->sequenceNumber = cp->sequenceNumber;
}


static int yaffs2_WriteCheckpointDevice(yaffs_Device *dev)
{
	yaffs_CheckpointDevice cp;
	__u32 nBytes;
	__u32 nBlocks = (dev->internalEndBlock - dev->internalStartBlock + 1);

	int ok;

	/* Write device runtime values*/
	yaffs2_DeviceToCheckpointDevice(&cp, dev);
	cp.structType = sizeof(cp);

	ok = (yaffs2_CheckpointWrite(dev, &cp, sizeof(cp)) == sizeof(cp));

	/* Write block info */
	if (ok) {
		nBytes = nBlocks * sizeof(yaffs_BlockInfo);
		ok = (yaffs2_CheckpointWrite(dev, dev->blockInfo, nBytes) == nBytes);
	}

	/* Write chunk bits */
	if (ok) {
		nBytes = nBlocks * dev->chunkBitmapStride;
		ok = (yaffs2_CheckpointWrite(dev, dev->chunkBits, nBytes) == nBytes);
	}
	return	 ok ? 1 : 0;

}

static int yaffs2_ReadCheckpointDevice(yaffs_Device *dev)
{
	yaffs_CheckpointDevice cp;
	__u32 nBytes;
	__u32 nBlocks = (dev->internalEndBlock - dev->internalStartBlock + 1);

	int ok;

	ok = (yaffs2_CheckpointRead(dev, &cp, sizeof(cp)) == sizeof(cp));
	if (!ok)
		return 0;

	if (cp.structType != sizeof(cp))
		return 0;


	yaffs2_CheckpointDeviceToDevice(dev, &cp);

	nBytes = nBlocks * sizeof(yaffs_BlockInfo);

	ok = (yaffs2_CheckpointRead(dev, dev->blockInfo, nBytes) == nBytes);

	if (!ok)
		return 0;
	nBytes = nBlocks * dev->chunkBitmapStride;

	ok = (yaffs2_CheckpointRead(dev, dev->chunkBits, nBytes) == nBytes);

	return ok ? 1 : 0;
}

static void yaffs2_ObjectToCheckpointObject(yaffs_CheckpointObject *cp,
					   yaffs_Object *obj)
{

	cp->objectId = obj->objectId;
	cp->parentId = (obj->parent) ? obj->parent->objectId : 0;
	cp->hdrChunk = obj->hdrChunk;
	cp->variantType = obj->variantType;
	cp->deleted = obj->deleted;
	cp->softDeleted = obj->softDeleted;
	cp->unlinked = obj->unlinked;
	cp->fake = obj->fake;
	cp->renameAllowed = obj->renameAllowed;
	cp->unlinkAllowed = obj->unlinkAllowed;
	cp->serial = obj->serial;
	cp->nDataChunks = obj->nDataChunks;

	if (obj->variantType == YAFFS_OBJECT_TYPE_FILE)
		cp->fileSizeOrEquivalentObjectId = obj->variant.fileVariant.fileSize;
	else if (obj->variantType == YAFFS_OBJECT_TYPE_HARDLINK)
		cp->fileSizeOrEquivalentObjectId = obj->variant.hardLinkVariant.equivalentObjectId;
}

static int yaffs2_CheckpointObjectToObject(yaffs_Object *obj, yaffs_CheckpointObject *cp)
{

	yaffs_Object *parent;

	if (obj->variantType != cp->variantType) {
		T(YAFFS_TRACE_ERROR, (TSTR("Checkpoint read object %d type %d "
			TCONT("chunk %d does not match existing object type %d")
			TENDSTR), cp->objectId, cp->variantType, cp->hdrChunk,
			obj->variantType));
		return 0;
	}

	obj->objectId = cp->objectId;

	if (cp->parentId)
		parent = yaffs_FindOrCreateObjectByNumber(
					obj->myDev,
					cp->parentId,
					YAFFS_OBJECT_TYPE_DIRECTORY);
	else
		parent = NULL;

	if (parent) {
		if (parent->variantType != YAFFS_OBJECT_TYPE_DIRECTORY) {
			T(YAFFS_TRACE_ALWAYS, (TSTR("Checkpoint read object %d parent %d type %d"
				TCONT(" chunk %d Parent type, %d, not directory")
				TENDSTR),
				cp->objectId, cp->parentId, cp->variantType,
				cp->hdrChunk, parent->variantType));
			return 0;
		}
		yaffs_AddObjectToDirectory(parent, obj);
	}

	obj->hdrChunk = cp->hdrChunk;
	obj->variantType = cp->variantType;
	obj->deleted = cp->deleted;
	obj->softDeleted = cp->softDeleted;
	obj->unlinked = cp->unlinked;
	obj->fake = cp->fake;
	obj->renameAllowed = cp->renameAllowed;
	obj->unlinkAllowed = cp->unlinkAllowed;
	obj->serial = cp->serial;
	obj->nDataChunks = cp->nDataChunks;

	if (obj->variantType == YAFFS_OBJECT_TYPE_FILE)
		obj->variant.fileVariant.fileSize = cp->fileSizeOrEquivalentObjectId;
	else if (obj->variantType == YAFFS_OBJECT_TYPE_HARDLINK)
		obj->variant.hardLinkVariant.equivalentObjectId = cp->fileSizeOrEquivalentObjectId;

	if (obj->hdrChunk > 0)
		obj->lazyLoaded = 1;
	return 1;
}



static int yaffs2_CheckpointTnodeWorker(yaffs_Object *in, yaffs_Tnode *tn,
					__u32 level, int chunkOffset)
{
	int i;
	yaffs_Device *dev = in->myDev;
	int ok = 1;

	if (tn) {
		if (level > 0) {

			for (i = 0; i < YAFFS_NTNODES_INTERNAL && ok; i++) {
				if (tn->internal[i]) {
					ok = yaffs2_CheckpointTnodeWorker(in,
							tn->internal[i],
							level - 1,
							(chunkOffset<<YAFFS_TNODES_INTERNAL_BITS) + i);
				}
			}
		} else if (level == 0) {
			__u32 baseOffset = chunkOffset <<  YAFFS_TNODES_LEVEL0_BITS;
			ok = (yaffs2_CheckpointWrite(dev, &baseOffset, sizeof(baseOffset)) == sizeof(baseOffset));
			if (ok)
				ok = (yaffs2_CheckpointWrite(dev, tn, dev->tnodeSize) == dev->tnodeSize);
		}
	}

	return ok;

}

static int yaffs2_WriteCheckpointTnodes(yaffs_Object *obj)
{
	__u32 endMarker = ~0;
	int ok = 1;

	if (obj->variantType == YAFFS_OBJECT_TYPE_FILE) {
		ok = yaffs2_CheckpointTnodeWorker(obj,
					    obj->variant.fileVariant.top,
					    obj->variant.fileVariant.topLevel,
					    0);
		if (ok)
			ok = (yaffs2_CheckpointWrite(obj->myDev, &endMarker, sizeof(endMarker)) ==
				sizeof(endMarker));
	}

	return ok ? 1 : 0;
}

static int yaffs2_ReadCheckpointTnodes(yaffs_Object *obj)
{
	__u32 baseChunk;
	int ok = 1;
	yaffs_Device *dev = obj->myDev;
	yaffs_FileStructure *fileStructPtr = &obj->variant.fileVariant;
	yaffs_Tnode *tn;
	int nread = 0;

	ok = (yaffs2_CheckpointRead(dev, &baseChunk, sizeof(baseChunk)) == sizeof(baseChunk));

	while (ok && (~baseChunk)) {
		nread++;
		/* Read level 0 tnode */


		tn = yaffs_GetTnode(dev);
		if (tn){
			ok = (yaffs2_CheckpointRead(dev, tn, dev->tnodeSize) == dev->tnodeSize);
		} else
			ok = 0;

		if (tn && ok)
			ok = yaffs_AddOrFindLevel0Tnode(dev,
							fileStructPtr,
							baseChunk,
							tn) ? 1 : 0;

		if (ok)
			ok = (yaffs2_CheckpointRead(dev, &baseChunk, sizeof(baseChunk)) == sizeof(baseChunk));

	}

	T(YAFFS_TRACE_CHECKPOINT, (
		TSTR("Checkpoint read tnodes %d records, last %d. ok %d" TENDSTR),
		nread, baseChunk, ok));

	return ok ? 1 : 0;
}


static int yaffs2_WriteCheckpointObjects(yaffs_Device *dev)
{
	yaffs_Object *obj;
	yaffs_CheckpointObject cp;
	int i;
	int ok = 1;
	struct ylist_head *lh;


	/* Iterate through the objects in each hash entry,
	 * dumping them to the checkpointing stream.
	 */

	for (i = 0; ok &&  i <  YAFFS_NOBJECT_BUCKETS; i++) {
		ylist_for_each(lh, &dev->objectBucket[i].list) {
			if (lh) {
				obj = ylist_entry(lh, yaffs_Object, hashLink);
				if (!obj->deferedFree) {
					yaffs2_ObjectToCheckpointObject(&cp, obj);
					cp.structType = sizeof(cp);

					T(YAFFS_TRACE_CHECKPOINT, (
						TSTR("Checkpoint write object %d parent %d type %d chunk %d obj addr %p" TENDSTR),
						cp.objectId, cp.parentId, cp.variantType, cp.hdrChunk, obj));

					ok = (yaffs2_CheckpointWrite(dev, &cp, sizeof(cp)) == sizeof(cp));

					if (ok && obj->variantType == YAFFS_OBJECT_TYPE_FILE)
						ok = yaffs2_WriteCheckpointTnodes(obj);
				}
			}
		}
	}

	/* Dump end of list */
	memset(&cp, 0xFF, sizeof(yaffs_CheckpointObject));
	cp.structType = sizeof(cp);

	if (ok)
		ok = (yaffs2_CheckpointWrite(dev, &cp, sizeof(cp)) == sizeof(cp));

	return ok ? 1 : 0;
}

static int yaffs2_ReadCheckpointObjects(yaffs_Device *dev)
{
	yaffs_Object *obj;
	yaffs_CheckpointObject cp;
	int ok = 1;
	int done = 0;
	yaffs_Object *hardList = NULL;

	while (ok && !done) {
		ok = (yaffs2_CheckpointRead(dev, &cp, sizeof(cp)) == sizeof(cp));
		if (cp.structType != sizeof(cp)) {
			T(YAFFS_TRACE_CHECKPOINT, (TSTR("struct size %d instead of %d ok %d"TENDSTR),
				cp.structType, (int)sizeof(cp), ok));
			ok = 0;
		}

		T(YAFFS_TRACE_CHECKPOINT, (TSTR("Checkpoint read object %d parent %d type %d chunk %d " TENDSTR),
			cp.objectId, cp.parentId, cp.variantType, cp.hdrChunk));

		if (ok && cp.objectId == ~0)
			done = 1;
		else if (ok) {
			obj = yaffs_FindOrCreateObjectByNumber(dev, cp.objectId, cp.variantType);
			if (obj) {
				ok = yaffs2_CheckpointObjectToObject(obj, &cp);
				if (!ok)
					break;
				if (obj->variantType == YAFFS_OBJECT_TYPE_FILE) {
					ok = yaffs2_ReadCheckpointTnodes(obj);
				} else if (obj->variantType == YAFFS_OBJECT_TYPE_HARDLINK) {
					obj->hardLinks.next =
						(struct ylist_head *) hardList;
					hardList = obj;
				}
			} else
				ok = 0;
		}
	}

	if (ok)
		yaffs_HardlinkFixup(dev, hardList);

	return ok ? 1 : 0;
}

static int yaffs2_WriteCheckpointSum(yaffs_Device *dev)
{
	__u32 checkpointSum;
	int ok;

	yaffs2_GetCheckpointSum(dev, &checkpointSum);

	ok = (yaffs2_CheckpointWrite(dev, &checkpointSum, sizeof(checkpointSum)) == sizeof(checkpointSum));

	if (!ok)
		return 0;

	return 1;
}

static int yaffs2_ReadCheckpointSum(yaffs_Device *dev)
{
	__u32 checkpointSum0;
	__u32 checkpointSum1;
	int ok;

	yaffs2_GetCheckpointSum(dev, &checkpointSum0);

	ok = (yaffs2_CheckpointRead(dev, &checkpointSum1, sizeof(checkpointSum1)) == sizeof(checkpointSum1));

	if (!ok)
		return 0;

	if (checkpointSum0 != checkpointSum1)
		return 0;

	return 1;
}


static int yaffs2_WriteCheckpointData(yaffs_Device *dev)
{
	int ok = 1;

	if (!yaffs2_CheckpointRequired(dev)) {
		T(YAFFS_TRACE_CHECKPOINT, (TSTR("skipping checkpoint write" TENDSTR)));
		ok = 0;
	}

	if (ok)
		ok = yaffs2_CheckpointOpen(dev, 1);

	if (ok) {
		T(YAFFS_TRACE_CHECKPOINT, (TSTR("write checkpoint validity" TENDSTR)));
		ok = yaffs2_WriteCheckpointValidityMarker(dev, 1);
	}
	if (ok) {
		T(YAFFS_TRACE_CHECKPOINT, (TSTR("write checkpoint device" TENDSTR)));
		ok = yaffs2_WriteCheckpointDevice(dev);
	}
	if (ok) {
		T(YAFFS_TRACE_CHECKPOINT, (TSTR("write checkpoint objects" TENDSTR)));
		ok = yaffs2_WriteCheckpointObjects(dev);
	}
	if (ok) {
		T(YAFFS_TRACE_CHECKPOINT, (TSTR("write checkpoint validity" TENDSTR)));
		ok = yaffs2_WriteCheckpointValidityMarker(dev, 0);
	}

	if (ok)
		ok = yaffs2_WriteCheckpointSum(dev);

	if (!yaffs2_CheckpointClose(dev))
		ok = 0;

	if (ok)
		dev->isCheckpointed = 1;
	else
		dev->isCheckpointed = 0;

	return dev->isCheckpointed;
}

static int yaffs2_ReadCheckpointData(yaffs_Device *dev)
{
	int ok = 1;
	
	if(!dev->param.isYaffs2)
		ok = 0;

	if (ok && dev->param.skipCheckpointRead) {
		T(YAFFS_TRACE_CHECKPOINT, (TSTR("skipping checkpoint read" TENDSTR)));
		ok = 0;
	}

	if (ok)
		ok = yaffs2_CheckpointOpen(dev, 0); /* open for read */

	if (ok) {
		T(YAFFS_TRACE_CHECKPOINT, (TSTR("read checkpoint validity" TENDSTR)));
		ok = yaffs2_ReadCheckpointValidityMarker(dev, 1);
	}
	if (ok) {
		T(YAFFS_TRACE_CHECKPOINT, (TSTR("read checkpoint device" TENDSTR)));
		ok = yaffs2_ReadCheckpointDevice(dev);
	}
	if (ok) {
		T(YAFFS_TRACE_CHECKPOINT, (TSTR("read checkpoint objects" TENDSTR)));
		ok = yaffs2_ReadCheckpointObjects(dev);
	}
	if (ok) {
		T(YAFFS_TRACE_CHECKPOINT, (TSTR("read checkpoint validity" TENDSTR)));
		ok = yaffs2_ReadCheckpointValidityMarker(dev, 0);
	}

	if (ok) {
		ok = yaffs2_ReadCheckpointSum(dev);
		T(YAFFS_TRACE_CHECKPOINT, (TSTR("read checkpoint checksum %d" TENDSTR), ok));
	}

	if (!yaffs2_CheckpointClose(dev))
		ok = 0;

	if (ok)
		dev->isCheckpointed = 1;
	else
		dev->isCheckpointed = 0;

	return ok ? 1 : 0;

}

void yaffs2_InvalidateCheckpoint(yaffs_Device *dev)
{
	if (dev->isCheckpointed ||
			dev->blocksInCheckpoint > 0) {
		dev->isCheckpointed = 0;
		yaffs2_CheckpointInvalidateStream(dev);
	}
	if (dev->param.markSuperBlockDirty)
		dev->param.markSuperBlockDirty(dev);
}


int yaffs_CheckpointSave(yaffs_Device *dev)
{

	T(YAFFS_TRACE_CHECKPOINT, (TSTR("save entry: isCheckpointed %d"TENDSTR), dev->isCheckpointed));

	yaffs_VerifyObjects(dev);
	yaffs_VerifyBlocks(dev);
	yaffs_VerifyFreeChunks(dev);

	if (!dev->isCheckpointed) {
		yaffs2_InvalidateCheckpoint(dev);
		yaffs2_WriteCheckpointData(dev);
	}

	T(YAFFS_TRACE_ALWAYS, (TSTR("save exit: isCheckpointed %d"TENDSTR), dev->isCheckpointed));

	return dev->isCheckpointed;
}

int yaffs2_CheckpointRestore(yaffs_Device *dev)
{
	int retval;
	T(YAFFS_TRACE_CHECKPOINT, (TSTR("restore entry: isCheckpointed %d"TENDSTR), dev->isCheckpointed));

	retval = yaffs2_ReadCheckpointData(dev);

	if (dev->isCheckpointed) {
		yaffs_VerifyObjects(dev);
		yaffs_VerifyBlocks(dev);
		yaffs_VerifyFreeChunks(dev);
	}

	T(YAFFS_TRACE_CHECKPOINT, (TSTR("restore exit: isCheckpointed %d"TENDSTR), dev->isCheckpointed));

	return retval;
}

int yaffs2_HandleHole(yaffs_Object *obj, loff_t newSize)
{
	/* if newsSize > oldFileSize.
	 * We're going to be writing a hole.
	 * If the hole is small then write zeros otherwise write a start of hole marker.
	 */
		

	loff_t oldFileSize;
	int increase;
	int smallHole   ;
	int result = YAFFS_OK;
	yaffs_Device *dev = NULL;

	__u8 *localBuffer = NULL;
	
	int smallIncreaseOk = 0;
	
	if(!obj)
		return YAFFS_FAIL;

	if(obj->variantType != YAFFS_OBJECT_TYPE_FILE)
		return YAFFS_FAIL;
	
	dev = obj->myDev;
	
	/* Bail out if not yaffs2 mode */
	if(!dev->param.isYaffs2)
		return YAFFS_OK;

	oldFileSize = obj->variant.fileVariant.fileSize;

	if (newSize <= oldFileSize)
		return YAFFS_OK;

	increase = newSize - oldFileSize;

	if(increase < YAFFS_SMALL_HOLE_THRESHOLD * dev->nDataBytesPerChunk &&
		yaffs_CheckSpaceForAllocation(dev, YAFFS_SMALL_HOLE_THRESHOLD + 1))
		smallHole = 1;
	else
		smallHole = 0;

	if(smallHole)
		localBuffer= yaffs_GetTempBuffer(dev, __LINE__);
	
	if(localBuffer){
		/* fill hole with zero bytes */
		int pos = oldFileSize;
		int thisWrite;
		int written;
		memset(localBuffer,0,dev->nDataBytesPerChunk);
		smallIncreaseOk = 1;

		while(increase > 0 && smallIncreaseOk){
			thisWrite = increase;
			if(thisWrite > dev->nDataBytesPerChunk)
				thisWrite = dev->nDataBytesPerChunk;
			written = yaffs_DoWriteDataToFile(obj,localBuffer,pos,thisWrite,0);
			if(written == thisWrite){
				pos += thisWrite;
				increase -= thisWrite;
			} else
				smallIncreaseOk = 0;
		}

		yaffs_ReleaseTempBuffer(dev,localBuffer,__LINE__);

		/* If we were out of space then reverse any chunks we've added */		
		if(!smallIncreaseOk)
			yaffs_ResizeDown(obj, oldFileSize);
	}
	
	if (!smallIncreaseOk &&
		obj->parent &&
		obj->parent->objectId != YAFFS_OBJECTID_UNLINKED &&
		obj->parent->objectId != YAFFS_OBJECTID_DELETED){
		/* Write a hole start header with the old file size */
		yaffs_UpdateObjectHeader(obj, NULL, 0, 1, 0, NULL);
	}

	return result;

}


typedef struct {
	int seq;
	int block;
} yaffs_BlockIndex;


static int yaffs2_ybicmp(const void *a, const void *b)
{
	register int aseq = ((yaffs_BlockIndex *)a)->seq;
	register int bseq = ((yaffs_BlockIndex *)b)->seq;
	register int ablock = ((yaffs_BlockIndex *)a)->block;
	register int bblock = ((yaffs_BlockIndex *)b)->block;
	if (aseq == bseq)
		return ablock - bblock;
	else
		return aseq - bseq;
}

int yaffs2_ScanBackwards(yaffs_Device *dev)
{
	yaffs_ExtendedTags tags;
	int blk;
	int blockIterator;
	int startIterator;
	int endIterator;
	int nBlocksToScan = 0;

	int chunk;
	int result;
	int c;
	int deleted;
	yaffs_BlockState state;
	yaffs_Object *hardList = NULL;
	yaffs_BlockInfo *bi;
	__u32 sequenceNumber;
	yaffs_ObjectHeader *oh;
	yaffs_Object *in;
	yaffs_Object *parent;
	int nBlocks = dev->internalEndBlock - dev->internalStartBlock + 1;
	int itsUnlinked;
	__u8 *chunkData;

	int fileSize;
	int isShrink;
	int foundChunksInBlock;
	int equivalentObjectId;
	int alloc_failed = 0;


	yaffs_BlockIndex *blockIndex = NULL;
	int altBlockIndex = 0;

	T(YAFFS_TRACE_SCAN,
	  (TSTR
	   ("yaffs2_ScanBackwards starts  intstartblk %d intendblk %d..."
	    TENDSTR), dev->internalStartBlock, dev->internalEndBlock));


	dev->sequenceNumber = YAFFS_LOWEST_SEQUENCE_NUMBER;

	blockIndex = YMALLOC(nBlocks * sizeof(yaffs_BlockIndex));

	if (!blockIndex) {
		blockIndex = YMALLOC_ALT(nBlocks * sizeof(yaffs_BlockIndex));
		altBlockIndex = 1;
	}

	if (!blockIndex) {
		T(YAFFS_TRACE_SCAN,
		  (TSTR("yaffs2_ScanBackwards() could not allocate block index!" TENDSTR)));
		return YAFFS_FAIL;
	}

	dev->blocksInCheckpoint = 0;

	chunkData = yaffs_GetTempBuffer(dev, __LINE__);

	/* Scan all the blocks to determine their state */
	bi = dev->blockInfo;
	for (blk = dev->internalStartBlock; blk <= dev->internalEndBlock; blk++) {
		yaffs_ClearChunkBits(dev, blk);
		bi->pagesInUse = 0;
		bi->softDeletions = 0;

		yaffs_QueryInitialBlockState(dev, blk, &state, &sequenceNumber);

		bi->blockState = state;
		bi->sequenceNumber = sequenceNumber;

		if (bi->sequenceNumber == YAFFS_SEQUENCE_CHECKPOINT_DATA)
			bi->blockState = state = YAFFS_BLOCK_STATE_CHECKPOINT;
		if (bi->sequenceNumber == YAFFS_SEQUENCE_BAD_BLOCK)
			bi->blockState = state = YAFFS_BLOCK_STATE_DEAD;

		T(YAFFS_TRACE_SCAN_DEBUG,
		  (TSTR("Block scanning block %d state %d seq %d" TENDSTR), blk,
		   state, sequenceNumber));


		if (state == YAFFS_BLOCK_STATE_CHECKPOINT) {
			dev->blocksInCheckpoint++;

		} else if (state == YAFFS_BLOCK_STATE_DEAD) {
			T(YAFFS_TRACE_BAD_BLOCKS,
			  (TSTR("block %d is bad" TENDSTR), blk));
		} else if (state == YAFFS_BLOCK_STATE_EMPTY) {
			T(YAFFS_TRACE_SCAN_DEBUG,
			  (TSTR("Block empty " TENDSTR)));
			dev->nErasedBlocks++;
			dev->nFreeChunks += dev->param.nChunksPerBlock;
		} else if (state == YAFFS_BLOCK_STATE_NEEDS_SCANNING) {

			/* Determine the highest sequence number */
			if (sequenceNumber >= YAFFS_LOWEST_SEQUENCE_NUMBER &&
			    sequenceNumber < YAFFS_HIGHEST_SEQUENCE_NUMBER) {

				blockIndex[nBlocksToScan].seq = sequenceNumber;
				blockIndex[nBlocksToScan].block = blk;

				nBlocksToScan++;

				if (sequenceNumber >= dev->sequenceNumber)
					dev->sequenceNumber = sequenceNumber;
			} else {
				/* TODO: Nasty sequence number! */
				T(YAFFS_TRACE_SCAN,
				  (TSTR
				   ("Block scanning block %d has bad sequence number %d"
				    TENDSTR), blk, sequenceNumber));

			}
		}
		bi++;
	}

	T(YAFFS_TRACE_SCAN,
	(TSTR("%d blocks to be sorted..." TENDSTR), nBlocksToScan));



	YYIELD();

	/* Sort the blocks by sequence number*/
	yaffs_qsort(blockIndex, nBlocksToScan, sizeof(yaffs_BlockIndex), yaffs2_ybicmp);

	YYIELD();

	T(YAFFS_TRACE_SCAN, (TSTR("...done" TENDSTR)));

	/* Now scan the blocks looking at the data. */
	startIterator = 0;
	endIterator = nBlocksToScan - 1;
	T(YAFFS_TRACE_SCAN_DEBUG,
	  (TSTR("%d blocks to be scanned" TENDSTR), nBlocksToScan));

	/* For each block.... backwards */
	for (blockIterator = endIterator; !alloc_failed && blockIterator >= startIterator;
			blockIterator--) {
		/* Cooperative multitasking! This loop can run for so
		   long that watchdog timers expire. */
		YYIELD();

		/* get the block to scan in the correct order */
		blk = blockIndex[blockIterator].block;

		bi = yaffs_GetBlockInfo(dev, blk);


		state = bi->blockState;

		deleted = 0;

		/* For each chunk in each block that needs scanning.... */
		foundChunksInBlock = 0;
		for (c = dev->param.nChunksPerBlock - 1;
		     !alloc_failed && c >= 0 &&
		     (state == YAFFS_BLOCK_STATE_NEEDS_SCANNING ||
		      state == YAFFS_BLOCK_STATE_ALLOCATING); c--) {
			/* Scan backwards...
			 * Read the tags and decide what to do
			 */

			chunk = blk * dev->param.nChunksPerBlock + c;

			result = yaffs_ReadChunkWithTagsFromNAND(dev, chunk, NULL,
							&tags);

			/* Let's have a good look at this chunk... */

			if (!tags.chunkUsed) {
				/* An unassigned chunk in the block.
				 * If there are used chunks after this one, then
				 * it is a chunk that was skipped due to failing the erased
				 * check. Just skip it so that it can be deleted.
				 * But, more typically, We get here when this is an unallocated
				 * chunk and his means that either the block is empty or
				 * this is the one being allocated from
				 */

				if (foundChunksInBlock) {
					/* This is a chunk that was skipped due to failing the erased check */
				} else if (c == 0) {
					/* We're looking at the first chunk in the block so the block is unused */
					state = YAFFS_BLOCK_STATE_EMPTY;
					dev->nErasedBlocks++;
				} else {
					if (state == YAFFS_BLOCK_STATE_NEEDS_SCANNING ||
					    state == YAFFS_BLOCK_STATE_ALLOCATING) {
						if (dev->sequenceNumber == bi->sequenceNumber) {
							/* this is the block being allocated from */

							T(YAFFS_TRACE_SCAN,
							  (TSTR
							   (" Allocating from %d %d"
							    TENDSTR), blk, c));

							state = YAFFS_BLOCK_STATE_ALLOCATING;
							dev->allocationBlock = blk;
							dev->allocationPage = c;
							dev->allocationBlockFinder = blk;
						} else {
							/* This is a partially written block that is not
							 * the current allocation block.
							 */

							 T(YAFFS_TRACE_SCAN,
							 (TSTR("Partially written block %d detected" TENDSTR),
							 blk));
						}
					}
				}

				dev->nFreeChunks++;

			} else if (tags.eccResult == YAFFS_ECC_RESULT_UNFIXED) {
				T(YAFFS_TRACE_SCAN,
				  (TSTR(" Unfixed ECC in chunk(%d:%d), chunk ignored"TENDSTR),
				  blk, c));

				  dev->nFreeChunks++;

			} else if (tags.objectId > YAFFS_MAX_OBJECT_ID ||
				tags.chunkId > YAFFS_MAX_CHUNK_ID ||
				(tags.chunkId > 0 && tags.byteCount > dev->nDataBytesPerChunk) ||
				tags.sequenceNumber != bi->sequenceNumber ) {
				T(YAFFS_TRACE_SCAN,
				  (TSTR("Chunk (%d:%d) with bad tags:obj = %d, chunkId = %d, byteCount = %d, ignored"TENDSTR),
				  blk, c,tags.objectId, tags.chunkId, tags.byteCount));

				  dev->nFreeChunks++;

			} else if (tags.chunkId > 0) {
				/* chunkId > 0 so it is a data chunk... */
				unsigned int endpos;
				__u32 chunkBase =
				    (tags.chunkId - 1) * dev->nDataBytesPerChunk;

				foundChunksInBlock = 1;


				yaffs_SetChunkBit(dev, blk, c);
				bi->pagesInUse++;

				in = yaffs_FindOrCreateObjectByNumber(dev,
								      tags.
								      objectId,
								      YAFFS_OBJECT_TYPE_FILE);
				if (!in) {
					/* Out of memory */
					alloc_failed = 1;
				}

				if (in &&
				    in->variantType == YAFFS_OBJECT_TYPE_FILE
				    && chunkBase < in->variant.fileVariant.shrinkSize) {
					/* This has not been invalidated by a resize */
					if (!yaffs_PutChunkIntoFile(in, tags.chunkId, chunk, -1)) {
						alloc_failed = 1;
					}

					/* File size is calculated by looking at the data chunks if we have not
					 * seen an object header yet. Stop this practice once we find an object header.
					 */
					endpos = chunkBase + tags.byteCount;

					if (!in->valid &&	/* have not got an object header yet */
					    in->variant.fileVariant.scannedFileSize < endpos) {
						in->variant.fileVariant.scannedFileSize = endpos;
						in->variant.fileVariant.fileSize = endpos;
					}

				} else if (in) {
					/* This chunk has been invalidated by a resize, or a past file deletion
					 * so delete the chunk*/
					yaffs_DeleteChunk(dev, chunk, 1, __LINE__);

				}
			} else {
				/* chunkId == 0, so it is an ObjectHeader.
				 * Thus, we read in the object header and make the object
				 */
				foundChunksInBlock = 1;

				yaffs_SetChunkBit(dev, blk, c);
				bi->pagesInUse++;

				oh = NULL;
				in = NULL;

				if (tags.extraHeaderInfoAvailable) {
					in = yaffs_FindOrCreateObjectByNumber(dev,
						tags.objectId,
						tags.extraObjectType);
					if (!in)
						alloc_failed = 1;
				}

				if (!in ||
				    (!in->valid && dev->param.disableLazyLoad) ||
				    tags.extraShadows ||
				    (!in->valid &&
				    (tags.objectId == YAFFS_OBJECTID_ROOT ||
				     tags.objectId == YAFFS_OBJECTID_LOSTNFOUND))) {

					/* If we don't have  valid info then we need to read the chunk
					 * TODO In future we can probably defer reading the chunk and
					 * living with invalid data until needed.
					 */

					result = yaffs_ReadChunkWithTagsFromNAND(dev,
									chunk,
									chunkData,
									NULL);

					oh = (yaffs_ObjectHeader *) chunkData;

					if (dev->param.inbandTags) {
						/* Fix up the header if they got corrupted by inband tags */
						oh->shadowsObject = oh->inbandShadowsObject;
						oh->isShrink = oh->inbandIsShrink;
					}

					if (!in) {
						in = yaffs_FindOrCreateObjectByNumber(dev, tags.objectId, oh->type);
						if (!in)
							alloc_failed = 1;
					}

				}

				if (!in) {
					/* TODO Hoosterman we have a problem! */
					T(YAFFS_TRACE_ERROR,
					  (TSTR
					   ("yaffs tragedy: Could not make object for object  %d at chunk %d during scan"
					    TENDSTR), tags.objectId, chunk));
					continue;
				}

				if (in->valid) {
					/* We have already filled this one.
					 * We have a duplicate that will be discarded, but
					 * we first have to suck out resize info if it is a file.
					 */

					if ((in->variantType == YAFFS_OBJECT_TYPE_FILE) &&
					     ((oh &&
					       oh->type == YAFFS_OBJECT_TYPE_FILE) ||
					      (tags.extraHeaderInfoAvailable  &&
					       tags.extraObjectType == YAFFS_OBJECT_TYPE_FILE))) {
						__u32 thisSize =
						    (oh) ? oh->fileSize : tags.
						    extraFileLength;
						__u32 parentObjectId =
						    (oh) ? oh->
						    parentObjectId : tags.
						    extraParentObjectId;


						isShrink =
						    (oh) ? oh->isShrink : tags.
						    extraIsShrinkHeader;

						/* If it is deleted (unlinked at start also means deleted)
						 * we treat the file size as being zeroed at this point.
						 */
						if (parentObjectId ==
						    YAFFS_OBJECTID_DELETED
						    || parentObjectId ==
						    YAFFS_OBJECTID_UNLINKED) {
							thisSize = 0;
							isShrink = 1;
						}

						if (isShrink && in->variant.fileVariant.shrinkSize > thisSize)
							in->variant.fileVariant.shrinkSize = thisSize;

						if (isShrink)
							bi->hasShrinkHeader = 1;

					}
					/* Use existing - destroy this one. */
					yaffs_DeleteChunk(dev, chunk, 1, __LINE__);

				}

				if (!in->valid && in->variantType !=
				    (oh ? oh->type : tags.extraObjectType))
					T(YAFFS_TRACE_ERROR, (
						TSTR("yaffs tragedy: Bad object type, "
					    TCONT("%d != %d, for object %d at chunk ")
					    TCONT("%d during scan")
						TENDSTR), oh ?
					    oh->type : tags.extraObjectType,
					    in->variantType, tags.objectId,
					    chunk));

				if (!in->valid &&
				    (tags.objectId == YAFFS_OBJECTID_ROOT ||
				     tags.objectId ==
				     YAFFS_OBJECTID_LOSTNFOUND)) {
					/* We only load some info, don't fiddle with directory structure */
					in->valid = 1;

					if (oh) {

						in->yst_mode = oh->yst_mode;
#ifdef CONFIG_YAFFS_WINCE
						in->win_atime[0] = oh->win_atime[0];
						in->win_ctime[0] = oh->win_ctime[0];
						in->win_mtime[0] = oh->win_mtime[0];
						in->win_atime[1] = oh->win_atime[1];
						in->win_ctime[1] = oh->win_ctime[1];
						in->win_mtime[1] = oh->win_mtime[1];
#else
						in->yst_uid = oh->yst_uid;
						in->yst_gid = oh->yst_gid;
						in->yst_atime = oh->yst_atime;
						in->yst_mtime = oh->yst_mtime;
						in->yst_ctime = oh->yst_ctime;
						in->yst_rdev = oh->yst_rdev;

						in->lazyLoaded = 0;

#endif
					} else
						in->lazyLoaded = 1;

					in->hdrChunk = chunk;

				} else if (!in->valid) {
					/* we need to load this info */

					in->valid = 1;
					in->hdrChunk = chunk;

					if (oh) {
						in->variantType = oh->type;

						in->yst_mode = oh->yst_mode;
#ifdef CONFIG_YAFFS_WINCE
						in->win_atime[0] = oh->win_atime[0];
						in->win_ctime[0] = oh->win_ctime[0];
						in->win_mtime[0] = oh->win_mtime[0];
						in->win_atime[1] = oh->win_atime[1];
						in->win_ctime[1] = oh->win_ctime[1];
						in->win_mtime[1] = oh->win_mtime[1];
#else
						in->yst_uid = oh->yst_uid;
						in->yst_gid = oh->yst_gid;
						in->yst_atime = oh->yst_atime;
						in->yst_mtime = oh->yst_mtime;
						in->yst_ctime = oh->yst_ctime;
						in->yst_rdev = oh->yst_rdev;
#endif

						if (oh->shadowsObject > 0)
							yaffs_HandleShadowedObject(dev,
									   oh->
									   shadowsObject,
									   1);
							


						yaffs_SetObjectNameFromOH(in, oh);
						parent =
						    yaffs_FindOrCreateObjectByNumber
							(dev, oh->parentObjectId,
							 YAFFS_OBJECT_TYPE_DIRECTORY);

						 fileSize = oh->fileSize;
						 isShrink = oh->isShrink;
						 equivalentObjectId = oh->equivalentObjectId;

					} else {
						in->variantType = tags.extraObjectType;
						parent =
						    yaffs_FindOrCreateObjectByNumber
							(dev, tags.extraParentObjectId,
							 YAFFS_OBJECT_TYPE_DIRECTORY);
						 fileSize = tags.extraFileLength;
						 isShrink = tags.extraIsShrinkHeader;
						 equivalentObjectId = tags.extraEquivalentObjectId;
						in->lazyLoaded = 1;

					}
					in->dirty = 0;

					if (!parent)
						alloc_failed = 1;

					/* directory stuff...
					 * hook up to parent
					 */

					if (parent && parent->variantType ==
					    YAFFS_OBJECT_TYPE_UNKNOWN) {
						/* Set up as a directory */
						parent->variantType =
							YAFFS_OBJECT_TYPE_DIRECTORY;
						YINIT_LIST_HEAD(&parent->variant.
							directoryVariant.
							children);
					} else if (!parent || parent->variantType !=
						   YAFFS_OBJECT_TYPE_DIRECTORY) {
						/* Hoosterman, another problem....
						 * We're trying to use a non-directory as a directory
						 */

						T(YAFFS_TRACE_ERROR,
						  (TSTR
						   ("yaffs tragedy: attempting to use non-directory as a directory in scan. Put in lost+found."
						    TENDSTR)));
						parent = dev->lostNFoundDir;
					}

					yaffs_AddObjectToDirectory(parent, in);

					itsUnlinked = (parent == dev->deletedDir) ||
						      (parent == dev->unlinkedDir);

					if (isShrink) {
						/* Mark the block as having a shrinkHeader */
						bi->hasShrinkHeader = 1;
					}

					/* Note re hardlinks.
					 * Since we might scan a hardlink before its equivalent object is scanned
					 * we put them all in a list.
					 * After scanning is complete, we should have all the objects, so we run
					 * through this list and fix up all the chains.
					 */

					switch (in->variantType) {
					case YAFFS_OBJECT_TYPE_UNKNOWN:
						/* Todo got a problem */
						break;
					case YAFFS_OBJECT_TYPE_FILE:

						if (in->variant.fileVariant.
						    scannedFileSize < fileSize) {
							/* This covers the case where the file size is greater
							 * than where the data is
							 * This will happen if the file is resized to be larger
							 * than its current data extents.
							 */
							in->variant.fileVariant.fileSize = fileSize;
							in->variant.fileVariant.scannedFileSize = fileSize;
						}

						if (in->variant.fileVariant.shrinkSize > fileSize)
							in->variant.fileVariant.shrinkSize = fileSize;
				

						break;
					case YAFFS_OBJECT_TYPE_HARDLINK:
						if (!itsUnlinked) {
							in->variant.hardLinkVariant.equivalentObjectId =
								equivalentObjectId;
							in->hardLinks.next =
								(struct ylist_head *) hardList;
							hardList = in;
						}
						break;
					case YAFFS_OBJECT_TYPE_DIRECTORY:
						/* Do nothing */
						break;
					case YAFFS_OBJECT_TYPE_SPECIAL:
						/* Do nothing */
						break;
					case YAFFS_OBJECT_TYPE_SYMLINK:
						if (oh) {
							in->variant.symLinkVariant.alias =
								yaffs_CloneString(oh->alias);
							if (!in->variant.symLinkVariant.alias)
								alloc_failed = 1;
						}
						break;
					}

				}

			}

		} /* End of scanning for each chunk */

		if (state == YAFFS_BLOCK_STATE_NEEDS_SCANNING) {
			/* If we got this far while scanning, then the block is fully allocated. */
			state = YAFFS_BLOCK_STATE_FULL;
		}


		bi->blockState = state;

		/* Now let's see if it was dirty */
		if (bi->pagesInUse == 0 &&
		    !bi->hasShrinkHeader &&
		    bi->blockState == YAFFS_BLOCK_STATE_FULL) {
			yaffs_BlockBecameDirty(dev, blk);
		}

	}
	
	yaffs_SkipRestOfBlock(dev);

	if (altBlockIndex)
		YFREE_ALT(blockIndex);
	else
		YFREE(blockIndex);

	/* Ok, we've done all the scanning.
	 * Fix up the hard link chains.
	 * We should now have scanned all the objects, now it's time to add these
	 * hardlinks.
	 */
	yaffs_HardlinkFixup(dev, hardList);


	yaffs_ReleaseTempBuffer(dev, chunkData, __LINE__);

	if (alloc_failed)
		return YAFFS_FAIL;

	T(YAFFS_TRACE_SCAN, (TSTR("yaffs2_ScanBackwards ends" TENDSTR)));

	return YAFFS_OK;
}
