/******************************************************************************
 * Intel mei_dal test Linux driver
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
#ifndef KDI_CMD_DEFS_H
#define KDI_CMD_DEFS_H

/**
 * enum kdi_command_id - cmd id to invoke in kdi module
 *
 * @KDI_SESSION_CREATE: call kdi "create session" function
 * @KDI_SESSION_CLOSE: call kdi "close session" function
 * @KDI_SEND_AND_RCV: call kdi "send and receive" function
 * @KDI_VERSION_GET_INFO: call kdi "get version" function
 * @KDI_EXCLUSIVE_ACCESS_SET: call kdi "set exclusive access" function
 * @KDI_EXCLUSIVE_ACCESS_REMOVE: call kdi "unset exclusive access" function
 */
enum kdi_command_id {
	KDI_SESSION_CREATE,
	KDI_SESSION_CLOSE,
	KDI_SEND_AND_RCV,
	KDI_VERSION_GET_INFO,
	KDI_EXCLUSIVE_ACCESS_SET,
	KDI_EXCLUSIVE_ACCESS_REMOVE
};

/**
 * struct kdi_test_command - contains the command received from user space
 *
 * @cmd_id: the command id
 * @data: the command data
 */
struct kdi_test_command {
	__u8 cmd_id;
	unsigned char data[0];
} __packed;

/**
 * struct session_create_cmd - create session cmd data
 *
 * @app_id_len: length of app_id arg
 * @acp_pkg_len: length of the acp_pkg arg
 * @init_param_len: length of init param arg
 * @is_session_handle_ptr: either send kdi a valid ptr to hold the
 *                         session handle or NULL
 * @data: buffer to hold the cmd arguments
 */
struct session_create_cmd {
	__u32 app_id_len;
	__u32 acp_pkg_len;
	__u32 init_param_len;
	__u8 is_session_handle_ptr;
	unsigned char data[0];
} __packed;

/**
 * struct session_create_resp - create session response
 *
 * @session_handle: the session handle
 * @test_mod_status: status returned from the test module
 * @status: status returned from kdi
 */
struct session_create_resp {
	__u64 session_handle;
	__s32 test_mod_status;
	__s32 status;
} __packed;

/**
 * struct session_close_cmd - close session cmd
 *
 * @session_handle: the session handle to close
 */
struct session_close_cmd {
	__u64 session_handle;
} __packed;

/**
 * struct session_close_resp - close session response
 *
 * @test_mod_status: status returned from the test module
 * @status: status returned from kdi
 */
struct session_close_resp {
	__s32 test_mod_status;
	__s32 status;
} __packed;

/**
 * struct send_and_rcv_cmd - send and receive cmd
 *
 * @session_handle: the session handle
 * @command_id: the cmd id to send the applet
 * @output_buf_len: the size of the output buffer
 * @is_output_buf: either send kdi a valid ptr to hold the output buffer or NULL
 * @is_output_len_ptr: either send kdi a valid ptr to hold
 *                     the output len or NULL
 * @is_response_code_ptr: either send kdi a valid ptr to hold
 *                        the applet response code or NULL
 * @input: the input data to send the applet
 */
struct send_and_rcv_cmd {
	__u64 session_handle;
	__u32 command_id;
	__u32 output_buf_len;
	__u8 is_output_buf;
	__u8 is_output_len_ptr;
	__u8 is_response_code_ptr;
	unsigned char input[0];
} __packed;

/**
 * struct send_and_rcv_resp - send and receive response
 *
 * @test_mod_status: status returned from the test module
 * @status: status returned from kdi
 * @response_code: response code returned from the applet
 * @output_len: length of output from the applet
 * @output: the output got from the applet
 */
struct send_and_rcv_resp {
	__s32 test_mod_status;
	__s32 status;
	__s32 response_code;
	__u32 output_len;
	unsigned char output[0];
} __packed;

/**
 * struct version_get_info_cmd - get version cmd
 *
 * @is_version_ptr: either send kdi a valid ptr to hold the version info or NULL
 */
struct version_get_info_cmd {
	__u8 is_version_ptr;
} __packed;

/**
 * struct version_get_info_resp - get version response
 *
 * @kdi_version: kdi version
 * @reserved: reserved bytes
 * @test_mod_status: status returned from the test module
 * @status: status returned from kdi
 */
struct version_get_info_resp {
	char kdi_version[32];
	__u32 reserved[4];
	__s32 test_mod_status;
	__s32 status;
} __packed;

/**
 * struct ta_access_set_remove_cmd - set/remove access cmd
 *
 * @app_id_len: length of app_id arg
 * @data: the cmd data. contains the app_id
 */
struct ta_access_set_remove_cmd {
	__u32 app_id_len;
	unsigned char data[0];
} __packed;

/**
 * struct ta_access_set_remove_resp - set/remove access response
 *
 * @test_mod_status: status returned from the test module
 * @status: status returned from kdi
 */
struct ta_access_set_remove_resp {
	__s32 test_mod_status;
	__s32 status;
} __packed;

#endif /* KDI_CMD_DEFS_H */
