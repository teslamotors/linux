/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2014 - 2016 Intel Corporation.
* All Rights Reserved.
*
* The source code contained or described herein and all documents
* related to the source code ("Material") are owned by Intel Corporation
* or licensors. Title to the Material remains with Intel
* Corporation or its licensors. The Material contains trade
* secrets and proprietary and confidential information of Intel or its
* licensors. The Material is protected by worldwide copyright
* and trade secret laws and treaty provisions. No part of the Material may
* be used, copied, reproduced, modified, published, uploaded, posted,
* transmitted, distributed, or disclosed in any way without Intel's prior
* express written permission.
*
* No License under any patent, copyright, trade secret or other intellectual
* property right is granted to or conferred upon you by disclosure or
* delivery of the Materials, either expressly, by implication, inducement,
* estoppel or otherwise. Any license under such intellectual property rights
* must be express and approved by Intel in writing.
*/

#ifndef _IA_CSS_CELL_H_
#define _IA_CSS_CELL_H_

#include "storage_class.h"
#include "type_support.h"

STORAGE_CLASS_INLINE unsigned int
ia_css_cell_get_stat_ctrl(unsigned int ssid, unsigned int cell_id);

STORAGE_CLASS_INLINE void
ia_css_cell_set_stat_ctrl(unsigned int ssid, unsigned int cell_id, unsigned int value);

STORAGE_CLASS_INLINE void
ia_css_cell_set_start_pc(unsigned int ssid, unsigned int cell_id, unsigned int pc);

STORAGE_CLASS_INLINE void
ia_css_cell_set_icache_base_address(unsigned int ssid, unsigned int cell_id, unsigned int value);

#if 0 /* To be implemented after completing cell device properties */
STORAGE_CLASS_INLINE void
ia_css_cell_set_icache_info_bits(unsigned int ssid, unsigned int cell_id, unsigned int value);

STORAGE_CLASS_INLINE unsigned int
ia_css_cell_get_debug_pc(unsigned int ssid, unsigned int cell_id);

STORAGE_CLASS_INLINE unsigned int
ia_css_cell_get_stall_bits(unsigned int ssid, unsigned int cell_id);
#endif

/* configure master ports */

STORAGE_CLASS_INLINE void
ia_css_cell_set_master_base_address(unsigned int ssid, unsigned int cell_id,
	unsigned int master, unsigned int value);

STORAGE_CLASS_INLINE void
ia_css_cell_set_master_segment_base_address(unsigned int ssid, unsigned int cell_id,
	unsigned int master, unsigned int segment, unsigned int value);

STORAGE_CLASS_INLINE void
ia_css_cell_set_master_info_bits(unsigned int ssid, unsigned int cell_id,
	unsigned int master, unsigned int value);

STORAGE_CLASS_INLINE void
ia_css_cell_set_master_segment_info_bits(unsigned int ssid, unsigned int cell_id,
	unsigned int master, unsigned int segment, unsigned int value);

STORAGE_CLASS_INLINE void
ia_css_cell_set_master_info_override_bits(unsigned int ssid, unsigned int cell,
	unsigned int master, unsigned int value);

STORAGE_CLASS_INLINE void
ia_css_cell_set_master_segment_info_override_bits(unsigned int ssid, unsigned int cell,
	unsigned int master, unsigned int segment, unsigned int value);

/* Access memories */

STORAGE_CLASS_INLINE void
ia_css_cell_mem_store_32(unsigned int ssid, unsigned int cell_id,
	unsigned int mem_id, unsigned int addr, unsigned int value);

STORAGE_CLASS_INLINE unsigned int
ia_css_cell_mem_load_32(unsigned int ssid, unsigned int cell_id,
	unsigned int mem_id, unsigned int addr);

/******************************************************************************************/

STORAGE_CLASS_INLINE unsigned int
ia_css_cell_is_ready(unsigned int ssid, unsigned int cell_id);

STORAGE_CLASS_INLINE void
ia_css_cell_set_start_bit(unsigned int ssid, unsigned int cell_id);

STORAGE_CLASS_INLINE void
ia_css_cell_set_run_bit(unsigned int ssid, unsigned int cell_id, unsigned int value);

STORAGE_CLASS_INLINE void
ia_css_cell_start(unsigned int ssid, unsigned int cell_id);

STORAGE_CLASS_INLINE void
ia_css_cell_start_prefetch(unsigned int ssid, unsigned int cell_id, bool prefetch);

STORAGE_CLASS_INLINE void
ia_css_cell_wait(unsigned int ssid, unsigned int cell_id);

/* include inline implementation */
#include "ia_css_cell_impl.h"

#endif /* _IA_CSS_CELL_H_ */
