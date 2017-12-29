/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * BSD LICENSE
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_VHM_IOCTL_DEFS_H_
#define	_VHM_IOCTL_DEFS_H_

/* Commmon structures for CWP/VHM/DM */
#include "cwp_common.h"

/*
 * Commmon IOCTL ID defination for VHM/DM
 */
#define _IC_ID(x, y) (((x)<<24)|(y))
#define IC_ID 0x5FUL

/* VM management */
#define IC_ID_VM_BASE                  0x0UL
#define IC_GET_API_VERSION             _IC_ID(IC_ID, IC_ID_VM_BASE + 0x00)
#define IC_CREATE_VM                   _IC_ID(IC_ID, IC_ID_VM_BASE + 0x01)
#define IC_DESTROY_VM                  _IC_ID(IC_ID, IC_ID_VM_BASE + 0x02)
#define IC_RESUME_VM                   _IC_ID(IC_ID, IC_ID_VM_BASE + 0x03)
#define IC_PAUSE_VM                    _IC_ID(IC_ID, IC_ID_VM_BASE + 0x04)
#define IC_QUERY_VMSTATE               _IC_ID(IC_ID, IC_ID_VM_BASE + 0x05)

#endif /* VHM_IOCTL_DEFS_H */
