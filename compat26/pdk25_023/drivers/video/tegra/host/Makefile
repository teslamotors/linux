nvhost-objs = \
	nvhost_acm.o \
	nvhost_syncpt.o \
	nvhost_cdma.o \
	nvhost_cpuaccess.o \
	nvhost_intr.o \
	nvhost_channel.o \
	dev.o \
	bus.o \
	3dctx_common.o \
	debug.o

obj-$(CONFIG_TEGRA_GRHOST) += t20/
obj-$(CONFIG_TEGRA_GRHOST) += t30/
obj-$(CONFIG_TEGRA_GRHOST) += nvhost.o

include $(srctree)/compat26/Makefile.inc
