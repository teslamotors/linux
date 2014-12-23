/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0 */
/*
 * Copyright (C) 2015-2018 Intel Corp. All rights reserved
 */
#ifndef _SPD_CMD_H
#define _SPD_CMD_H

#include <linux/types.h>

/**
 * enum spd_cmd_type - available commands
 *
 * @SPD_NONE_CMD       : Lower command sentinel.
 * @SPD_START_STOP_CMD : start stop command (deprecated). [Host -> TEE]
 * @SPD_RPMB_WRITE_CMD : RPMB write request.              [TEE -> Host]
 * @SPD_RPMB_READ_CMD  : RPMB read  request.              [TEE -> Host]
 * @SPD_RPMB_GET_COUNTER_CMD: get counter request         [TEE -> Host]
 * @SPD_GPP_WRITE_CMD  : GPP write request.               [TEE -> Host]
 * @SPD_GPP_READ_CMD   : GPP read request.                [TEE -> Host]
 * @SPD_TRIM_CMD       : TRIM command                     [TEE -> Host]
 * @SPD_INIT_CMD : initial handshake between host and fw. [Host -> TEE]
 * @SPD_STORAGE_STATUS_CMD : the backing storage status.  [Host -> TEE]
 * @SPD_MAX_CMD: Upper command sentinel.
 */
enum spd_cmd_type {
	SPD_NONE_CMD = 0,
	SPD_START_STOP_CMD,
	SPD_RPMB_WRITE_CMD,
	SPD_RPMB_READ_CMD,
	SPD_RPMB_GET_COUNTER_CMD,
	SPD_GPP_WRITE_CMD,
	SPD_GPP_READ_CMD,
	SPD_TRIM_CMD,
	SPD_INIT_CMD,
	SPD_STORAGE_STATUS_CMD,
	SPD_MAX_CMD,
};

enum spd_status {
	SPD_STATUS_SUCCESS             = 0,
	SPD_STATUS_GENERAL_FAILURE     = 1,
	SPD_STATUS_NOT_READY           = 2,
	SPD_STATUS_NOT_SUPPORTED       = 3,
	SPD_STATUS_INVALID_COMMAND     = 4,
};

/**
 * enum spd_storage_type - storage device type
 *
 * @SPD_TYPE_UNDEF: lower enum sentinel
 * @SPD_TYPE_EMMC:  emmc device
 * @SPD_TYPE_UFS:   ufs device
 * @SPD_TYPE_MAX:   upper enum sentinel
 */
enum spd_storage_type {
	SPD_TYPE_UNDEF = 0,
	SPD_TYPE_EMMC  = 1,
	SPD_TYPE_UFS   = 2,
	SPD_TYPE_MAX
};

/**
 * struct spd_cmd_hdr - Host storage Command Header
 *
 * @command_type: SPD_TYPES
 * @is_response: 1 == Response, 0 == Request
 * @len: command length
 * @status: command status
 * @reserved: reserved
 */
struct spd_cmd_hdr {
	u32    command_type :  7;
	u32    is_response  :  1;
	u32    len          : 13;
	u32    status       :  8;
	u32    reserved     :  3;
} __packed;

/**
 * RPMB Frame Size as defined by the JDEC spec
 */
#define SPD_CLIENT_RPMB_DATA_MAX_SIZE  (512)

/**
 * struct spd_cmd_init_resp
 *    commandType == HOST_STORAGE_INIT_CMD
 *
 * @gpp_partition_id: gpp_partition:
 *     UFS:  LUN Number (0-7)
 *     EMMC: 1-4.
 *     0xff: GPP not supported
 * @type: storage hw type
 *    SPD_TYPE_EMMC
 *    SPD_TYPE_UFS
 * @serial_no_sz: serial_no size
 * @serial_no: device serial number
 */
struct spd_cmd_init_resp {
	u32 gpp_partition_id;
	u32 type;
	u32 serial_no_sz;
	u8  serial_no[0];
};

/**
 * struct spd_cmd_storage_status_req
 *    commandType == SPD_STORAGE_STATUS_CMD
 *
 * @gpp_on: availability of the gpp backing storage
 *      0 - GP partition is accessible
 *      1 - GP partition is not accessible
 * @rpmb_on: availability of the backing storage
 *      0 - RPMB partition is accessible
 *      1 - RPBM partition is not accessible
 */
struct spd_cmd_storage_status_req {
	u32 gpp_on;
	u32 rpmb_on;
} __packed;

/**
 * struct spd_cmd_rpmb_write
 *    command_type == SPD_RPMB_WRITE_CMD
 *
 * @rpmb_frame: RPMB frame are constant size (512)
 */
struct spd_cmd_rpmb_write {
	u8    rpmb_frame[0];
} __packed;

/**
 * struct spd_cmd_rpmb_read
 *    command_type == SPD_RPMB_READ_CMD
 *
 * @rpmb_frame: RPMB frame are constant size (512)
 */
struct spd_cmd_rpmb_read {
	u8    rpmb_frame[0];
} __packed;

/**
 * struct spd_cmd_rpmb_get_counter
 *    command_type == SPD_RPMB_GET_COUNTER_CMD
 *
 * @rpmb_frame: frame containing frame counter
 */
struct spd_cmd_rpmb_get_counter {
	u8    rpmb_frame[0];
} __packed;

/**
 * struct spd_cmd_gpp_write_req
 *    command_type == SPD_GPP_WRITE_CMD
 *
 * @offset: frame offset in partition
 * @data: 4K page
 */
struct spd_cmd_gpp_write_req {
	u32    offset;
	u8     data[0];
} __packed;

/**
 * struct spd_cmd_gpp_write_rsp
 *    command_type == SPD_GPP_WRITE_CMD
 *
 * @reserved: reserved
 */
struct spd_cmd_gpp_write_rsp {
	u32    reserved[2];
} __packed;

/**
 * struct spd_cmd_gpp_read_req
 *    command_type == SPD_GPP_READ_CMD
 *
 * @offset: offset of a frame on GPP partition
 * @size_to_read: data length to read (must be )
 */
struct spd_cmd_gpp_read_req {
	u32    offset;
	u32    size_to_read;
} __packed;

/**
 * struct spd_cmd_gpp_read_rsp
 *    command_type == SPD_GPP_READ_CMD
 *
 * @reserved: reserved
 * @data: data
 */
struct spd_cmd_gpp_read_rsp {
	u32    reserved;
	u8     data[0];
} __packed;

#define SPD_GPP_READ_DATA_LEN(cmd)  ((cmd).header.len - \
			(sizeof(struct spd_cmd_hdr) + \
			 sizeof(struct spd_cmd_gpp_read_rsp)))

#define SPD_GPP_WRITE_DATA_LEN(cmd) ((cmd).header.len - \
			(sizeof(struct spd_cmd_hdr) + \
			 sizeof(struct spd_cmd_gpp_write_req)))

struct spd_cmd {
	struct spd_cmd_hdr    header;

	union {
		struct spd_cmd_rpmb_write         rpmb_write;
		struct spd_cmd_rpmb_read          rpmb_read;
		struct spd_cmd_rpmb_get_counter   rpmb_get_counter;

		struct spd_cmd_gpp_write_req      gpp_write_req;
		struct spd_cmd_gpp_write_rsp      gpp_write_rsp;

		struct spd_cmd_gpp_read_req       gpp_read_req;
		struct spd_cmd_gpp_read_rsp       gpp_read_resp;

		struct spd_cmd_init_resp          init_rsp;
		struct spd_cmd_storage_status_req status_req;
	};
} __packed;

/* GPP Max data 4K */
#define SPD_CLIENT_GPP_DATA_MAX_SIZE  (4096)

const char *spd_cmd_str(enum spd_cmd_type cmd);
const char *mei_spd_dev_str(enum spd_storage_type type);

#endif /* _SPD_CMD_H */
