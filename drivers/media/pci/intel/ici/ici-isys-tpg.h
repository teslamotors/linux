/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef ICI_ISYS_TPG_H
#define ICI_ISYS_TPG_H

#include "ici-isys-frame-buf.h"
#include "ici-isys-subdev.h"
#include "ici-isys-stream.h"

struct intel_ipu4_isys_tpg_pdata;

#define TPG_PAD_SOURCE			0
#define NR_OF_TPG_PADS			1

/*
 * struct ici_isys_tpg
 *
 *
*/
struct ici_isys_tpg {
	struct intel_ipu4_isys_tpg_pdata *pdata;
	struct ici_isys *isys;
	struct ici_isys_subdev asd;
	struct ici_isys_stream as;

	void __iomem *base;
	void __iomem *sel;
	int streaming;
	u32 receiver_errors;
	unsigned int nlanes;
	unsigned int index;
	atomic_t sof_sequence;
};

#define to_ici_isys_tpg(sd)					\
	container_of(sd, struct ici_isys_tpg, asd)

int ici_isys_tpg_init(struct ici_isys_tpg *tpg,
				struct ici_isys *isys,
				void __iomem *base, void __iomem *sel,
				unsigned int index);
void ici_isys_tpg_cleanup(struct ici_isys_tpg *tpg);

#endif /* ICI_ISYS_TPG_H */
