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
# MODULE is PSYSAPI
#
ifdef _H_PSYSAPI_MK
$(error ERROR: psysapi.mk included multiple times, please check makefile)
else
_H_PSYSAPI_MK=1
endif

include $(MODULES_DIR)/config/psys/subsystem_$(IPU_SYSVER).mk

PSYSAPI_DIR = $${MODULES_DIR}/psysapi

PSYSAPI_PROCESS_HOST_FILES      = $(PSYSAPI_DIR)/dynamic/src/ia_css_psys_process.c
PSYSAPI_PROCESS_HOST_FILES     += $(PSYSAPI_DIR)/dynamic/src/ia_css_psys_process_group.c
PSYSAPI_PROCESS_HOST_FILES     += $(PSYSAPI_DIR)/dynamic/src/ia_css_psys_buffer_set.c
PSYSAPI_PROCESS_HOST_FILES     += $(PSYSAPI_DIR)/dynamic/src/ia_css_psys_terminal.c
PSYSAPI_PROCESS_HOST_FILES     += $(PSYSAPI_DIR)/param/src/ia_css_program_group_param.c

# Use PSYS_MANIFEST_HOST_FILES when only accessing manifest functions
PSYSAPI_MANIFEST_HOST_FILES    += $(PSYSAPI_DIR)/static/src/ia_css_psys_program_group_manifest.c
PSYSAPI_MANIFEST_HOST_FILES    += $(PSYSAPI_DIR)/static/src/ia_css_psys_program_manifest.c
PSYSAPI_MANIFEST_HOST_FILES    += $(PSYSAPI_DIR)/static/src/ia_css_psys_terminal_manifest.c
PSYSAPI_MANIFEST_HOST_FILES    += $(PSYSAPI_DIR)/sim/src/vied_nci_psys_system.c
PSYSAPI_MANIFEST_HOST_FILES    += $(PSYSAPI_DIR)/kernel/src/ia_css_kernel_bitmap.c
PSYSAPI_MANIFEST_HOST_FILES    += $(PSYSAPI_DIR)/data/src/ia_css_program_group_data.c
PSYSAPI_MANIFEST_HOST_FILES    += $(PSYSAPI_DIR)/resource_model/$(PSYS_RESOURCE_MODEL_VERSION)/vied_nci_psys_resource_model.c
PSYSAPI_MANIFEST_HOST_FILES    += $(PSYSAPI_DIR)/psys_server_manifest/$(PSYS_SERVER_MANIFEST_VERSION)/ia_css_psys_server_manifest.c

# Use only kernel bitmap functionality from PSYS API
PSYSAPI_KERNEL_BITMAP_FILES    += $(PSYSAPI_DIR)/kernel/src/ia_css_kernel_bitmap.c
PSYSAPI_KERNEL_BITMAP_CPPFLAGS += -I$(PSYSAPI_DIR)/kernel/interface
PSYSAPI_KERNEL_BITMAP_CPPFLAGS += -I$(PSYSAPI_DIR)/interface

# Use PSYSAPI_HOST_FILES when program and process group are both needed
PSYSAPI_HOST_FILES = $(PSYSAPI_PROCESS_HOST_FILES) $(PSYSAPI_MANIFEST_HOST_FILES)

# Use PSYSAPI_PROCESS_GROUP_HOST_FILES when program and process group are both needed but there is no
# implementation (yet) of the user customization functions defined in ia_css_psys_process_group_cmd_impl.h.
# Dummy implementations are provided in $(PSYSAPI_DIR)/sim/src/ia_css_psys_process_group_cmd_impl.c
PSYSAPI_PROCESS_GROUP_HOST_FILES  = $(PSYSAPI_HOST_FILES)
PSYSAPI_PROCESS_GROUP_HOST_FILES += $(PSYSAPI_DIR)/sim/src/ia_css_psys_process_group_cmd_impl.c

# for now disabled, implementation for now provided by psys api impl
#PSYSAPI_HOST_FILES    += $(PSYSAPI_DIR)/device/src/ia_css_psys_device.c

PSYSAPI_HOST_CPPFLAGS  = -I$(PSYSAPI_DIR)/interface
PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/device/interface
PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/device/interface/$(IPU_SYSVER)
PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/dynamic/interface
PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/dynamic/src
PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/data/interface
PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/data/src
PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/static/interface
PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/static/src
PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/kernel/interface
PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/param/interface
PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/param/src
PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/sim/interface
PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/sim/src
PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/resource_model/$(PSYS_RESOURCE_MODEL_VERSION)
PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/resource_model/$(PSYS_RESOURCE_MODEL_VERSION)/private
PSYSAPI_HOST_CPPFLAGS += -I$(PSYSAPI_DIR)/psys_server_manifest/$(PSYS_SERVER_MANIFEST_VERSION)

PSYSAPI_FW_CPPFLAGS = $(PSYSAPI_HOST_CPPFLAGS)
PSYSAPI_FW_CPPFLAGS += -I$(PSYSAPI_DIR)/static/interface
PSYSAPI_FW_CPPFLAGS += -I$(PSYSAPI_DIR)/static/src
PSYSAPI_FW_CPPFLAGS += -I$(PSYSAPI_DIR)/resource_model/$(PSYS_RESOURCE_MODEL_VERSION)
PSYSAPI_FW_CPPFLAGS += -I$(PSYSAPI_DIR)/resource_model/$(PSYS_RESOURCE_MODEL_VERSION)/private
PSYSAPI_FW_CPPFLAGS += -I$(PSYSAPI_DIR)/psys_server_manifest/$(PSYS_SERVER_MANIFEST_VERSION)
PSYSAPI_SYSTEM_GLOBAL_CPPFLAGS += -I$(PSYSAPI_DIR)/sim/interface
PSYSAPI_SYSTEM_GLOBAL_CPPFLAGS += -I$(PSYSAPI_DIR)/resource_model/$(PSYS_RESOURCE_MODEL_VERSION)
PSYSAPI_SYSTEM_GLOBAL_CPPFLAGS += -I$(PSYSAPI_DIR)/resource_model/$(PSYS_RESOURCE_MODEL_VERSION)/private
PSYSAPI_SYSTEM_GLOBAL_CPPFLAGS += -I$(PSYSAPI_DIR)/psys_server_manifest/$(PSYS_SERVER_MANIFEST_VERSION)

# Defining the trace level for the PSYSAPI
PSYSAPI_HOST_CPPFLAGS += -DPSYSAPI_TRACE_CONFIG=PSYSAPI_TRACE_LOG_LEVEL_NORMAL
# Enable/Disable 'late binding' support and it's additional queues
PSYSAPI_HOST_CPPFLAGS += -DHAS_LATE_BINDING_SUPPORT=$(PSYS_HAS_LATE_BINDING_SUPPORT)

#Example: how to switch to a different log level for a sub-module
#PSYSAPI_HOST_CPPFLAGS += -DPSYSAPI_DYNAMIC_TRACING_OVERRIDE=PSYSAPI_TRACE_LOG_LEVEL_DEBUG

# enable host side implementation
# TODO: better name for the flag to enable the impl...
PSYSAPI_HOST_CPPFLAGS += -D__X86_SIM__

# Files for Firmware
PSYSAPI_FW_FILES = $(PSYSAPI_DIR)/dynamic/src/ia_css_psys_process.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/dynamic/src/ia_css_psys_process_group.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/dynamic/src/ia_css_psys_terminal.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/dynamic/src/ia_css_psys_buffer_set.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/param/src/ia_css_program_group_param.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/data/src/ia_css_program_group_data.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/sim/src/vied_nci_psys_system.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/sim/src/ia_css_psys_sim_data.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/static/src/ia_css_psys_program_group_manifest.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/static/src/ia_css_psys_program_manifest.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/static/src/ia_css_psys_terminal_manifest.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/resource_model/$(PSYS_RESOURCE_MODEL_VERSION)/vied_nci_psys_resource_model.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/psys_server_manifest/$(PSYS_SERVER_MANIFEST_VERSION)/ia_css_psys_server_manifest.c
PSYSAPI_FW_FILES += $(PSYSAPI_DIR)/kernel/src/ia_css_kernel_bitmap.c

# resource model
PSYSAPI_RESOURCE_MODEL_FILES = $(PSYSAPI_DIR)/resource_model/$(PSYS_RESOURCE_MODEL_VERSION)/vied_nci_psys_resource_model.c

ifeq ($(PSYS_HAS_DUAL_CMD_CTX_SUPPORT), 1)
PSYSAPI_HOST_CPPFLAGS += -DHAS_DUAL_CMD_CTX_SUPPORT=$(PSYS_HAS_DUAL_CMD_CTX_SUPPORT)
endif
