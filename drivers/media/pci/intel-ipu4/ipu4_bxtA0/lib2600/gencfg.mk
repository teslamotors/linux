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
# Configuration Generator


ifndef IPU_SYSVER
$(error IPU_SYSVER not defined)
endif
include $(MODULES_DIR)/config/system_$(IPU_SYSVER).mk

ifeq ($(SYSTEM),input_system_system)
	# SUBSYSTEM must be isys
	ifdef SUBSYSTEM
		ifneq ($(SUBSYSTEM), isys)
			foo = $(error SUBSYSTEM must be 'isys')
		endif
	else
		SUBSYSTEM = isys
	endif
else
ifeq ($(SYSTEM),processing_system_system)
	# SUBSYSTEM must be psys
	ifdef SUBSYSTEM
		ifneq ($(SUBSYSTEM), psys)
			foo = $(error SUBSYSTEM must be 'psys')
		endif
	else
		SUBSYSTEM = psys
	endif
else
ifeq ($(SYSTEM),css_broxton_system)
	# SUBSYSTEM must be isys or psys
	ifdef SUBSYSTEM
		ifneq ($(SUBSYSTEM), isys)
			ifneq ($(SUBSYSTEM), psys)
				foo = $(error SUBSYSTEM must be 'isys' or 'psys')
			endif
		endif
	else
		foo = $(error SUBSYSTEM not defined)
	endif
endif
endif
endif

ifeq ($(SUBSYSTEM), isys)
	SUBSYSTEM_INCLUDE_STRING = \<isys.h\>
	PTYPE_STRING = PTYPE_SP
	SYSTEM_STRING = input_system_
	PROCESSOR_TYPE_STRING = unis_logic_
	PROCESSOR_TYPE_STRINGMMU = unis_logic_
	PROCESSOR_SUBTYPE_STRING = sp_control_tile
	PROCESSOR_INSTANCE_STRING = _
	PROCESSOR_TYPE_STRINGSUFFIX = sp
else
ifeq ($(SUBSYSTEM), psys)
	SUBSYSTEM_INCLUDE_STRING = \<psys_system_defs.h\>
	SYSTEM_STRING = processing_system_
	PROCESSOR_TYPE_STRINGMMU = unps_logic_
	ifeq ($(PROCESSOR_TYPE),sp)
		PTYPE_STRING = PTYPE_SP
		PROCESSOR_TYPE_STRING = unps_logic_
		ifeq ($(PROCESSOR_SUBTYPE),c)
			PROCESSOR_SUBTYPE_STRING = spc_tile
			PROCESSOR_INSTANCE_STRING = _
		else
		ifeq ($(PROCESSOR_SUBTYPE),p)
			PROCESSOR_SUBTYPE_STRING = spp_tile
			PROCESSOR_INSTANCE_STRING = $(PROCESSOR_INSTANCE)_
		else
		ifeq ($(PROCESSOR_SUBTYPE),f)
			PROCESSOR_SUBTYPE_STRING = spf_tile
			PROCESSOR_INSTANCE_STRING = _
		endif
		endif
		endif
		PROCESSOR_TYPE_STRINGSUFFIX = sp
	else
	ifeq ($(PROCESSOR_TYPE),isp)
		PTYPE_STRING = PTYPE_ISP
		PROCESSOR_TYPE_STRING = isp_
		PROCESSOR_SUBTYPE_STRING = tile
		PROCESSOR_INSTANCE_STRING = $(PROCESSOR_INSTANCE)_
		PROCESSOR_TYPE_STRINGSUFFIX = logic_isp
	endif
	endif
endif
endif

CELL_STRING = $(SYSTEM_STRING)$(PROCESSOR_TYPE_STRING)$(PROCESSOR_SUBTYPE_STRING)$(PROCESSOR_INSTANCE_STRING)$(PROCESSOR_TYPE_STRINGSUFFIX)
CMMU_STRING = $(SYSTEM_STRING)$(PROCESSOR_TYPE_STRINGMMU)mmu_at_system_mmu0_ctrl_in_master_port_address
CBUSCFGSP_STRING = $(SYSTEM_STRING)$(PROCESSOR_TYPE_STRING)$(PROCESSOR_SUBTYPE_STRING)$(PROCESSOR_INSTANCE_STRING)cfg_bus_sl_master_port_address
CBUSCFGISP_STRING = $(SYSTEM_STRING)$(PROCESSOR_TYPE_STRING)$(PROCESSOR_SUBTYPE_STRING)$(PROCESSOR_INSTANCE_STRING)logic_cfg_bus_sl_master_port_address

ifeq ($(PROCESSOR_TYPE_STRINGSUFFIX),logic_isp)
PROG_CELL_STRING     = $(ISP_CELL)
else
ifeq ($(PROCESSOR_SUBTYPE_STRING),spp_tile)
PROG_CELL_STRING     = $(SP_PROXY_CELL)
else
ifeq ($(PROCESSOR_SUBTYPE_STRING),spf_tile)
PROG_CELL_STRING     = $(SP_FP_CELL)
else
PROG_CELL_STRING     = $(SP_CONTROL_CELL)
endif
endif
endif
