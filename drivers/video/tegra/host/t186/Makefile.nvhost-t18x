GCOV_PROFILE := y
ccflags-y += -I../nvhost/drivers/video/tegra/host
ccflags-y += -Idrivers/video/tegra/host
ccflags-y += -Idrivers/video/tegra/host/host1x
ccflags-y += -Idrivers/video/tegra/camera
ccflags-y += -Idrivers/media/platform/tegra/camera
ccflags-y += -I../t18x/drivers/video/tegra/host/t186
ccflags-y += -I../t18x/drivers/video/tegra/host
ccflags-y += -I../nvhost-t18x/drivers/video/tegra/host
ccflags-y += -Idrivers/video/tegra/host/t186
ccflags-y += -Wno-multichar
ccflags-y += -Werror

nvhost-t186-objs  = \
        t186.o

obj-$(CONFIG_TEGRA_GRHOST) += nvhost-t186.o
