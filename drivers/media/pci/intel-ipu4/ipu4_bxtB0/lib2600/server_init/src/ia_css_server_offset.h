/*
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

#ifndef __IA_CSS_SERVER_OFFSET_H__
#define __IA_CSS_SERVER_OFFSET_H__

#include "ia_css_xmem.h"
#include "ia_css_pkg_dir_iunit.h"

unsigned int
ia_css_server_offset(int mmid,
	ia_css_xmem_address_t pkg_dir_address,
	enum ia_css_pkg_dir_index server_index);

#endif /*__IA_CSS_SERVER_OFFSET_H__*/
