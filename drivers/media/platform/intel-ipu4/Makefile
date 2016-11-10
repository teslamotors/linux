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

ccflags-y += -I$(srcpath)/$(src)/../../../../include/

obj-$(CONFIG_INTEL_IPU4_BXT_RVP_PDATA)	+= intel-ipu4-bxt-rvp-pdata.o
obj-$(CONFIG_INTEL_IPU4_BXT_RVP_PDATA)	+= intel-ipu4-bxt-p-apl-pdata.o
obj-$(CONFIG_INTEL_IPU4_BXT_GP_PDATA)   += intel-ipu4-bxt-gp-pdata.o
obj-$(CONFIG_INTEL_IPU4_BXT_P_PDATA)   += intel-ipu4-bxt-p-pdata.o
obj-$(CONFIG_INTEL_IPU4_BXT_GT_PDATA)   += intel-ipu4-bxt-gt-pdata.o
obj-$(CONFIG_VIDEO_INTEL_IPU4)		+= intel-ipu4-acpi.o
obj-$(CONFIG_INTEL_IPU5_FPGA_PDATA)   += intel-ipu5-fpga-pdata.o
