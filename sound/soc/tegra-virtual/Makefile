GCOV_PROFILE := y

subdir-ccflags-y := -Werror
ccflags-y += -I$(src)/../tegra-alt
ccflags-y += -I$(src)/../codecs

# Tegra platform Support
snd-soc-tegra124-virt-slave-apbif-objs := tegra124_virt_apbif_slave.o
snd-soc-tegra-virt-utils-objs := tegra_virt_utils.o

obj-$(CONFIG_SND_SOC_TEGRA124_VIRT_APBIF_SLAVE) += snd-soc-tegra124-virt-slave-apbif.o
obj-$(CONFIG_SND_SOC_TEGRA124_VIRT_APBIF_SLAVE) += snd-soc-tegra-virt-utils.o

# Tegra machine Support for slave
snd-soc-tegra-virt-vcm30t124-slave-objs := tegra_virt_vcm30t124_slave.o
obj-$(CONFIG_SND_SOC_TEGRA_VIRT_VCM30T124_SLAVE) += snd-soc-tegra-virt-vcm30t124-slave.o

