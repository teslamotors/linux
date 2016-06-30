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

#ifndef _REGMEM_ACCESS_HOST_H_
#define _REGMEM_ACCESS_HOST_H_

#include "regmem_access.h" /* implemented interface */

#ifdef C_RUN
#error C_RUN not supported
#endif

#include "storage_class.h"
#include "regmem_const.h"
#include <vied/vied_subsystem_access.h>

STORAGE_CLASS_INLINE
unsigned int
regmem_load_32(unsigned int mem_addr, unsigned int reg, unsigned int ssid)
{
	return vied_subsystem_load_32(ssid, mem_addr + REGMEM_OFFSET + (4*reg));
}

STORAGE_CLASS_INLINE
void
regmem_store_32(unsigned int mem_addr, unsigned int reg, unsigned int value, unsigned int ssid)
{
	vied_subsystem_store_32(ssid, mem_addr + REGMEM_OFFSET + (4*reg), value);
}

#endif
