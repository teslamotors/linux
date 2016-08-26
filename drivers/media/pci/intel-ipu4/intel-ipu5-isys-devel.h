/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * Put ipu5 isys development time tricks and hacks to this file
 */

#ifndef __INTEL_IPU5_ISYS_DEVEL_H__
#define __INTEL_IPU5_ISYS_DEVEL_H__

struct intel_ipu4_isys;

#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU5)

int intel_ipu5_isys_load_pkg_dir(struct intel_ipu4_isys *isys);

#else

static int intel_ipu5_isys_load_pkg_dir(struct intel_ipu4_isys *isys)
{
	return 0;
}

#endif
#endif
