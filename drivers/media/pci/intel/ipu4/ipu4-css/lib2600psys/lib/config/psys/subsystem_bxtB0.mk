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
# This file is used to specify versions and properties of PSYS firmware
# components. Please note that these are subsystem specific. System specific
# properties should go to system_$IPU_SYSVER.mk. Also the device versions
# should be defined under "devices" or should be taken from the SDK.
############################################################################

# Activate loading params and storing stats DDR<->REGs with DMA
PSYS_USE_ISA_DMA				= 1

# Used in ISA module
PSYS_ISL_DPC_DPC_V2				= 0

# Assume OFS will be running concurrently with IPF, and prioritize according to rates of services on devproxy
CONCURRENT_OFS_IPF_PRIORITY_OPTIMIZATION_ENABLED	= 1

# Use the DMA for terminal loading in Psys server
PSYS_SERVER_ENABLE_TERMINAL_LOAD_DMA = 1

HAS_GMEM						= 1
# use DMA NCI for OFS Service to reduce load in tproxy
DMA_NCI_IN_OFS_SERVICE = 1

# See HSD 1805169230
HAS_FWDMA_ALIGNMENT_ISSUE_SIGHTING	= 1

HAS_SPC				= 1
HAS_SPP0			= 1
HAS_SPP1			= 1
HAS_ISP0			= 1
HAS_ISP1			= 1
HAS_ISP2			= 1
HAS_ISP3			= 1

# Specification for Psys server's fixed globals' locations
REGMEM_OFFSET				= 0	# Starting from 0
REGMEM_SIZE				= 18
REGMEM_WORD_BYTES			= 4
REGMEM_SIZE_BYTES			= 72
GPC_ISP_PERF_DATA_OFFSET		= 72	# Taken from REGMEM_OFFSET + REGMEM_SIZE_BYTES
GPC_ISP_PERF_DATA_SIZE_BYTES		= 80
FW_LOAD_NO_OF_REQUEST_OFFSET		= 152	# Taken from GPC_ISP_PERF_DATA_OFFSET + GPC_ISP_PERF_DATA_SIZE_BYTES
FW_LOAD_NO_OF_REQUEST_SIZE_BYTES	= 4
DISPATCHER_SCRATCH_SPACE_OFFSET		= 156	# Taken from FW_LOAD_NO_OF_REQUEST_OFFSET + FW_LOAD_NO_OF_REQUEST_SIZE_BYTES

# TODO  use version naming scheme "v#" to decouple
# IPU_SYSVER from version.
PSYS_SERVER_MANIFEST_VERSION     = bxtB0
PSYS_RESOURCE_MODEL_VERSION      = bxtB0
PSYS_ACCESS_BLOCKER_VERSION      = v1

# Disable support for PPG protocol to save codesize
PSYS_HAS_PPG_SUPPORT			= 0
# Disable support for late binding
PSYS_HAS_LATE_BINDING_SUPPORT		= 0

# Specify PSYS server context spaces for caching context from DDR
PSYS_SERVER_NOF_CACHES				= 4
PSYS_SERVER_MAX_NUM_PROC_GRP			= $(PSYS_SERVER_NOF_CACHES)
PSYS_SERVER_MAX_NUM_EXEC_PROC_GRP		= 8	# Max PG's running, 4 running on Cores, 4 being updated on the host upon executing.
PSYS_SERVER_MAX_PROC_GRP_SIZE			= 4052
PSYS_SERVER_MAX_MANIFEST_SIZE			= 3732
PSYS_SERVER_MAX_CLIENT_PKG_SIZE			= 2420
PSYS_SERVER_MAX_BUFFER_SET_SIZE			= 0
PSYS_SERVER_MAX_NUMBER_OF_TERMINAL_SECTIONS	= 88
PSYS_SERVER_MAX_NUMBER_OF_TERMINAL_STORE_SECTIONS = 1
# The caching scheme for this subsystem suits the method of queueing ahead separate PGs for frames in an interleaved
# fashion. As such there should be as many caches to support to heaviest two concurrent PGs, times two. This results
# in the following distribution of caches: two large ones for the maximum sized PG, two smaller ones for the
# second-largest sized PG.
PSYS_SERVER_CACHE_0_PROC_GRP_SIZE		= $(PSYS_SERVER_MAX_PROC_GRP_SIZE)
PSYS_SERVER_CACHE_0_MANIFEST_SIZE		= $(PSYS_SERVER_MAX_MANIFEST_SIZE)
PSYS_SERVER_CACHE_0_CLIENT_PKG_SIZE		= $(PSYS_SERVER_MAX_CLIENT_PKG_SIZE)
PSYS_SERVER_CACHE_0_BUFFER_SET_SIZE		= $(PSYS_SERVER_MAX_BUFFER_SET_SIZE)
PSYS_SERVER_CACHE_0_NUMBER_OF_TERMINAL_SECTIONS	= $(PSYS_SERVER_MAX_NUMBER_OF_TERMINAL_SECTIONS)
PSYS_SERVER_CACHE_0_NUMBER_OF_TERMINAL_STORE_SECTIONS = $(PSYS_SERVER_MAX_NUMBER_OF_TERMINAL_STORE_SECTIONS)
PSYS_SERVER_CACHE_1_PROC_GRP_SIZE		= $(PSYS_SERVER_CACHE_0_PROC_GRP_SIZE)
PSYS_SERVER_CACHE_1_MANIFEST_SIZE		= $(PSYS_SERVER_CACHE_0_MANIFEST_SIZE)
PSYS_SERVER_CACHE_1_CLIENT_PKG_SIZE		= $(PSYS_SERVER_CACHE_0_CLIENT_PKG_SIZE)
PSYS_SERVER_CACHE_1_BUFFER_SET_SIZE		= $(PSYS_SERVER_CACHE_0_BUFFER_SET_SIZE)
PSYS_SERVER_CACHE_1_NUMBER_OF_TERMINAL_SECTIONS	= $(PSYS_SERVER_CACHE_0_NUMBER_OF_TERMINAL_SECTIONS)
PSYS_SERVER_CACHE_1_NUMBER_OF_TERMINAL_STORE_SECTIONS = $(PSYS_SERVER_MAX_NUMBER_OF_TERMINAL_STORE_SECTIONS)
PSYS_SERVER_CACHE_2_PROC_GRP_SIZE		= 1800
PSYS_SERVER_CACHE_2_MANIFEST_SIZE		= 2344
PSYS_SERVER_CACHE_2_CLIENT_PKG_SIZE		= 1240
PSYS_SERVER_CACHE_2_BUFFER_SET_SIZE		= 0
PSYS_SERVER_CACHE_2_NUMBER_OF_TERMINAL_SECTIONS	= 45
PSYS_SERVER_CACHE_2_NUMBER_OF_TERMINAL_STORE_SECTIONS = $(PSYS_SERVER_MAX_NUMBER_OF_TERMINAL_STORE_SECTIONS)

PSYS_SERVER_CACHE_3_PROC_GRP_SIZE		= $(PSYS_SERVER_CACHE_2_PROC_GRP_SIZE)
PSYS_SERVER_CACHE_3_MANIFEST_SIZE		= $(PSYS_SERVER_CACHE_2_MANIFEST_SIZE)
PSYS_SERVER_CACHE_3_CLIENT_PKG_SIZE		= $(PSYS_SERVER_CACHE_2_CLIENT_PKG_SIZE)
PSYS_SERVER_CACHE_3_BUFFER_SET_SIZE		= $(PSYS_SERVER_CACHE_2_BUFFER_SET_SIZE)
PSYS_SERVER_CACHE_3_NUMBER_OF_TERMINAL_SECTIONS	= $(PSYS_SERVER_CACHE_2_NUMBER_OF_TERMINAL_SECTIONS)
PSYS_SERVER_CACHE_3_NUMBER_OF_TERMINAL_STORE_SECTIONS = $(PSYS_SERVER_MAX_NUMBER_OF_TERMINAL_STORE_SECTIONS)
