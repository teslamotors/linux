/*
 * Copyright (C) 2017 Tesla Motors, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MALI_C71_V4L_H__
#define __MALI_C71_V4L_H__

int mali_isp_register_if(struct platform_device *pdev);
void mali_isp_unregister_if(struct platform_device *pdev);

#endif /* __MALI_C71_V4L_H__ */
