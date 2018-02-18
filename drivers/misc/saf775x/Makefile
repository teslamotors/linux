#
# drivers/misc/saf775x/Makefile
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
#

obj-$(CONFIG_SAF775x_HWDEP) := saf775x-hwdep.o

saf775x-hwdep-objs := \
	saf775x_stub.o \
	saf775x_ioctl.o

obj-$(CONFIG_SAF36XX_HWDEP) += saf36xx-hwdep.o

saf36xx-hwdep-objs := \
	saf36xx.o
