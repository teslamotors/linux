/*
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
#ifndef _HRT_VIED_MEMORY_ACCESS_TYPES_H
#define _HRT_VIED_MEMORY_ACCESS_TYPES_H

/** Types for the VIED memory access interface */

#include "vied_types.h"

/**
 * \brief An identifier for a system memory.
 *
 * This identifier must be a compile-time constant. It is used in
 * access to system memory.
 */
typedef unsigned int    vied_memory_t;

#ifndef __HIVECC
/**
 * \brief The type for a physical address
 */
#if  defined(C_RUN) || defined(crun_hostlib)
typedef int* vied_physical_address_t;
#else
typedef unsigned long long    vied_physical_address_t;
#endif
#endif

#endif /* _HRT_VIED_MEMORY_ACCESS_TYPES_H */
