/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2014 - 2015 Intel Corporation.
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

#ifndef _IPU_DEVICE_CELL_PROPERTIES_STRUCT_H_
#define _IPU_DEVICE_CELL_PROPERTIES_STRUCT_H_

/* definitions for all cell types */

struct ipu_device_cell_count_s {
	unsigned int num_memories;
	unsigned int num_master_ports;
	unsigned int num_stall_bits;
	unsigned int icache_align;
};

struct ipu_device_cell_master_properties_s {
	unsigned int segment_bits;
	unsigned int stride; /* offset to register of next segment */
	unsigned int base_address_register; /* address of first base address
					       register */
	unsigned int info_bits_register;
	unsigned int info_override_bits_register;
};

struct ipu_device_cell_type_properties_s {
	const struct ipu_device_cell_count_s *count;
	const struct ipu_device_cell_master_properties_s *master;
	const unsigned int *reg_offset; /* offsets of registers, some depend
					   on cell type */
	const unsigned int *mem_size;
};

struct ipu_device_cell_properties_s {
	const struct ipu_device_cell_type_properties_s *type_properties;
	const unsigned int *mem_address;
	const unsigned int *mem_databus_address;
	/* const cell_master_port_properties_s* master_port_properties; */
};

#endif /* _IPU_DEVICE_CELL_PROPERTIES_STRUCT_H_ */
