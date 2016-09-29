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

LOGICAL_FW_INPUT_SYSTEM          = ipu_system
LOGICAL_FW_PROCESSING_SYSTEM     = ipu_system
LOGICAL_FW_IPU_SYSTEM            = ipu_system
LOGICAL_FW_ISP_SYSTEM            = idsp_system
SP_CONTROL_CELL                  = sp_control
SP_PROXY_CELL                    = sp_proxy
ISP_CELL                         = idsp
# The non-capital define isp2601 is used in the sdk, in order to distinguish
# between different isp versions the ISP_CELL_IDENTIFIER define is added.
ISP_CELL_IDENTIFIER              = IDSP

# The ISL-IS has two data paths - one for handling the main camera which may go
# directly to PS (the nonsoc path).
# The other path can handle up to 8 streams for SoC sensors and additional raw
# sensors (the soc path),
# In IPU5 the nonsoc path has str2mmio devices instead of s2v devices
HAS_S2M_IN_ISYS_ISL_NONSOC_PATH  = 1
HAS_S2V_IN_ISYS_ISL_NONSOC_PATH  = 0
# ISL-IS non-SoC path doesn't have ISA in IPU5-A0
HAS_ISA_IN_ISYS_ISL              = 0
HAS_PAF_IN_ISYS_ISL              = 0
HAS_DPC_PEXT_IN_ISYS_ISL         = 0

HAS_ISL_PIFCONV                  = 1
HAS_OFS_OUT_CONVERTER            = 1

HAS_MIPIBE_IN_PSYS_ISL           = 0

DLI_SYSTEM                       ?= ipu5_system
HOST_CPPFLAGS                    += -DHAS_BUTTRESS
OFS_OUTPUT_TO_TRANSFER_VMEM      = 1
RESOURCE_MANAGER_VERSION         = v3
PROGDESC_ACC_SYMBOLS_VERSION     = v2

