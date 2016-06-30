/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2013 - 2016 Intel Corporation.
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

#ifndef _REGMEM_ACCESS_H_
#define _REGMEM_ACCESS_H_

#include "storage_class.h"

enum regmem_id {
	PKG_DIR_ADDR_REG	= 0,	/* pass pkg_dir address to SPC in non-secure mode */
	SYSCOM_CONFIG_REG	= 1,	/* pass syscom configuration to SPC */
	SYSCOM_STATE_REG	= 2,	/* syscom state - modified by SP */
	SYSCOM_COMMAND_REG	= 3,	/* syscom commands - modified by the host */
	SYSCOM_QPR_BASE_REG	= 4	/* first syscom queue pointer register */
};

STORAGE_CLASS_INLINE
unsigned int
regmem_load_32(unsigned int mem_address, unsigned int reg, unsigned int ssid);

STORAGE_CLASS_INLINE
void
regmem_store_32(unsigned int mem_address, unsigned int reg, unsigned int value, unsigned int ssid);

#ifdef __VIED_CELL
#include "regmem_access_cell.h"
#else
#include "regmem_access_host.h"
#endif

#endif /*_REGMEM_ACCESS_H_*/
