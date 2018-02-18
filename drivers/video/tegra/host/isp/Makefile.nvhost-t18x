GCOV_PROFILE := y
ccflags-y += -I../nvhost/drivers/video/tegra/host
ccflags-y += -Idrivers/video/tegra/host
ccflags-y += -Idrivers/video/tegra/camera
ccflags-y += -Werror

nvhost-isp-t186-objs  = \
		isp_isr_v2.o

obj-$(CONFIG_TEGRA_GRHOST_ISP) += nvhost-isp-t186.o
