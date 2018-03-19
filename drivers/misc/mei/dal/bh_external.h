/******************************************************************************
 * Intel mei_dal Linux driver
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
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
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * Contact Information:
 *	Intel Corporation.
 *	linux-mei@linux.intel.com
 *	http://www.intel.com
 *
 * BSD LICENSE
 *
 * Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 *****************************************************************************/

#ifndef __BH_EXTERNAL_H
#define __BH_EXTERNAL_H

#include <linux/kernel.h>
#include "bh_cmd_defs.h"

# define MSG_SEQ_START_NUMBER BIT_ULL(32)

bool bh_is_initialized(void);
void bh_init_internal(void);
void bh_deinit_internal(void);

int bh_ta_session_open(u64 *host_id, const char *ta_id, const u8 *ta_pkg,
		       size_t pkg_len, const u8 *init_param, size_t init_len);

int bh_ta_session_close(u64 host_id);

int bh_ta_session_command(u64 host_id, int command_id, const void *input,
			  size_t length, void **output, size_t *output_length,
			  int *response_code);

const struct bh_command_header *bh_msg_cmd_hdr(const void *msg, size_t len);

typedef int (*bh_filter_func)(const struct bh_command_header *hdr,
			      size_t count, void *ctx);

int bh_filter_hdr(const struct bh_command_header *hdr, size_t count, void *ctx,
		  const bh_filter_func tbl[]);

bool bh_msg_is_cmd_open_session(const struct bh_command_header *hdr);

const uuid_t *bh_open_session_ta_id(const struct bh_command_header *hdr,
				    size_t count);

void bh_prep_access_denied_response(const char *cmd,
				    struct bh_response_header *res);

bool bh_msg_is_cmd(const void *msg, size_t len);
bool bh_msg_is_response(const void *msg, size_t len);

#endif /* __BH_EXTERNAL_H */
