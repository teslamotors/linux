# SPDX-License-Identifier: GPL-2.0


cbc-ldisc-y := cbc_core.o
cbc-ldisc-y += cbc_device.o
cbc-ldisc-y += cbc_device_manager.o
cbc-ldisc-y += cbc_link_checksum.o
cbc-ldisc-y += cbc_link_layer.o
cbc-ldisc-y += cbc_memory.o
cbc-ldisc-y += cbc_mux_multiplexer.o

obj-$(CONFIG_CBC_LDISC) += cbc-ldisc.o

ccflags-y :=  -O2 -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security -fstack-protector