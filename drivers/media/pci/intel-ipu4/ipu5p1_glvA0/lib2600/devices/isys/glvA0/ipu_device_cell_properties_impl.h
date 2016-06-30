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

/*
 * TODO: IPU5_SDK Verify that this file is correct when IPU5 SDK has been released.
 */

#ifndef _IPU_DEVICE_CELL_PROPERTIES_IMPL_H_
#define _IPU_DEVICE_CELL_PROPERTIES_IMPL_H_

/* define properties for all cells uses in ISYS */

#include "ipu_device_sp2600_control_properties_impl.h"
#include "ipu_device_cell_properties_defs.h"
#include "ipu_device_cell_devices.h"
#include "ipu_device_cell_type_properties.h" /* IPU_DEVICE_INVALID_MEM_ADDRESS */

static const unsigned int ipu_device_spc0_mem_address[IPU_DEVICE_SP2600_CONTROL_NUM_MEMORIES] = {
	SPC0_REGS_CBUS_ADDRESS,
	IPU_DEVICE_INVALID_MEM_ADDRESS,	/* no pmem */
	SPC0_DMEM_CBUS_ADDRESS
};

static const unsigned int ipu_device_spc0_databus_mem_address[IPU_DEVICE_SP2600_CONTROL_NUM_MEMORIES] = {
	IPU_DEVICE_INVALID_MEM_ADDRESS, /* regs not accessible from DBUS */
	IPU_DEVICE_INVALID_MEM_ADDRESS,	/* no pmem */
	SPC0_DMEM_DBUS_ADDRESS
};

static const struct ipu_device_cell_properties_s ipu_device_cell_properties[NUM_CELLS] = {
	{&ipu_device_sp2600_control_properties, ipu_device_spc0_mem_address, ipu_device_spc0_databus_mem_address}
};

#ifdef C_RUN

/* Mapping between hrt_hive_processors enum and cell_id's used in FW */
static const int ipu_device_map_cell_id_to_crun_proc_id[NUM_CELLS] = {
	1  /* SPC0 */
};

#endif

#endif /* _IPU_DEVICE_CELL_PROPERTIES_IMPL_H_ */
