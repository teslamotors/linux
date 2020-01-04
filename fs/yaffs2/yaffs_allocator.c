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



#include "yaffs_allocator.h"
#include "yaffs_guts.h"
#include "yaffs_trace.h"
#include "yportenv.h"

#ifdef CONFIG_YAFFS_YMALLOC_ALLOCATOR

void yaffs_DeinitialiseRawTnodesAndObjects(yaffs_Device *dev)
{
	dev = dev;
}

void yaffs_InitialiseRawTnodesAndObjects(yaffs_Device *dev)
{
	dev = dev;
}

yaffs_Tnode *yaffs_AllocateRawTnode(yaffs_Device *dev)
{
	return (yaffs_Tnode *)YMALLOC(dev->tnodeSize);
}

void yaffs_FreeRawTnode(yaffs_Device *dev, yaffs_Tnode *tn)
{
	dev = dev;
	YFREE(tn);
}

void yaffs_InitialiseRawObjects(yaffs_Device *dev)
{
	dev = dev;
}

void yaffs_DeinitialiseRawObjects(yaffs_Device *dev)
{
	dev = dev;
}

yaffs_Object *yaffs_AllocateRawObject(yaffs_Device *dev)
{
	dev = dev;
	return (yaffs_Object *) YMALLOC(sizeof(yaffs_Object));
}


void yaffs_FreeRawObject(yaffs_Device *dev, yaffs_Object *obj)
{

	dev = dev;
	YFREE(obj);
}

#else

struct yaffs_TnodeList_struct {
	struct yaffs_TnodeList_struct *next;
	yaffs_Tnode *tnodes;
};

typedef struct yaffs_TnodeList_struct yaffs_TnodeList;

struct yaffs_ObjectList_struct {
	yaffs_Object *objects;
	struct yaffs_ObjectList_struct *next;
};

typedef struct yaffs_ObjectList_struct yaffs_ObjectList;


struct yaffs_AllocatorStruct {
	int nTnodesCreated;
	yaffs_Tnode *freeTnodes;
	int nFreeTnodes;
	yaffs_TnodeList *allocatedTnodeList;

	int nObjectsCreated;
	yaffs_Object *freeObjects;
	int nFreeObjects;

	yaffs_ObjectList *allocatedObjectList;
};

typedef struct yaffs_AllocatorStruct yaffs_Allocator;


static void yaffs_DeinitialiseRawTnodes(yaffs_Device *dev)
{

	yaffs_Allocator *allocator = (yaffs_Allocator *)dev->allocator;

	yaffs_TnodeList *tmp;

	if(!allocator){
		YBUG();
		return;
	}

	while (allocator->allocatedTnodeList) {
		tmp = allocator->allocatedTnodeList->next;

		YFREE(allocator->allocatedTnodeList->tnodes);
		YFREE(allocator->allocatedTnodeList);
		allocator->allocatedTnodeList = tmp;

	}

	allocator->freeTnodes = NULL;
	allocator->nFreeTnodes = 0;
	allocator->nTnodesCreated = 0;
}

static void yaffs_InitialiseRawTnodes(yaffs_Device *dev)
{
	yaffs_Allocator *allocator = dev->allocator;

	if(allocator){
		allocator->allocatedTnodeList = NULL;
		allocator->freeTnodes = NULL;
		allocator->nFreeTnodes = 0;
		allocator->nTnodesCreated = 0;
	} else
		YBUG();
}

static int yaffs_CreateTnodes(yaffs_Device *dev, int nTnodes)
{
	yaffs_Allocator *allocator = (yaffs_Allocator *)dev->allocator;
	int i;
	yaffs_Tnode *newTnodes;
	__u8 *mem;
	yaffs_Tnode *curr;
	yaffs_Tnode *next;
	yaffs_TnodeList *tnl;

	if(!allocator){
		YBUG();
		return YAFFS_FAIL;
	}

	if (nTnodes < 1)
		return YAFFS_OK;


	/* make these things */

	newTnodes = YMALLOC(nTnodes * dev->tnodeSize);
	mem = (__u8 *)newTnodes;

	if (!newTnodes) {
		T(YAFFS_TRACE_ERROR,
			(TSTR("yaffs: Could not allocate Tnodes" TENDSTR)));
		return YAFFS_FAIL;
	}

	/* New hookup for wide tnodes */
	for (i = 0; i < nTnodes - 1; i++) {
		curr = (yaffs_Tnode *) &mem[i * dev->tnodeSize];
		next = (yaffs_Tnode *) &mem[(i+1) * dev->tnodeSize];
		curr->internal[0] = next;
	}

	curr = (yaffs_Tnode *) &mem[(nTnodes - 1) * dev->tnodeSize];
	curr->internal[0] = allocator->freeTnodes;
	allocator->freeTnodes = (yaffs_Tnode *)mem;

	allocator->nFreeTnodes += nTnodes;
	allocator->nTnodesCreated += nTnodes;

	/* Now add this bunch of tnodes to a list for freeing up.
	 * NB If we can't add this to the management list it isn't fatal
	 * but it just means we can't free this bunch of tnodes later.
	 */

	tnl = YMALLOC(sizeof(yaffs_TnodeList));
	if (!tnl) {
		T(YAFFS_TRACE_ERROR,
		  (TSTR
		   ("yaffs: Could not add tnodes to management list" TENDSTR)));
		   return YAFFS_FAIL;
	} else {
		tnl->tnodes = newTnodes;
		tnl->next = allocator->allocatedTnodeList;
		allocator->allocatedTnodeList = tnl;
	}

	T(YAFFS_TRACE_ALLOCATE, (TSTR("yaffs: Tnodes added" TENDSTR)));

	return YAFFS_OK;
}


yaffs_Tnode *yaffs_AllocateRawTnode(yaffs_Device *dev)
{
	yaffs_Allocator *allocator = (yaffs_Allocator *)dev->allocator;
	yaffs_Tnode *tn = NULL;

	if(!allocator){
		YBUG();
		return NULL;
	}

	/* If there are none left make more */
	if (!allocator->freeTnodes)
		yaffs_CreateTnodes(dev, YAFFS_ALLOCATION_NTNODES);

	if (allocator->freeTnodes) {
		tn = allocator->freeTnodes;
		allocator->freeTnodes = allocator->freeTnodes->internal[0];
		allocator->nFreeTnodes--;
	}

	return tn;
}

/* FreeTnode frees up a tnode and puts it back on the free list */
void yaffs_FreeRawTnode(yaffs_Device *dev, yaffs_Tnode *tn)
{
	yaffs_Allocator *allocator = dev->allocator;

	if(!allocator){
		YBUG();
		return;
	}

	if (tn) {
		tn->internal[0] = allocator->freeTnodes;
		allocator->freeTnodes = tn;
		allocator->nFreeTnodes++;
	}
	dev->nCheckpointBlocksRequired = 0; /* force recalculation*/
}



static void yaffs_InitialiseRawObjects(yaffs_Device *dev)
{
	yaffs_Allocator *allocator = dev->allocator;

	if(allocator) {
		allocator->allocatedObjectList = NULL;
		allocator->freeObjects = NULL;
		allocator->nFreeObjects = 0;
	} else
		YBUG();
}

static void yaffs_DeinitialiseRawObjects(yaffs_Device *dev)
{
	yaffs_Allocator *allocator = dev->allocator;
	yaffs_ObjectList *tmp;

	if(!allocator){
		YBUG();
		return;
	}

	while (allocator->allocatedObjectList) {
		tmp = allocator->allocatedObjectList->next;
		YFREE(allocator->allocatedObjectList->objects);
		YFREE(allocator->allocatedObjectList);

		allocator->allocatedObjectList = tmp;
	}

	allocator->freeObjects = NULL;
	allocator->nFreeObjects = 0;
	allocator->nObjectsCreated = 0;
}


static int yaffs_CreateFreeObjects(yaffs_Device *dev, int nObjects)
{
	yaffs_Allocator *allocator = dev->allocator;

	int i;
	yaffs_Object *newObjects;
	yaffs_ObjectList *list;

	if(!allocator){
		YBUG();
		return YAFFS_FAIL;
	}

	if (nObjects < 1)
		return YAFFS_OK;

	/* make these things */
	newObjects = YMALLOC(nObjects * sizeof(yaffs_Object));
	list = YMALLOC(sizeof(yaffs_ObjectList));

	if (!newObjects || !list) {
		if (newObjects){
			YFREE(newObjects);
			newObjects = NULL;
		}
		if (list){
			YFREE(list);
			list = NULL;
		}
		T(YAFFS_TRACE_ALLOCATE,
		  (TSTR("yaffs: Could not allocate more objects" TENDSTR)));
		return YAFFS_FAIL;
	}

	/* Hook them into the free list */
	for (i = 0; i < nObjects - 1; i++) {
		newObjects[i].siblings.next =
				(struct ylist_head *)(&newObjects[i + 1]);
	}

	newObjects[nObjects - 1].siblings.next = (void *)allocator->freeObjects;
	allocator->freeObjects = newObjects;
	allocator->nFreeObjects += nObjects;
	allocator->nObjectsCreated += nObjects;

	/* Now add this bunch of Objects to a list for freeing up. */

	list->objects = newObjects;
	list->next = allocator->allocatedObjectList;
	allocator->allocatedObjectList = list;

	return YAFFS_OK;
}

yaffs_Object *yaffs_AllocateRawObject(yaffs_Device *dev)
{
	yaffs_Object *obj = NULL;
	yaffs_Allocator *allocator = dev->allocator;

	if(!allocator) {
		YBUG();
		return obj;
	}

	/* If there are none left make more */
	if (!allocator->freeObjects)
		yaffs_CreateFreeObjects(dev, YAFFS_ALLOCATION_NOBJECTS);

	if (allocator->freeObjects) {
		obj = allocator->freeObjects;
		allocator->freeObjects =
			(yaffs_Object *) (allocator->freeObjects->siblings.next);
		allocator->nFreeObjects--;
	}

	return obj;
}


void yaffs_FreeRawObject(yaffs_Device *dev, yaffs_Object *obj)
{

	yaffs_Allocator *allocator = dev->allocator;

	if(!allocator)
		YBUG();
	else {
		/* Link into the free list. */
		obj->siblings.next = (struct ylist_head *)(allocator->freeObjects);
		allocator->freeObjects = obj;
		allocator->nFreeObjects++;
	}
}

void yaffs_DeinitialiseRawTnodesAndObjects(yaffs_Device *dev)
{
	if(dev->allocator){
		yaffs_DeinitialiseRawTnodes(dev);
		yaffs_DeinitialiseRawObjects(dev);

		YFREE(dev->allocator);
		dev->allocator=NULL;
	} else
		YBUG();
}

void yaffs_InitialiseRawTnodesAndObjects(yaffs_Device *dev)
{
	yaffs_Allocator *allocator;

	if(!dev->allocator){
		allocator = YMALLOC(sizeof(yaffs_Allocator));
		if(allocator){
			dev->allocator = allocator;
			yaffs_InitialiseRawTnodes(dev);
			yaffs_InitialiseRawObjects(dev);
		}
	} else
		YBUG();
}


#endif
