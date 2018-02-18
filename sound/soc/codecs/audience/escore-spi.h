/*
 * escore-spi.h  --  Audience eS705 SPI interface
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Hemal Meghpara <hmeghpara@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef _ESCORE_SPI_H
#define _ESCORE_SPI_H

extern struct spi_driver escore_spi_driver;

#define ES_SPI_BOOT_CMD			0x0001

#define ES_SPI_BOOT_ACK			0x0001

#define ES_SPI_SYNC_CMD		0x0000
#define ES_SPI_SYNC_ACK		0x0000

#define ES_SPI_8BITS_PER_WORD	8
#define ES_SPI_16BITS_PER_WORD	16
#define ES_SPI_32BITS_PER_WORD	32

/* This is obtained after discussion with FW team.*/
#define ESCORE_SPI_PACKET_LEN 256
/* This value is obtained after experimentation. Worst case streaming bandwidth
 * requirement is 3 FB channels. We could get this use case working only with
 * the delay given below (in usecs)
 */
#define ES_SPI_STREAM_READ_DELAY 30

#ifdef CONFIG_SND_SOC_ES_SPI_WRITE_DMA_MODE
#define ES_SPI_DMA_MIN_BYTES	512
#endif

extern struct es_stream_device es_spi_streamdev;
extern int escore_spi_init(void);
extern void escore_spi_exit(void);

#endif
