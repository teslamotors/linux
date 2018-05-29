# SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)

crlmodule-lite-objs			+= crlmodule-core.o crlmodule-data.o \
					crlmodule-regs.o crlmodule-nvm.o \
					crl_adv7481_hdmi_configuration.o \
					crlmodule-msrlist.o
obj-$(CONFIG_VIDEO_CRLMODULE_LITE)	+= crlmodule-lite.o

ccflags-y += -Idrivers/media/i2c
