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

#ifndef _ctrlboardobj_h_
#define _ctrlboardobj_h_

struct ctrl_boardobj {
	u8    type;
};

#define CTRL_BOARDOBJGRP_TYPE_INVALID 0x00
#define CTRL_BOARDOBJGRP_TYPE_E32 0x01
#define CTRL_BOARDOBJGRP_TYPE_E255 0x02

#define CTRL_BOARDOBJGRP_E32_MAX_OBJECTS  32

#define CTRL_BOARDOBJGRP_E255_MAX_OBJECTS 255

#define CTRL_BOARDOBJ_MAX_BOARD_OBJECTS                                 \
	CTRL_BOARDOBJGRP_E32_MAX_OBJECTS

#define CTRL_BOARDOBJ_IDX_INVALID 255

#define CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_BIT_SIZE  32

#define CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(_bit)                  \
	((_bit) / CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_BIT_SIZE)

#define CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(_bit)                 \
	((_bit) % CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_BIT_SIZE)

#define CTRL_BOARDOBJGRP_MASK_DATA_SIZE(_bits)                          \
	(CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX((_bits) - 1) + 1)


#define CTRL_BOARDOBJGRP_MASK_ARRAY_START_SIZE  1
#define CTRL_BOARDOBJGRP_MASK_ARRAY_EXTENSION_SIZE(_bits)          \
	(CTRL_BOARDOBJGRP_MASK_DATA_SIZE(_bits) -                           \
	 CTRL_BOARDOBJGRP_MASK_ARRAY_START_SIZE)

struct ctrl_boardobjgrp_mask {
	u32   data[1];
};

struct ctrl_boardobjgrp_mask_e32 {
	struct ctrl_boardobjgrp_mask super;
};

struct ctrl_boardobjgrp_mask_e255 {
	struct ctrl_boardobjgrp_mask super;
	u32   data_e255[7];
};

struct ctrl_boardobjgrp_super {
	struct ctrl_boardobjgrp_mask obj_mask;
};

struct ctrl_boardobjgrp_e32 {
	struct ctrl_boardobjgrp_mask_e32 obj_mask;
};

struct  CTRL_boardobjgrp_e255 {
	struct ctrl_boardobjgrp_mask_e255 obj_mask;
};

struct ctrl_boardobjgrp {
	u32    obj_mask;
};

#endif

