/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef ICI_ISYS_CSI2_BE_H
#define ICI_ISYS_CSI2_BE_H

#include "ici-isys-subdev.h"
#include "ici-isys-stream.h"

#define CSI2_BE_ICI_PAD_SINK			0
#define CSI2_BE_ICI_PAD_SOURCE			1
#define NR_OF_CSI2_BE_ICI_PADS			2

#define CSI2_BE_ICI_CROP_HOR        (1 << 0)
#define CSI2_BE_ICI_CROP_VER        (1 << 1)
#define CSI2_BE_ICI_CROP_MASK       (CSI2_BE_ICI_CROP_VER | CSI2_BE_ICI_CROP_HOR)

struct ici_isys_csi2_be_pdata;
/*
 * struct ici_isys_csi2_be
 */
struct ici_isys_csi2_be {
	struct ici_isys_csi2_be_pdata *pdata;
	struct ici_isys_subdev asd;
	struct ici_isys_stream as;
};

int ici_isys_csi2_be_init(struct ici_isys_csi2_be
					*csi2_be,
			  struct ici_isys *isys, unsigned int type);
void ici_isys_csi2_be_cleanup(struct ici_isys_csi2_be
					*csi2_be);

#endif /* ICI_ISYS_CSI2_BE_H */
