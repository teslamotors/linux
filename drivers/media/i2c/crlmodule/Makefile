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

crlmodule-objs			+= crlmodule-core.o crlmodule-data.o \
				   crlmodule-regs.o crlmodule-nvm.o \
				   crlmodule-msrlist.o
obj-$(CONFIG_VIDEO_CRLMODULE)	+= crlmodule.o

ccflags-y += -Idrivers/media/i2c
