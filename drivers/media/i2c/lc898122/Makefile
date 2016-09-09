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

obj-$(CONFIG_VIDEO_LC898122) += lc898122x.o

lc898122x-objs := lc898122.o lc898122-oiscmd.o lc898122-oisinit.o


ccflags-y += -Werror -I$(srcpath)/$(src)/../../../../include/
