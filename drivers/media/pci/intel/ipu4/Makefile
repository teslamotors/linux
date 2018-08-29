# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2010 - 2018, Intel Corporation.

ifneq ($(EXTERNAL_BUILD), 1)
srcpath := $(srctree)
endif

ifdef CONFIG_VIDEO_INTEL_IPU4
ccflags-y += -DHAS_DUAL_CMD_CTX_SUPPORT=0
ccflags-y += -DIPU_META_DATA_SUPPORT

intel-ipu4-objs				+= ../ipu.o \
					   ../ipu-bus.o \
					   ../ipu-dma.o \
					   ../ipu-buttress.o \
					   ../ipu-trace.o \
					   ../ipu-cpd.o \
					   ../ipu-fw-com.o \
					   ipu4.o

obj-$(CONFIG_VIDEO_INTEL_IPU)		+= intel-ipu4.o

intel-ipu4-mmu-objs			+= ../ipu-mmu.o
obj-$(CONFIG_VIDEO_INTEL_IPU)		+= intel-ipu4-mmu.o

intel-ipu4-isys-objs			+= ../ipu-isys.o \
					   ../ipu-isys-csi2.o \
					   ipu4-isys.o \
					   ipu4-isys-csi2.o \
					   ../ipu-isys-csi2-be-soc.o \
					   ../ipu-isys-csi2-be.o \
					   ../ipu-fw-isys.o \
					   ipu4-isys-isa.o \
					   ../ipu-isys-video.o \
					   ../ipu-isys-queue.o \
					   ../ipu-isys-subdev.o \
					   ../ipu-isys-tpg.o

obj-$(CONFIG_VIDEO_INTEL_IPU)		+= intel-ipu4-isys.o

intel-ipu4-psys-objs			+= ../ipu-psys.o \
					   ipu4-psys.o \
					   ipu4-resources.o \

ifndef CONFIG_VIDEO_INTEL_IPU_FW_LIB
intel-ipu4-psys-objs			+= ipu4-fw-resources.o \
					   ../ipu-fw-psys.o
endif

ifeq ($(CONFIG_COMPAT),y)
intel-ipu4-psys-objs			+= ../ipu-psys-compat32.o
endif

obj-$(CONFIG_VIDEO_INTEL_IPU)		+= intel-ipu4-psys.o

ifdef CONFIG_VIDEO_INTEL_IPU_FW_LIB
include $(srcpath)/$(src)/ipu4-css/Makefile.isyslib
include $(srcpath)/$(src)/ipu4-css/Makefile.psyslib
endif

ccflags-y += -I$(srcpath)/$(src)/../../../../../include/
ccflags-y += -I$(srcpath)/$(src)/../
ccflags-y += -I$(srcpath)/$(src)/
ifdef CONFIG_VIDEO_INTEL_IPU_FW_LIB
ccflags-y += -I$(srcpath)/$(src)/ipu4-css
endif

ccflags-y += -DPARAMETER_INTERFACE_V2
endif

ifdef CONFIG_VIDEO_INTEL_IPU4P
ccflags-y += -DHAS_DUAL_CMD_CTX_SUPPORT=0
ccflags-y += -DIPU_META_DATA_SUPPORT

intel-ipu4p-objs			+= ../ipu.o \
					   ../ipu-bus.o \
					   ../ipu-dma.o \
					   ../ipu-buttress.o \
					   ../ipu-trace.o \
					   ../ipu-cpd.o \
					   ../ipu-fw-com.o \
					   ipu4.o

obj-$(CONFIG_VIDEO_INTEL_IPU)		+= intel-ipu4p.o

intel-ipu4p-mmu-objs			+= ../ipu-mmu.o
obj-$(CONFIG_VIDEO_INTEL_IPU)		+= intel-ipu4p-mmu.o

intel-ipu4p-isys-objs			+= ../ipu-isys.o \
					   ../ipu-isys-csi2.o \
					   ipu4-isys.o \
					   ipu4p-isys-csi2.o \
					   ../ipu-isys-csi2-be-soc.o \
					   ../ipu-isys-csi2-be.o \
					   ../ipu-fw-isys.o \
                                           ipu4-isys-isa.o \
					   ../ipu-isys-video.o \
					   ../ipu-isys-queue.o \
					   ../ipu-isys-subdev.o \
					   ../ipu-isys-tpg.o
obj-$(CONFIG_VIDEO_INTEL_IPU)		+= intel-ipu4p-isys.o

intel-ipu4p-psys-objs			+= ../ipu-psys.o \
					   ipu4-psys.o \
					   ipu4-resources.o \

ifndef CONFIG_VIDEO_INTEL_IPU_FW_LIB
intel-ipu4p-psys-objs			+= ipu4-fw-resources.o \
					   ../ipu-fw-psys.o
endif

ifeq ($(CONFIG_COMPAT),y)
intel-ipu4p-psys-objs			+= ../ipu-psys-compat32.o
endif

obj-$(CONFIG_VIDEO_INTEL_IPU)		+= intel-ipu4p-psys.o

ifdef CONFIG_VIDEO_INTEL_IPU_FW_LIB
include $(srcpath)/$(src)/ipu4p-css/Makefile.isyslib
include $(srcpath)/$(src)/ipu4p-css/Makefile.psyslib
endif

ccflags-y += -I$(srcpath)/$(src)/../../../../../include/
ccflags-y += -I$(srcpath)/$(src)/../
ccflags-y += -I$(srcpath)/$(src)/
ifdef CONFIG_VIDEO_INTEL_IPU_FW_LIB
ccflags-y += -I$(srcpath)/$(src)/ipu4p-css
endif

ccflags-y += -DPARAMETER_INTERFACE_V2
endif
