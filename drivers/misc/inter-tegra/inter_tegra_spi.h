/*
 * drivers/misc/inter_tegra_spi.h
 *
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LINUX_INTER_TEGRA_SPI_H
#define _LINUX_INTER_TEGRA_SPI_H

#define PROTOCOL_VERSION	1

#define HEAD			PROTOCOL_VERSION

#define CMD_TEMP		0x01
#define CMD_PWM			0x02
#define CMD_TACH		0x03
#define CMD_10GBE		0x04

#define CMD_SUCCESS		0x10
#define CMD_ERROR		0x20

enum {
	TYPE_TEMP,
	TYPE_10GBE,
};

struct inter_tegra_data {
	struct spi_device *spi;
	struct mutex mutex;

	int is_master;
	const char *send_tz_type;
	struct thermal_zone_device *tx_thz;
	struct delayed_work work;
	struct mutex tx_mutex;

	struct task_struct *thread;
	struct thermal_zone_device *rx_thz;

	int send_period_ms;
	int receive_timeout_ms;

	int send_enable;
	int receive_enable;

	int tx_temp;
	int rx_temp;
};

struct inter_tegra_pkt_data {
	int value;
};

struct inter_tegra_pkt {
	u8 head;
	u8 cmd;
	u8 len;
	struct inter_tegra_pkt_data pkt_data;
	u8 check_sum;
} __attribute__((__packed__));

#endif /* _LINUX_INTER_TEGRA_SPI_H */
