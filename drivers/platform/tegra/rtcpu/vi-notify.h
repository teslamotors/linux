/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDE_VI_NOTIFY_H
#define INCLUDE_VI_NOTIFY_H

/* Extended message types */
#define VI_NOTIFY_MSG_INVALID	0x00000000
#define VI_NOTIFY_MSG_ACK	0x00000002
#define VI_NOTIFY_MSG_STATUS	0x00000004

/* This must match libnvvi API header and vi-notifier enum in FW */
enum {
	VI_CAPTURE_STATUS_NONE,
	VI_CAPTURE_STATUS_SUCCESS,
	VI_CAPTURE_STATUS_CSIMUX_FRAME,
	VI_CAPTURE_STATUS_CSIMUX_STREAM,
	VI_CAPTURE_STATUS_CHANSEL_FAULT,
	VI_CAPTURE_STATUS_CHANSEL_FAULT_FE,
	VI_CAPTURE_STATUS_CHANSEL_COLLISION,
	VI_CAPTURE_STATUS_CHANSEL_SHORT_FRAME,
	VI_CAPTURE_STATUS_ATOMP_PACKER_OVERFLOW,
	VI_CAPTURE_STATUS_ATOMP_FRAME_TRUNCATED,
	VI_CAPTURE_STATUS_ATOMP_FRAME_TOSSED,
	VI_CAPTURE_STATUS_ISPBUF_FIFO_OVERFLOW,
	VI_CAPTURE_STATUS_SYNC_FAILURE,
	VI_CAPTURE_STATUS_NOTIFIER_BACKEND_DOWN,
};

/* Extended VI notify message */
struct vi_notify_msg_ex {
	u32 type;	/* message type (LSB=0) */
	u32 dest;	/* destination channels (bitmask) */
	u32 size;	/* data size */
	u8 data[];	/* payload data */
};

int tegra_ivc_vi_notify_report(const struct vi_notify_msg_ex *msg);

#endif /* INCLUDE_VI_NOTIFY_H */
