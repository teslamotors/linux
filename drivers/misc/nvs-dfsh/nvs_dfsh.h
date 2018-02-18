/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef NVS_DFSH_H
#define NVS_DFSH_H

/*
 * SENSOR HUB FIRMWARE INTERFACE
 *
------------------------------------------------------
|            |            |            |             |
|  Start (S) |    Type    |  Payload   |   CRC32     |
|  (1-byte)  |  (1-byte)  |(0-18 bytes)|  (4-bytes)  |
|            |            |            |             |
------------------------------------------------------
*/

/* Packet start */
#define SENSOR_HUB_START		'S'

/* Packet type */
/* Messages from sensor hub to AP */
#define MSG_MCU				0x00
#define MSG_CAMERA			0x01
#define MSG_ACCEL			0x02
#define MSG_GYRO			0x03
#define MSG_MAGN			0x04
#define MSG_SENSOR_START		MSG_MCU
#define MSG_SENSOR_END			MSG_MAGN

#define SNSR_MSG_ID_START		MSG_CAMERA
#define SNSR_MSG_ID_END			MSG_SENSOR_END

/* Commands from AP to sensor hub */
#define CMD_VERSION			0x20
#define CMD_PING			0x21
#define CMD_START_TS		0x22
#define CMD_STOP_TS			0x23
#define CMD_CAM_PWR_ON		0x24
#define CMD_CAM_PWR_OFF		0x25
#define CMD_CAM_PWR_RESET	0x26
#define CMD_CAM_FSIN_START	0x27
#define CMD_CAM_FSIN_STOP	0x28

#define CMD_CAM_CAM0_PWDN_HIGH	0x29
#define CMD_CAM_CAM0_PWDN_LOW	0x2A
#define CMD_CAM_CAM1_PWDN_HIGH	0x2B
#define CMD_CAM_CAM1_PWDN_LOW	0x2C
#define CMD_CAM_CAM2_PWDN_HIGH	0x2D
#define CMD_CAM_CAM2_PWDN_LOW	0x2E

#define CMD_START			CMD_PING
#define CMD_END				CMD_CAM_CAM2_PWDN_LOW

/* Responses from sensor hub to AP */
#define RSP_VER				0x20
#define RSP_PING			0x21
#define RSP_START_TS		0x22
#define RSP_STOP_TS			0x23
#define RSP_CAM_PWR_ON		0x24
#define RSP_CAM_PWR_OFF		0x25
#define RSP_CAM_PWR_RESET	0x26
#define RSP_CAM_FSIN_START	0x27
#define RSP_CAM_FSIN_STOP	0x28

#define RSP_CAM_CAM0_PWDN_HIGH	0x29
#define RSP_CAM_CAM0_PWDN_LOW	0x2A
#define RSP_CAM_CAM1_PWDN_HIGH	0x2B
#define RSP_CAM_CAM1_PWDN_LOW	0x2C
#define RSP_CAM_CAM2_PWDN_HIGH	0x2D
#define RSP_CAM_CAM2_PWDN_LOW	0x2E

/* Packet payload structures */
struct __attribute__ ((__packed__)) camera_payload_t {
	uint64_t	timestamp;
};

struct  __attribute__ ((__packed__)) accel_payload_t {
	uint64_t	timestamp;
	uint16_t	accel[3];
};

struct  __attribute__ ((__packed__)) gyro_payload_t {
	uint64_t	timestamp;
	uint16_t	gyro[3];
};

struct  __attribute__ ((__packed__)) magn_payload_t {
	uint64_t	timestamp;
	uint32_t	magn[3];
};

struct  __attribute__ ((__packed__)) mcu_payload_t {
	union {
		uint32_t	cmd;
		uint32_t	rsp;
	};
};

/* Packet header */
struct __attribute__ ((__packed__)) dfsh_pkt_hdr_t {
	unsigned char	start;
	unsigned char	type;
};

struct __attribute__ ((__packed__)) dfsh_pkt_t {
	struct dfsh_pkt_hdr_t header;
	union {
		struct camera_payload_t	cam_payload;
		struct accel_payload_t	accel_payload;
		struct gyro_payload_t	gyro_payload;
		struct magn_payload_t	magn_payload;
		struct mcu_payload_t	mcu_payload;
	} payload;
	uint32_t crc32;
};

#endif /* NVS_DFSH_H */
