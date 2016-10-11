/**
* Support for Intel Camera Imaging ISP subsystem.
* Copyright (c) 2010 - 2016, Intel Corporation.
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

/* implementation of buffer access from the CPU */
/* using shared_memory interface */

#include "buffer_access.h"
#include "vied/shared_memory_access.h"

void
buffer_load(
	buffer_address address,
	void *data,
	unsigned int bytes,
	unsigned int mm_id)
{
	shared_memory_load(mm_id, address, data, bytes);
}

void
buffer_store(
	buffer_address address,
	const void *data,
	unsigned int bytes,
	unsigned int mm_id)
{
	shared_memory_store(mm_id, address, data, bytes);
}

