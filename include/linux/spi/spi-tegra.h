/*
 * spi-tegra.h: SPI interface for Nvidia slink/spi controller.
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _LINUX_SPI_TEGRA_H
#define _LINUX_SPI_TEGRA_H

#include <linux/spi/spi.h>

struct tegra_spi_platform_data {
	int dma_req_sel;
	unsigned int spi_max_frequency;
	bool is_clkon_always;
	bool is_polling_mode;
	bool boost_reg_access;
	bool runtime_pm;
	u8 def_chip_select;
	int rx_trig_words;
	int ls_bit;
	int gpio_slave_ready;
	bool slave_ready_active_high;
	int max_dma_buffer_size;
	const char *clk_pin;
};

/*
 * Controller data from device to pass some info like
 * hw based chip select can be used or not and if yes
 * then CS hold and setup time.
 */
struct tegra_spi_device_controller_data {
	bool is_hw_based_cs;
	bool variable_length_transfer;
	/* Fix Me :: variable name*/
	bool new_features;
	int cs_setup_clk_count;
	int cs_hold_clk_count;
	int rx_clk_tap_delay;
	int tx_clk_tap_delay;
	int cs_inactive_cycles;
	int clk_delay_between_packets;
	int cs_gpio;
};

typedef int (*spi_callback)(void *client_data);

/**
 * Client can register atmost two callbacks for notification depending upon
 * the requirement. Ready_func can be used to get notification that the
 * controller is ready for transction. In the callback functions client can
 * do specific action like toggling gpio to inform master if slave is ready
 * or not.
 *
 * @spi: struct spi_device - refer to linux/spi/spi.h
 * @ready_func: Callback function to notify clilent that slave is ready.
 * @isr_func: Callback function to notify clilent that slave got interrupt.
 * @client_data: callback related data
 * Context: can not sleep
 */

int tegra_spi_slave_register_callback(struct spi_device *spi,
		spi_callback func_ready, spi_callback func_isr,
		void *client_data);

#endif /* _LINUX_SPI_TEGRA_H */
