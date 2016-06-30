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

IPU_STEP = glvA0
ccflags-y += -DIPU_STEP_GLVA0
include $(srctree)/$(src)/../Makefile.common
include $(srctree)/$(src)/../Makefile.lib2600.ipu5

# ignore psys for isys only fw release package
# include $(srctree)/$(src)/../Makefile.lib2600psys

ccflags-y += -I$(srctree)/$(src)/../../../../../include/
ccflags-y += -I$(srctree)/$(src)/../
ccflags-y += -DPARAMETER_INTERFACE_V2
