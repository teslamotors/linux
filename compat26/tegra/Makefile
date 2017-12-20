# compat26/tegra/Makefile
#
# Tegra specific 2.6 compatilibty functions

obj-y	+= clock.o
obj-y	+= core.o
obj-y	+= devices.o
obj-y	+= kfuse.o
obj-y	+= mc.o
obj-y	+= reset.o
obj-y	+= syncpt-irq.o
obj-y	+= procfs.o

obj-$(CONFIG_ARCH_TEGRA_3x_SOC)	+= csi.o
obj-$(CONFIG_ARCH_TEGRA_3x_SOC)	+= powergate.o
obj-$(CONFIG_ARCH_TEGRA_3x_SOC)	+= latency-allowance.o

obj-$(CONFIG_DEBUG_FS) += debugfs.o

include $(srctree)/compat26/Makefile-pdk.inc
