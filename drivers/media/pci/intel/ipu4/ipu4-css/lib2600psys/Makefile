#
#  Copyright (c) 2010 - 2018, Intel Corporation.
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

include $(srcpath)/$(src)/../Makefile.ipu4psys_src
include $(srcpath)/$(src)/../Makefile.ipu4psys_inc

SSID        = 0
MMID        = 0

IPU_PSYSLIB_ROOT_REL = lib
IPU_PSYSLIB_ROOT = $(srcpath)/$(src)/$(IPU_PSYSLIB_ROOT_REL)

ccflags-y += -I$(srcpath)/$(src)/../../../
ccflags-y += -I$(srcpath)/$(src)/../../
ccflags-y += -DHAS_DUAL_CMD_CTX_SUPPORT=0 -DHAS_LATE_BINDING_SUPPORT=0 -DIPU_PSYS_LEGACY

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

intel-ipu4-psys-csslib-objs := ../../../ipu-wrapper.o \
			$(IPU_PSYSLIB_SRC)

obj-$(CONFIG_VIDEO_INTEL_IPU)	+= intel-ipu4-psys-csslib.o
ccflags-y += $(IPU_PSYSLIB_INC) $(HOST_DEFINES) -fno-common -v

