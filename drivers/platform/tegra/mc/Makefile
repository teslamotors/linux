#
# Memory controller code.
#

GCOV_PROFILE := y

ccflags-y += -I$(srctree)/arch/arm/mach-tegra/include \
             -I$(srctree)/arch/arm/mach-tegra \
             -I$(srctree)/drivers/platform/tegra/include

obj-y                                   += mc.o

# MC error reporting.
obj-y                                   += mcerr.o
obj-$(CONFIG_ARCH_TEGRA_12x_SOC)        += mcerr-t12.o
obj-$(CONFIG_ARCH_TEGRA_21x_SOC)        += mcerr-t21.o

# EMC clock control
obj-$(CONFIG_ARCH_TEGRA_21x_SOC)        += tegra21_emc.o
obj-$(CONFIG_ARCH_TEGRA_21x_SOC)        += tegra21_emc_cc_r21012.o
obj-$(CONFIG_ARCH_TEGRA_21x_SOC)        += tegra21_emc_cc_r21015.o
obj-$(CONFIG_ARCH_TEGRA_21x_SOC)        += tegra21_emc_cc_r21021.o

# Latency allowance
obj-y                                   += latency_allowance.o
obj-$(CONFIG_ARCH_TEGRA_12x_SOC)        += tegra12x_la.o
obj-$(CONFIG_ARCH_TEGRA_21x_SOC)        += tegra21x_la.o

ifeq ($(CONFIG_PM_SLEEP),y)
obj-$(CONFIG_ARCH_TEGRA_12x_SOC)        += mc-timing-t12x.o
obj-$(CONFIG_ARCH_TEGRA_21x_SOC)        += mc-timing-t21x.o
endif
obj-${CONFIG_TEGRA_ISOMGR}              += isomgr.o
obj-$(CONFIG_TEGRA_BWMGR)               += emc_bwmgr.o

ifeq ($(CONFIG_TEGRA_MC),y)
obj-y += tegra_emc.o
obj-y                                   += tegra_emc_timers.o

obj-$(CONFIG_ARCH_TEGRA_12x_SOC) += tegra12_emc.o
obj-$(CONFIG_ARCH_TEGRA_12x_SOC) += tegra_emc_dt_parse.o
obj-$(CONFIG_ARCH_TEGRA_21x_SOC) += tegra_emc_dt_parse.o
endif
