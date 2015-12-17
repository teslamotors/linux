#
#  Copyright (c) 2010 - 2015, Intel Corporation.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms and conditions of the GNU General Public License,
#  version 2, as published by the Free Software Foundation.
#
#  This program is distributed in the hope it will be useful, but WITHOUT
#  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
#  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
#  more details.
#

MODULES_DIR = lib
INCLUDE_DIR = $(srctree)/$(src)/$(MODULES_DIR)
SYSTEM      = processing_system_system
SUBSYSTEM   = psys
SSID        = 0
MMID        = 0
IPU_SYSVER  = bxtB0
INCLUDE_DIR = $(srctree)/$(src)/lib

BXT_INTERFACES_ENABLED=1

ifeq ($(FW_LOAD_DMA), 1)
FW_LOAD_IMPL	= fwdma
else
FW_LOAD_IMPL	= xmem
endif

#HOST_INCLUDES += -I$(INCLUDE_DIR)/../hive_isp_css_include
#HOST_INCLUDES += -I$(INCLUDE_DIR)/../isp/kernels/io_ls/common
#HOST_INCLUDES += -DPSYSAPI_TRACE_CONFIG=PSYSAPI_TRACE_LOG_LEVEL_NORMAL
#HOST_INCLUDES += -D__X86_SIM__
#VIED_PARAMETERS_HOST_INCLUDES += -DVIED_PARAMETERS_TRACE_CONFIG=$(VIED_PARAMETERS_TRACE_CONFIG_HOST)
#VIED_PARAMETERS_FW_CPPFLAGS += -DVIED_PARAMETERS_TRACE_CONFIG=$(VIED_PARAMETERS_TRACE_CONFIG_FW)

HOST_INCLUDES  = \
		 -I$(INCLUDE_DIR)/buffer/interface \
		 -I$(INCLUDE_DIR)/cell/interface \
		 -I$(INCLUDE_DIR)/cell/src \
		 -I$(INCLUDE_DIR)/cell_program_load/interface \
		 -I$(INCLUDE_DIR)/cell_program_load/src \
		 -I$(INCLUDE_DIR)/client_pkg/interface \
		 -I$(INCLUDE_DIR)/client_pkg/src \
		 -I$(INCLUDE_DIR)/devices \
		 -I$(INCLUDE_DIR)/devices/interface \
		 -I$(INCLUDE_DIR)/devices/src \
		 -I$(INCLUDE_DIR)/devices/$(SUBSYSTEM)/$(IPU_SYSVER) \
		 -I$(INCLUDE_DIR)/devices/$(SYSTEM)/$(IPU_SYSVER) \
		 -I$(INCLUDE_DIR)/device_access/interface \
		 -I$(INCLUDE_DIR)/device_access/src \
		 -I$(INCLUDE_DIR)/fw_abi_common_types \
		 -I$(INCLUDE_DIR)/fw_load/interface \
		 -I$(INCLUDE_DIR)/pkg_dir/interface \
		 -I$(INCLUDE_DIR)/pkg_dir/src \
		 -I$(INCLUDE_DIR)/PO_program_group \
		 -I$(INCLUDE_DIR)/port/interface \
		 -I$(INCLUDE_DIR)/psysapi/interface \
		 -I$(INCLUDE_DIR)/psysapi/data/interface \
		 -I$(INCLUDE_DIR)/psysapi/device/interface \
		 -I$(INCLUDE_DIR)/psysapi/dynamic/interface \
		 -I$(INCLUDE_DIR)/psysapi/kernel/interface \
		 -I$(INCLUDE_DIR)/psysapi/param/interface \
		 -I$(INCLUDE_DIR)/psysapi/param/src \
		 -I$(INCLUDE_DIR)/psysapi/sim/interface \
		 -I$(INCLUDE_DIR)/psysapi/static/interface \
		 -I$(INCLUDE_DIR)/psys_infobits/interface \
		 -I$(INCLUDE_DIR)/psys_private_pg/interface \
		 -I$(INCLUDE_DIR)/psys_server/interface \
		 -I$(INCLUDE_DIR)/regmem/interface \
		 -I$(INCLUDE_DIR)/regmem/src \
		 -I$(INCLUDE_DIR)/reg_dump/src \
		 -I$(INCLUDE_DIR)/syscom/interface \
		 -I$(INCLUDE_DIR)/syscom/src \
		 -I$(INCLUDE_DIR)/support \
		 -I$(INCLUDE_DIR)/trace/interface \
		 -I$(INCLUDE_DIR)/usecase_generated \
		 -I$(INCLUDE_DIR)/vied \
		 -I$(INCLUDE_DIR)/vied_nci_acb/interface \
		 -I$(INCLUDE_DIR)/vied_parameters/interface \

HOST_SRC = libcsspsys2600.o \
	   $(MODULES_DIR)/reg_dump/src/reg_dump_generic_bridge.o \
	   $(MODULES_DIR)/syscom/src/ia_css_syscom.o \
	   $(MODULES_DIR)/syscom/src/syscom_cell.o \
	   $(MODULES_DIR)/port/src/send_port.o \
	   $(MODULES_DIR)/port/src/recv_port.o \
	   $(MODULES_DIR)/port/src/queue.o \
	   $(MODULES_DIR)/port/src/send_port.o \
	   $(MODULES_DIR)/port/src/recv_port.o \
	   $(MODULES_DIR)/fw_load/src/$(FW_LOAD_IMPL)/ia_css_fw_load.o \
	   $(MODULES_DIR)/psysapi/dynamic/src/ia_css_psys_process.o \
	   $(MODULES_DIR)/psysapi/dynamic/src/ia_css_psys_process_group.o \
	   $(MODULES_DIR)/psysapi/dynamic/src/ia_css_psys_terminal.o \
	   $(MODULES_DIR)/psysapi/static/src/ia_css_psys_program_group_manifest.o \
	   $(MODULES_DIR)/psysapi/static/src/ia_css_psys_program_manifest.o \
	   $(MODULES_DIR)/psysapi/static/src/ia_css_psys_terminal_manifest.o \
	   $(MODULES_DIR)/psysapi/sim/src/vied_nci_psys_system.o \
	   $(MODULES_DIR)/psysapi/kernel/src/ia_css_kernel_bitmap.o \
	   $(MODULES_DIR)/psysapi/data/src/ia_css_program_group_data.o \
	   $(MODULES_DIR)/psysapi/sim/src/ia_css_psys_sim_data.o \
	   $(MODULES_DIR)/psysapi/param/src/ia_css_program_group_param.o \
	   $(MODULES_DIR)/psysapi/dynamic/src/ia_css_psys_process.o \
	   $(MODULES_DIR)/psysapi/dynamic/src/ia_css_psys_process_group.o \
	   $(MODULES_DIR)/psysapi/dynamic/src/ia_css_psys_terminal.o \
	   $(MODULES_DIR)/psysapi/data/src/ia_css_program_group_data.o \
	   $(MODULES_DIR)/psysapi/sim/src/vied_nci_psys_system.o \
	   $(MODULES_DIR)/psysapi/sim/src/ia_css_psys_sim_data.o \
	   $(MODULES_DIR)/psysapi/static/src/ia_css_psys_program_group_manifest.o \
	   $(MODULES_DIR)/psysapi/static/src/ia_css_psys_program_manifest.o \
	   $(MODULES_DIR)/psysapi/static/src/ia_css_psys_terminal_manifest.o \
	   $(MODULES_DIR)/psysapi/device/src/ia_css_psys_device.o \
	   $(MODULES_DIR)/cell_program_load/src/ia_css_cell_program_load.o \
	   $(MODULES_DIR)/buffer/src/cpu/ia_css_buffer.o \
	   $(MODULES_DIR)/buffer/src/cpu/ia_css_output_buffer.o \
	   $(MODULES_DIR)/buffer/src/cpu/ia_css_input_buffer.o \
	   $(MODULES_DIR)/buffer/src/cpu/ia_css_shared_buffer.o \
	   $(MODULES_DIR)/buffer/src/cpu/buffer_access.o \
	   $(MODULES_DIR)/psys_server/src/bxt_spctrl_process_group_cmd_impl.o \
	   $(MODULES_DIR)/client_pkg/src/ia_css_client_pkg.o \
	   $(MODULES_DIR)/pkg_dir/src/ia_css_pkg_dir.o \
	   $(MODULES_DIR)/vied_parameters/src/ia_css_terminal.o \
	   $(MODULES_DIR)/vied_parameters/src/ia_css_terminal_manifest.o \
	   $(MODULES_DIR)/vied_parameters/src/ia_css_terminal.o \
	   $(MODULES_DIR)/vied_parameters/src/ia_css_terminal_manifest.o

ifdef VERBOSE
$(warning HOST_INCLUDES =)
$(foreach line,$(HOST_INCLUDES),$(warning ${line}))
endif

#CFLAGS = -W -Wall -Wstrict-prototypes -Wmissing-prototypes -O2 -fomit-frame-pointer -Wno-unused-variable
HOST_DEFINES += -DSSID=$(SSID)
HOST_DEFINES += -DMMID=$(MMID)
HOST_DEFINES += -DHRT_ON_VIED_SUBSYSTEM_ACCESS=$(SSID)
HOST_DEFINES += -DCFG_VIED_SUBSYSTEM_ACCESS_LIB_IMPL
HOST_DEFINES += -DHRT_USE_VIR_ADDRS
HOST_DEFINES += -DHRT_HW
HOST_DEFINES += -DVIED_NCI_TUNIT_PSYS
HOST_DEFINES += -DFIRMWARE_RELEASE_VERSION
HOST_DEFINES += -DPSYS_SERVER_ON_SPC

lib2600psys-mod-$(IPU_SYSVER)-objs := $(HOST_SRC)
obj-$(CONFIG_VIDEO_INTEL_IPU4)	+= lib2600psys-mod-$(IPU_SYSVER).o
ccflags-y += $(HOST_INCLUDES) $(HOST_DEFINES) -fno-common -v

