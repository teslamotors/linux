#
# Copyright (c) 2013-2016, NVIDIA Corporation. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#

GCOV_PROFILE := y
subdir-ccflags-y += -Werror

#For OTE over Trusty
ifeq ($(CONFIG_OTE_TRUSTY),y)
tlk_driver-objs += ote-trusty/ote_device.o
tlk_driver-objs += ote-trusty/ote_comms.o
tlk_driver-objs += ote-trusty/ote_fs.o
ifeq ($(CONFIG_ARM64),y)
tlk_driver-objs += ote-trusty/ote_page_arm64.o
else
tlk_driver-objs += ote-trusty/ote_page.o
endif
endif

#For OTE over TLK
ifeq ($(CONFIG_TRUSTED_LITTLE_KERNEL), y)
tlk_driver-objs += ote_device.o
tlk_driver-objs += ote_comms.o
tlk_driver-objs += ote_fs.o
tlk_driver-objs += ote_asm.o
obj-$(CONFIG_ARM64) += ote_fiq_glue.o
endif

# Either of CONFIG_TRUSTED_LITTLE_KERNEL or CONFIG_OTE_TRUSTY are enabled.
ifneq ($(filter y,$(CONFIG_TRUSTED_LITTLE_KERNEL) $(CONFIG_OTE_TRUSTY)),)
tlk_driver-objs += ote_log.o
endif

ifeq ($(CONFIG_ARM),y)
plus_sec := $(call as-instr,.arch_extension sec,+sec)
AFLAGS_ote_asm.o :=-Wa,-march=armv7-a$(plus_sec)
endif

obj-y += tlk_driver.o
