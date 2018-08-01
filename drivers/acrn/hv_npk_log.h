/*
 * ACRN Hypervisor NPK Log
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2018 Intel Corporation. All rights reserved.
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
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _HV_NPK_LOG_H_
#define _HV_NPK_LOG_H_

#define NPK_DRV_NAME "intel_th_pci"

#define NPK_CHAN_SIZE    64
#define TH_MMIO_CONFIG   0
#define TH_MMIO_SW       2

#define PCI_REG_FW_LBAR 0x70
#define PCI_REG_FW_UBAR 0x74

#define REG_STH_STHCAP0 0x4000
#define REG_STH_STHCAP1 0x4004
#define REG_STH_STHCAP2 0x407C

#define HV_NPK_LOG_ENABLED   1
#define HV_NPK_LOG_DISABLED  0
#define HV_NPK_LOG_UNKNOWN   (-1)
#define HV_NPK_LOG_MAX_PARAM 4

#define HV_NPK_LOG_DFT_MASTER  74
#define HV_NPK_LOG_DFT_CHANNEL 0

enum {
	HV_NPK_LOG_CMD_INVALID,
	HV_NPK_LOG_CMD_CONF,
	HV_NPK_LOG_CMD_ENABLE,
	HV_NPK_LOG_CMD_DISABLE,
	HV_NPK_LOG_CMD_QUERY,
};

enum {
	HV_NPK_LOG_RES_INVALID,
	HV_NPK_LOG_RES_OK,
	HV_NPK_LOG_RES_KO,
	HV_NPK_LOG_RES_ENABLED,
	HV_NPK_LOG_RES_DISABLED,
};

struct hv_npk_log_conf {
	int status;
	int master;
	int channel;
	int loglevel;
	unsigned int fw_start;
	unsigned int fw_end;
	unsigned int sw_start;
	unsigned int sw_end;
	unsigned int nchan;
	phys_addr_t stmr_base;
	phys_addr_t stmr_end;
	phys_addr_t ftmr_base;
	phys_addr_t ftmr_end;
	phys_addr_t ch_addr;
};

#endif /* _HV_NPK_LOG_H_ */
