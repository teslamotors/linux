GCOV_PROFILE := y

subdir-ccflags-y := -Werror
ccflags-y += -I$(src)/../tegra-alt
ccflags-y += -I$(src)/../codecs

# Tegra platform Support
snd-soc-tegra-virt-alt-ivc-objs := tegra_virt_alt_ivc.o

obj-$(CONFIG_SND_SOC_TEGRA_VIRT_ALT) += snd-soc-tegra-virt-alt-ivc.o

snd-soc-tegra210-virt-alt-admaif-objs := tegra210_virt_alt_admaif.o
snd-soc-tegra-virt-t210ref-pcm-objs := tegra_virt_t210ref_pcm.o
obj-$(CONFIG_SND_SOC_TEGRA_VIRT_T210REF_PCM) += snd-soc-tegra210-virt-alt-admaif.o
obj-$(CONFIG_SND_SOC_TEGRA_VIRT_T210REF_PCM) += snd-soc-tegra-virt-t210ref-pcm.o
