# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2010 - 2018, Intel Corporation.

# force check the compile warning to make sure zero warnings
# note we may have build issue when gcc upgraded.
subdir-ccflags-y := -Wall -Wextra
subdir-ccflags-y += $(call cc-disable-warning, unused-parameter)
subdir-ccflags-y += $(call cc-disable-warning, implicit-fallthrough)
subdir-ccflags-y += $(call cc-disable-warning, missing-field-initializers)
subdir-ccflags-$(CONFIG_VIDEO_INTEL_IPU_WERROR) += -Werror

obj-$(CONFIG_VIDEO_INTEL_ICI)	+= ici/
obj-$(CONFIG_VIDEO_INTEL_IPU_ACRN)	+= virtio/
ifndef CONFIG_VIDEO_INTEL_ICI
obj-$(CONFIG_VIDEO_INTEL_IPU4)	+= ipu4/
endif
obj-$(CONFIG_VIDEO_INTEL_IPU4P)	+= ipu4/
