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

#ifndef __TRIP_FW_H__
#define __TRIP_FW_H__

/* Header information covering whole firwmare file */
struct trip_fw_hdr {
	char		version[TRIP_FW_HDR_VERSION_STR_LEN];
	uint64_t	timestamp;
	uint32_t	crc32;
	uint32_t	features;
	uint64_t	load_addr;
	uint64_t	tcp_size;
	uint64_t	spare_addr;
};

/* One descriptor per network (Trip control program) in firmware file */
struct trip_fw_tcp_desc {
	char		version[TRIP_FW_TCPD_VERSION_STR_LEN];
	uint64_t	start_addr;
	uint64_t	data_bufsz;
	uint64_t	weight_bufsz;
	uint64_t	out_bufsz;
	uint8_t		function_uuid[TRIP_FW_TCPD_FN_UUID_LEN];
	uint32_t	runtime_ms;
	uint32_t	progress_wdt;
	uint32_t	layer_wdt;
	uint32_t	network_wdt;
};

/* Records information about currently loaded firmware, if any */
struct trip_fw_info {
	spinlock_t fw_load_lock;
	bool fw_load_ready;
	bool fw_available;

	bool fw_valid;
	struct trip_fw_hdr hdr;

	uint32_t n_tcpd;
	struct trip_fw_tcp_desc *tcpd;
};

#ifdef CONFIG_TESLA_TRIP_FW
int trip_fw_load(struct platform_device *pdev, const char *filename,
			struct trip_fw_info *fw_info);
#else /* CONFIG_TESLA_TRIP_FW */
static inline int trip_fw_load(struct platform_device *pdev,
				const char *filename,
				struct trip_fw_info *fw_info)
{
	return 0;
}
#endif /* CONFIG_TESLA_TRIP_FW */

#endif /* __TRIP_FW_H__ */
