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

#ifndef __BUFFER_ACCESS_H
#define __BUFFER_ACCESS_H

#include "buffer_type.h"
/* #def to keep consistent the buffer load interfaces for host and css */
#define IDM				0

void
buffer_load(
	buffer_address address,
	void *data,
	unsigned int size,
	unsigned int mm_id);

void
buffer_store(
	buffer_address address,
	const void *data,
	unsigned int size,
	unsigned int mm_id);

#endif /* __BUFFER_ACCESS_H */
