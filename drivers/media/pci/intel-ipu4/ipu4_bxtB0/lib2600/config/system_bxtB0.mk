# #
# Support for Intel Camera Imaging ISP subsystem.
# Copyright (c) 2010 - 2016, Intel Corporation.
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

HAS_MIPIBE_IN_PSYS_ISL           = 1

DLI_SYSTEM                       = hive_isp_css_2600_system
RESOURCE_MANAGER_VERSION         = v1
OFS_SCALER_1_4K_TILEY_422_SUPPORT= 1
PROGDESC_ACC_SYMBOLS_VERSION     = v1
DEVPROXY_INTERFACE_VERSION       = v1
FW_ABI_IPU_TYPES_VERSION         = v1

MMU_INTERFACE_VERSION            = v1
