/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef _AON_SPI_MESSAGES_H_
#define _AON_SPI_MESSAGES_H_

#include <linux/types.h>
#define TEGRA_IVC_ALIGN 64
/* 24640 is emperically derived by observing the max len transactions
 * touch.
 */
#define AON_SPI_MAX_DATA_SIZE 24640

/* All the enums and the fields inside the structs described in this header
 * file supports only uX type, where X can be 8,16,32. For inter CPU commun-
 * ication, it is more stable to use this type.
 */

/* This enum represents the types of spi requests assocaited
 * with AON.
 */
enum aon_spi_request_type {
	AON_SPI_REQUEST_TYPE_INIT = 1,
	AON_SPI_REQUEST_TYPE_SETUP = 2,
	AON_SPI_REQUEST_TYPE_XFER = 3,
	AON_SPI_REQUEST_TYPE_SUSPEND = 4,
	AON_SPI_REQUEST_TYPE_RESUME = 5,
};

/*
 * This enum indicates the status of the request from CCPLEX.
 */
enum aon_spi_status {
	AON_SPI_STATUS_OK = 0,
	AON_SPI_STATUS_ERROR = 1,
};

/* This enum represents whether the current SPI transaction
 * is a read or write. Also indicates whether the current
 * message is the first or last in the context of a big xfer
 * split into multiple xfers.
 */
enum aon_spi_xfer_flag {
	AON_SPI_XFER_FLAG_WRITE = BIT(1),
	AON_SPI_XFER_FLAG_READ = BIT(2),
	AON_SPI_XFER_FIRST_MSG = BIT(3),
	AON_SPI_XFER_LAST_MSG = BIT(4),
	AON_SPI_XFER_HANDLE_CACHE = BIT(5),
};

/* This struct is used to setup the SPI client setup for AON
 * SPI controller.
 * Fields:
 * cs_setup_clk_count:		CS pin setup clock count
 * cs_hold_clk_count:		CS pin hold clock count
 * cs_inactive_cycles:		CS pin inactive clock count
 * set_rx_tap_delay:		Specify if the SPI device need to set
				RX tap delay
 * spi_max_clk_rate:		Specify the default clock rate of SPI client
 * spi_no_dma:			Flag to indicate pio or dma mode
 */
struct aon_spi_setup_request {
	u32 cs_setup_clk_count;
	u32 cs_hold_clk_count;
	u32 cs_inactive_cycles;
	u32 spi_max_clk_rate;
	u8 chip_select;
	u8 set_rx_tap_delay;
	bool spi_no_dma;
};

/* This struct indicates the parameters for SPI xfer from CCPLEX to SPE.
 * Fields:
 * spi_clk_rate:	Specify clock rate for current transfer
 * flags:		Indicate first/last message
 * length:		Current transfer length
 * tx_buf_offset:	Offset in the data field of the aon_spi_xfer_request
 *			struct for tx_buf contents. The buffer memory need to
 *			be aligned for DMA transfer
 * rx_buf_offset:	Offset in the data field of the aon_spi_xfer_request
 *			struct for rx_buf. The buffer memory need to be
 *			aligned for DMA transfer
 * chip_select:		Chip select of the slave device
 * bits_per_word:	Select bits_per_word
 * tx_nbits:		Number of bits used for writing
 * rx_nbits:		Number of bits used for reading
 *
 * When SPI can transfer in 1x,2x or 4x. It can get this tranfer information
 * from device through tx_nbits and rx_nbits. In Bi-direction, these
 * two should both be set. User can set transfer mode with SPI_NBITS_SINGLE(1x)
 * SPI_NBITS_DUAL(2x) and SPI_NBITS_QUAD(4x) to support these three transfer.
 */
struct aon_spi_xfer_params {
	u32 spi_clk_rate;
	/* enum aon_spi_xfer_flag */
	u16 flags;
	u16 length;
	u16 tx_buf_offset;
	u16 rx_buf_offset;
	u16 mode;
	u8 chip_select;
	u8 bits_per_word;
	u8 tx_nbits;
	u8 rx_nbits;
};

/* This struct indicates the contents of the xfer request from CCPLEX to SPE
 * for the AON SPI controller.
 *
 * Fields:
 * xfers;	Paramters for the current transfers.
 * data:	Buffer that holds the data for the current SPI ransaction.
 *		The size is aligned to the size of the cache line.
 */
struct aon_spi_xfer_request {
	union {
		struct aon_spi_xfer_params xfers;
		u8 align_t[TEGRA_IVC_ALIGN - sizeof(u32)];
	};
	u8 data[AON_SPI_MAX_DATA_SIZE];
};

/* This structure indicates the contents of the response from the remote CPU
 * i.e SPE for the previously requested transaction via CCPLEX proxy driver.
 *
 * Fields:
 * data:	This just matches the data field in the xfer request struct.
 *		All the response is stored in this buf and can be accessed by
 *		the offset fields known in the xfer params.
 */
struct aon_spi_xfer_response {
	union {
		u8 align_t[TEGRA_IVC_ALIGN - sizeof(u32)];
	};
	u8 data[AON_SPI_MAX_DATA_SIZE];
};

/* This structure indicates the current SPI request from CCPLEX to SPE for the
 * AON SPI controller.
 *
 * Fields:
 * req_type:	Indicates the type of request. Supports init, setup and xfer.
 * data:	Union of structs of all the request types.
 */
struct aon_spi_request {
	/* enum aon_spi_request_type */
	u32 req_type;
	union {
		struct aon_spi_setup_request setup;
		struct aon_spi_xfer_request xfer;
	} data;
};

/* This structure indicates the response for the SPI request from SPE to CCPLEX
 * for the AON SPI controller.
 *
 * Fields:
 * status:	Response in regard to the request i.e success/failure.
 * data:	Union of structs of all the response types.
 */
struct aon_spi_response {
	u32 status;
	union {
		struct aon_spi_xfer_response xfer;
	} data;
};

#endif
