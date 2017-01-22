# #
# Support for Intel Camera Imaging ISP subsystem.
# Copyright (c) 2010 - 2017, Intel Corporation.
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
# This file is used to specify versions and properties of PSYS firmware
# components. Please note that these are subsystem specific. System specific
# properties should go to system_$IPU_SYSVER.mk. Also the device versions
# should be defined under "devices" or should be taken from the SDK.
############################################################################

# Activate loading params and storing stats DDR<->REGs with DMA
PSYS_USE_ISA_DMA                 = 1

# Used in ISA module
PSYS_ISL_DPC_DPC_V2              = 0

# Assume OFS will be running concurrently with IPF, and prioritize according to rates of services on devproxy
CONCURRENT_OFS_IPF_PRIORITY_OPTIMIZATION_ENABLED	= 1

HAS_GMEM						= 1
