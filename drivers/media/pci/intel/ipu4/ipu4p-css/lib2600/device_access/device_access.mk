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

ifndef _DEVICE_ACCESS_MK_
_DEVICE_ACCESS_MK_ = 1

# DEVICE_ACCESS_VERSION=
include $(MODULES_DIR)/config/system_$(IPU_SYSVER).mk

DEVICE_ACCESS_DIR=$${MODULES_DIR}/device_access
DEVICE_ACCESS_INTERFACE=$(DEVICE_ACCESS_DIR)/interface
DEVICE_ACCESS_SOURCES=$(DEVICE_ACCESS_DIR)/src

DEVICE_ACCESS_HOST_FILES =

DEVICE_ACCESS_FW_FILES =

DEVICE_ACCESS_HOST_CPPFLAGS = \
	-I$(DEVICE_ACCESS_INTERFACE) \
	-I$(DEVICE_ACCESS_SOURCES)

DEVICE_ACCESS_FW_CPPFLAGS = \
	-I$(DEVICE_ACCESS_INTERFACE) \
	-I$(DEVICE_ACCESS_SOURCES)

DEVICE_ACCESS_FW_CPPFLAGS += \
		-I$(DEVICE_ACCESS_SOURCES)/$(DEVICE_ACCESS_VERSION)
endif
