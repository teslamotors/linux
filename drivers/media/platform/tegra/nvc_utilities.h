/*
 * Copyright (c) 2013-2014 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NVC_UTILITIES_H__
#define __NVC_UTILITIES_H__

#include <linux/of.h>
#include <media/nvc.h>
#include <media/nvc_image.h>
#include <media/nvc_focus.h>

int nvc_imager_parse_caps(struct device_node *np,
		struct nvc_imager_cap *imager_cap,
		struct nvc_imager_static_nvc *imager_static);

unsigned long nvc_imager_get_mclk(const struct nvc_imager_cap *cap,
				  const struct nvc_imager_cap *cap_default,
				  int profile);

int nvc_focuser_parse_caps(struct device_node *np,
		struct nvc_focus_cap *focuser_cap,
		struct nvc_focus_nvc *static_info);

#endif /* __NVC_UTILITIES_H__ */
