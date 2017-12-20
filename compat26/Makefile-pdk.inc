# makefile include fragment for ported device systems
#
# Copyright 2016 Codethink Ltd

compat-dir-y := unset

compat-dir-$(CONFIG_TEGRA_PDK_19_006) = pdk19_006
compat-dir-$(CONFIG_TEGRA_PDK_25_023) = pdk25_023

ccflags-y += -Icompat26/include
ccflags-y += -Icompat26/$(compat-dir-y)/include
