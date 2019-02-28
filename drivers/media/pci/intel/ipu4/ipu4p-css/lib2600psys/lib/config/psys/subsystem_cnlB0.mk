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

# define for DPCM Compression/ Decompression module
HAS_DPCM						= 1

# See HSD 1805169230
HAS_FWDMA_ALIGNMENT_ISSUE_SIGHTING	= 1

# Activate loading params and storing stats DDR<->REGs with DMA.
PSYS_USE_ISA_DMA                 = 1

# Used in ISA module
PSYS_ISL_DPC_DPC_V2              = 0

# Use the DMA for terminal loading in Psys server
PSYS_SERVER_ENABLE_TERMINAL_LOAD_DMA = 1

# Assume OFS will be running concurrently with IPF, and prioritize according to rates of services on devproxy
CONCURRENT_OFS_IPF_PRIORITY_OPTIMIZATION_ENABLED	= 1

# Enable clock gating of input feeder ibufctrl
ENABLE_IPFD_IBUFCTRL_CLK_GATE    = 1

# Enable clock gating of input slice light ibufctrl
ENABLE_ISL_IBUFCTRL_CLK_GATE     = 1

# Enable clock gating of GDC0
ENABLE_GDC0_CLK_GATE     = 1


# define for VCA_VCR2_FF
HAS_VCA_VCR2_FF	= 1

HAS_GMEM						= 1
HAS_64KB_GDC_MEM                = 1

# define for enabling mmu_stream_id_lut support
ENABLE_MMU_STREAM_ID_LUT = 1

# define for enabling rgbir related chnages in devproxy
HAS_RGBIR = 1

# Specification for Psys server's fixed globals' locations
REGMEM_OFFSET				= 0
REGMEM_SECURE_OFFSET			= 4096
REGMEM_SIZE				= 20
REGMEM_WORD_BYTES			= 4
REGMEM_SIZE_BYTES			= 80
GPC_ISP_PERF_DATA_OFFSET		= 80	# Taken from REGMEM_OFFSET + REGMEM_SIZE_BYTES
GPC_ISP_PERF_DATA_SIZE_BYTES		= 80
FW_LOAD_NO_OF_REQUEST_OFFSET		= 160	# Taken from GPC_ISP_PERF_DATA_OFFSET + GPC_ISP_PERF_DATA_SIZE_BYTES
FW_LOAD_NO_OF_REQUEST_SIZE_BYTES	= 4
DISPATCHER_SCRATCH_SPACE_OFFSET 	= 4176	# Taken from REGMEM_SECURE_OFFSET + REGMEM_SIZE_BYTES
# Total Used (@ REGMEM_OFFSET)		= 164	# FW_LOAD_NO_OF_REQUEST_OFFSET + FW_LOAD_NO_OF_REQUEST_SIZE_BYTES
# Total Used (@ REGMEM_SECURE_OFFSET)	= 80	# REGMEM_SIZE_BYTES

# use DMA NCI for OFS Service to reduce load in tproxy
DMA_NCI_IN_OFS_SERVICE = 1
# TODO  use version naming scheme "v#" to decouple
# IPU_SYSVER from version.
PSYS_SERVER_MANIFEST_VERSION     = cnlB0
PSYS_RESOURCE_MODEL_VERSION      = cnlB0
PSYS_ACCESS_BLOCKER_VERSION      = v1

# Disable support for PPG protocol to save codesize
PSYS_HAS_PPG_SUPPORT			= 0
# Disable support for late binding
PSYS_HAS_LATE_BINDING_SUPPORT		= 0

# Specify PSYS server context spaces for caching context from DDR
PSYS_SERVER_NOF_CACHES				= 4
PSYS_SERVER_MAX_NUM_PROC_GRP			= $(PSYS_SERVER_NOF_CACHES)
PSYS_SERVER_MAX_NUM_EXEC_PROC_GRP		= 8	# Max PG's running, 4 running on Cores, 4 being updated on the host upon executing.
PSYS_SERVER_MAX_PROC_GRP_SIZE			= 3352
PSYS_SERVER_MAX_MANIFEST_SIZE			= 3420
PSYS_SERVER_MAX_CLIENT_PKG_SIZE			= 2360
PSYS_SERVER_MAX_BUFFER_SET_SIZE			= 0
PSYS_SERVER_MAX_NUMBER_OF_TERMINAL_SECTIONS	= 90
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
PSYS_SERVER_CACHE_2_PROC_GRP_SIZE		= 1624
PSYS_SERVER_CACHE_2_MANIFEST_SIZE		= 1248
PSYS_SERVER_CACHE_2_CLIENT_PKG_SIZE		= 1040
PSYS_SERVER_CACHE_2_BUFFER_SET_SIZE		= 0
PSYS_SERVER_CACHE_2_NUMBER_OF_TERMINAL_SECTIONS	= 43
PSYS_SERVER_CACHE_2_NUMBER_OF_TERMINAL_STORE_SECTIONS = $(PSYS_SERVER_MAX_NUMBER_OF_TERMINAL_STORE_SECTIONS)
PSYS_SERVER_CACHE_3_PROC_GRP_SIZE		= $(PSYS_SERVER_CACHE_2_PROC_GRP_SIZE)
PSYS_SERVER_CACHE_3_MANIFEST_SIZE		= $(PSYS_SERVER_CACHE_2_MANIFEST_SIZE)
PSYS_SERVER_CACHE_3_CLIENT_PKG_SIZE		= $(PSYS_SERVER_CACHE_2_CLIENT_PKG_SIZE)
PSYS_SERVER_CACHE_3_BUFFER_SET_SIZE		= $(PSYS_SERVER_CACHE_2_BUFFER_SET_SIZE)
PSYS_SERVER_CACHE_3_NUMBER_OF_TERMINAL_SECTIONS	= $(PSYS_SERVER_CACHE_2_NUMBER_OF_TERMINAL_SECTIONS)
PSYS_SERVER_CACHE_3_NUMBER_OF_TERMINAL_STORE_SECTIONS = $(PSYS_SERVER_MAX_NUMBER_OF_TERMINAL_STORE_SECTIONS)
# Support dual command context for VTIO - concurrent secure and non-secure streams
PSYS_HAS_DUAL_CMD_CTX_SUPPORT	= 1

HAS_SPC				= 1
HAS_SPP0			= 1
HAS_SPP1			= 1
HAS_ISP0			= 1
HAS_ISP1			= 1
HAS_ISP2			= 1
HAS_ISP3			= 1

AB_CONFIG_ARRAY_SIZE = 50
