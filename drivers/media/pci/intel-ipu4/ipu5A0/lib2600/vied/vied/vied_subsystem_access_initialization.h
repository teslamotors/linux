/*
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2016, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
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
