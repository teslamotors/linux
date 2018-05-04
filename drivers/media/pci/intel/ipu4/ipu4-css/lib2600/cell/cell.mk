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
ifndef _CELL_MK_
_CELL_MK_ = 1


CELL_DIR=$${MODULES_DIR}/cell
CELL_INTERFACE=$(CELL_DIR)/interface
CELL_SOURCES=$(CELL_DIR)/src

CELL_HOST_FILES =
CELL_FW_FILES =

CELL_HOST_CPPFLAGS = \
	-I$(CELL_INTERFACE) \
	-I$(CELL_SOURCES)

CELL_FW_CPPFLAGS = \
	-I$(CELL_INTERFACE) \
	-I$(CELL_SOURCES)

ifdef 0
# Disabled until it is decided to go this way or not
include $(MODULES_DIR)/device_access/device_access.mk
CELL_HOST_FILES += $(DEVICE_ACCESS_HOST_FILES)
CELL_FW_FILES += $(DEVICE_ACCESS_FW_FILES)
CELL_HOST_CPPFLAGS += $(DEVICE_ACCESS_HOST_CPPFLAGS)
CELL_FW_CPPFLAGS += $(DEVICE_ACCESS_FW_CPPFLAGS)
endif

endif
