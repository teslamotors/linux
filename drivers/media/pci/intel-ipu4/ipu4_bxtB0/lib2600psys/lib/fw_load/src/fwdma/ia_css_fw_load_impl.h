/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2015, Intel Corporation.
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

#ifndef _IA_CSS_FW_LOAD_IMPL_H_
#define _IA_CSS_FW_LOAD_IMPL_H_

#include "fw_dma.h"
#include "ia_css_fw_load_non_blocking_impl.h"
#include "ia_css_fw_load.h"

IA_CSS_FW_LOAD_STORAGE_CLASS_C void
ia_css_fw_load_init(void)
{
	OP___printstring("[FWDMA] Load_init()\n");
	no_of_request = 0;
	/* Initialize fw_load eq for very first trasnfer
	e.g. SPC DMEM initialization when we use FWDMA we will use this eq for ack */
	fw_load_eq_init();
	/* set up terminal/channel/global descr for DMA firmware */
	fw_dma_setup();
}

IA_CSS_FW_LOAD_STORAGE_CLASS_C unsigned int
ia_css_fw_load_get_mode(void)
{
	return IA_CSS_DBUS_ADDRESS;
}

#endif /* _IA_CSS_FW_LOAD_IMPL_H_ */
