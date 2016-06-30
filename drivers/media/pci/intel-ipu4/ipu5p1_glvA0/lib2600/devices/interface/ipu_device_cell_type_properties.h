/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2015 - 2016 Intel Corporation.
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

#ifndef _IPU_DEVICE_CELL_TYPE_PROPERTIES_H_
#define _IPU_DEVICE_CELL_TYPE_PROPERTIES_H_

#define IPU_DEVICE_INVALID_MEM_ADDRESS 0xFFFFFFF0

enum ipu_device_cell_stat_ctrl_bit {
	IPU_DEVICE_CELL_STAT_CTRL_RESET_BIT = 0,
	IPU_DEVICE_CELL_STAT_CTRL_START_BIT = 1,
	IPU_DEVICE_CELL_STAT_CTRL_RUN_BIT = 3,
	IPU_DEVICE_CELL_STAT_CTRL_READY_BIT = 5,
	IPU_DEVICE_CELL_STAT_CTRL_SLEEP_BIT = 6,
	IPU_DEVICE_CELL_STAT_CTRL_STALL_BIT = 7,
	IPU_DEVICE_CELL_STAT_CTRL_CLEAR_IRQ_MASK_FLAG_BIT = 8,
	IPU_DEVICE_CELL_STAT_CTRL_BROKEN_IRQ_MASK_FLAG_BIT = 9,
	IPU_DEVICE_CELL_STAT_CTRL_READY_IRQ_MASK_FLAG_BIT = 10,
	IPU_DEVICE_CELL_STAT_CTRL_SLEEP_IRQ_MASK_FLAG_BIT = 11,
	IPU_DEVICE_CELL_STAT_CTRL_INVALIDATE_ICACHE_BIT = 12,
	IPU_DEVICE_CELL_STAT_CTRL_ICACHE_ENABLE_PREFETCH_BIT = 13
};

enum ipu_device_cell_reg_addr {
	IPU_DEVICE_CELL_STAT_CTRL_REG_ADDRESS	= 0x0,
	IPU_DEVICE_CELL_START_PC_REG_ADDRESS	= 0x4,
	IPU_DEVICE_CELL_ICACHE_BASE_REG_ADDRESS	= 0x10,
	IPU_DEVICE_CELL_ICACHE_INFO_BITS_REG_ADDRESS = 0x14
};

enum ipu_device_cell_reg {
	IPU_DEVICE_CELL_STAT_CTRL_REG,
	IPU_DEVICE_CELL_START_PC_REG,
	IPU_DEVICE_CELL_ICACHE_BASE_REG,
	IPU_DEVICE_CELL_DEBUG_PC_REG,
	IPU_DEVICE_CELL_STALL_REG,
	IPU_DEVICE_CELL_NUM_REGS
};

enum ipu_device_cell_mem {
	IPU_DEVICE_CELL_REGS,	/* memory id of registers */
	IPU_DEVICE_CELL_PMEM,	/* memory id of pmem */
	IPU_DEVICE_CELL_DMEM,	/* memory id of dmem */
	IPU_DEVICE_CELL_BAMEM,	/* memory id of bamem */
	IPU_DEVICE_CELL_VMEM	/* memory id of vmem */
};
#define IPU_DEVICE_CELL_NUM_MEMORIES (IPU_DEVICE_CELL_VMEM + 1)

enum ipu_device_cell_master {
	IPU_DEVICE_CELL_MASTER_ICACHE,	/* master port id of icache */
	IPU_DEVICE_CELL_MASTER_QMEM,
	IPU_DEVICE_CELL_MASTER_CMEM,
	IPU_DEVICE_CELL_MASTER_XMEM,
	IPU_DEVICE_CELL_MASTER_XVMEM
};
#define IPU_DEVICE_CELL_MASTER_NUM_MASTERS (IPU_DEVICE_CELL_MASTER_XVMEM + 1)

#endif /* _IPU_DEVICE_CELL_TYPE_PROPERTIES_H_ */
