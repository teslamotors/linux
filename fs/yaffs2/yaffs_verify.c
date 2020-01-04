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


#include "yaffs_verify.h"
#include "yaffs_trace.h"
#include "yaffs_bitmap.h"
#include "yaffs_getblockinfo.h"
#include "yaffs_nand.h"

int yaffs_SkipVerification(yaffs_Device *dev)
{
	dev=dev;
	return !(yaffs_traceMask & (YAFFS_TRACE_VERIFY | YAFFS_TRACE_VERIFY_FULL));
}

static int yaffs_SkipFullVerification(yaffs_Device *dev)
{
	dev=dev;
	return !(yaffs_traceMask & (YAFFS_TRACE_VERIFY_FULL));
}

static int yaffs_SkipNANDVerification(yaffs_Device *dev)
{
	dev=dev;
	return !(yaffs_traceMask & (YAFFS_TRACE_VERIFY_NAND));
}


static const char *blockStateName[] = {
"Unknown",
"Needs scanning",
"Scanning",
"Empty",
"Allocating",
"Full",
"Dirty",
"Checkpoint",
"Collecting",
"Dead"
};


void yaffs_VerifyBlock(yaffs_Device *dev, yaffs_BlockInfo *bi, int n)
{
	int actuallyUsed;
	int inUse;

	if (yaffs_SkipVerification(dev))
		return;

	/* Report illegal runtime states */
	if (bi->blockState >= YAFFS_NUMBER_OF_BLOCK_STATES)
		T(YAFFS_TRACE_VERIFY, (TSTR("Block %d has undefined state %d"TENDSTR), n, bi->blockState));

	switch (bi->blockState) {
	case YAFFS_BLOCK_STATE_UNKNOWN:
	case YAFFS_BLOCK_STATE_SCANNING:
	case YAFFS_BLOCK_STATE_NEEDS_SCANNING:
		T(YAFFS_TRACE_VERIFY, (TSTR("Block %d has bad run-state %s"TENDSTR),
		n, blockStateName[bi->blockState]));
	}

	/* Check pages in use and soft deletions are legal */

	actuallyUsed = bi->pagesInUse - bi->softDeletions;

	if (bi->pagesInUse < 0 || bi->pagesInUse > dev->param.nChunksPerBlock ||
	   bi->softDeletions < 0 || bi->softDeletions > dev->param.nChunksPerBlock ||
	   actuallyUsed < 0 || actuallyUsed > dev->param.nChunksPerBlock)
		T(YAFFS_TRACE_VERIFY, (TSTR("Block %d has illegal values pagesInUsed %d softDeletions %d"TENDSTR),
		n, bi->pagesInUse, bi->softDeletions));


	/* Check chunk bitmap legal */
	inUse = yaffs_CountChunkBits(dev, n);
	if (inUse != bi->pagesInUse)
		T(YAFFS_TRACE_VERIFY, (TSTR("Block %d has inconsistent values pagesInUse %d counted chunk bits %d"TENDSTR),
			n, bi->pagesInUse, inUse));

}



void yaffs_VerifyCollectedBlock(yaffs_Device *dev, yaffs_BlockInfo *bi, int n)
{
	yaffs_VerifyBlock(dev, bi, n);

	/* After collection the block should be in the erased state */

	if (bi->blockState != YAFFS_BLOCK_STATE_COLLECTING &&
			bi->blockState != YAFFS_BLOCK_STATE_EMPTY) {
		T(YAFFS_TRACE_ERROR, (TSTR("Block %d is in state %d after gc, should be erased"TENDSTR),
			n, bi->blockState));
	}
}

void yaffs_VerifyBlocks(yaffs_Device *dev)
{
	int i;
	int nBlocksPerState[YAFFS_NUMBER_OF_BLOCK_STATES];
	int nIllegalBlockStates = 0;

	if (yaffs_SkipVerification(dev))
		return;

	memset(nBlocksPerState, 0, sizeof(nBlocksPerState));

	for (i = dev->internalStartBlock; i <= dev->internalEndBlock; i++) {
		yaffs_BlockInfo *bi = yaffs_GetBlockInfo(dev, i);
		yaffs_VerifyBlock(dev, bi, i);

		if (bi->blockState < YAFFS_NUMBER_OF_BLOCK_STATES)
			nBlocksPerState[bi->blockState]++;
		else
			nIllegalBlockStates++;
	}

	T(YAFFS_TRACE_VERIFY, (TSTR(""TENDSTR)));
	T(YAFFS_TRACE_VERIFY, (TSTR("Block summary"TENDSTR)));

	T(YAFFS_TRACE_VERIFY, (TSTR("%d blocks have illegal states"TENDSTR), nIllegalBlockStates));
	if (nBlocksPerState[YAFFS_BLOCK_STATE_ALLOCATING] > 1)
		T(YAFFS_TRACE_VERIFY, (TSTR("Too many allocating blocks"TENDSTR)));

	for (i = 0; i < YAFFS_NUMBER_OF_BLOCK_STATES; i++)
		T(YAFFS_TRACE_VERIFY,
		  (TSTR("%s %d blocks"TENDSTR),
		  blockStateName[i], nBlocksPerState[i]));

	if (dev->blocksInCheckpoint != nBlocksPerState[YAFFS_BLOCK_STATE_CHECKPOINT])
		T(YAFFS_TRACE_VERIFY,
		 (TSTR("Checkpoint block count wrong dev %d count %d"TENDSTR),
		 dev->blocksInCheckpoint, nBlocksPerState[YAFFS_BLOCK_STATE_CHECKPOINT]));

	if (dev->nErasedBlocks != nBlocksPerState[YAFFS_BLOCK_STATE_EMPTY])
		T(YAFFS_TRACE_VERIFY,
		 (TSTR("Erased block count wrong dev %d count %d"TENDSTR),
		 dev->nErasedBlocks, nBlocksPerState[YAFFS_BLOCK_STATE_EMPTY]));

	if (nBlocksPerState[YAFFS_BLOCK_STATE_COLLECTING] > 1)
		T(YAFFS_TRACE_VERIFY,
		 (TSTR("Too many collecting blocks %d (max is 1)"TENDSTR),
		 nBlocksPerState[YAFFS_BLOCK_STATE_COLLECTING]));

	T(YAFFS_TRACE_VERIFY, (TSTR(""TENDSTR)));

}

/*
 * Verify the object header. oh must be valid, but obj and tags may be NULL in which
 * case those tests will not be performed.
 */
void yaffs_VerifyObjectHeader(yaffs_Object *obj, yaffs_ObjectHeader *oh, yaffs_ExtendedTags *tags, int parentCheck)
{
	if (obj && yaffs_SkipVerification(obj->myDev))
		return;

	if (!(tags && obj && oh)) {
		T(YAFFS_TRACE_VERIFY,
				(TSTR("Verifying object header tags %p obj %p oh %p"TENDSTR),
				tags, obj, oh));
		return;
	}

	if (oh->type <= YAFFS_OBJECT_TYPE_UNKNOWN ||
			oh->type > YAFFS_OBJECT_TYPE_MAX)
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d header type is illegal value 0x%x"TENDSTR),
			tags->objectId, oh->type));

	if (tags->objectId != obj->objectId)
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d header mismatch objectId %d"TENDSTR),
			tags->objectId, obj->objectId));


	/*
	 * Check that the object's parent ids match if parentCheck requested.
	 *
	 * Tests do not apply to the root object.
	 */

	if (parentCheck && tags->objectId > 1 && !obj->parent)
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d header mismatch parentId %d obj->parent is NULL"TENDSTR),
			tags->objectId, oh->parentObjectId));

	if (parentCheck && obj->parent &&
			oh->parentObjectId != obj->parent->objectId &&
			(oh->parentObjectId != YAFFS_OBJECTID_UNLINKED ||
			obj->parent->objectId != YAFFS_OBJECTID_DELETED))
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d header mismatch parentId %d parentObjectId %d"TENDSTR),
			tags->objectId, oh->parentObjectId, obj->parent->objectId));

	if (tags->objectId > 1 && oh->name[0] == 0) /* Null name */
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d header name is NULL"TENDSTR),
			obj->objectId));

	if (tags->objectId > 1 && ((__u8)(oh->name[0])) == 0xff) /* Trashed name */
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d header name is 0xFF"TENDSTR),
			obj->objectId));
}


#if 0
/* Not being used, but don't want to throw away yet */
int yaffs_VerifyTnodeWorker(yaffs_Object *obj, yaffs_Tnode *tn,
					__u32 level, int chunkOffset)
{
	int i;
	yaffs_Device *dev = obj->myDev;
	int ok = 1;

	if (tn) {
		if (level > 0) {

			for (i = 0; i < YAFFS_NTNODES_INTERNAL && ok; i++) {
				if (tn->internal[i]) {
					ok = yaffs_VerifyTnodeWorker(obj,
							tn->internal[i],
							level - 1,
							(chunkOffset<<YAFFS_TNODES_INTERNAL_BITS) + i);
				}
			}
		} else if (level == 0) {
			yaffs_ExtendedTags tags;
			__u32 objectId = obj->objectId;

			chunkOffset <<=  YAFFS_TNODES_LEVEL0_BITS;

			for (i = 0; i < YAFFS_NTNODES_LEVEL0; i++) {
				__u32 theChunk = yaffs_GetChunkGroupBase(dev, tn, i);

				if (theChunk > 0) {
					/* T(~0,(TSTR("verifying (%d:%d) %d"TENDSTR),tags.objectId,tags.chunkId,theChunk)); */
					yaffs_ReadChunkWithTagsFromNAND(dev, theChunk, NULL, &tags);
					if (tags.objectId != objectId || tags.chunkId != chunkOffset) {
						T(~0, (TSTR("Object %d chunkId %d NAND mismatch chunk %d tags (%d:%d)"TENDSTR),
							objectId, chunkOffset, theChunk,
							tags.objectId, tags.chunkId));
					}
				}
				chunkOffset++;
			}
		}
	}

	return ok;

}

#endif

void yaffs_VerifyFile(yaffs_Object *obj)
{
	int requiredTallness;
	int actualTallness;
	__u32 lastChunk;
	__u32 x;
	__u32 i;
	yaffs_Device *dev;
	yaffs_ExtendedTags tags;
	yaffs_Tnode *tn;
	__u32 objectId;

	if (!obj)
		return;

	if (yaffs_SkipVerification(obj->myDev))
		return;

	dev = obj->myDev;
	objectId = obj->objectId;

	/* Check file size is consistent with tnode depth */
	lastChunk =  obj->variant.fileVariant.fileSize / dev->nDataBytesPerChunk + 1;
	x = lastChunk >> YAFFS_TNODES_LEVEL0_BITS;
	requiredTallness = 0;
	while (x > 0) {
		x >>= YAFFS_TNODES_INTERNAL_BITS;
		requiredTallness++;
	}

	actualTallness = obj->variant.fileVariant.topLevel;

	/* Check that the chunks in the tnode tree are all correct.
	 * We do this by scanning through the tnode tree and
	 * checking the tags for every chunk match.
	 */

	if (yaffs_SkipNANDVerification(dev))
		return;

	for (i = 1; i <= lastChunk; i++) {
		tn = yaffs_FindLevel0Tnode(dev, &obj->variant.fileVariant, i);

		if (tn) {
			__u32 theChunk = yaffs_GetChunkGroupBase(dev, tn, i);
			if (theChunk > 0) {
				/* T(~0,(TSTR("verifying (%d:%d) %d"TENDSTR),objectId,i,theChunk)); */
				yaffs_ReadChunkWithTagsFromNAND(dev, theChunk, NULL, &tags);
				if (tags.objectId != objectId || tags.chunkId != i) {
					T(~0, (TSTR("Object %d chunkId %d NAND mismatch chunk %d tags (%d:%d)"TENDSTR),
						objectId, i, theChunk,
						tags.objectId, tags.chunkId));
				}
			}
		}
	}
}


void yaffs_VerifyHardLink(yaffs_Object *obj)
{
	if (obj && yaffs_SkipVerification(obj->myDev))
		return;

	/* Verify sane equivalent object */
}

void yaffs_VerifySymlink(yaffs_Object *obj)
{
	if (obj && yaffs_SkipVerification(obj->myDev))
		return;

	/* Verify symlink string */
}

void yaffs_VerifySpecial(yaffs_Object *obj)
{
	if (obj && yaffs_SkipVerification(obj->myDev))
		return;
}

void yaffs_VerifyObject(yaffs_Object *obj)
{
	yaffs_Device *dev;

	__u32 chunkMin;
	__u32 chunkMax;

	__u32 chunkIdOk;
	__u32 chunkInRange;
	__u32 chunkShouldNotBeDeleted;
	__u32 chunkValid;

	if (!obj)
		return;

	if (obj->beingCreated)
		return;

	dev = obj->myDev;

	if (yaffs_SkipVerification(dev))
		return;

	/* Check sane object header chunk */

	chunkMin = dev->internalStartBlock * dev->param.nChunksPerBlock;
	chunkMax = (dev->internalEndBlock+1) * dev->param.nChunksPerBlock - 1;

	chunkInRange = (((unsigned)(obj->hdrChunk)) >= chunkMin && ((unsigned)(obj->hdrChunk)) <= chunkMax);
	chunkIdOk = chunkInRange || (obj->hdrChunk == 0);
	chunkValid = chunkInRange &&
			yaffs_CheckChunkBit(dev,
					obj->hdrChunk / dev->param.nChunksPerBlock,
					obj->hdrChunk % dev->param.nChunksPerBlock);
	chunkShouldNotBeDeleted = chunkInRange && !chunkValid;

	if (!obj->fake &&
			(!chunkIdOk || chunkShouldNotBeDeleted)) {
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d has chunkId %d %s %s"TENDSTR),
			obj->objectId, obj->hdrChunk,
			chunkIdOk ? "" : ",out of range",
			chunkShouldNotBeDeleted ? ",marked as deleted" : ""));
	}

	if (chunkValid && !yaffs_SkipNANDVerification(dev)) {
		yaffs_ExtendedTags tags;
		yaffs_ObjectHeader *oh;
		__u8 *buffer = yaffs_GetTempBuffer(dev, __LINE__);

		oh = (yaffs_ObjectHeader *)buffer;

		yaffs_ReadChunkWithTagsFromNAND(dev, obj->hdrChunk, buffer,
				&tags);

		yaffs_VerifyObjectHeader(obj, oh, &tags, 1);

		yaffs_ReleaseTempBuffer(dev, buffer, __LINE__);
	}

	/* Verify it has a parent */
	if (obj && !obj->fake &&
			(!obj->parent || obj->parent->myDev != dev)) {
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d has parent pointer %p which does not look like an object"TENDSTR),
			obj->objectId, obj->parent));
	}

	/* Verify parent is a directory */
	if (obj->parent && obj->parent->variantType != YAFFS_OBJECT_TYPE_DIRECTORY) {
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d's parent is not a directory (type %d)"TENDSTR),
			obj->objectId, obj->parent->variantType));
	}

	switch (obj->variantType) {
	case YAFFS_OBJECT_TYPE_FILE:
		yaffs_VerifyFile(obj);
		break;
	case YAFFS_OBJECT_TYPE_SYMLINK:
		yaffs_VerifySymlink(obj);
		break;
	case YAFFS_OBJECT_TYPE_DIRECTORY:
		yaffs_VerifyDirectory(obj);
		break;
	case YAFFS_OBJECT_TYPE_HARDLINK:
		yaffs_VerifyHardLink(obj);
		break;
	case YAFFS_OBJECT_TYPE_SPECIAL:
		yaffs_VerifySpecial(obj);
		break;
	case YAFFS_OBJECT_TYPE_UNKNOWN:
	default:
		T(YAFFS_TRACE_VERIFY,
		(TSTR("Obj %d has illegaltype %d"TENDSTR),
		obj->objectId, obj->variantType));
		break;
	}
}

void yaffs_VerifyObjects(yaffs_Device *dev)
{
	yaffs_Object *obj;
	int i;
	struct ylist_head *lh;

	if (yaffs_SkipVerification(dev))
		return;

	/* Iterate through the objects in each hash entry */

	for (i = 0; i <  YAFFS_NOBJECT_BUCKETS; i++) {
		ylist_for_each(lh, &dev->objectBucket[i].list) {
			if (lh) {
				obj = ylist_entry(lh, yaffs_Object, hashLink);
				yaffs_VerifyObject(obj);
			}
		}
	}
}


void yaffs_VerifyObjectInDirectory(yaffs_Object *obj)
{
	struct ylist_head *lh;
	yaffs_Object *listObj;

	int count = 0;

	if (!obj) {
		T(YAFFS_TRACE_ALWAYS, (TSTR("No object to verify" TENDSTR)));
		YBUG();
		return;
	}

	if (yaffs_SkipVerification(obj->myDev))
		return;

	if (!obj->parent) {
		T(YAFFS_TRACE_ALWAYS, (TSTR("Object does not have parent" TENDSTR)));
		YBUG();
		return;
	}

	if (obj->parent->variantType != YAFFS_OBJECT_TYPE_DIRECTORY) {
		T(YAFFS_TRACE_ALWAYS, (TSTR("Parent is not directory" TENDSTR)));
		YBUG();
	}

	/* Iterate through the objects in each hash entry */

	ylist_for_each(lh, &obj->parent->variant.directoryVariant.children) {
		if (lh) {
			listObj = ylist_entry(lh, yaffs_Object, siblings);
			yaffs_VerifyObject(listObj);
			if (obj == listObj)
				count++;
		}
	 }

	if (count != 1) {
		T(YAFFS_TRACE_ALWAYS, (TSTR("Object in directory %d times" TENDSTR), count));
		YBUG();
	}
}

void yaffs_VerifyDirectory(yaffs_Object *directory)
{
	struct ylist_head *lh;
	yaffs_Object *listObj;

	if (!directory) {
		YBUG();
		return;
	}

	if (yaffs_SkipFullVerification(directory->myDev))
		return;

	if (directory->variantType != YAFFS_OBJECT_TYPE_DIRECTORY) {
		T(YAFFS_TRACE_ALWAYS, (TSTR("Directory has wrong type: %d" TENDSTR), directory->variantType));
		YBUG();
	}

	/* Iterate through the objects in each hash entry */

	ylist_for_each(lh, &directory->variant.directoryVariant.children) {
		if (lh) {
			listObj = ylist_entry(lh, yaffs_Object, siblings);
			if (listObj->parent != directory) {
				T(YAFFS_TRACE_ALWAYS, (TSTR("Object in directory list has wrong parent %p" TENDSTR), listObj->parent));
				YBUG();
			}
			yaffs_VerifyObjectInDirectory(listObj);
		}
	}
}

static int yaffs_freeVerificationFailures;

void yaffs_VerifyFreeChunks(yaffs_Device *dev)
{
	int counted;
	int difference;

	if (yaffs_SkipVerification(dev))
		return;

	counted = yaffs_CountFreeChunks(dev);

	difference = dev->nFreeChunks - counted;

	if (difference) {
		T(YAFFS_TRACE_ALWAYS,
		  (TSTR("Freechunks verification failure %d %d %d" TENDSTR),
		   dev->nFreeChunks, counted, difference));
		yaffs_freeVerificationFailures++;
	}
}

int yaffs_VerifyFileSanity(yaffs_Object *in)
{
#if 0
	int chunk;
	int nChunks;
	int fSize;
	int failed = 0;
	int objId;
	yaffs_Tnode *tn;
	yaffs_Tags localTags;
	yaffs_Tags *tags = &localTags;
	int theChunk;
	int chunkDeleted;

	if (in->variantType != YAFFS_OBJECT_TYPE_FILE)
		return YAFFS_FAIL;

	objId = in->objectId;
	fSize = in->variant.fileVariant.fileSize;
	nChunks =
	    (fSize + in->myDev->nDataBytesPerChunk - 1) / in->myDev->nDataBytesPerChunk;

	for (chunk = 1; chunk <= nChunks; chunk++) {
		tn = yaffs_FindLevel0Tnode(in->myDev, &in->variant.fileVariant,
					   chunk);

		if (tn) {

			theChunk = yaffs_GetChunkGroupBase(dev, tn, chunk);

			if (yaffs_CheckChunkBits
			    (dev, theChunk / dev->param.nChunksPerBlock,
			     theChunk % dev->param.nChunksPerBlock)) {

				yaffs_ReadChunkTagsFromNAND(in->myDev, theChunk,
							    tags,
							    &chunkDeleted);
				if (yaffs_TagsMatch
				    (tags, in->objectId, chunk, chunkDeleted)) {
					/* found it; */

				}
			} else {

				failed = 1;
			}

		} else {
			/* T(("No level 0 found for %d\n", chunk)); */
		}
	}

	return failed ? YAFFS_FAIL : YAFFS_OK;
#else
	in=in;
	return YAFFS_OK;
#endif
}
