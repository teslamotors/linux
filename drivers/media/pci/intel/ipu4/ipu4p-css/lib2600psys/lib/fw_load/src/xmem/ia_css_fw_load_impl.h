/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2018, Intel Corporation.
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

#ifndef __IA_CSS_FW_LOAD_IMPL_H
#define __IA_CSS_FW_LOAD_IMPL_H

#include "ia_css_fw_load.h"

IA_CSS_FW_LOAD_STORAGE_CLASS_C unsigned int
ia_css_fw_load_get_mode(void)
{
	return IA_CSS_CBUS_ADDRESS;
}

#endif /* __IA_CSS_FW_LOAD_IMPL_H */
