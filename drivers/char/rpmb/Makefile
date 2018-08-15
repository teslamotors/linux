# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_RPMB) += rpmb.o
rpmb-objs += core.o
rpmb-$(CONFIG_RPMB_INTF_DEV) += cdev.o
obj-$(CONFIG_RPMB_SIM) += rpmb_sim.o

ccflags-y += -D__CHECK_ENDIAN__
