#
# Makefile for Hypervisor interface
#

ifeq ($(CONFIG_ARCH_TEGRA_12x_SOC),y)
obj-$(CONFIG_TEGRA_VIRTUALIZATION)	+= hyp_syscall.o sys.o
endif
obj-$(CONFIG_TEGRA_HV_MANAGER)		+= tegra_hv.o ivc-cdev.o
obj-$(CONFIG_TEGRA_HV_MANAGER)		+= hv_sys_test.o
obj-$(CONFIG_TEGRA_VIRTUALIZATION)	+= hv_sysinfo.o
