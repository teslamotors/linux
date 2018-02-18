/*
 * tegra_cpc.c - Access CPC storage blocks through i2c bus
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef _TEGRA_CPC_H
#define _TEGRA_CPC_H

#include <linux/ioctl.h>
#include <linux/types.h>

enum req_resp_t {
	CPC_READ_M_COUNT = 0x2,
	CPC_WRITE_FRAME,
	CPC_READ_FRAME,
	CPC_GET_RESULT = 0x5,
	CPC_GET_VERSION = 0x6,
};

enum result_t {
	CPC_RESULT_OK = 0x0,
	CPC_RESULT_GENERAL_FAILURE,
	CPC_RESULT_AUTH_FAILURE,
	CPC_RESULT_COUNTER_FAILURE,
	CPC_RESULT_ADDR_FAILURE,
	CPC_RESULT_WRITE_FAILURE = 0x5,
	CPC_RESULT_READ_FAILURE,
	CPC_RESULT_UNKNOWN_REQUEST,
	CPC_RESULT_KEY_PROG_SEQ_FAILURE,
	CPC_RESULT_KEY_PROG_PG_CORRUPT,
	CPC_RESULT_KEY_PROG_DONE_PRIOR = 0xA,
	CPC_RESULT_DATA_LENGTH_MISMATCH,
	CPC_RESULT_UNEXPECTED_CONDITION,
	CPC_RESULT_CONTROLLER_BUSY
};

#define CPC_MAX_DATA_SIZE	48
#define CPC_KEY_SIZE		32
#define CPC_HMAC_SIZE		32
#define CPC_DERIVATION_SIZE	28
#define CPC_NONCE_SIZE		4
#define CPC_COUNTER_SIZE	4
#define CPC_MAJOR_VER_SIZE	3
#define CPC_MINOR_VER_SIZE	1

/*
  CPC field description

  req		is a command ID
  result	is the status as returned by uC. If the communication to uC was
		not succesful, this value will be set by the host
  len		is the byte indicating how many bytes in data are valid data
		For read, this tells the kernel what is the expected size of
		data which will be returned by uC
		For write, this tells the kernel how many bytes in the data
		field are valid data.
		The range of this is 0 < n && n <= CPC_MAX_DATA_SIZE
  nonce		is the counter which will be used for HMAC calculation. It is
		also used to correlate the response to the request. Host
		uses this value to match the response and the request
  write_counter	is the counter which indicates how many successful commits have
		been made by uC for the life time of uC
  data		is the field which the client of uC could use to send / receive
		data which will be stored permanently by uC.
  derivation_value
		This field should be used to pass the unique data during
		read counter, which will be used for encryption later by
		both parties
  HMAC		is the field which the client of uC could use to authenticate
		packets sent to and received from uC. If this field does not
		match uC's expectation, the packet will be rejected.
		HMAC is sent last tegra=>uC
		HMAC is sent first uC=>tegra
*/

/*
  Structs used for serialization and deserialization. The implementation is
  order sensitive. Any change to the struct would require a change to
  oneshot serialization / deserialization

  Notation: "Used for *" applies to all fields below until the end of struct
 */

struct tegra_cpc_read_counter_data {
	/* Used for request */
	__u8 nonce[CPC_NONCE_SIZE];

	/* Used for response */
	__u8 write_counter[CPC_COUNTER_SIZE];
	/* Used for request */
	__u8 derivation_value[CPC_DERIVATION_SIZE];
	/* Used for response */
	__u8 hmac[CPC_HMAC_SIZE];
} __packed;

struct tegra_cpc_write_frame_data {
	/* Used for request */
	__u8 length;
	__u8 nonce[CPC_NONCE_SIZE];
	__u8 write_counter[CPC_COUNTER_SIZE];
	__u8 data[CPC_MAX_DATA_SIZE];
	__u8 hmac[CPC_HMAC_SIZE];
} __packed;

struct tegra_cpc_read_frame_data {
	/* Used for request */
	__u8 length;
	__u8 nonce[CPC_NONCE_SIZE];

	/* Not used by kernel or uC as of now */
	__u8 write_counter[CPC_COUNTER_SIZE];

	/* Used for response */
	__u8 data[CPC_MAX_DATA_SIZE];
	__u8 hmac[CPC_HMAC_SIZE];
} __packed;

struct tegra_cpc_get_version_data {
	/* Used for request */
	__u8 nonce[CPC_NONCE_SIZE];

	/* Used for response */
	__u8 minor_ver[CPC_MINOR_VER_SIZE];
	__u8 major_ver[CPC_MAJOR_VER_SIZE];
} __packed;

union tegra_cpc_cmd_data {
	struct tegra_cpc_read_counter_data read_counter;
	struct tegra_cpc_read_frame_data read_frame;
	struct tegra_cpc_write_frame_data write_frame;
	struct tegra_cpc_get_version_data get_version;
};

struct tegra_cpc_frame {
	/* Used for request */
	__u8 req;

	/* Used for both request and response */
	union tegra_cpc_cmd_data cmd_data;

	/* Used for response of GET_STATUS */
	__u8 hmac[CPC_HMAC_SIZE];
	__u8 result;
	__u8 nonce[CPC_NONCE_SIZE];
} __packed;

#define CPC_CMD_FIELD_SIZE(target_type, field) \
	sizeof(((struct target_type *) 0)->field)

#define CPC_FIELD_SIZE(field) \
	CPC_CMD_FIELD_SIZE(struct tegra_cpc_frame, field)

#define NVCPC_IOC_MAGIC 'C'

/*
  Returns 0 if a communication with uC was successful, regardless of operation
  pass or fail.
*/
#define NVCPC_IOCTL_DO_IO	_IOWR(NVCPC_IOC_MAGIC, 2, \
					struct tegra_cpc_frame)

#endif
