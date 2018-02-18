GCOV_PROFILE := y

ccflags-y += -I$(srctree)/../nvhost-t18x/drivers/video/tegra/host
ccflags-y += -I$(srctree)/drivers/video/tegra/host
ccflags-y += -I$(srctree)/../nvhost/drivers/video/tegra/host
ccflags-y += -I$(srctree)/../nvhost/include
ccflags-y += -I$(srctree)/include
obj-$(CONFIG_CRYPTO_DEV_TEGRA_SE_USE_HOST1X_INTERFACE) += tegra-se-nvhost.o
obj-$(CONFIG_CRYPTO_DEV_TEGRA_VIRTUAL_SE_INTERFACE) += tegra-hv-vse.o
obj-$(CONFIG_CRYPTO_DEV_TEGRA_ELLIPTIC_SE) += elliptic/
