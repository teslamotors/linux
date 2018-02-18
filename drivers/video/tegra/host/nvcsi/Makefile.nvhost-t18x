GCOV_PROFILE := y
ccflags-y += -I../nvhost/drivers/video/tegra/host
ccflags-y += -Idrivers/video/tegra/host
ccflags-y += -Idrivers/video/tegra/camera
ccflags-y += -Idrivers/media/platform/tegra/
ccflags-y += -Idrivers/media/platform/tegra/csi
ccflags-y += -I../nvhost-t18x/drivers/video/tegra/host
ccflags-y += -Werror

nvhost-nvcsi-objs  = \
		nvcsi.o

obj-$(CONFIG_TEGRA_GRHOST) += nvhost-nvcsi.o
