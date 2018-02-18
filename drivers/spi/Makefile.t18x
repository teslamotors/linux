GCOV_PROFILE := y

ccflags-y += -Werror
ccflags-$(CONFIG_SPI_DEBUG) := -DDEBUG

obj-$(CONFIG_QSPI_TEGRA186)		+= spi-tegra186-qspi.o
obj-$(CONFIG_SPI_TEGRA186_AON)		+= spi-tegra-aon.o

