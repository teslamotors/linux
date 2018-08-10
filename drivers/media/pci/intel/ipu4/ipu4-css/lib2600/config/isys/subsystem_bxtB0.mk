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

############################################################################
# This file is used to specify versions and properties of ISYS firmware
# components. Please note that these are subsystem specific. System specific
# properties should go to system_$IPU_SYSVER.mk. Also the device versions
# should be defined under "devices" or should be taken from the SDK.
############################################################################

############################################################################
# FIRMWARE RELATED VARIABLES
############################################################################

# Activate loading params and storing stats DDR<->REGs with DMA
ISYS_USE_ISA_DMA                 = 1
# Used in ISA module
ISYS_ISL_DPC_DPC_V2              = 0

# Specification for Isys server's fixed globals' locations
REGMEM_OFFSET				= 0	# Starting from 0
REGMEM_SIZE				= 34
REGMEM_WORD_BYTES			= 4
FW_LOAD_NO_OF_REQUEST_OFFSET		= 136	# Taken from REGMEM_OFFSET + REGMEM_SIZE_BYTES
FW_LOAD_NO_OF_REQUEST_SIZE_BYTES	= 4

# Workarounds:

# This WA is not to pipeline store frame commands for SID processors that control a Str2Vec (ISA output)
WA_HSD1304553438                 = 1

# Larger than specified frames that complete mid-line
WA_HSD1209062354		 = 1

# WA to disable clock gating for the devices in the CSI receivers needed for using the mipi_pkt_gen device
WA_HSD1805168877		 = 0

# Support IBUF soft-reset at stream start
SOFT_RESET_IBUF_STREAM_START_SUPPORT = 1

############################################################################
# TESTING RELATED VARIABLES
############################################################################

# TODO: This define should be entirely removed.
# Used in mipi_capture
ISYS_DISABLE_VERIFY_RECEIVED_SOF_EOF     = 0

ISYS_ACCESS_BLOCKER_VERSION      = v1
