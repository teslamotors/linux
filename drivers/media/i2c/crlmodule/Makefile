# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2010 - 2018, Intel Corporation.

# force check the compile warning to make sure zero warnings
# note we may have build issue when gcc upgraded.
ccflags-y := -Wall -Wextra
ccflags-y += $(call cc-disable-warning, unused-parameter)
ccflags-y += $(call cc-disable-warning, implicit-fallthrough)
ccflags-y += $(call cc-disable-warning, missing-field-initializers)
ccflags-$(CONFIG_VIDEO_INTEL_IPU_WERROR) += -Werror

crlmodule-objs			+= crlmodule-core.o crlmodule-data.o \
				   crlmodule-regs.o crlmodule-nvm.o \
				   crl_adv7481_hdmi_configuration.o \
				   crlmodule-msrlist.o
obj-$(CONFIG_VIDEO_CRLMODULE)	+= crlmodule.o

ccflags-y += -Idrivers/media/i2c
