# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2010 - 2018, Intel Corporation.

ifneq ($(EXTERNAL_BUILD), 1)
srcpath := $(srctree)
endif

ifdef CONFIG_VIDEO_INTEL_ICI
ccflags-y += -DHAS_DUAL_CMD_CTX_SUPPORT=0 -DIPU_VC_SUPPORT -DIPU_HAS_ISA -DIPU_PSYS_LEGACY -Wframe-larger-than=4096


# work-around to re-use ipu4-css and libintel-ipu4_ici.c together when
# compiling ICI-ISYS
$(shell cp -r $(srcpath)/$(src)/../ipu4/ipu4-css/ $(srcpath)/$(src)/)
$(shell cp -f $(srcpath)/$(src)/libintel-ipu4_ici.c $(srcpath)/$(src)/ipu4-css/libintel-ipu4.c)
$(shell cp -f $(srcpath)/$(src)/../ipu4/ipu-platform-resources.h $(srcpath)/$(src)/)

intel-ipu4-objs			+=../ipu.o \
				  ../ipu-bus.o \
				  ici-dma.o \
				  ../ipu-buttress.o \
				  ../ipu-trace.o \
				  ../ipu-cpd.o \
				  ../ipu-fw-com.o \
				  ../ipu4/ipu4.o

obj-$(CONFIG_VIDEO_INTEL_IPU)		+= intel-ipu4.o

intel-ipu4-mmu-objs			+= ../ipu-mmu.o
obj-$(CONFIG_VIDEO_INTEL_IPU)		+= intel-ipu4-mmu.o

ici-isys-mod-objs += \
                       ici-isys.o \
                       ici-isys-csi2.o \
                       ici-isys-tpg.o \
                       ici-isys-csi2-be.o \
                       ici-isys-stream.o \
                       ici-isys-frame-buf.o \
                       ici-isys-subdev.o \
                       ici-isys-pipeline.o \
                       ici-isys-pipeline-device.o \
                       ici-isys-stream-device.o
obj-$(CONFIG_VIDEO_INTEL_IPU)          += ici-isys-mod.o

intel-ipu4-psys-objs	+= ../ipu-psys.o \
			   ../ipu4/ipu4-resources.o \
			   ../ipu4/ipu4-psys.o \

ifndef CONFIG_VIDEO_INTEL_IPU_FW_LIB
intel-ipu4-psys-objs	+= ipu4-fw-resources.o \
			../ipu-fw-psys.o
endif

ifeq ($(CONFIG_COMPAT),y)
intel-ipu4-psys-objs	+= ../ipu-psys-compat32.o
endif

obj-$(CONFIG_VIDEO_INTEL_IPU)		+= intel-ipu4-psys.o

ifdef CONFIG_VIDEO_INTEL_IPU_FW_LIB
include $(srcpath)/$(src)/ipu4-css/Makefile.isyslib
include $(srcpath)/$(src)/ipu4-css/Makefile.psyslib
endif

ccflags-y += -I$(srcpath)/$(src)/../../../../../include/
ccflags-y += -I$(srcpath)/$(src)/../
ccflags-y += -I$(srcpath)/$(src)/../ipu4/
ccflags-y += -I$(srcpath)/$(src)/
ccflags-y += -I$(srcpath)/$(src)/ipu4-css

ccflags-y += -DPARAMETER_INTERFACE_V2

endif
