#
#  Copyright (c) 2010 - 2018 Intel Corporation.
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
ccflags-y += -I$(srcpath)/$(src)/../../pci/intel/

obj-$(CONFIG_INTEL_IPU4_BXT_P_PDATA)   += ipu4-bxt-p-pdata.o
obj-$(CONFIG_INTEL_IPU4P_CNL_RVP_PDATA)   += ipu4p-cnl-rvp-pdata.o
