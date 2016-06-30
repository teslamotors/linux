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

#ifndef _IPU_DEVICE_CELL_PROPERTIES_H_
#define _IPU_DEVICE_CELL_PROPERTIES_H_

#include "storage_class.h"
#include "ipu_device_cell_type_properties.h"

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_num_devices(void);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_num_memories(const unsigned int cell_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_memory_size(const unsigned int cell_id, const unsigned int mem_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_memory_address(const unsigned int cell_id, const unsigned int mem_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_databus_memory_address(const unsigned int cell_id, const unsigned int mem_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_num_masters(const unsigned int cell_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_segment_bits(const unsigned int cell_id, const unsigned int master_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_num_segments(const unsigned int cell_id, const unsigned int master_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_segment_size(const unsigned int cell_id, const unsigned int master_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_stride(const unsigned int cell_id, const unsigned int master_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_base_reg(const unsigned int cell_id, const unsigned int master_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_master_info_reg(const unsigned int cell_id, const unsigned int master_id);

STORAGE_CLASS_INLINE unsigned int
ipu_device_cell_icache_align(unsigned int cell_id);

#ifdef C_RUN
STORAGE_CLASS_INLINE int
ipu_device_cell_id_crun(int cell_id);
#endif

#include "ipu_device_cell_properties_func.h"

#endif /* _IPU_DEVICE_CELL_PROPERTIES_H_ */
