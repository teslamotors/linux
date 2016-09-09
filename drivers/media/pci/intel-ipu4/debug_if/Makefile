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

obj-$(CONFIG_VIDEO_INTEL_IPU4_DEBUG)	+= intel-ipu4_subaccess_userif.o

ccflags-y += -I$(srcpath)/$(src)/../lib2600 \
		-I$(srcpath)/$(src)/../ \
		-I$(srcpath)/$(src)/../lib2600/isysapi/src \
		-I$(srcpath)/$(src)/../lib2600/isysapi/interface \
		-I$(srcpath)/$(src)/../lib2600/syscom/interface \
		-I$(srcpath)/$(src)/../lib2600/buffer/interface \
		-I$(srcpath)/$(src)/../lib2600/buffer/src/cpu \
		-I$(srcpath)/$(src)/../lib2600/regmem/src/dmem \
		-I$(srcpath)/$(src)/../lib2600/utils/system_defs \
		-I$(srcpath)/$(src)/../lib2600/utils/shared_memory \
		-I$(srcpath)/$(src)/../lib2600/utils/info_bits \
		-I$(srcpath)/$(src)/../lib2600/port/interface \
		-I$(srcpath)/$(src)/../lib2600/port/src \
		-I$(srcpath)/$(src)/../lib2600/support \
		-I$(srcpath)/$(src)/../lib2600/utils/system_defs/ \
		-I$(srcpath)/$(src)/../lib2600/utils/temp \
		-I$(srcpath)/$(src)/../lib2600/utils/support \
		-I$(srcpath)/$(src)/../lib2600/regmem/src/dmem \
		-I$(srcpath)/$(src)/../lib2600/regmem/src/gpreg \
		-I$(srcpath)/$(src)/../lib2600/regmem/interface \
		-I$(srcpath)/$(src)/../lib2600/hrt/include \
		-I$(srcpath)/$(src)/../lib2600/hrt/include/hrt \
		-I$(srcpath)/$(src)/../lib2600/hrt/include2 \
		-I$(srcpath)/$(src)/../lib2600/hrt/include3 \
		-I$(srcpath)/$(src)/../lib2600/hrt/cores/hrt/include \
		-I$(srcpath)/$(src)/../lib2600/vied \
		-I$(srcpath)/$(src)/../lib2600/vied/vied \
		-I$(srcpath)/$(src)/../lib2600/vied/host \
		-I$(srcpath)/$(src)/../lib2600//isysapi/test/sched/ \
		-I$(srcpath)/$(src)/../lib2600/boot/interface \
		-DCFG_VIED_SUBSYSTEM_ACCESS_LIB_IMPL

