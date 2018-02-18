/*
 * tegra_nvfx_plugin.h - Shared NVFX interface for different plugins between
 *                       Tegra ADSP ALSA driver and ADSP side user space code.
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef _TEGRA_NVFX_PLUGIN_H_
#define _TEGRA_NVFX_PLUGIN_H_

/* Mode */
#define ADMA_MODE_ONESHOT       1
#define ADMA_MODE_CONTINUOUS    2
#define ADMA_MODE_LINKED_LIST   4

/* Direction */
#define ADMA_MEMORY_TO_MEMORY   1
#define ADMA_AHUB_TO_MEMORY     2
#define ADMA_MEMORY_TO_AHUB     4
#define ADMA_AHUB_TO_AHUB       8

#define EAVB_RX_DMA		0
#define EAVB_TX_DMA		1

/* EAVB DMA PLUGIN NAMES */
#define EAVB_RX_PLUGIN		"eavb_dma_rx"
#define EAVB_TX_PLUGIN		"eavb_dma_tx"

/* ADMA plugin related interface */
enum {
	/* NVFX ADMA params */
	nvfx_adma_method_init = nvfx_method_external_start,
	nvfx_adma_method_get_position,
};

typedef struct {
	nvfx_call_params_t call_params;
	int32_t adma_channel;
	int32_t mode;
	int32_t direction;
	int32_t ahub_channel;
	int32_t periods;
	variant_t event;
} nvfx_adma_init_params_t;

typedef struct {
	nvfx_call_params_t nvfx_call_params;
	uint32_t is_enabled;
	uint32_t buffer;
	uint32_t offset;
	uint64_t bytes_transferred;
} nvfx_adma_get_position_params_t;

typedef union {
	nvfx_req_call_params_t req_call;
	nvfx_adma_init_params_t init;
	nvfx_adma_get_position_params_t get_position;
} nvfx_adma_req_call_params_t;

/* EAVB DMA plugin related interface */
enum {
	/* NVFX EAVB DMA params */
	nvfx_eavbdma_method_init = nvfx_method_external_start,
	nvfx_eavbdma_method_get_position,
};

typedef struct {
	nvfx_call_params_t call_params;
	int32_t direction;
	variant_t event;
} nvfx_eavbdma_init_params_t;

typedef union {
	nvfx_req_call_params_t req_call;
	nvfx_eavbdma_init_params_t init;
} nvfx_eavbdma_req_call_params_t;

#endif /* #ifndef _TEGRA_NVFX_PLUGIN_H_ */
