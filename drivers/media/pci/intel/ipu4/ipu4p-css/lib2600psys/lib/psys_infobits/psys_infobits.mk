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
# PSYS_INFOBITS
#

PSYS_INFOBITS_DIR = $${MODULES_DIR}/psys_infobits

PSYS_INFOBITS_INTERFACE = $(PSYS_INFOBITS_DIR)/interface
PSYS_INFOBITS_SOURCES = $(PSYS_INFOBITS_DIR)/src

PSYS_INFOBITS_CPPFLAGS := \
		-I$(PSYS_INFOBITS_INTERFACE)

PSYS_INFOBITS_HOST_FILES = \
	$(PSYS_INFOBITS_SOURCES)/psys_infobits.c

PSYS_INFOBITS_FW_FILES = $(PSYS_INFOBITS_HOST_FILES)

