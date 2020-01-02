/*
 * Copyright (C) 2018 Tesla, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __TRIP_FW_FMT_H__
#define __TRIP_FW_FMT_H__

/* Trip Firmware Header */
#define TRIP_FW_HDR_LEN			512

#define TRIP_FW_HDR_MAGIC_OFS		0
#define TRIP_FW_HDR_MAGIC_LEN		4
#define TRIP_FW_HDR_MAGIC		0x31574654

#define TRIP_FW_HDR_CRC32_OFS		4
#define TRIP_FW_HDR_CRC32_LEN		4

#define TRIP_FW_HDR_VERSION_STR_OFS	8
#define TRIP_FW_HDR_VERSION_STR_LEN	256

#define TRIP_FW_HDR_TIMESTAMP_OFS	264
#define TRIP_FW_HDR_TIMESTAMP_LEN	8

#define TRIP_FW_HDR_FEATURES_OFS	272
#define TRIP_FW_HDR_FEATURES_LEN	4

#define TRIP_FW_HDR_N_TCP_OFS		276
#define TRIP_FW_HDR_N_TCP_LEN		4

#define TRIP_FW_HDR_FILESIZE_OFS	280
#define TRIP_FW_HDR_FILESIZE_LEN	8

#define TRIP_FW_HDR_LOAD_ADDR_OFS	288
#define TRIP_FW_HDR_LOAD_ADDR_LEN	8

#define TRIP_FW_HDR_TCP_SIZE_OFS	296
#define TRIP_FW_HDR_TCP_SIZE_LEN	8

#define TRIP_FW_HDR_SPARE_ADDR_OFS	304
#define TRIP_FW_HDR_SPARE_ADDR_LEN	8


/* Trip Control Program Descriptors */
#define TRIP_FW_TCPD_LEN		256

#define TRIP_FW_TCPD_VERSION_STR_OFS	0
#define TRIP_FW_TCPD_VERSION_STR_LEN	128

#define TRIP_FW_TCPD_START_ADDR_OFS	128
#define TRIP_FW_TCPD_START_ADDR_LEN	8

#define TRIP_FW_TCPD_DATA_BUFSZ_OFS	136
#define TRIP_FW_TCPD_DATA_BUFSZ_LEN	8

#define TRIP_FW_TCPD_WEIGHT_BUFSZ_OFS	144
#define TRIP_FW_TCPD_WEIGHT_BUFSZ_LEN	8

#define TRIP_FW_TCPD_OUT_BUFSZ_OFS	152
#define TRIP_FW_TCPD_OUT_BUFSZ_LEN	8

#define TRIP_FW_TCPD_FN_UUID_OFS	160
#define TRIP_FW_TCPD_FN_UUID_LEN	16

#define TRIP_FW_TCPD_RUNTIME_MS_OFS	176
#define TRIP_FW_TCPD_RUNTIME_MS_LEN	4

#define TRIP_FW_TCPD_PROGRESS_WDT_OFS	180
#define TRIP_FW_TCPD_PROGRESS_WDT_LEN	4

#define TRIP_FW_TCPD_LAYER_WDT_OFS	184
#define TRIP_FW_TCPD_LAYER_WDT_LEN	4

#define TRIP_FW_TCPD_NETWORK_WDT_OFS	188
#define TRIP_FW_TCPD_NETWORK_WDT_LEN	4


#define TRIP_TCP_BLOB_ALIGN		4096

#endif /* __TRIP_FW_FMT_H__ */
