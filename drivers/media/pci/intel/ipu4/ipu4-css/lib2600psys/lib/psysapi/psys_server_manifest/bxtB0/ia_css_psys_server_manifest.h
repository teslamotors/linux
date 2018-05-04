/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2018, Intel Corporation.
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

#ifndef __IA_CSS_PSYS_SERVER_MANIFEST_H
#define __IA_CSS_PSYS_SERVER_MANIFEST_H

#include "vied_nci_psys_resource_model.h"

/**
 * Manifest of resources in use by PSYS itself
 */

#define PSYS_SERVER_DMA_CHANNEL_SIZE	2
#define PSYS_SERVER_DMA_CHANNEL_OFFSET	28

extern const vied_nci_resource_spec_t psys_server_manifest;

#endif /* __IA_CSS_PSYS_SERVER_MANIFEST_H */
