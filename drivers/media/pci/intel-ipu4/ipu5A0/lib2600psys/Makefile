#
#  Copyright (c) 2010 - 2016, Intel Corporation.
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

ifneq ($(EXTERNAL_BUILD), 1)
srcpath := $(srctree)
endif

include $(srcpath)/$(src)/../Makefile.ipu5psys_src
include $(srcpath)/$(src)/../Makefile.ipu5psys_inc

SSID        = 0
MMID        = 0
IPU_SYSVER  = ipu5A0

IPU_PSYSLIB_ROOT_REL = lib
IPU_PSYSLIB_ROOT = $(srcpath)/$(src)/$(IPU_PSYSLIB_ROOT_REL)

IPU_PSYSLIB_SRC += libcsspsys2600.o

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
HOST_DEFINES += -DAPI_SPLIT_START_STATE_UPDATE

lib2600psys-mod-$(IPU_SYSVER)-objs := $(IPU_PSYSLIB_SRC)
obj-$(CONFIG_VIDEO_INTEL_IPU5)	+= lib2600psys-mod-$(IPU_SYSVER).o
ccflags-y += $(IPU_PSYSLIB_INC) $(HOST_DEFINES) -fno-common -v

