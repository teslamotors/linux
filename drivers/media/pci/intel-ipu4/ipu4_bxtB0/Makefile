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

IPU_STEP = bxtB0
ccflags-y += -DIPU_STEP_BXTB0 -DIPU_STEP_BXTC0
include $(srctree)/$(src)/../Makefile.common
include $(srctree)/$(src)/../Makefile.lib2600
include $(srctree)/$(src)/../Makefile.lib2600psys
ccflags-y += -I$(srctree)/$(src)/../../../../../include/
ccflags-y += -I$(srctree)/$(src)/../
ccflags-y += -DPARAMETER_INTERFACE_V2
