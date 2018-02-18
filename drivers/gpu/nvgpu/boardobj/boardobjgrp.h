/*
* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
*
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public License,
* version 2, as published by the Free Software Foundation.
*
* This program is distributed in the hope it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*/

#ifndef _BOARDOBJGRP_H_
#define _BOARDOBJGRP_H_

struct boardobjgrp;

/* ------------------------ Includes ----------------------------------------*/
#include <linux/nvgpu.h>
#include "gk20a/gk20a.h"
#include "gk20a/pmu_gk20a.h"
#include "ctrl/ctrlboardobj.h"
#include "boardobj.h"
#include "boardobjgrpmask.h"
#include "pmuif/gpmuifboardobj.h"

/*
* Board Object Group destructor.
*
*/
typedef u32 boardobjgrp_destruct(struct boardobjgrp *pboardobjgrp);

/*
* Inserts a previously constructed Board Object into a Board Object Group for
* tracking. Objects are inserted in the array based on the given index.
*/
typedef u32 boardobjgrp_objinsert(struct boardobjgrp *pboardobjgrp,
		struct boardobj *pboardobj, u8 index);

/*
* Retrieves a Board Object from a Board Object Group using the group's index.
*
*/
typedef struct boardobj *boardobjgrp_objgetbyidx(
		struct boardobjgrp *pBobrdobjgrp, u8 index);

/*
* Retrieve Board Object immediately following one pointed by @ref pcurrentindex
* filtered out by the provided mask. If (pMask == NULL) => no filtering.
*/
typedef struct boardobj *boardobjgrp_objgetnext(
		struct boardobjgrp *pboardobjgrp,
		u8 *currentindex, struct boardobjgrpmask *mask);

/*
* Board Object Group Remover and destructor. This is used to remove and
* destruct specific entry from the Board Object Group.
*/
typedef u32 boardobjgrp_objremoveanddestroy(struct boardobjgrp *pboardobjgrp,
	u8 index);

/*
* BOARDOBJGRP handler for PMU_UNIT_INIT.  Calls the PMU_UNIT_INIT handlers
* for the constructed PMU CMDs, and then sets the object via the
* PMU_BOARDOBJ_CMD_GRP interface (if constructed).
*/
typedef u32 boardobjgrp_pmuinithandle(struct gk20a *g,
		struct boardobjgrp *pboardobjGrp);

/*
* Fills out the appropriate the PMU_BOARDOBJGRP_<xyz> driver<->PMU description
* header structure, more specifically a mask of BOARDOBJs.
*/
typedef u32 boardobjgrp_pmuhdrdatainit(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp,
		struct nv_pmu_boardobjgrp_super *pboardobjgrppmu,
		struct boardobjgrpmask *mask);

/*
* Fills out the appropriate the PMU_BOARDOBJGRP_<xyz> driver->PMU description
* structure, describing the BOARDOBJGRP and all of its BOARDOBJs to the PMU.
*/
typedef u32 boardobjgrp_pmudatainit(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp,
		struct nv_pmu_boardobjgrp_super *pboardobjgrppmu);

/*
* Sends a BOARDOBJGRP to the PMU via the PMU_BOARDOBJ_CMD_GRP interface.
* This interface leverages @ref boardobjgrp_pmudatainit to populate the
* structure.
*/
typedef u32 boardobjgrp_pmuset(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp);

/*
* Gets the dynamic status of the PMU BOARDOBJGRP via the
* PMU_BOARDOBJ_CMD_GRP GET_STATUS interface.
*/
typedef u32 boardobjgrp_pmugetstatus(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp,
		struct boardobjgrpmask *mask);

typedef u32 boardobjgrp_pmudatainstget(struct gk20a *g,
	struct nv_pmu_boardobjgrp  *boardobjgrppmu,
	struct nv_pmu_boardobj **ppboardobjpmudata, u8 idx);

typedef u32 boardobjgrp_pmustatusinstget(struct gk20a *g, void *pboardobjgrppmu,
	struct nv_pmu_boardobj_query **ppBoardobjpmustatus, u8 idx);

/*
* Structure describing an PMU CMD for interacting with the representaition
* of this BOARDOBJGRP within the PMU.
*/
struct boardobjgrp_pmu_cmd {
	u8   id;
	u8   msgid;
	u8   hdrsize;
	u8   entrysize;
	u16  fbsize;
	struct nv_pmu_boardobjgrp_super *buf;
	struct pmu_surface surf;
};

/*
* Structure of state describing how to communicate with representation of this
* BOARDOBJGRP in the PMU.
*/
struct boardobjgrp_pmu {
	u8   unitid;
	u8   classid;
	bool bset;
	struct boardobjgrp_pmu_cmd set;
	struct boardobjgrp_pmu_cmd getstatus;
};

/*
* Function by which a class which implements BOARDOBJGRP can construct a PMU
* CMD.  This provides the various information describing the PMU CMD including
* the CMD and MSG ID and the size of the various sturctures in the payload.
*/
typedef u32 boardobjgrp_pmucmd_construct(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp,
		struct boardobjgrp_pmu_cmd *cmd, u8 id, u8 msgid,
		u8 hdrsize, u8 entrysize, u16 fbsize);

/*
* Destroys BOARDOBJGRP PMU SW state.  CMD.
*/
typedef u32 boardobjgrp_pmucmd_destroy(struct boardobjgrp_pmu_cmd *cmd);

/*
* init handler for the BOARDOBJGRP PMU CMD.  Allocates and maps the
* PMU CMD payload within both the PMU and driver so that it can be referenced
* at run-time.
*/
typedef u32 boardobjgrp_pmucmd_pmuinithandle(struct gk20a *g,
	struct boardobjgrp *pboardobjgrp,
	struct boardobjgrp_pmu_cmd *cmd);

/*
* Base Class Group for all physical or logical device on the PCB.
* Contains fields common to all devices on the board. Specific types of
* devices groups may extend this object adding any details specific to that
* device group or device-type.
*/
struct boardobjgrp {
	u32  objmask;
	bool bconstructed;
	u8 type;
	u8 classid;
	struct boardobj **ppobjects;
	struct boardobjgrpmask *mask;
	u8  objslots;
	u8  objmaxidx;
	struct boardobjgrp_pmu  pmu;

	/* Basic interfaces */
	boardobjgrp_destruct  *destruct;
	boardobjgrp_objinsert *objinsert;
	boardobjgrp_objgetbyidx *objgetbyidx;
	boardobjgrp_objgetnext  *objgetnext;
	boardobjgrp_objremoveanddestroy  *objremoveanddestroy;

	/* PMU interfaces */
	boardobjgrp_pmuinithandle *pmuinithandle;
	boardobjgrp_pmuhdrdatainit  *pmuhdrdatainit;
	boardobjgrp_pmudatainit *pmudatainit;
	boardobjgrp_pmuset  *pmuset;
	boardobjgrp_pmugetstatus *pmugetstatus;

	boardobjgrp_pmudatainstget *pmudatainstget;
	boardobjgrp_pmustatusinstget *pmustatusinstget;
};

/*
* Macro test whether a specified index into the BOARDOBJGRP is valid.
*
*/
#define boardobjgrp_idxisvalid(_pboardobjgrp, _idx)                           \
	(((_idx) < (_pboardobjgrp)->objslots) &&                               \
	((_pboardobjgrp)->ppobjects[(_idx)] != NULL))

/*
* Macro test whether a specified BOARDOBJGRP is empty.
*/
#define BOARDOBJGRP_IS_EMPTY(_pboardobjgrp)                                    \
	((!((_pboardobjgrp)->bconstructed)) ||                                 \
	((_pboardobjgrp)->objmaxidx == CTRL_BOARDOBJ_IDX_INVALID))

#define boardobjgrp_objinsert(_pboardobjgrp, _pboardobj, _idx)                 \
	((_pboardobjgrp)->objinsert((_pboardobjgrp), (_pboardobj), (_idx)))

/*
* Helper macro to determine the "next" open/empty index after all allocated
* objects.  This is intended to be used to find the index at which objects can
* be inserted contiguously (i.e. w/o fear of colliding with existing objects).
*/
#define BOARDOBJGRP_NEXT_EMPTY_IDX(_pboardobjgrp)                              \
	((CTRL_BOARDOBJ_IDX_INVALID == (_pboardobjgrp)->objmaxidx) ? 0 :       \
	((((_pboardobjgrp)->objmaxidx + 1) >= (_pboardobjgrp)->objslots) ?     \
	(u8)CTRL_BOARDOBJ_IDX_INVALID : (u8)((_pboardobjgrp)->objmaxidx + 1)))

/*
* Helper macro to determine the number of @ref BOARDOBJ pointers
* that are required to be allocated in PMU @ref ppObjects.
*/
#define BOARDOBJGRP_PMU_SLOTS_GET(_pboardobjgrp)                            \
	((CTRL_BOARDOBJ_IDX_INVALID == (_pboardobjgrp)->objmaxidx) ? 0 :    \
	(u8)((_pboardobjgrp)->objmaxidx + 1))

#define BOARDOBJGRP_OBJ_GET_BY_IDX(_pboardobjgrp, _idx)                     \
	((_pboardobjgrp)->objgetbyidx((_pboardobjgrp), (_idx)))

/*
* macro to look-up next object while tolerating error if
* Board Object Group is not constructed.
*/

#define boardobjgrpobjgetnextsafe(_pgrp, _pindex, _pmask)                      \
	((_pgrp)->bconstructed ?                                               \
	(_pgrp)->objgetnext((_pgrp), (_pindex), (_pmask)) : NULL)

/*
* Used to traverse all Board Objects stored within @ref _pgrp in the increasing
* index order.
* If @ref _pmask is provided only objects specified by the mask are traversed.
*/
#define BOARDOBJGRP_ITERATOR(_pgrp, _ptype, _pobj, _index, _pmask)             \
	for (_index = CTRL_BOARDOBJ_IDX_INVALID,                             \
	_pobj  = (_ptype)boardobjgrpobjgetnextsafe((_pgrp), &_index, (_pmask));\
	_pobj != NULL;                                                         \
	_pobj  = (_ptype)boardobjgrpobjgetnextsafe((_pgrp), &_index, (_pmask)))
#define BOARDOBJGRP_FOR_EACH(_pgrp, _ptype, _pobj, _index)                     \
	BOARDOBJGRP_ITERATOR(_pgrp, _ptype, _pobj, _index, NULL)

#define BOARDOBJGRP_FOR_EACH_INDEX_IN_MASK(mask_width, index, mask)        \
{                                                           \
	u##mask_width lcl_msk = (u##mask_width)(mask);         \
	for (index = 0; lcl_msk != 0; index++, lcl_msk >>= 1) {     \
		if (((u##mask_width)((u64)1) & lcl_msk) == 0) {     \
			continue;                                       \
		}

#define BOARDOBJGRP_FOR_EACH_INDEX_IN_MASK_END                          \
	}                                                       \
}


/*!
* Invalid UNIT_ID.  Used to indicate that the implementing class has not set
* @ref BOARDOBJGRP::unitId and, thus, certain BOARDOBJGRP PMU interfaces are
* not supported.
*/
#define BOARDOBJGRP_UNIT_ID_INVALID                                   255

/*!
* Invalid UNIT_ID.  Used to indicate that the implementing class has not set
* @ref BOARDOBJGRP::grpType and, thus, certain BOARDOBJGRP PMU interfaces are
* not supported.
*/
#define BOARDOBJGRP_GRP_CLASS_ID_INVALID                              255

/*!
* Invalid UNIT_ID.  Used to indicate that the implementing class has not set
* @ref BOARDOBJGRP::grpSetCmdId and, thus, certain BOARDOBJGRP PMU interfaces
* are not supported.
*/
#define BOARDOBJGRP_GRP_CMD_ID_INVALID                                255

/*!
* Helper macro to construct a BOARDOBJGRP's PMU SW state.
*
* @param[out] pboardobjgrp    BOARDOBJGRP pointer
* @param[in]  _eng
*     Implementing engine/unit which manages the BOARDOBJGRP.
* @param[in]  _class
*     Class ID of BOARDOBJGRP.
*/
#define BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, _ENG, _CLASS)	              \
do {                                                                          \
	(pboardobjgrp)->pmu.unitid  = PMU_UNIT_##_ENG;                        \
	(pboardobjgrp)->pmu.classid =                                         \
	NV_PMU_##_ENG##_BOARDOBJGRP_CLASS_ID_##_CLASS;                        \
} while (0)

#define BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(pgpu, pboardobjgrp, eng, ENG,   \
	class, CLASS)                                                         \
	boardobjgrp_pmucmd_construct_impl(                                    \
	pgpu,                                              /* pgpu */         \
	pboardobjgrp,                                      /* pboardobjgrp */ \
	&((pboardobjgrp)->pmu.set),                        /* pcmd */         \
	NV_PMU_##ENG##_CMD_ID_BOARDOBJ_GRP_SET,               /* id */        \
	NV_PMU_##ENG##_MSG_ID_BOARDOBJ_GRP_SET,               /* msgid */     \
	(u32)sizeof(union nv_pmu_##eng##_##class##_boardobjgrp_set_header_aligned), \
	(u32)sizeof(union nv_pmu_##eng##_##class##_boardobj_set_union_aligned), \
	(u32)sizeof(struct nv_pmu_##eng##_##class##_boardobj_grp_set))

#define BOARDOBJGRP_PMU_CMD_GRP_GET_STATUS_CONSTRUCT(pgpu, pboardobjgrp,      \
	eng, ENG, class, CLASS)                                           \
	boardobjgrp_pmucmd_construct_impl(                                    \
	pgpu,                                              /* pGpu */         \
	pboardobjgrp,                                      /* pBoardObjGrp */ \
	&((pboardobjgrp)->pmu.getstatus),                  /* pCmd */         \
	NV_PMU_##ENG##_CMD_ID_BOARDOBJ_GRP_GET_STATUS,        /* id */        \
	NV_PMU_##ENG##_MSG_ID_BOARDOBJ_GRP_GET_STATUS,        /* msgid */     \
	(u32)sizeof(union nv_pmu_##eng##_##class##_boardobjgrp_get_status_header_aligned), \
	(u32)sizeof(union nv_pmu_##eng##_##class##_boardobj_get_status_union_aligned), \
	(u32)sizeof(struct nv_pmu_##eng##_##class##_boardobj_grp_get_status))

/* ------------------------ Function Prototypes ----------------------------- */
/* Constructor and destructor */
u32 boardobjgrp_construct_super(struct boardobjgrp *pboardobjgrp);
boardobjgrp_destruct boardobjgrp_destruct_impl;
boardobjgrp_destruct boardobjgrp_destruct_super;

/* PMU_CMD interfaces */
boardobjgrp_pmucmd_construct     boardobjgrp_pmucmd_construct_impl;
boardobjgrp_pmucmd_destroy         boardobjgrp_pmucmd_destroy_impl;
boardobjgrp_pmucmd_pmuinithandle   boardobjgrp_pmucmd_pmuinithandle_impl;

/* BOARDOBJGRP interfaces */
boardobjgrp_pmuinithandle    boardobjgrp_pmuinithandle_impl;
boardobjgrp_pmuhdrdatainit   boardobjgrp_pmuhdrdatainit_super;
boardobjgrp_pmudatainit      boardobjgrp_pmudatainit_super;

boardobjgrp_pmudatainit      boardobjgrp_pmudatainit_legacy;
boardobjgrp_pmuset           boardobjgrp_pmuset_impl;
boardobjgrp_pmugetstatus     boardobjgrp_pmugetstatus_impl;

void boardobjgrpe32hdrset(struct nv_pmu_boardobjgrp *hdr, u32 objmask);

/* ------------------------ Include Derived Types --------------------------- */
#include "boardobj/boardobjgrp_e32.h"
#include "boardobj/boardobjgrp_e255.h"

#define HIGHESTBITIDX_32(n32)   \
{                               \
	u32 count = 0;        \
	while (n32 >>= 1)       \
		count++;        \
	n32 = count;            \
}

#define LOWESTBIT(x)            ((x) &  (((x)-1) ^ (x)))

#define HIGHESTBIT(n32)     \
{                           \
	HIGHESTBITIDX_32(n32);  \
	n32 = NVBIT(n32);       \
}

#define ONEBITSET(x)            ((x) && (((x) & ((x)-1)) == 0))

#define LOWESTBITIDX_32(n32)  \
{                             \
	n32 = LOWESTBIT(n32); \
	IDX_32(n32);         \
}

#define NUMSETBITS_32(n32)                                         \
{                                                                  \
	n32 = n32 - ((n32 >> 1) & 0x55555555);                         \
	n32 = (n32 & 0x33333333) + ((n32 >> 2) & 0x33333333);          \
	n32 = (((n32 + (n32 >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;  \
}

#define IDX_32(n32)                     \
{                                       \
	u32 idx = 0;                      \
	if ((n32) & 0xFFFF0000)             \
		idx += 16;                  \
	if ((n32) & 0xFF00FF00)             \
		idx += 8;                   \
	if ((n32) & 0xF0F0F0F0)             \
		idx += 4;                   \
	if ((n32) & 0xCCCCCCCC)             \
		idx += 2;                   \
	if ((n32) & 0xAAAAAAAA)             \
		idx += 1;                   \
	(n32) = idx;                        \
}
#endif
