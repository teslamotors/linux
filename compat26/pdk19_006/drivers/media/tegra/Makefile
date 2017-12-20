#
# Makefile for the video capture/playback device drivers.
#
obj-y				+= avp/
obj-$(CONFIG_TEGRA_MEDIASERVER)	+= mediaserver/
obj-$(CONFIG_TEGRA_CAMERA)	+= tegra_camera.o
obj-$(CONFIG_TEGRA_ITU656)	+= tegra_itu656.o
obj-$(CONFIG_VIDEO_OV5650)	+= ov5650.o

include $(srctree)/compat26/Makefile.inc
