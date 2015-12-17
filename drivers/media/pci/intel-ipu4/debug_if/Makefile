#
#  Copyright (c) 2010 - 2015, Intel Corporation.
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

obj-$(CONFIG_VIDEO_INTEL_IPU4_DEBUG)	+= intel-ipu4_subaccess_userif.o

ccflags-y += -I$(srctree)/$(src)/../lib2600 \
		-I$(srctree)/$(src)/../ \
		-I$(srctree)/$(src)/../lib2600/isysapi/src \
		-I$(srctree)/$(src)/../lib2600/isysapi/interface \
		-I$(srctree)/$(src)/../lib2600/syscom/interface \
		-I$(srctree)/$(src)/../lib2600/buffer/interface \
		-I$(srctree)/$(src)/../lib2600/buffer/src/cpu \
		-I$(srctree)/$(src)/../lib2600/regmem/src/dmem \
		-I$(srctree)/$(src)/../lib2600/utils/system_defs \
		-I$(srctree)/$(src)/../lib2600/utils/shared_memory \
		-I$(srctree)/$(src)/../lib2600/utils/info_bits \
		-I$(srctree)/$(src)/../lib2600/port/interface \
		-I$(srctree)/$(src)/../lib2600/port/src \
		-I$(srctree)/$(src)/../lib2600/support \
		-I$(srctree)/$(src)/../lib2600/utils/system_defs/ \
		-I$(srctree)/$(src)/../lib2600/utils/temp \
		-I$(srctree)/$(src)/../lib2600/utils/support \
		-I$(srctree)/$(src)/../lib2600/regmem/src/dmem \
		-I$(srctree)/$(src)/../lib2600/regmem/src/gpreg \
		-I$(srctree)/$(src)/../lib2600/regmem/interface \
		-I$(srctree)/$(src)/../lib2600/hrt/include \
		-I$(srctree)/$(src)/../lib2600/hrt/include/hrt \
		-I$(srctree)/$(src)/../lib2600/hrt/include2 \
		-I$(srctree)/$(src)/../lib2600/hrt/include3 \
		-I$(srctree)/$(src)/../lib2600/hrt/cores/hrt/include \
		-I$(srctree)/$(src)/../lib2600/vied \
		-I$(srctree)/$(src)/../lib2600/vied/vied \
		-I$(srctree)/$(src)/../lib2600/vied/host \
		-I$(srctree)/$(src)/../lib2600//isysapi/test/sched/ \
		-I$(srctree)/$(src)/../lib2600/boot/interface \
		-DCFG_VIED_SUBSYSTEM_ACCESS_LIB_IMPL

