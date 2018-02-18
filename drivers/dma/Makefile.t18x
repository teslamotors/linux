ccflags-$(CONFIG_DMADEVICES_DEBUG)  := -DDEBUG
ccflags-$(CONFIG_DMADEVICES_VDEBUG) += -DVERBOSE_DEBUG

ccflags-y += -Werror
ccflags-$(CONFIG_DMADEVICES) += -I$(srctree)/drivers/dma
ccflags-$(CONFIG_DMADEVICES) += -I$(srctree)
ccflags-$(CONFIG_DMADEVICES) += -I$(srctree)/../t18x

obj-$(CONFIG_TEGRA186_GPC_DMA) += tegra186-gpc-dma.o
obj-$(CONFIG_TEGRA186_ADMA) += tegra18x-adma.o
