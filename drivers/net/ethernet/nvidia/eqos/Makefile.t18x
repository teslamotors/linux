# Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.

ccflags-y += -DHWA_NV_1640420
ccflags-y += -DHWA_NV_1637630
ccflags-y += -DHWA_NV_1650337
ccflags-y += -DHWA_FPGA_ONLY

## default build settings.
ccflags-y += -DCONFIG_PTPSUPPORT_OBJ

obj-$(CONFIG_EQOS)  += eqos.o

eqos-y += dev.o
eqos-y += drv.o
eqos-y += desc.o
eqos-y += ethtool.o
eqos-y += init.o
eqos-y += mdio.o
eqos-y += nvidia_test.o
eqos-y += eee.o
eqos-$(CONFIG_DEBUGFS_OBJ) += debug_operation.o
eqos-y += ptp.o

