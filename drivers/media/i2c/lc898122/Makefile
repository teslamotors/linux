# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2010 - 2018, Intel Corporation.

ifneq ($(EXTERNAL_BUILD), 1)
srcpath := $(srctree)
endif

obj-$(CONFIG_VIDEO_LC898122) += lc898122x.o

lc898122x-objs := lc898122.o lc898122-oiscmd.o lc898122-oisinit.o


ccflags-y += -Werror -I$(srcpath)/$(src)/../../../../include/
