# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2010 - 2018, Intel Corporation.

ifneq ($(EXTERNAL_BUILD), 1)
srcpath := $(srctree)
endif

# force check the compile warning to make sure zero warnings
# note we may have build issue when gcc upgraded.
ccflags-y := -Wall -Wextra
ccflags-y += $(call cc-disable-warning, unused-parameter)
ccflags-y += $(call cc-disable-warning, implicit-fallthrough)
ccflags-y += $(call cc-disable-warning, missing-field-initializers)
ccflags-$(CONFIG_VIDEO_INTEL_IPU_WERROR) += -Werror

ccflags-y += -I$(srcpath)/$(src)/../../../../include/
ccflags-y += -I$(srcpath)/$(src)/../../pci/intel/

obj-$(CONFIG_INTEL_IPU4_ICI_BXT_P_PDATA)   += ipu4-ici-bxt-p-pdata.o
obj-$(CONFIG_VIDEO_INTEL_IPU)   += ipu4-acpi.o
obj-$(CONFIG_INTEL_IPU4_BXT_P_PDATA)   += ipu4-bxt-p-pdata.o
obj-$(CONFIG_INTEL_IPU4_BXT_GP_PDATA)   += ipu4-bxt-gp-pdata.o
obj-$(CONFIG_INTEL_IPU4P_CNL_RVP_PDATA)   += ipu4p-cnl-rvp-pdata.o
