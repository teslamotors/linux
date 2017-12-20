
# add tegra 2.6 drivers
obj-y += drivers/media/tegra/

# objects from tegra26
obj-y += tegra26/iovmm.o
obj-y += tegra26/iovmm-gart.o

# forward port of the 2.6 tegra video driver
obj-$(CONFIG_FB_TEGRA_26)		  += drivers/video/tegra/

include $(srctree)/compat26/Makefile.inc

