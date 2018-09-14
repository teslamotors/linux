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
# MODULE is PSYS_SERVER

include $(MODULES_DIR)/config/system_$(IPU_SYSVER).mk
include $(MODULES_DIR)/config/$(SUBSYSTEM)/subsystem_$(IPU_SYSVER).mk

PSYS_SERVER_DIR=${MODULES_DIR}/psys_server

# The watchdog should never be merged enabled
PSYS_SERVER_WATCHDOG_ENABLE ?= 0

PSYS_SERVER_INTERFACE=$(PSYS_SERVER_DIR)/interface
PSYS_SERVER_SOURCES=$(PSYS_SERVER_DIR)/src

# PSYS API implementation files. Consider a new module for those to avoid
# having them together with firmware.
PSYS_SERVER_HOST_FILES += $${MODULES_DIR}/psysapi/device/src/ia_css_psys_device.c
PSYS_SERVER_HOST_FILES += $(PSYS_SERVER_SOURCES)/bxt_spctrl_process_group_cmd_impl.c

PSYS_SERVER_HOST_CPPFLAGS += -I$(PSYS_SERVER_INTERFACE)

PSYS_SERVER_HOST_CPPFLAGS += -DSSID=$(SSID)
PSYS_SERVER_HOST_CPPFLAGS += -DMMID=$(MMID)


PSYS_SERVER_FW_FILES += $(PSYS_SERVER_SOURCES)/psys_cmd_queue_fw.c
PSYS_SERVER_FW_FILES += $(PSYS_SERVER_SOURCES)/psys_event_queue_fw.c
PSYS_SERVER_FW_FILES += $(PSYS_SERVER_SOURCES)/psys_init_fw.c
PSYS_SERVER_FW_FILES += $(PSYS_SERVER_SOURCES)/psys_process_group_fw.c

# Files that server modules need to use
PSYS_SERVER_SUPPORT_FILES = $(PSYS_SERVER_SOURCES)/dev_access_conv/$(IPU_SYSVER)/ia_css_psys_server_dev_access_type_conv.c
PSYS_SERVER_SUPPORT_FILES += $(PSYS_SERVER_SOURCES)/ia_css_psys_server_config.c

# Include those to build the release firmware. Otherwise replace by test code.
PSYS_SERVER_RELEASE_FW_FILES = $(PSYS_SERVER_SOURCES)/psys_server.c
PSYS_SERVER_RELEASE_FW_FILES += $(PSYS_SERVER_SOURCES)/ia_css_psys_proxy.c
PSYS_SERVER_RELEASE_FW_FILES += $(PSYS_SERVER_SOURCES)/ia_css_psys_server_dev_access.c
PSYS_SERVER_RELEASE_FW_FILES += $(PSYS_SERVER_SOURCES)/ia_css_psys_server_terminal_load.c
PSYS_SERVER_RELEASE_FW_FILES += $(PSYS_SERVER_SOURCES)/ia_css_psys_server_remote_obj_access.c
PSYS_SERVER_RELEASE_FW_FILES += $(PSYS_SERVER_SOURCES)/ia_css_psys_server_dma_access.c
ifeq ($(HAS_DEC400), 1)
PSYS_SERVER_RELEASE_FW_FILES += $(PSYS_SERVER_SOURCES)/ia_css_psys_server_dec400_access.c
endif
PSYS_SERVER_RELEASE_FW_FILES += $(PSYS_SERVER_SUPPORT_FILES)

PSYS_SERVER_FW_CPPFLAGS += -I$(PSYS_SERVER_INTERFACE)
PSYS_SERVER_FW_CPPFLAGS += -I$(PSYS_SERVER_SOURCES)
PSYS_SERVER_FW_CPPFLAGS += -I$(PSYS_SERVER_SOURCES)/$(IPU_SYSVER)
PSYS_SERVER_FW_CPPFLAGS += -I$(PSYS_SERVER_SOURCES)/$(PSYS_SERVER_VERSION)
PSYS_SERVER_FW_CPPFLAGS += -I$(PSYS_SERVER_SOURCES)/loader/$(PSYS_SERVER_LOADER_VERSION)
PSYS_SERVER_FW_CPPFLAGS += -I$(PSYS_SERVER_SOURCES)/access_blocker/$(PSYS_ACCESS_BLOCKER_VERSION)
PSYS_SERVER_FW_CPPFLAGS += -I$(PSYS_SERVER_SOURCES)/access_blocker/src

PSYS_SERVER_FW_CPPFLAGS += -DSSID=$(SSID)
PSYS_SERVER_FW_CPPFLAGS += -DMMID=$(MMID)
PSYS_SERVER_FW_CPPFLAGS += -DHAS_DPCM=$(if $(HAS_DPCM),1,0)

# PSYS server watchdog for debugging
ifeq ($(PSYS_SERVER_WATCHDOG_ENABLE), 1)
	PSYS_SERVER_FW_FILES += $(PSYS_SERVER_SOURCES)/ia_css_psys_server_watchdog.c
	PSYS_SERVER_FW_CPPFLAGS += -DPSYS_SERVER_WATCHDOG_DEBUG
endif

PSYS_SERVER_FW_CPPFLAGS += -D$(PSYS_HW_VERSION)

PSYS_SERVER_FW_CPPFLAGS += -DENABLE_TPROXY=$(PSYS_SERVER_ENABLE_TPROXY)
PSYS_SERVER_FW_CPPFLAGS += -DENABLE_DEVPROXY=$(PSYS_SERVER_ENABLE_DEVPROXY)
