/**
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

#include "ia_css_psys_server_manifest.h"

/**
 * Manifest of resources in use by PSYS itself
 */

const vied_nci_resource_spec_t psys_server_manifest = {
	/* internal memory */
	{	/* resource id			size			offset*/
		{VIED_NCI_GMEM_TYPE_ID,		0,			0},
		{VIED_NCI_DMEM_TYPE_ID,	VIED_NCI_DMEM0_MAX_SIZE,	0},
		{VIED_NCI_VMEM_TYPE_ID,		0,			0},
		{VIED_NCI_BAMEM_TYPE_ID,	0,			0},
		{VIED_NCI_PMEM_TYPE_ID,		0,			0}
	},
	/* external memory */
	{	/* resource id			size			offset*/
		{VIED_NCI_N_MEM_ID,		0,			0},
		{VIED_NCI_N_MEM_ID,		0,			0},
		{VIED_NCI_N_MEM_ID,		0,			0},
		{VIED_NCI_N_MEM_ID,		0,			0},
	},
	/* device channel */
	{	/* resource id				size		offset*/
		{VIED_NCI_DEV_CHN_DMA_EXT0_ID,
				PSYS_SERVER_DMA_CHANNEL_SIZE,
						PSYS_SERVER_DMA_CHANNEL_OFFSET},
		{VIED_NCI_DEV_CHN_GDC_ID,		0,		0},
		{VIED_NCI_DEV_CHN_DMA_EXT1_READ_ID,	0,		0},
		{VIED_NCI_DEV_CHN_DMA_EXT1_WRITE_ID,	0,		0},
		{VIED_NCI_DEV_CHN_DMA_INTERNAL_ID,	0,		0},
		{VIED_NCI_DEV_CHN_DMA_IPFD_ID,		0,		0},
		{VIED_NCI_DEV_CHN_DMA_ISA_ID,		0,		0},
		{VIED_NCI_DEV_CHN_DMA_FW_ID,		0,		0}
	}
};

