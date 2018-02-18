GCOV_PROFILE := y

subdir-ccflags-y := -Werror
subdir-ccflags-y += -I$(srctree)/drivers/platform/tegra/include

EXTRA_CFLAGS += -I../nvhost/drivers/video/tegra/host
EXTRA_CFLAGS += -Idrivers/video/tegra/host

ccflags-$(CONFIG_ADF_TEGRA) += -Idrivers/staging/android

ifeq ($(CONFIG_ARM64),y)
subdir-ccflags-y += -Iarch/arm/mach-tegra/include
ccflags          += -Iarch/arm/mach-tegra/include
endif

obj-$(CONFIG_TEGRA_NVMAP)  += $(call tegra-path,nvmap/,drivers/video/tegra/nvmap/)
ifneq ($(wildcard $(srctree)/../nvhost/),)
obj-$(CONFIG_TEGRA_GRHOST) += $(call tegra-path,nvhost/,drivers/video/tegra/host/)
endif
ifneq ($(wildcard $(srctree)/../display/),)
obj-$(CONFIG_TEGRA_DC)     += $(call tegra-path,display,drivers/video/tegra/dc/)
endif
obj-$(CONFIG_FB_TEGRA)     += $(call tegra-path,display,drivers/video/tegra/fb.o)
obj-$(CONFIG_ADF_TEGRA)    += tegra_adf.o
obj-$(CONFIG_TEGRA_CAMERA_PLATFORM) += camera/
obj-$(CONFIG_TEGRA_GR_VIRTUALIZATION) += virt/
