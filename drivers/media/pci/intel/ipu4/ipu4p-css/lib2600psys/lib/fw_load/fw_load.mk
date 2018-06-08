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
#
# MODULE is FW_LOAD

# select implementation for fw_load
ifeq ($(FW_LOAD_DMA), 1)
FW_LOAD_IMPL	= fwdma
else
FW_LOAD_IMPL	= xmem
endif

FW_LOAD_FW_CPPFLAGS =

# select DMA instance for fw_load
ifeq ($(FW_LOAD_DMA_INSTANCE),)
$(error FW_LOAD_DMA_INSTANCE not specified)
else
ifeq ($(FW_LOAD_DMA_INSTANCE), NCI_DMA_EXT0)
FW_LOAD_FW_CPPFLAGS	+= -DFW_LOAD_INSTANCE_USE_DMA_EXT0
else
ifeq ($(FW_LOAD_DMA_INSTANCE), NCI_DMA_FW)
FW_LOAD_FW_CPPFLAGS	+= -DFW_LOAD_INSTANCE_USE_DMA_FW
else
$(error FW_LOAD_DMA_INSTANCE $(FW_LOAD_DMA_INSTANCE) not supported)
endif
endif
endif

FW_LOAD_DIR  		= $${MODULES_DIR}/fw_load
FW_LOAD_INTERFACE	= $(FW_LOAD_DIR)/interface
FW_LOAD_SOURCES		= $(FW_LOAD_DIR)/src/$(FW_LOAD_IMPL)

# XMEM/FWDMA supports on SP side
FW_LOAD_FW_FILES	= $(FW_LOAD_SOURCES)/ia_css_fw_load.c
FW_LOAD_FW_CPPFLAGS	+= -I$(FW_LOAD_INTERFACE) \
					-I$(FW_LOAD_SOURCES) \
					-I$(FW_LOAD_DIR)/src

# Only XMEM supports on Host side
FW_LOAD_HOST_FILES	= $(FW_LOAD_DIR)/src/xmem/ia_css_fw_load.c
FW_LOAD_HOST_CPPFLAGS	= -I$(FW_LOAD_INTERFACE) \
					-I$(FW_LOAD_DIR)/src/xmem \
					-I$(FW_LOAD_DIR)/src

ifdef FW_LOAD_NO_OF_REQUEST_OFFSET
FW_LOAD_FW_CPPFLAGS	+= -DFW_LOAD_NO_OF_REQUEST_ADDRESS=$(FW_LOAD_NO_OF_REQUEST_OFFSET)
endif # FW_LOAD_NO_OF_REQUEST_OFFSET
