# Makefile for Tegra SoC camera driver

tegra_camera-objs += common.o

ifeq ($(CONFIG_VIDEO_TEGRA_VI2),y)
	tegra_camera-objs += vi2.o
endif
ifeq ($(CONFIG_VIDEO_TEGRA_VI_BYPASS),y)
	tegra_camera-objs += vi_bypass.o
endif

obj-$(CONFIG_VIDEO_TEGRA) += tegra_camera.o

ccflags-y += -I$(srctree)/../nvhost/drivers/video/tegra/host
