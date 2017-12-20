#
# Makefile for the video capture/playback device drivers.
#
obj-y				+= avp/
obj-$(CONFIG_TEGRA_MEDIASERVER)	+= mediaserver/
obj-$(CONFIG_TEGRA_NVAVP)	+= nvavp/
obj-$(CONFIG_TEGRA_CAMERA)	+= tegra_camera.o
obj-$(CONFIG_VIDEO_AR0832)	+= ar0832_main.o
obj-$(CONFIG_VIDEO_OV5650)	+= ov5650.o
obj-$(CONFIG_VIDEO_OV14810)	+= ov14810.o
obj-$(CONFIG_VIDEO_OV9726)	+= ov9726.o
obj-$(CONFIG_VIDEO_OV2710)	+= ov2710.o
obj-$(CONFIG_VIDEO_SOC380)	+= soc380.o
obj-$(CONFIG_TORCH_SSL3250A)	+= ssl3250a.o
obj-$(CONFIG_TORCH_TPS61050)	+= tps61050.o
obj-$(CONFIG_VIDEO_SH532U)	+= sh532u.o
obj-$(CONFIG_VIDEO_AD5820)	+= ad5820.o

include $(srctree)/compat26/Makefile.inc
