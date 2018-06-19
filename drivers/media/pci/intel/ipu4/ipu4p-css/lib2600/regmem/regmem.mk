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
ifndef REGMEM_MK
REGMEM_MK=1

# MODULE is REGMEM

REGMEM_DIR=$${MODULES_DIR}/regmem

REGMEM_INTERFACE=$(REGMEM_DIR)/interface
REGMEM_SOURCES=$(REGMEM_DIR)/src

REGMEM_HOST_FILES =
REGMEM_FW_FILES = $(REGMEM_SOURCES)/regmem.c

REGMEM_CPPFLAGS = -I$(REGMEM_INTERFACE) -I$(REGMEM_SOURCES)
REGMEM_HOST_CPPFLAGS = $(REGMEM_CPPFLAGS)
REGMEM_FW_CPPFLAGS = $(REGMEM_CPPFLAGS)

endif
