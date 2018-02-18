#
# Makefile for linux/drivers/platform/tegra
# Tegra Platform-Specific Drivers
#

GCOV_PROFILE := y

subdir-ccflags-y += -Werror \
		    -I$(srctree)/arch/arm/mach-tegra
obj-$(CONFIG_TEGRA_POWERGATE) += powergate.o
obj-$(CONFIG_TEGRA_POWERGATE) += powergate-ops-t1xx.o
ifeq ($(CONFIG_TEGRA_POWERGATE),y)
obj-$(CONFIG_ARCH_TEGRA_12x_SOC)        += powergate-t12x.o
obj-$(CONFIG_ARCH_TEGRA_21x_SOC)        += powergate-t21x.o
endif
