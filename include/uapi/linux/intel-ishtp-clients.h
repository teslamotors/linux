/*
 * Intel ISHTP Clients Interface Header
 *
 * Copyright (c) 2016, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 */

#ifndef _INTEL_ISHTP_CLIENTS_H
#define _INTEL_ISHTP_CLIENTS_H

#include <linux/uuid.h>

/*
 * This IOCTL is used to associate the current file descriptor with a
 * FW Client (given by UUID). This opens a communication channel
 * between a host client and a FW client. From this point every read and write
 * will communicate with the associated FW client.
 * Only in close() (file_operation release()) the communication between
 * the clients is disconnected
 *
 * The IOCTL argument is a struct with a union that contains
 * the input parameter and the output parameter for this IOCTL.
 *
 * The input parameter is UUID of the FW Client.
 * The output parameter is the properties of the FW client
 * (FW protocol version and max message size).
 *
 */
#define IOCTL_ISHTP_CONNECT_CLIENT	_IOWR('H', 0x01,	\
				struct ishtp_connect_client_data)

/* Configuration: set number of Rx/Tx buffers. Must be used before connection */
#define IOCTL_ISHTP_SET_RX_FIFO_SIZE	_IOWR('H', 0x02, long)
#define IOCTL_ISHTP_SET_TX_FIFO_SIZE	_IOWR('H', 0x03, long)

/* Get FW status */
#define IOCTL_ISH_GET_FW_STATUS	_IO('H', 0x04)

#define IOCTL_ISH_HW_RESET	_IO('H', 0x05)

/*
 * Intel ISHTP client information struct
 */
struct ishtp_client {
	uint32_t max_msg_length;
	uint8_t protocol_version;
	uint8_t reserved[3];
};

/*
 * IOCTL Connect Client Data structure
 */
struct ishtp_connect_client_data {
	union {
		uuid_le in_client_uuid;
		struct ishtp_client out_client_properties;
	};
};

#endif /* _INTEL_ISHTP_CLIENTS_H */
