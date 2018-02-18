ccflags-$(CONFIG_DRM_TEGRA_DEBUG) += -DDEBUG

ccflags-y += -I../t18x/drivers/gpu/drm/tegra

ifeq ($(CONFIG_ARCH_TEGRA_18x_SOC),y)
obj-$(CONFIG_TEGRA_HOST1X) += $(call tegra-path,t18x/,drivers/gpu/drm/tegra/)
endif

tegra-drm-y := \
	drm.o \
	gem.o \
	fb.o \
	dc.o \
	output.o \
	rgb.o \
	hdmi.o \
	mipi-phy.o \
	dsi.o \
	sor.o \
	dpaux.o \
	gr2d.o \
	gr3d.o \
	falcon.o \
	vic.o \
	nvdec.o \
	nvjpg.o \
	nvenc.o \
	tsec.o \
	isp.o \
	vi.o

obj-$(CONFIG_DRM_TEGRA) += tegra-drm.o
