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

ifndef _CELL_PROGRAM_LOAD_MK_
_CELL_PROGRAM_LOAD_MK_ = 1

CELL_PROGRAM_LOAD_DIR=$${MODULES_DIR}/cell_program_load
CELL_PROGRAM_LOAD_INTERFACE=$(CELL_PROGRAM_LOAD_DIR)/interface
CELL_PROGRAM_LOAD_SOURCES=$(CELL_PROGRAM_LOAD_DIR)/src

CELL_PROGRAM_LOAD_HOST_FILES = $(CELL_PROGRAM_LOAD_SOURCES)/ia_css_cell_program_load.c

CELL_PROGRAM_LOAD_FW_FILES = $(CELL_PROGRAM_LOAD_SOURCES)/ia_css_cell_program_load.c

CELL_PROGRAM_LOAD_HOST_CPPFLAGS = \
	-I$(CELL_PROGRAM_LOAD_INTERFACE) \
	-I$(CELL_PROGRAM_LOAD_SOURCES)

CELL_PROGRAM_LOAD_FW_CPPFLAGS = \
	-I$(CELL_PROGRAM_LOAD_INTERFACE) \
	-I$(CELL_PROGRAM_LOAD_SOURCES)

ifeq ($(CRUN_DYNAMIC_LINK_PROGRAMS), 1)
CELL_PROGRAM_LOAD_HOST_CPPFLAGS += -DCRUN_DYNAMIC_LINK_PROGRAMS=1
CELL_PROGRAM_LOAD_FW_CPPFLAGS += -DCRUN_DYNAMIC_LINK_PROGRAMS=1
endif

endif
