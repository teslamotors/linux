ccflags-y += -I../t18x/drivers/gpu/host1x

ifeq ($(CONFIG_ARCH_TEGRA_18x_SOC),y)
obj-$(CONFIG_TEGRA_HOST1X) += $(call tegra-path,t18x/,drivers/gpu/host1x/)
endif

host1x-y = \
	bus.o \
	syncpt.o \
	dev.o \
	intr.o \
	cdma.o \
	channel.o \
	job.o \
	debug.o \
	mipi.o \
	hw/host1x01.o \
	hw/host1x02.o \
	hw/host1x04.o

host1x-$(CONFIG_SYNC) += fence.o

obj-$(CONFIG_TEGRA_HOST1X) += host1x.o
