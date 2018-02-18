/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDE_CAMRTC_COMMANDS_H
#define INCLUDE_CAMRTC_COMMANDS_H

#define RTCPU_COMMAND(id, value)	((RTCPU_CMD_ ## id << 24) | (value))

#define RTCPU_GET_COMMAND_ID(value)	(((value) >> 24) & 0x7f)

#define RTCPU_GET_COMMAND_VALUE(value)	((value) & 0xffffff)

enum {
	RTCPU_CMD_INIT = 0,
	RTCPU_CMD_FW_VERSION = 1,
	RTCPU_CMD_IVC_READY,
	RTCPU_CMD_PING,
	RTCPU_CMD_PM_SUSPEND,
	RTCPU_CMD_FW_HASH,
	RTCPU_CMD_ERROR = 0x7f,
};

#define RTCPU_FW_DB_VERSION (0)
#define RTCPU_FW_VERSION (1)
#define RTCPU_FW_SM2_VERSION (2)

#define RTCPU_IVC_SANS_TRACE (1)
#define RTCPU_IVC_WITH_TRACE (2)

#define RTCPU_FW_HASH_SIZE (20)

#endif /* INCLUDE_CAMRTC_COMMANDS_H */
