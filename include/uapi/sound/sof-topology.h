/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note)) OR BSD-3-Clause) */
/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/*
 * Topology IDs and tokens.
 *
 * ** MUST BE ALIGNED WITH TOPOLOGY CONFIGURATION TOKEN VALUES **
 */

#ifndef __INCLUDE_UAPI_SOF_TOPOLGOY_H__
#define __INCLUDE_UAPI_SOF_TOPOLOGY_H__

/*
 * Kcontrol IDs
 */
#define SOF_TPLG_KCTL_VOL_ID	256
#define SOF_TPLG_KCTL_ENUM_ID	257
#define SOF_TPLG_KCTL_BYTES_ID	258

/*
 * Tokens - must match values in topology configurations
 */

/* buffers */
#define SOF_TKN_BUF_SIZE			100
#define SOF_TKN_BUF_CAPS			101

/* DAI */
#define SOF_TKN_DAI_DMAC			151
#define	SOF_TKN_DAI_DMAC_CHAN			152
#define	SOF_TKN_DAI_DMAC_CONFIG			153
#define SOF_TKN_DAI_TYPE			154
#define SOF_TKN_DAI_INDEX			155
#define SOF_TKN_DAI_SAMPLE_BITS			156

/* scheduling */
#define SOF_TKN_SCHED_DEADLINE			200
#define SOF_TKN_SCHED_PRIORITY			201
#define SOF_TKN_SCHED_MIPS			202
#define SOF_TKN_SCHED_CORE			203
#define SOF_TKN_SCHED_FRAMES			204
#define SOF_TKN_SCHED_TIMER			205

/* volume */
#define SOF_TKN_VOLUME_RAMP_STEP_TYPE		250
#define SOF_TKN_VOLUME_RAMP_STEP_MS		251

/* SRC */
#define SOF_TKN_SRC_RATE_IN			300
#define SOF_TKN_SRC_RATE_OUT			301

/* PCM */
#define SOF_TKN_PCM_DMAC			351
#define SOF_TKN_PCM_DMAC_CHAN			352
#define SOF_TKN_PCM_DMAC_CONFIG			353

/* Generic components */
#define SOF_TKN_COMP_PERIOD_SINK_COUNT		400
#define SOF_TKN_COMP_PERIOD_SOURCE_COUNT	401
#define SOF_TKN_COMP_FORMAT			402
#define SOF_TKN_COMP_PRELOAD_COUNT		403

#endif
