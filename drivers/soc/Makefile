#
# Makefile for the Linux Kernel SOC specific device drivers.
#

obj-$(CONFIG_ARCH_QCOM)		+= qcom/
ifneq ($(CONFIG_PLATFORM_TEGRA),y)
obj-$(CONFIG_ARCH_TEGRA)	+= tegra/
endif
obj-$(CONFIG_SOC_TI)		+= ti/
obj-$(CONFIG_PLAT_VERSATILE)	+= versatile/
