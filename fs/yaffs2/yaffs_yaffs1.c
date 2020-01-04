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

#include "yaffs_yaffs1.h"
#include "yportenv.h"
#include "yaffs_trace.h"
#include "yaffs_bitmap.h"
#include "yaffs_getblockinfo.h"
#include "yaffs_nand.h"


int yaffs1_Scan(yaffs_Device *dev)
{
	yaffs_ExtendedTags tags;
	int blk;
	int blockIterator;
	int startIterator;
	int endIterator;
	int result;

	int chunk;
	int c;
	int deleted;
	yaffs_BlockState state;
	yaffs_Object *hardList = NULL;
	yaffs_BlockInfo *bi;
	__u32 sequenceNumber;
	yaffs_ObjectHeader *oh;
	yaffs_Object *in;
	yaffs_Object *parent;

	int alloc_failed = 0;

	struct yaffs_ShadowFixerStruct *shadowFixerList = NULL;


	__u8 *chunkData;



	T(YAFFS_TRACE_SCAN,
	  (TSTR("yaffs1_Scan starts  intstartblk %d intendblk %d..." TENDSTR),
	   dev->internalStartBlock, dev->internalEndBlock));

	chunkData = yaffs_GetTempBuffer(dev, __LINE__);

	dev->sequenceNumber = YAFFS_LOWEST_SEQUENCE_NUMBER;

	/* Scan all the blocks to determine their state */
	bi = dev->blockInfo;
	for (blk = dev->internalStartBlock; blk <= dev->internalEndBlock; blk++) {
		yaffs_ClearChunkBits(dev, blk);
		bi->pagesInUse = 0;
		bi->softDeletions = 0;

		yaffs_QueryInitialBlockState(dev, blk, &state, &sequenceNumber);

		bi->blockState = state;
		bi->sequenceNumber = sequenceNumber;

		if (bi->sequenceNumber == YAFFS_SEQUENCE_BAD_BLOCK)
			bi->blockState = state = YAFFS_BLOCK_STATE_DEAD;

		T(YAFFS_TRACE_SCAN_DEBUG,
		  (TSTR("Block scanning block %d state %d seq %d" TENDSTR), blk,
		   state, sequenceNumber));

		if (state == YAFFS_BLOCK_STATE_DEAD) {
			T(YAFFS_TRACE_BAD_BLOCKS,
			  (TSTR("block %d is bad" TENDSTR), blk));
		} else if (state == YAFFS_BLOCK_STATE_EMPTY) {
			T(YAFFS_TRACE_SCAN_DEBUG,
			  (TSTR("Block empty " TENDSTR)));
			dev->nErasedBlocks++;
			dev->nFreeChunks += dev->param.nChunksPerBlock;
		}
		bi++;
	}

	startIterator = dev->internalStartBlock;
	endIterator = dev->internalEndBlock;

	/* For each block.... */
	for (blockIterator = startIterator; !alloc_failed && blockIterator <= endIterator;
	     blockIterator++) {

		YYIELD();

		YYIELD();

		blk = blockIterator;

		bi = yaffs_GetBlockInfo(dev, blk);
		state = bi->blockState;

		deleted = 0;

		/* For each chunk in each block that needs scanning....*/
		for (c = 0; !alloc_failed && c < dev->param.nChunksPerBlock &&
		     state == YAFFS_BLOCK_STATE_NEEDS_SCANNING; c++) {
			/* Read the tags and decide what to do */
			chunk = blk * dev->param.nChunksPerBlock + c;

			result = yaffs_ReadChunkWithTagsFromNAND(dev, chunk, NULL,
							&tags);

			/* Let's have a good look at this chunk... */

			if (tags.eccResult == YAFFS_ECC_RESULT_UNFIXED || tags.chunkDeleted) {
				/* YAFFS1 only...
				 * A deleted chunk
				 */
				deleted++;
				dev->nFreeChunks++;
				/*T((" %d %d deleted\n",blk,c)); */
			} else if (!tags.chunkUsed) {
				/* An unassigned chunk in the block
				 * This means that either the block is empty or
				 * this is the one being allocated from
				 */

				if (c == 0) {
					/* We're looking at the first chunk in the block so the block is unused */
					state = YAFFS_BLOCK_STATE_EMPTY;
					dev->nErasedBlocks++;
				} else {
					/* this is the block being allocated from */
					T(YAFFS_TRACE_SCAN,
					  (TSTR
					   (" Allocating from %d %d" TENDSTR),
					   blk, c));
					state = YAFFS_BLOCK_STATE_ALLOCATING;
					dev->allocationBlock = blk;
					dev->allocationPage = c;
					dev->allocationBlockFinder = blk;
					/* Set block finder here to encourage the allocator to go forth from here. */

				}

				dev->nFreeChunks += (dev->param.nChunksPerBlock - c);
			} else if (tags.chunkId > 0) {
				/* chunkId > 0 so it is a data chunk... */
				unsigned int endpos;

				yaffs_SetChunkBit(dev, blk, c);
				bi->pagesInUse++;

				in = yaffs_FindOrCreateObjectByNumber(dev,
								      tags.
								      objectId,
								      YAFFS_OBJECT_TYPE_FILE);
				/* PutChunkIntoFile checks for a clash (two data chunks with
				 * the same chunkId).
				 */

				if (!in)
					alloc_failed = 1;

				if (in) {
					if (!yaffs_PutChunkIntoFile(in, tags.chunkId, chunk, 1))
						alloc_failed = 1;
				}

				endpos =
				    (tags.chunkId - 1) * dev->nDataBytesPerChunk +
				    tags.byteCount;
				if (in &&
				    in->variantType == YAFFS_OBJECT_TYPE_FILE
				    && in->variant.fileVariant.scannedFileSize <
				    endpos) {
					in->variant.fileVariant.
					    scannedFileSize = endpos;
					if (!dev->param.useHeaderFileSize) {
						in->variant.fileVariant.
						    fileSize =
						    in->variant.fileVariant.
						    scannedFileSize;
					}

				}
				/* T((" %d %d data %d %d\n",blk,c,tags.objectId,tags.chunkId));   */
			} else {
				/* chunkId == 0, so it is an ObjectHeader.
				 * Thus, we read in the object header and make the object
				 */
				yaffs_SetChunkBit(dev, blk, c);
				bi->pagesInUse++;

				result = yaffs_ReadChunkWithTagsFromNAND(dev, chunk,
								chunkData,
								NULL);

				oh = (yaffs_ObjectHeader *) chunkData;

				in = yaffs_FindObjectByNumber(dev,
							      tags.objectId);
				if (in && in->variantType != oh->type) {
					/* This should not happen, but somehow
					 * Wev'e ended up with an objectId that has been reused but not yet
					 * deleted, and worse still it has changed type. Delete the old object.
					 */

					yaffs_DeleteObject(in);

					in = 0;
				}

				in = yaffs_FindOrCreateObjectByNumber(dev,
								      tags.
								      objectId,
								      oh->type);

				if (!in)
					alloc_failed = 1;

				if (in && oh->shadowsObject > 0) {

					struct yaffs_ShadowFixerStruct *fixer;
					fixer = YMALLOC(sizeof(struct yaffs_ShadowFixerStruct));
					if (fixer) {
						fixer->next = shadowFixerList;
						shadowFixerList = fixer;
						fixer->objectId = tags.objectId;
						fixer->shadowedId = oh->shadowsObject;
						T(YAFFS_TRACE_SCAN,
						  (TSTR
						   (" Shadow fixer: %d shadows %d" TENDSTR),
						   fixer->objectId, fixer->shadowedId));

					}

				}

				if (in && in->valid) {
					/* We have already filled this one. We have a duplicate and need to resolve it. */

					unsigned existingSerial = in->serial;
					unsigned newSerial = tags.serialNumber;

					if (((existingSerial + 1) & 3) == newSerial) {
						/* Use new one - destroy the exisiting one */
						yaffs_DeleteChunk(dev,
								  in->hdrChunk,
								  1, __LINE__);
						in->valid = 0;
					} else {
						/* Use existing - destroy this one. */
						yaffs_DeleteChunk(dev, chunk, 1,
								  __LINE__);
					}
				}

				if (in && !in->valid &&
				    (tags.objectId == YAFFS_OBJECTID_ROOT ||
				     tags.objectId == YAFFS_OBJECTID_LOSTNFOUND)) {
					/* We only load some info, don't fiddle with directory structure */
					in->valid = 1;
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
					in->hdrChunk = chunk;
					in->serial = tags.serialNumber;

				} else if (in && !in->valid) {
					/* we need to load this info */

					in->valid = 1;
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
					in->hdrChunk = chunk;
					in->serial = tags.serialNumber;

					yaffs_SetObjectNameFromOH(in, oh);
					in->dirty = 0;

					/* directory stuff...
					 * hook up to parent
					 */

					parent =
					    yaffs_FindOrCreateObjectByNumber
					    (dev, oh->parentObjectId,
					     YAFFS_OBJECT_TYPE_DIRECTORY);
					if (!parent)
						alloc_failed = 1;
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

					if (0 && (parent == dev->deletedDir ||
						  parent == dev->unlinkedDir)) {
						in->deleted = 1;	/* If it is unlinked at start up then it wants deleting */
						dev->nDeletedFiles++;
					}
					/* Note re hardlinks.
					 * Since we might scan a hardlink before its equivalent object is scanned
					 * we put them all in a list.
					 * After scanning is complete, we should have all the objects, so we run through this
					 * list and fix up all the chains.
					 */

					switch (in->variantType) {
					case YAFFS_OBJECT_TYPE_UNKNOWN:
						/* Todo got a problem */
						break;
					case YAFFS_OBJECT_TYPE_FILE:
						if (dev->param.useHeaderFileSize)

							in->variant.fileVariant.
							    fileSize =
							    oh->fileSize;

						break;
					case YAFFS_OBJECT_TYPE_HARDLINK:
						in->variant.hardLinkVariant.
							equivalentObjectId =
							oh->equivalentObjectId;
						in->hardLinks.next =
							(struct ylist_head *)
							hardList;
						hardList = in;
						break;
					case YAFFS_OBJECT_TYPE_DIRECTORY:
						/* Do nothing */
						break;
					case YAFFS_OBJECT_TYPE_SPECIAL:
						/* Do nothing */
						break;
					case YAFFS_OBJECT_TYPE_SYMLINK:
						in->variant.symLinkVariant.alias =
						    yaffs_CloneString(oh->alias);
						if (!in->variant.symLinkVariant.alias)
							alloc_failed = 1;
						break;
					}

				}
			}
		}

		if (state == YAFFS_BLOCK_STATE_NEEDS_SCANNING) {
			/* If we got this far while scanning, then the block is fully allocated.*/
			state = YAFFS_BLOCK_STATE_FULL;
		}

		if (state == YAFFS_BLOCK_STATE_ALLOCATING) {
			/* If the block was partially allocated then treat it as fully allocated.*/
			state = YAFFS_BLOCK_STATE_FULL;
			dev->allocationBlock = -1;
		}

		bi->blockState = state;

		/* Now let's see if it was dirty */
		if (bi->pagesInUse == 0 &&
		    !bi->hasShrinkHeader &&
		    bi->blockState == YAFFS_BLOCK_STATE_FULL) {
			yaffs_BlockBecameDirty(dev, blk);
		}

	}


	/* Ok, we've done all the scanning.
	 * Fix up the hard link chains.
	 * We should now have scanned all the objects, now it's time to add these
	 * hardlinks.
	 */

	yaffs_HardlinkFixup(dev, hardList);

	/* Fix up any shadowed objects */
	{
		struct yaffs_ShadowFixerStruct *fixer;
		yaffs_Object *obj;

		while (shadowFixerList) {
			fixer = shadowFixerList;
			shadowFixerList = fixer->next;
			/* Complete the rename transaction by deleting the shadowed object
			 * then setting the object header to unshadowed.
			 */
			obj = yaffs_FindObjectByNumber(dev, fixer->shadowedId);
			if (obj)
				yaffs_DeleteObject(obj);

			obj = yaffs_FindObjectByNumber(dev, fixer->objectId);

			if (obj)
				yaffs_UpdateObjectHeader(obj, NULL, 1, 0, 0, NULL);

			YFREE(fixer);
		}
	}

	yaffs_ReleaseTempBuffer(dev, chunkData, __LINE__);

	if (alloc_failed)
		return YAFFS_FAIL;

	T(YAFFS_TRACE_SCAN, (TSTR("yaffs1_Scan ends" TENDSTR)));


	return YAFFS_OK;
}

