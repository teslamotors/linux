# INTEL CONFIDENTIAL
#
# Copyright (C) 2016 - 2016 Intel Corporation.
# All Rights Reserved.
#
# The source code contained or described herein and all documents
# related to the source code ("Material") are owned by Intel Corporation
# or licensors. Title to the Material remains with Intel
# Corporation or its licensors. The Material contains trade
# secrets and proprietary and confidential information of Intel or its
# licensors. The Material is protected by worldwide copyright
# and trade secret laws and treaty provisions. No part of the Material may
# be used, copied, reproduced, modified, published, uploaded, posted,
# transmitted, distributed, or disclosed in any way without Intel's prior
# express written permission.
#
# No License under any patent, copyright, trade secret or other intellectual
# property right is granted to or conferred upon you by disclosure or
# delivery of the Materials, either expressly, by implication, inducement,
# estoppel or otherwise. Any license under such intellectual property rights
# must be express and approved by Intel in writing.

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
HAS_STR2MMIO_IN_ISL_NONSOC_PATH	= 1
HAS_PIXEL_FORMATTER_IN_ISYS	= 0

DLI_SYSTEM                       ?= ipu5_system
HOST_CPPFLAGS                    += -DHAS_BUTTRESS
OFS_OUTPUT_TO_TRANSFER_VMEM      = 1
RESOURCE_MANAGER_VERSION         = v3
