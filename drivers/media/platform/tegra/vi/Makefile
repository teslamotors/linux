GCOV_PROFILE := y
ccflags-y += -I../nvhost/drivers/video/tegra/host
ccflags-y += -Idrivers/video/tegra/host
ccflags-y += -Idrivers/video/tegra/camera
ccflags-y += -Idrivers/media/platform/tegra/camera
ccflags-y += -Idrivers/media/platform/tegra/vi
ccflags-y += -Werror

ifeq ($(CONFIG_ARCH_TEGRA_18x_SOC),y)
ccflags-y += -I../nvhost-t18x/drivers/video/tegra/host/
ccflags-y += -I../t18x/drivers/video/tegra/host/
endif

ifneq (,$(filter $(CONFIG_VIDEO_TEGRA_VI),y m))
obj-y += vi_irq.o
obj-y += tegra_vi.o
endif

nvhost-vi-objs += vi.o
obj-$(CONFIG_VIDEO_TEGRA_VI) += nvhost-vi.o

ifeq ($(CONFIG_ARCH_TEGRA_18x_SOC),y)
obj-$(CONFIG_VIDEO_TEGRA_VI) += \
       $(call tegra-path,t18x,drivers/video/tegra/host/vi/vi_notify.o) \
       $(call tegra-path,t18x,drivers/video/tegra/host/vi/host_vi_notify.o) \
       $(call tegra-path,t18x,drivers/video/tegra/host/vi/vi4.o)
endif
