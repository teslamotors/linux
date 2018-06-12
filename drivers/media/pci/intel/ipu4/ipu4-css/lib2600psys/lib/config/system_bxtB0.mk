# # #
# Support for Intel Camera Imaging ISP subsystem.
# Copyright (c) 2010 - 2018, Intel Corporation.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details
#

LOGICAL_FW_INPUT_SYSTEM          = input_system_system
LOGICAL_FW_PROCESSING_SYSTEM     = processing_system_system
LOGICAL_FW_IPU_SYSTEM            = css_broxton_system
LOGICAL_FW_ISP_SYSTEM            = isp2601_default_system
SP_CONTROL_CELL                  = sp2601_control
SP_PROXY_CELL                    = sp2601_proxy
SP_FP_CELL                       = sp2601_fp
ISP_CELL                         = isp2601
# The non-capital define isp2601 is used in the sdk, in order to distinguish
# between different isp versions the ISP_CELL_IDENTIFIER define is added.
ISP_CELL_IDENTIFIER              = ISP2601
HAS_IPFD                         = 1
HAS_S2M_IN_ISYS_ISL_NONSOC_PATH  = 0
HAS_S2V_IN_ISYS_ISL_NONSOC_PATH  = 1
# ISL-IS non-SoC path has ISA without PAF and DPC-Pext support for IPU4-B0
HAS_ISA_IN_ISYS_ISL              = 1
HAS_PAF_IN_ISYS_ISL              = 0
HAS_DPC_PEXT_IN_ISYS_ISL         = 0
HAS_PMA_IF                       = 0

HAS_MIPIBE_IN_PSYS_ISL           = 1

HAS_VPLESS_SUPPORT               = 0

DLI_SYSTEM                       = hive_isp_css_2600_system
RESOURCE_MANAGER_VERSION         = v1
MEM_RESOURCE_VALIDATION_ERROR    = 0
OFS_SCALER_1_4K_TILEY_422_SUPPORT= 1
PROGDESC_ACC_SYMBOLS_VERSION     = v1
DEVPROXY_INTERFACE_VERSION       = v1
FW_ABI_IPU_TYPES_VERSION         = v1

HAS_ONLINE_MODE_SUPPORT_IN_ISYS_PSYS = 0

MMU_INTERFACE_VERSION            = v1
DEVICE_ACCESS_VERSION            = v2
PSYS_SERVER_VERSION              = v2
PSYS_SERVER_LOADER_VERSION       = v1
PSYS_HW_VERSION                  = BXT_B0_HW

# Enable FW_DMA for loading firmware
PSYS_SERVER_ENABLE_FW_LOAD_DMA          = 1

NCI_SPA_VERSION                  = v1
MANIFEST_TOOL_VERSION            = v2
PSYS_CON_MGR_TOOL_VERSION        = v1
# TODO: Should be removed after performance issues OTF are solved
PSYS_PROC_MGR_VERSION            = v1
IPU_RESOURCES_VERSION            = v1

HAS_ACC_CLUSTER_PAF_PAL          = 0
HAS_ACC_CLUSTER_PEXT_PAL         = 0
HAS_ACC_CLUSTER_GBL_PAL          = 1

# TODO  use version naming scheme "v#" to decouple
# IPU_SYSVER from version.
PARAMBINTOOL_ISA_INIT_VERSION    = bxtB0

# Select EQC2EQ version
# Version 1: uniform address space, equal EQ addresses regardless of EQC device
# Version 2: multiple addresses per EQ, depending on location of EQC device
EQC2EQ_VERSION                   = v1

# Select DMA instance for fw_load
FW_LOAD_DMA_INSTANCE		= NCI_DMA_FW

HAS_DMA_FW			= 1

HAS_SIS					= 0
HAS_IDS					= 1

PSYS_SERVER_ENABLE_TPROXY   = 1
PSYS_SERVER_ENABLE_DEVPROXY = 1
NCI_OFS_VERSION             = v1
