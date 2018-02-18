GCOV_PROFILE := y
ccflags-y += -I../nvhost/drivers/video/tegra/host
ccflags-y += -I../nvhost-t18x/drivers/video/tegra/host
ccflags-y += -Idrivers/devfreq
ccflags-y += -Wno-multichar

subdir-ccflags-y := -Werror

obj-$(CONFIG_TEGRA_GRHOST) += iommu_context_dev.o

obj-$(CONFIG_TEGRA_GRHOST) += t186/
obj-$(CONFIG_TEGRA_GRHOST_NVCSI) += nvcsi/
obj-$(CONFIG_TEGRA_GRHOST_ISP) += isp/
