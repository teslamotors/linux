# makefile include fragment for ported device systems
#
# Copyright 2016 Codethink Ltd

include $(srctree)/compat26/Makefile-pdk.inc

ccflags-y += -include $(srctree)/compat26/include/compat26.h
ccflags-y += -include $(srctree)/compat26/include/compat26_clk.h
