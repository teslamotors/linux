/*
 * tegra_nvfx.h - Shared NVFX interface between Tegra ADSP ALSA driver and
 *                ADSP side user space code.
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _TEGRA_NVFX_H_
#define _TEGRA_NVFX_H_

/**
 * variant_t
 *
 */
typedef union {
	uint64_t ptr;

	/* Standard C */
	void *pvoid;
	char *pchar;
	unsigned char *puchar;
	short *pshort;
	unsigned short *pushort;
	long *plong;
	unsigned long *pulong;
	int *pint;
	unsigned int *puint;
	float *pfloat;
	long long *pllong;
	unsigned long long *pullong;
	double *pdouble;

	/* stdint.h */
	int8_t    *pint8;
	uint8_t   *puint8;
	int16_t   *pint16;
	uint16_t  *puint16;
	int32_t   *pint32;
	uint32_t  *puint32;
} variant_t;

/* Memory can not be used for in-place transform */
#define NVFX_MEM_READ_ACCESS     0x1

/* Memory can only be written to */
#define NVFX_MEM_WRITE_ACCESS 0x2

/* Memory can read from or written to */
#define NVFX_MEM_ALL_ACCESS \
	(NVFX_MEM_READ_ACCESS | NVFX_MEM_WRITE_ACCESS)

/**
 * nvfx_process_state_t - Required shared process state
 *
 * @state           Current process state (i.e. active, inactive, etc.)
 * @count           Number invfx_t::process calls
 * @ts_last         Timestamp of last invfx_t::process call
 * @time_total      Time spent in all invfx_t::process calls
 * @time_high       Highest time spent in all invfx_t::process calls
 * @time_low        Lowest time spent in all invfx_t::process calls
 * @time_last       Time spent in the last invfx_t::process calls
 * @period          Actual scheduling period
 *
 */
typedef struct {
	int32_t  state;
	uint32_t count;
	uint64_t ts_last;
	uint64_t time_total;
	uint32_t time_high;
	uint32_t time_low;
	uint32_t time_last;
	uint32_t period;
} nvfx_process_state_t;

/**
 * nvfx_pin_state_t - Required shared pin state
 *
 * @bytes           Bytes consumed or produced
 * @frames          Frames consumed or produced
 *
 */
typedef struct {
	uint64_t bytes;
	uint32_t frames;
} nvfx_pin_state_t;

/**
 * Pin counts
 *
 * @NVFX_MAX_INPUT_PINS  The maximum number of input pins for an effect
 * @NVFX_MAX_OUTPUT_PINS The maximum number of output pins for an effect
 */
#define NVFX_MAX_INPUT_PINS         6
#define NVFX_MAX_OUTPUT_PINS        2
#define NVFX_MAX_RAW_DATA_WSIZE		1024

/**
 * nvfx_shared_state_t - Required shared state information
 *
 * @process         invfx_t::process related state
 * @input           State of input pins
 * @output          State of output pins
 *
 */
typedef struct {
	nvfx_process_state_t process;
	nvfx_pin_state_t input[NVFX_MAX_INPUT_PINS];
	nvfx_pin_state_t output[NVFX_MAX_OUTPUT_PINS];
	/* custom params */
} nvfx_shared_state_t;

/**
 * nvfx_call_params_t
 *
 * @NVFX_MAX_CALL_PARAMS_WSIZE  Max size in int32s of unioned call parameters
 * @size                        Size of the call parameters
 * @method                      The index of the function to call
 * [custom params]              Variable length buffer of parameters e.g.:
 *                                  uint32_t custom_params[...]
 */
typedef struct {
#define NVFX_MAX_CALL_PARAMS_WSIZE   128
	uint32_t size;
	uint32_t method;
	/* custom params */
} nvfx_call_params_t;

/**
 * Required FX Methods
 *
 * @nvfx_method_reset           Resets FX to default state
 * @nvfx_method_set_state       Sets the FX state
 * @nvfx_method_flush           Flushes input and cached data buffers
 *                              to support seek operations
 *
 * @nvfx_method_external_start: Start of externally defined FX methods
 *
 */
enum {
	nvfx_method_reset = 0,
	nvfx_method_set_state,
	nvfx_method_flush,

	nvfx_method_external_start = 65536,

	nvfx_method_force32 = 0x7fffffff
};

/**
 * nvfx_reset_params_t - Resets FX to default state
 *
 * @call                nvfx_call_t parameters
 *
 */
typedef struct {
	nvfx_call_params_t call;
} nvfx_reset_params_t;

/**
 * nvfx_method_set_state - Required FX States
 *
 * @nvfx_state_inactive         No processing except copying cached data
 * @nvfx_state_active           Process cached and/or new input data
 *
 * @nvfx_state_external_start   Start of externally defined FX States
 *
 */
enum {
	nvfx_state_inactive = 0,
	nvfx_state_active,

	nvfx_state_external_start = 65536,

	nvfx_state_force32 = 0x7fffffff
};

/**
 * nvfx_set_state_params_t - Sets the FX state
 *
 * @call                nvfx_call_t parameters
 * @state               State to set the FX
 *
 */
typedef struct {
	nvfx_call_params_t call;
	int32_t state;
} nvfx_set_state_params_t;

/**
 * nvfx_flush_params_t - Flushes input and cached data buffers to
 *                       support seek operations
 *
 * @call                nvfx_call_t parameters
 *
 */
typedef struct {
	nvfx_call_params_t call;
} nvfx_flush_params_t;

/**
 * nvfx_req_call_params_t - Parameters for required call functions
 *
 */
typedef union {
	nvfx_call_params_t call;
	nvfx_reset_params_t reset;
	nvfx_set_state_params_t set_state;
	nvfx_flush_params_t flush;
} nvfx_req_call_params_t;

/**
 * FX Types
 *
 * @NVFX_TYPE_HARDWARE      No software intervention (e.g. HW Module)
 * @NVFX_TYPE_IN_PORT       Brings data in (e.g. Stream)
 * @NVFX_TYPE_OUT_PORT      Returns data out (e.g. Stream, DMA)
 * @NVFX_TYPE_N_INPUT       Requires n input buffers (e.g. Mix)
 * @NVFX_TYPE_N_OUTPUT      Requires n output buffers (e.g. Splitter)
 * @NVFX_TYPE_IN_PLACE      Supports in place processing (e.g. Volume)
 * @NVFX_TYPE_REFORMAT      Reformats the data (e.g. Deycrypt, Decoder, SRC)
 * @NVFX_TYPE_MULTIPASS     Multiple processing passes (e.g. AEC)
 * @NVFX_TYPE_NONLINEAR     Non-linear processing (e.g. Reverb)
 *
 */
#define NVFX_TYPE_HARDWARE  0x1
#define NVFX_TYPE_IN_PORT   0x2
#define NVFX_TYPE_OUT_PORT  0x4
#define NVFX_TYPE_N_INPUT   0x10
#define NVFX_TYPE_N_OUTPUT  0x20
#define NVFX_TYPE_IN_PLACE  0x40
#define NVFX_TYPE_REFORMAT  0x80
#define NVFX_TYPE_MULTIPASS 0x100
#define NVFX_TYPE_NONLINEAR 0x200

#endif /* #ifndef _TEGRA_NVFX_H_ */
