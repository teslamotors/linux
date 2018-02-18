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
#ifndef _GPMUIFBOARDOBJ_H_
#define _GPMUIFBOARDOBJ_H_

#include "gk20a/gk20a.h"
#include "gk20a/pmu_gk20a.h"
#include "ctrl/ctrlboardobj.h"

/*
 * Base structure describing a BOARDOBJ for communication between Kernel and
 * PMU.
 */
struct nv_pmu_boardobj {
	u8 type;
};

/*
 * Base structure describing a BOARDOBJ for Query interface between Kernel and
 * PMU.
 */
struct nv_pmu_boardobj_query {
	u8 type;
};

/*
 * Virtual base structure describing a BOARDOBJGRP interface between Kernel and
 * PMU.
 */
struct nv_pmu_boardobjgrp_super {
	u8 type;
	u8 class_id;
	u8 obj_slots;
	u8 rsvd;
};

struct nv_pmu_boardobjgrp {
	struct nv_pmu_boardobjgrp_super super;
	u32 obj_mask;
};

struct nv_pmu_boardobjgrp_e32 {
	struct nv_pmu_boardobjgrp_super super;
	struct ctrl_boardobjgrp_mask_e32 obj_mask;
};

struct nv_pmu_boardobjgrp_e255 {
	struct nv_pmu_boardobjgrp_super super;
	struct ctrl_boardobjgrp_mask_e255 obj_mask;
};

struct nv_pmu_boardobj_cmd_grp_payload {
	struct pmu_allocation_v3 dmem_buf;
	struct flcn_mem_desc_v0 fb;
	u8 hdr_size;
	u8 entry_size;
};

struct nv_pmu_boardobj_cmd_grp {
	u8 cmd_type;
	u8 pad[2];
	u8 class_id;
	struct nv_pmu_boardobj_cmd_grp_payload grp;
};

#define NV_PMU_BOARDOBJ_GRP_ALLOC_OFFSET                                       \
	(NV_OFFSETOF(NV_PMU_BOARDOBJ_CMD_GRP, grp))

struct nv_pmu_boardobj_cmd {
	union {
		u8 cmd_type;
		struct nv_pmu_boardobj_cmd_grp grp;
		struct nv_pmu_boardobj_cmd_grp grp_set;
		struct nv_pmu_boardobj_cmd_grp grp_get_status;
	};
};

struct nv_pmu_boardobj_msg_grp {
	u8 msg_type;
	bool b_success;
	flcn_status flcn_status;
	u8 class_id;
};

struct nv_pmu_boardobj_msg {
	union {
		u8 msg_type;
		struct nv_pmu_boardobj_msg_grp grp;
		struct nv_pmu_boardobj_msg_grp grp_set;
		struct nv_pmu_boardobj_msg_grp grp_get_status;
	};
};

/*
* Macro generating structures describing classes which implement
* NV_PMU_BOARDOBJGRP via the NV_PMU_BOARDBOBJ_CMD_GRP SET interface.
*
* @para    _eng    Name of implementing engine in which this structure is
* found.
* @param   _class  Class ID of Objects within Board Object Group.
* @param   _slots  Max number of elements this group can contain.
*/
#define NV_PMU_BOARDOBJ_GRP_SET_MAKE(_eng, _class, _slots)                     \
	NV_PMU_MAKE_ALIGNED_STRUCT(                                            \
	nv_pmu_##_eng##_##_class##_boardobjgrp_set_header, one_structure);     \
	NV_PMU_MAKE_ALIGNED_UNION(                                             \
	nv_pmu_##_eng##_##_class##_boardobj_set_union, one_union);             \
	struct nv_pmu_##_eng##_##_class##_boardobj_grp_set {                   \
	union nv_pmu_##_eng##_##_class##_boardobjgrp_set_header_aligned  hdr;        \
	union nv_pmu_##_eng##_##_class##_boardobj_set_union_aligned objects[(_slots)];\
	}

/*
* Macro generating structures describing classes which implement
* NV_PMU_BOARDOBJGRP_E32 via the NV_PMU_BOARDBOBJ_CMD_GRP SET interface.
*
* @para    _eng    Name of implementing engine in which this structure is
* found.
* @param   _class  Class ID of Objects within Board Object Group.
*/
#define NV_PMU_BOARDOBJ_GRP_SET_MAKE_E32(_eng, _class)                         \
	NV_PMU_BOARDOBJ_GRP_SET_MAKE(_eng, _class,                             \
	CTRL_BOARDOBJGRP_E32_MAX_OBJECTS)

/*
* Macro generating structures describing classes which implement
* NV_PMU_BOARDOBJGRP_E255 via the NV_PMU_BOARDBOBJ_CMD_GRP SET interface.
*
* @para    _eng    Name of implementing engine in which this structure is
* found.
* @param   _class  Class ID of Objects within Board Object Group.
*/
#define NV_PMU_BOARDOBJ_GRP_SET_MAKE_E255(_eng, _class)                        \
	NV_PMU_BOARDOBJ_GRP_SET_MAKE(_eng, _class,                             \
	CTRL_BOARDOBJGRP_E255_MAX_OBJECTS)

/*
* Macro generating structures for querying dynamic state for classes which
* implement NV_PMU_BOARDOBJGRP via the NV_PMU_BOARDOBJ_CMD_GRP GET_STATUS
* interface.
*
* @para    _eng    Name of implementing engine in which this structure is
* found.
* @param   _class  Class ID of Objects within Board Object Group.
* @param   _slots  Max number of elements this group can contain.
*/
#define NV_PMU_BOARDOBJ_GRP_GET_STATUS_MAKE(_eng, _class, _slots)              \
	NV_PMU_MAKE_ALIGNED_STRUCT(                                            \
	nv_pmu_##_eng##_##_class##_boardobjgrp_get_status_header, struct);     \
	NV_PMU_MAKE_ALIGNED_UNION(                                             \
	nv_pmu_##_eng##_##_class##_boardobj_get_status_union, union);          \
	struct  nv_pmu_##_eng##_##_class##_boardobj_grp_get_status {           \
	union nv_pmu_##_eng##_##_class##_boardobjgrp_get_status_header_aligned \
	hdr;                                                                   \
	union nv_pmu_##_eng##_##_class##_boardobj_get_status_union_aligned     \
	objects[(_slots)];                                                     \
	}

/*
* Macro generating structures for querying dynamic state for classes which
* implement NV_PMU_BOARDOBJGRP_E32  via the NV_PMU_BOARDOBJ_CMD_GRP GET_STATUS
* interface.
*
* @para    _eng    Name of implementing engine in which this structure is
* found.
* @param   _class  Class ID of Objects within Board Object Group.
*/
#define NV_PMU_BOARDOBJ_GRP_GET_STATUS_MAKE_E32(_eng, _class)                  \
	NV_PMU_BOARDOBJ_GRP_GET_STATUS_MAKE(_eng, _class,                      \
	CTRL_BOARDOBJGRP_E32_MAX_OBJECTS)

/*
* Macro generating structures for querying dynamic state for classes which
* implement NV_PMU_BOARDOBJGRP_E255 via the NV_PMU_BOARDOBJ_CMD_GRP GET_STATUS
* interface.
*
* @para    _eng    Name of implementing engine in which this structure is
* found.
* @param   _class  Class ID of Objects within Board Object Group.
*/
#define NV_PMU_BOARDOBJ_GRP_GET_STATUS_MAKE_E255(_eng, _class)                 \
	NV_PMU_BOARDOBJ_GRP_GET_STATUS_MAKE(_eng, _class,                      \
	CTRL_BOARDOBJGRP_E255_MAX_OBJECTS)


#endif /*  _GPMUIFBOARDOBJ_H_ */
