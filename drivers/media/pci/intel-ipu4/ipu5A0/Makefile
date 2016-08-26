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

IPU_STEP = ipu5A0
ccflags-y += -DIPU_STEP_IPU5A0

include $(srcpath)/$(src)/../Makefile.link
include $(srcpath)/$(src)/../Makefile.common
include $(srcpath)/$(src)/../Makefile.lib2600.ipu5
include $(srcpath)/$(src)/../Makefile.lib2600psys.ipu5

ccflags-y += -I$(srcpath)/$(src)/../../../../../include/
ccflags-y += -I$(srcpath)/$(src)/../
ccflags-y += -DPARAMETER_INTERFACE_V2
