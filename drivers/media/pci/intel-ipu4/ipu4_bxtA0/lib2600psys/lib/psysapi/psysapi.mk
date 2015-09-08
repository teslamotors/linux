# Support for Intel Camera Imaging ISP subsystem.
# Copyright (c) 2010 - 2015, Intel Corporation.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
# 
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
# MODULE is PSYSAPI
#
ifdef _H_PSYSAPI_MK
$(error ERROR: psysapi.mk included multiple times, please check makefile)
else
_H_PSYSAPI_MK=1
endif


PSYSAPI_DIR = $${MODULES_DIR}/psysapi

PSYSAPI_HOST_FILES     = $(PSYSAPI_DIR)/dynamic/src/ia_css_psys_process.c
PSYSAPI_HOST_FILES    += $(PSYSAPI_DIR)/dynamic/src/ia_css_psys_process_group.c
PSYSAPI_HOST_FILES    += $(PSYSAPI_DIR)/dynamic/src/ia_css_psys_terminal.c

#PSYSAPI_HOST_FILES    += $(PSYSAPI_DIR)/data/src/ia_css_program_group_data.sim.c
#PSYSAPI_HOST_FILES    += $(PSYSAPI_DIR)/static/src/ia_css_psys_program_group_manifest.sim.c
#PSYSAPI_HOST_FILES    += $(PSYSAPI_DIR)/static/src/ia_css_psys_program_manifest.sim.c
#PSYSAPI_HOST_FILES    += $(PSYSAPI_DIR)/static/src/ia_css_psys_terminal_manifest.sim.c

PSYSAPI_MANIFEST_HOST_FILES    += $(PSYSAPI_DIR)/static/src/ia_css_psys_program_group_manifest.c
PSYSAPI_MANIFEST_HOST_FILES    += $(PSYSAPI_DIR)/static/src/ia_css_psys_program_manifest.c
PSYSAPI_MANIFEST_HOST_FILES    += $(PSYSAPI_DIR)/static/src/ia_css_psys_terminal_manifest.c
PSYSAPI_MANIFEST_HOST_FILES    += $(PSYSAPI_DIR)/sim/src/vied_nci_psys_system.c
PSYSAPI_MANIFEST_HOST_FILES    += $(PSYSAPI_DIR)/kernel/src/ia_css_kernel_bitmap.c
PSYSAPI_MANIFEST_HOST_FILES    += $(PSYSAPI_DIR)/data/src/ia_css_program_group_data.c

PSYSAPI_HOST_FILES += $(PSYSAPI_MANIFEST_HOST_FILES)

PSYSAPI_HOST_FILES    += $(PSYSAPI_DIR)/sim/src/ia_css_psys_sim_data.c
#PSYSAPI_HOST_FILES    += $(PSYSAPI_DIR)/sim/src/ia_css_psys_process_group_cmd_impl.c

PSYSAPI_HOST_FILES    += $(PSYSAPI_DIR)/param/src/ia_css_program_group_param.c
#PSYSAPI_HOST_FILES    += $(PSYSAPI_DIR)/param/src/ia_css_program_group_param.sim.c

# for now disabled, implementation for now provided by bxt psys api impl
#PSYSAPI_HOST_FILES    += $(PSYSAPI_DIR)/device/src/ia_css_psys_device.c

ifndef BXT_INTERFACES_ENABLED
	PSYSAPI_HOST_CPPFLAGS  = -I$(PSYSAPI_DIR)/interface
	PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/device/interface
	PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/dynamic/interface
	PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/data/interface
	PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/static/interface
	PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/kernel/interface
	PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/param/interface
	PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/param/src
	PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/sim/interface

	PSYSAPI_FW_CPPFLAGS = $(PSYSAPI_HOST_CPPFLAGS)
	PSYSAPI_FW_CPPFLAGS += -I$(PSYSAPI_DIR)/static/src
	PSYSAPI_SYSTEM_GLOBAL_CPPFLAGS += -I$(PSYSAPI_DIR)/sim/interface
endif

# This one needs fixing. This should not be a PSYS api include!!!
PSYSAPI_HOST_CPPFLAGS += -I$${MODULES_DIR}/../hive_isp_css_include

PSYSAPI_HOST_CPPFLAGS += -I$${MODULES_DIR}/../isp/kernels/io_ls/common

#PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/../syscom/interface

# Defining the trace level for the PSYSAPI
PSYSAPI_HOST_CPPFLAGS += -DPSYSAPI_TRACE_CONFIG=PSYSAPI_TRACE_LOG_LEVEL_NORMAL

#Example: how to switch to a different log level for a sub-module
#PSYSAPI_HOST_CPPFLAGS += -DPSYSAPI_DYNAMIC_TRACING_OVERRIDE=PSYSAPI_TRACE_LOG_LEVEL_DEBUG

# enable host side implementation
# TODO: better name for the flag to enable the impl...
PSYSAPI_HOST_CPPFLAGS += -D__X86_SIM__

# Files for Firmware
PSYSAPI_FW_FILES = $(PSYSAPI_DIR)/dynamic/src/ia_css_psys_process.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/dynamic/src/ia_css_psys_process_group.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/dynamic/src/ia_css_psys_terminal.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/data/src/ia_css_program_group_data.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/sim/src/vied_nci_psys_system.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/sim/src/ia_css_psys_sim_data.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/static/src/ia_css_psys_program_group_manifest.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/static/src/ia_css_psys_program_manifest.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/static/src/ia_css_psys_terminal_manifest.c

PSYSAPI_FW_CPPFLAGS += -I$${MODULES_DIR}/../isp/kernels/io_ls/common

