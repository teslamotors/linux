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
#ifndef _HRT_VIED_SUBSYSTEM_ACCESS_INITIALIZE_H
#define _HRT_VIED_SUBSYSTEM_ACCESS_INITIALIZE_H

#include "vied_subsystem_access_types.h"

/** @brief Initialises the access of a subsystem.
 *  @param[in]   system               The subsystem for which the access has to be initialised.
 *
 * vied_subsystem_access_initialize initilalises the access a subsystem.
 * It sets the base address of the subsystem. This base address is extracted from the hsd file.
 *
 */
void
vied_subsystem_access_initialize(vied_subsystem_t system);


/** @brief Initialises the access of multiple subsystems.
 *  @param[in]   nr _subsystems       The number of subsystems for which the access has to be initialised.
 *  @param[in]   dev_base_addresses   A pointer to an array of base addresses of subsystems.
 *                                    The size of this array must be "nr_subsystems".
 *                                    This array must be available during the accesses of the subsystem.
 *
 * vied_subsystems_access_initialize initilalises the access to multiple subsystems.
 * It sets the base addresses of the subsystems that are provided by the array dev_base_addresses.
 *
 */
void
vied_subsystems_access_initialize( unsigned int nr_subsystems
                                 , const vied_subsystem_base_address_t *base_addresses);

#endif /* _HRT_VIED_SUBSYSTEM_ACCESS_INITIALIZE_H */
