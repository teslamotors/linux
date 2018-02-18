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

#include "gk20a/gk20a.h"
#include "gk20a/pmu_gk20a.h"
#include "boardobjgrp.h"
#include "ctrl/ctrlboardobj.h"
#include "boardobj.h"

static boardobjgrp_objinsert   boardobjgrp_objinsert_final;
static boardobjgrp_objgetbyidx   boardobjgrp_objgetbyidx_final;
static boardobjgrp_objgetnext   boardobjgrp_objgetnext_final;
static boardobjgrp_objremoveanddestroy  boardobjgrp_objremoveanddestroy_final;
static boardobjgrp_pmudatainstget boardobjgrp_pmudatainstget_stub;
static boardobjgrp_pmustatusinstget boardobjgrp_pmustatusinstget_stub;
static u32 boardobjgrp_pmucmdsend(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp,
		struct boardobjgrp_pmu_cmd *pcmd);
struct boardobjgrp_pmucmdhandler_params {
	/* Pointer to the BOARDOBJGRP associated with this CMD */
	struct boardobjgrp *pboardobjgrp;
	/* Pointer to structure representing this NV_PMU_BOARDOBJ_CMD_GRP */
	struct boardobjgrp_pmu_cmd *pcmd;
	/* Boolean indicating whether the PMU successfully handled the CMD */
	u32 success;
};

u32 boardobjgrp_construct_super(struct boardobjgrp *pboardobjgrp)
{
	gk20a_dbg_info("");

	if (pboardobjgrp == NULL)
		return -EINVAL;

	if (pboardobjgrp->ppobjects == NULL)
		return -EINVAL;

	if (pboardobjgrp->mask == NULL)
		return -EINVAL;

	pboardobjgrp->objmask = 0;

	pboardobjgrp->classid = 0;
	pboardobjgrp->pmu.unitid = BOARDOBJGRP_UNIT_ID_INVALID;
	pboardobjgrp->pmu.classid = BOARDOBJGRP_GRP_CLASS_ID_INVALID;
	pboardobjgrp->pmu.set.id = BOARDOBJGRP_GRP_CMD_ID_INVALID;
	pboardobjgrp->pmu.getstatus.id = BOARDOBJGRP_GRP_CMD_ID_INVALID;

	/* Initialize basic interfaces */
	pboardobjgrp->destruct =  boardobjgrp_destruct_super;
	pboardobjgrp->objinsert  = boardobjgrp_objinsert_final;
	pboardobjgrp->objgetbyidx = boardobjgrp_objgetbyidx_final;
	pboardobjgrp->objgetnext = boardobjgrp_objgetnext_final;
	pboardobjgrp->objremoveanddestroy =
		boardobjgrp_objremoveanddestroy_final;

	pboardobjgrp->pmuinithandle = boardobjgrp_pmuinithandle_impl;
	pboardobjgrp->pmuhdrdatainit = boardobjgrp_pmuhdrdatainit_super;
	pboardobjgrp->pmudatainit = boardobjgrp_pmudatainit_super;
	pboardobjgrp->pmuset = boardobjgrp_pmuset_impl;
	pboardobjgrp->pmugetstatus = boardobjgrp_pmugetstatus_impl;

	pboardobjgrp->pmudatainstget = boardobjgrp_pmudatainstget_stub;
	pboardobjgrp->pmustatusinstget = boardobjgrp_pmustatusinstget_stub;

	pboardobjgrp->objmaxidx = CTRL_BOARDOBJ_IDX_INVALID;
	pboardobjgrp->bconstructed = true;

	return 0;
}

u32 boardobjgrp_destruct_impl(struct boardobjgrp *pboardobjgrp)
{
	gk20a_dbg_info("");

	if (pboardobjgrp == NULL)
		return -EINVAL;

	if (!pboardobjgrp->bconstructed)
		return 0;

	return pboardobjgrp->destruct(pboardobjgrp);
}

u32 boardobjgrp_destruct_super(struct boardobjgrp *pboardobjgrp)
{
	struct boardobj *pboardobj;
	u32 status = 0;
	u32 stat;
	u8  index;

	gk20a_dbg_info("");

	if (pboardobjgrp->mask == NULL)
		return -EINVAL;
	if (pboardobjgrp->ppobjects == NULL)
		return -EINVAL;

	BOARDOBJGRP_FOR_EACH(pboardobjgrp, struct boardobj*, pboardobj, index) {
		stat = pboardobjgrp->objremoveanddestroy(pboardobjgrp, index);
		if (status == 0)
			status = stat;

		pboardobjgrp->ppobjects[index] = NULL;
		pboardobjgrp->objmask &= ~BIT(index);
	}

	pboardobjgrp->objmask = 0;

	if (pboardobjgrp->objmaxidx != CTRL_BOARDOBJ_IDX_INVALID) {
		if (status == 0)
			status = -EINVAL;

		WARN_ON(true);
	}

	/* Destroy the PMU CMD data */
	stat = boardobjgrp_pmucmd_destroy_impl(&pboardobjgrp->pmu.set);
	if (status == 0)
		status = stat;

	stat = boardobjgrp_pmucmd_destroy_impl(&pboardobjgrp->pmu.getstatus);
	if (status == 0)
		status = stat;

	pboardobjgrp->bconstructed = false;

	return status;
}

u32 boardobjgrp_pmucmd_construct_impl(struct gk20a *g, struct boardobjgrp
	*pboardobjgrp, struct boardobjgrp_pmu_cmd *cmd, u8 id, u8 msgid,
	u8 hdrsize, u8 entrysize, u16 fbsize)
{
	gk20a_dbg_info("");

	/* Copy the parameters into the CMD*/
	cmd->id   = id;
	cmd->msgid = msgid;
	cmd->hdrsize   = hdrsize;
	cmd->entrysize = entrysize;
	cmd->fbsize    = fbsize;

	return 0;
}


u32 boardobjgrp_pmucmd_destroy_impl(struct boardobjgrp_pmu_cmd *cmd)
{
	return 0;
}

u32 boardobjgrp_pmucmd_pmuinithandle_impl(struct gk20a *g,
	struct boardobjgrp *pboardobjgrp,
	struct boardobjgrp_pmu_cmd *pcmd)
{
	u32 status = 0;
	struct mem_desc *sysmem_desc = &pcmd->surf.sysmem_desc;

	gk20a_dbg_info("");

	if (pcmd->id == BOARDOBJGRP_GRP_CMD_ID_INVALID)
		goto boardobjgrp_pmucmd_pmuinithandle_exit;

	gk20a_pmu_sysmem_surface_alloc(g, sysmem_desc, pcmd->fbsize);
	/* we only have got sysmem later this will get copied to vidmem
	surface*/
	pcmd->surf.vidmem_desc.size = 0;

	pcmd->buf = (struct nv_pmu_boardobjgrp_super *)sysmem_desc->cpu_va;

boardobjgrp_pmucmd_pmuinithandle_exit:
	return status;
}

u32 boardobjgrp_pmuinithandle_impl(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp)
{
	u32 status = 0;

	gk20a_dbg_info("");

	status = boardobjgrp_pmucmd_pmuinithandle_impl(g, pboardobjgrp,
		&pboardobjgrp->pmu.set);
	if (status) {
		gk20a_err(dev_from_gk20a(g), "failed to init pmu set cmd");
		goto boardobjgrp_pmuinithandle_exit;
	}

	status = boardobjgrp_pmucmd_pmuinithandle_impl(g, pboardobjgrp,
		&pboardobjgrp->pmu.getstatus);
	if (status) {
		gk20a_err(dev_from_gk20a(g), "failed to init get status command");
		goto boardobjgrp_pmuinithandle_exit;
	}

	/* If the GRP_SET CMD has not been allocated, nothing left to do. */
	if (((pboardobjgrp->pmu.set.id) == BOARDOBJGRP_GRP_CMD_ID_INVALID) ||
		(BOARDOBJGRP_IS_EMPTY(pboardobjgrp)))
		goto boardobjgrp_pmuinithandle_exit;

	/* Send the BOARDOBJGRP to the pmu via RM_PMU_BOARDOBJ_CMD_GRP. */
	status = pboardobjgrp->pmuset(g, pboardobjgrp);
	if (status)
		gk20a_err(dev_from_gk20a(g), "failed to send boardobg grp to PMU");

boardobjgrp_pmuinithandle_exit:
	return status;
}


u32 boardobjgrp_pmuhdrdatainit_super(struct gk20a *g, struct boardobjgrp
	*pboardobjgrp, struct nv_pmu_boardobjgrp_super *pboardobjgrppmu,
	struct boardobjgrpmask *mask)
{
	gk20a_dbg_info("");

	if (pboardobjgrp == NULL)
		return -EINVAL;
	if (pboardobjgrppmu == NULL)
		return -EINVAL;
	pboardobjgrppmu->type = pboardobjgrp->type;
	pboardobjgrppmu->class_id = pboardobjgrp->classid;
	pboardobjgrppmu->obj_slots = BOARDOBJGRP_PMU_SLOTS_GET(pboardobjgrp);

	gk20a_dbg_info(" Done");
	return 0;
}

u32 boardobjgrp_pmudatainstget_stub(struct gk20a *g,
	struct nv_pmu_boardobjgrp *boardobjgrppmu,
	struct nv_pmu_boardobj **ppboardobjpmudata, u8 idx)
{
	gk20a_dbg_info("");
	return -EINVAL;
}


u32 boardobjgrp_pmustatusinstget_stub(struct gk20a *g, void *pboardobjgrppmu,
	struct nv_pmu_boardobj_query **ppBoardobjpmustatus, u8 idx)
{
	gk20a_dbg_info("");
	return -EINVAL;
}

u32 boardobjgrp_pmudatainit_legacy(struct gk20a *g,
	struct boardobjgrp 	*pboardobjgrp,
	struct nv_pmu_boardobjgrp_super *pboardobjgrppmu)
{
	u32 status = 0;
	struct boardobj *pboardobj = NULL;
	struct nv_pmu_boardobj *ppmudata = NULL;
	u8 index;

	gk20a_dbg_info("");

	if (pboardobjgrp == NULL)
		return -EINVAL;
	if (pboardobjgrppmu == NULL)
		return -EINVAL;

	boardobjgrpe32hdrset((struct nv_pmu_boardobjgrp *)pboardobjgrppmu,
							pboardobjgrp->objmask);

	BOARDOBJGRP_FOR_EACH_INDEX_IN_MASK(32, index, pboardobjgrp->objmask) {
		/* Obtain pointer to the current instance of the Object from the Group */
		pboardobj = pboardobjgrp->objgetbyidx(pboardobjgrp, index);
		if (NULL == pboardobj) {
			gk20a_err(dev_from_gk20a(g),
				"could not get object instance");
			status = -EINVAL;
			goto boardobjgrppmudatainit_legacy_done;
		}

		status = pboardobjgrp->pmudatainstget(g,
				(struct nv_pmu_boardobjgrp *)pboardobjgrppmu,
				&ppmudata, index);
		if (status) {
			gk20a_err(dev_from_gk20a(g),
				"could not get object instance");
			goto boardobjgrppmudatainit_legacy_done;
		}

		/* Initialize the PMU Data */
		status = pboardobj->pmudatainit(g, pboardobj, ppmudata);
		if (status) {
			gk20a_err(dev_from_gk20a(g),
				"could not parse pmu for device %d", index);
			goto boardobjgrppmudatainit_legacy_done;
		}
	}
	BOARDOBJGRP_FOR_EACH_INDEX_IN_MASK_END

boardobjgrppmudatainit_legacy_done:
	gk20a_dbg_info(" Done");
	return status;
}

u32 boardobjgrp_pmudatainit_super(struct gk20a *g, struct boardobjgrp
	*pboardobjgrp, struct nv_pmu_boardobjgrp_super *pboardobjgrppmu)
{
	u32 status = 0;
	struct boardobj *pboardobj = NULL;
	struct nv_pmu_boardobj	*ppmudata = NULL;
	u8 index;

	gk20a_dbg_info("");

	if (pboardobjgrp == NULL)
		return -EINVAL;
	if (pboardobjgrppmu == NULL)
		return -EINVAL;

	/* Initialize the PMU HDR data.*/
	status = pboardobjgrp->pmuhdrdatainit(g, pboardobjgrp, pboardobjgrppmu,
							pboardobjgrp->mask);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"unable to init boardobjgrp pmuhdr data");
		goto boardobjgrppmudatainit_super_done;
	}

	BOARDOBJGRP_FOR_EACH(pboardobjgrp, struct boardobj*, pboardobj, index) {
		status = pboardobjgrp->pmudatainstget(g,
				(struct nv_pmu_boardobjgrp *)pboardobjgrppmu,
				&ppmudata, index);
		if (status) {
			gk20a_err(dev_from_gk20a(g),
				"could not get object instance");
			goto boardobjgrppmudatainit_super_done;
		}

		/* Initialize the PMU Data and send to PMU */
		status = pboardobj->pmudatainit(g, pboardobj, ppmudata);
		if (status) {
			gk20a_err(dev_from_gk20a(g),
				"could not parse pmu for device %d", index);
			goto boardobjgrppmudatainit_super_done;
		}
	}

boardobjgrppmudatainit_super_done:
	gk20a_dbg_info(" Done");
	return status;
}

u32 boardobjgrp_pmuset_impl(struct gk20a *g, struct boardobjgrp *pboardobjgrp)
{
	u32 status = 0;
	struct boardobjgrp_pmu_cmd *pcmd = &pboardobjgrp->pmu.set;
	gk20a_dbg_info("");

	if (pboardobjgrp == NULL)
		return -EINVAL;

	if (!pboardobjgrp->bconstructed)
		return -EINVAL;

	if (pboardobjgrp->pmu.unitid == BOARDOBJGRP_UNIT_ID_INVALID)
		return -EINVAL;

	if (pboardobjgrp->pmu.classid == BOARDOBJGRP_GRP_CLASS_ID_INVALID)
		return -EINVAL;

	if (pboardobjgrp->pmu.set.id == BOARDOBJGRP_GRP_CMD_ID_INVALID)
		return -EINVAL;

	if ((pcmd->hdrsize == 0) ||
		(pcmd->entrysize == 0) ||
		(pcmd->buf == NULL))
		return -EINVAL;

	/* If no objects in the group, return early */
	if (BOARDOBJGRP_IS_EMPTY(pboardobjgrp))
		return -EINVAL;

	/* Initialize PMU buffer with BOARDOBJGRP data. */
	memset(pcmd->buf, 0x0, pcmd->fbsize);
	status = pboardobjgrp->pmudatainit(g, pboardobjgrp,
			pcmd->buf);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"could not parse pmu data");
		goto boardobjgrp_pmuset_exit;
	}

	/*
	 * Reset the boolean that indicates set status for most recent
	 * instance of BOARDOBJGRP.
	 */
	pboardobjgrp->pmu.bset = false;

	/*
	 * alloc mem in vidmem & copy constructed pmu boardobjgrp data from
	 * sysmem to vidmem
	 */
	if (pcmd->surf.vidmem_desc.size == 0) {
		gk20a_pmu_vidmem_surface_alloc(g, &pcmd->surf.vidmem_desc,
			pcmd->fbsize);
	}
	gk20a_mem_wr_n(g, &pcmd->surf.vidmem_desc, 0, pcmd->buf, pcmd->fbsize);

	/* Send the SET PMU CMD to the PMU */
	status = boardobjgrp_pmucmdsend(g, pboardobjgrp,
			pcmd);
	if (status) {
		gk20a_err(dev_from_gk20a(g), "could not send SET CMD to PMU");
		goto boardobjgrp_pmuset_exit;
	}

	pboardobjgrp->pmu.bset = true;

boardobjgrp_pmuset_exit:
	return status;
}

u32
boardobjgrp_pmugetstatus_impl(struct gk20a *g, struct boardobjgrp *pboardobjgrp,
	struct boardobjgrpmask *mask)
{
	u32 status  = 0;
	struct boardobjgrp_pmu_cmd *pcmd = &pboardobjgrp->pmu.getstatus;
	struct boardobjgrp_pmu_cmd *pset = &pboardobjgrp->pmu.set;

	gk20a_dbg_info("");

	if (pboardobjgrp == NULL)
		return -EINVAL;

	if (!pboardobjgrp->bconstructed)
		return -EINVAL;

	if (pboardobjgrp->pmu.unitid == BOARDOBJGRP_UNIT_ID_INVALID)
		return -EINVAL;

	if (pboardobjgrp->pmu.classid == BOARDOBJGRP_GRP_CLASS_ID_INVALID)
		return -EINVAL;

	if (pboardobjgrp->pmu.set.id == BOARDOBJGRP_GRP_CMD_ID_INVALID)
		return -EINVAL;

	if ((pcmd->hdrsize == 0) ||
		(pcmd->entrysize == 0) ||
		(pcmd->buf == NULL))
		return -EINVAL;

	/* If no objects in the group, return early */
	if (BOARDOBJGRP_IS_EMPTY(pboardobjgrp))
		return -EINVAL;

	/*
	 * Can only GET_STATUS if the BOARDOBJGRP has been previously SET to the
	 * PMU
	 */
	if (!pboardobjgrp->pmu.bset)
		return -EINVAL;

	/*
	 * alloc mem in vidmem & copy constructed pmu boardobjgrp data from
	 * sysmem to vidmem
	 */
	if (pcmd->surf.vidmem_desc.size == 0) {
		gk20a_pmu_vidmem_surface_alloc(g, &pcmd->surf.vidmem_desc,
			pcmd->fbsize);
	}

	/*
	 * Initialize PMU buffer with the mask of BOARDOBJGRPs for which to
	 * retrieve status
	 */

	memset(pcmd->buf, 0x0, pcmd->fbsize);
	status = pboardobjgrp->pmuhdrdatainit(g, pboardobjgrp,
					pcmd->buf, mask);
	if (status) {
		gk20a_err(dev_from_gk20a(g), "could not init PMU HDR data");
		goto boardobjgrp_pmugetstatus_exit;
	}

	gk20a_mem_wr_n(g, &pcmd->surf.vidmem_desc, 0, pset->buf, pset->hdrsize);
	/* Send the GET_STATUS PMU CMD to the PMU */
	status = boardobjgrp_pmucmdsend(g, pboardobjgrp,
				&pboardobjgrp->pmu.getstatus);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
				"could not send GET_STATUS cmd to PMU");
		goto boardobjgrp_pmugetstatus_exit;
	}

	/*copy the data back to sysmem buffer that belongs to command*/
	gk20a_mem_rd_n(g, &pcmd->surf.vidmem_desc, 0, pcmd->buf, pcmd->fbsize);

boardobjgrp_pmugetstatus_exit:
	return status;
}

static u32
boardobjgrp_objinsert_final(struct boardobjgrp *pboardobjgrp,
	struct boardobj *pboardobj, u8 index)
{

	gk20a_dbg_info("");

	if (pboardobjgrp == NULL)
		return -EINVAL;

	if (pboardobj == NULL)
		return -EINVAL;

	if (index > pboardobjgrp->objslots)
		return -EINVAL;

	if (pboardobjgrp->ppobjects[index] != NULL)
		return -EINVAL;

	/*
	 * Check that this BOARDOBJ has not already been added to a
	 * BOARDOBJGRP
	 */
	if (pboardobj->idx != CTRL_BOARDOBJ_IDX_INVALID)
		return -EINVAL;

	pboardobjgrp->ppobjects[index] = pboardobj;
	pboardobjgrp->objmaxidx = BOARDOBJGRP_IS_EMPTY(pboardobjgrp) ?
			index : max(pboardobjgrp->objmaxidx, index);
	pboardobj->idx = index;

	pboardobjgrp->objmask |= BIT(index);

	gk20a_dbg_info(" Done");

	return boardobjgrpmask_bitset(pboardobjgrp->mask, index);
}

static struct boardobj *boardobjgrp_objgetbyidx_final(
		struct boardobjgrp *pboardobjgrp, u8 index)
{
	if (!boardobjgrp_idxisvalid(pboardobjgrp, index))
		return NULL;
	return pboardobjgrp->ppobjects[index];
}

static struct boardobj *boardobjgrp_objgetnext_final(
		struct boardobjgrp *pboardobjgrp, u8 *currentindex,
		struct boardobjgrpmask *mask)
{
	struct boardobj *pboardobjnext = NULL;
	u8 objmaxidx;
	u8 index;

	if (currentindex == NULL)
		return NULL;

	if (pboardobjgrp == NULL)
		return NULL;

	/* Search from next element unless first object was requested */
	index = (*currentindex != CTRL_BOARDOBJ_IDX_INVALID) ?
		(*currentindex + 1) : 0;

	/* For the cases below in which we have to return NULL */
	*currentindex = CTRL_BOARDOBJ_IDX_INVALID;


	/* Validate provided mask */
	if (mask != NULL) {
		if (!(boardobjgrpmask_sizeeq(pboardobjgrp->mask, mask)))
			return NULL;
	}

	objmaxidx = pboardobjgrp->objmaxidx;

	if (objmaxidx != CTRL_BOARDOBJ_IDX_INVALID) {
		for (; index <= objmaxidx; index++) {
			pboardobjnext = pboardobjgrp->ppobjects[index];
			if (pboardobjnext != NULL) {
				/* Filter results using client provided mask.*/
				if (mask != NULL) {
					if (!boardobjgrpmask_bitget(mask,
						index)) {
						pboardobjnext = NULL;
						continue;
					}
				}
				*currentindex = index;
				break;
			}
		}
	}

	return pboardobjnext;
}

static u32 boardobjgrp_objremoveanddestroy_final(
		struct boardobjgrp *pboardobjgrp,
		u8 index)
{
	u32 status = 0;
	u32 stat;

	gk20a_dbg_info("");

	if (!boardobjgrp_idxisvalid(pboardobjgrp, index))
		return -EINVAL;

	if (pboardobjgrp->objmaxidx == CTRL_BOARDOBJ_IDX_INVALID)
		return -EINVAL;

	status = pboardobjgrp->ppobjects[index]->destruct(
			pboardobjgrp->ppobjects[index]);

	pboardobjgrp->ppobjects[index] = NULL;

	pboardobjgrp->objmask &= ~BIT(index);

	stat = boardobjgrpmask_bitclr(pboardobjgrp->mask, index);
	if (stat) {
		if (status == 0)
			status = stat;
	}

	/* objmaxidx requires update only if that very object was removed */
	if (pboardobjgrp->objmaxidx == index) {
		pboardobjgrp->objmaxidx =
			boardobjgrpmask_bitidxhighest(pboardobjgrp->mask);
	}

	return status;
}

void boardobjgrpe32hdrset(struct nv_pmu_boardobjgrp *hdr, u32 objmask)
{
	u32 slots = objmask;

	gk20a_dbg_info("");

	HIGHESTBITIDX_32(slots);
	slots++;

	hdr->super.type = CTRL_BOARDOBJGRP_TYPE_E32;
	hdr->super.class_id  = 0;
	hdr->super.obj_slots = (u8)slots;
	hdr->obj_mask = objmask;
}

static void boardobjgrp_pmucmdhandler(struct gk20a *g, struct pmu_msg *msg,
			void *param, u32 handle, u32 status)
{
	struct nv_pmu_boardobj_msg_grp	*pgrpmsg;
	struct boardobjgrp_pmucmdhandler_params *phandlerparams =
		(struct boardobjgrp_pmucmdhandler_params *)param;
	struct boardobjgrp *pboardobjgrp = phandlerparams->pboardobjgrp;
	struct boardobjgrp_pmu_cmd *pgrpcmd = phandlerparams->pcmd;

	gk20a_dbg_info("");

	pgrpmsg = &msg->msg.boardobj.grp;

	if (pgrpmsg->class_id != pboardobjgrp->pmu.classid) {
		gk20a_err(dev_from_gk20a(g),
			"Unrecognized GRP type: unit %x class id=0x%02x cmd id %x",
			msg->hdr.unit_id, pboardobjgrp->pmu.classid,
			pgrpcmd->id);
		return;
	}

	if (msg->msg.boardobj.msg_type != pgrpcmd->msgid) {
		gk20a_err(dev_from_gk20a(g),
			"unsupported msg for unit %x class %x cmd id %x msg %x",
			msg->hdr.unit_id, pboardobjgrp->pmu.classid,
			pgrpcmd->id, msg->msg.boardobj.msg_type);
		return;
	}

	if (msg->msg.boardobj.grp_set.flcn_status != 0) {
		gk20a_err(dev_from_gk20a(g),
			"cmd abort for unit %x class %x cmd id %x status %x",
			msg->hdr.unit_id, pboardobjgrp->pmu.classid,
			pgrpcmd->id,
			msg->msg.boardobj.grp_set.flcn_status);
		return;
	}

	phandlerparams->success = pgrpmsg->b_success ? 1 : 0;

	if (!pgrpmsg->b_success) {
		gk20a_err(dev_from_gk20a(g),
			"failed GRPCMD: msgtype=0x%x, classid=0x%x, cmd id %x",
			pgrpmsg->msg_type, pgrpmsg->class_id,
			pgrpcmd->id);
		return;
	}
}

static u32 boardobjgrp_pmucmdsend(struct gk20a *g,
				  struct boardobjgrp *pboardobjgrp,
				  struct boardobjgrp_pmu_cmd *pcmd)
{
	struct boardobjgrp_pmucmdhandler_params handlerparams = { 0 };
	struct pmu_payload payload = { {0} };
	struct nv_pmu_boardobj_cmd_grp *pgrpcmd;
	struct pmu_cmd cmd;
	u32 seqdesc;
	u32 status = 0;

	gk20a_dbg_info("");

	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id	= pboardobjgrp->pmu.unitid;
	cmd.hdr.size = sizeof(struct nv_pmu_boardobj_cmd_grp) +
					sizeof(struct pmu_hdr);

	pgrpcmd  = &cmd.cmd.boardobj.grp;
	pgrpcmd->cmd_type = pcmd->id;
	pgrpcmd->class_id = pboardobjgrp->pmu.classid;
	pgrpcmd->grp.hdr_size = pcmd->hdrsize;
	pgrpcmd->grp.entry_size = pcmd->entrysize;

	/*
	 * copy vidmem information to boardobj_cmd_grp
	 */
	gk20a_pmu_surface_describe(g, &pcmd->surf.vidmem_desc,
			&pgrpcmd->grp.fb);

	/*
	 * PMU reads command from sysmem so assigned
	 * "payload.in.buf = pcmd->buf"
	 * but PMU access pmu boardobjgrp data from vidmem copied above
	 */
	payload.in.buf = pcmd->buf;
	payload.in.size = max(pcmd->hdrsize, pcmd->entrysize);
	payload.in.fb_size = PMU_CMD_SUBMIT_PAYLOAD_PARAMS_FB_SIZE_UNUSED;
	payload.in.offset = offsetof(struct nv_pmu_boardobj_cmd_grp, grp);

	/* Setup the handler params to communicate back results.*/
	handlerparams.pboardobjgrp = pboardobjgrp;
	handlerparams.pcmd = pcmd;
	handlerparams.success = 0;

	status = gk20a_pmu_cmd_post(g, &cmd, NULL, &payload,
				PMU_COMMAND_QUEUE_LPQ,
				boardobjgrp_pmucmdhandler,
				(void *)&handlerparams,
				&seqdesc, ~0);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"unable to post boardobj grp cmd for unit %x cmd id %x",
			cmd.hdr.unit_id, pcmd->id);
		goto boardobjgrp_pmucmdsend_exit;
	}
	pmu_wait_message_cond(&g->pmu,
			gk20a_get_gr_idle_timeout(g),
			&handlerparams.success, 1);
	if (handlerparams.success == 0) {
		gk20a_err(dev_from_gk20a(g), "could not process cmd\n");
		status = -ETIMEDOUT;
		goto boardobjgrp_pmucmdsend_exit;
	}

boardobjgrp_pmucmdsend_exit:
	return status;
}
