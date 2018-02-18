GCOV_PROFILE := y

subdir-ccflags-y := -Werror

subdir-ccflags-y += -I$(srctree)/../t18x
subdir-ccflags-y += -I$(srctree)

# Tegra platform Support
snd-soc-tegra-alt-pcm-objs := tegra_pcm_alt.o
CFLAGS_tegra_pcm_alt.o += -Wno-shift-count-overflow
snd-soc-tegra-alt-utils-objs := tegra_asoc_utils_alt.o
snd-soc-tegra-alt-hwdep-objs := tegra_asoc_hwdep_alt.o
snd-soc-tegra-alt-machine-objs := tegra_asoc_machine_alt.o
snd-soc-tegra-alt-isomgr-objs := tegra_isomgr_bw_alt.o
snd-soc-tegra30-alt-apbif-objs := tegra30_apbif_alt.o
snd-soc-tegra30-alt-xbar-objs := tegra30_xbar_alt.o
snd-soc-tegra30-alt-i2s-objs := tegra30_i2s_alt.o
snd-soc-tegra30-alt-dam-objs := tegra30_dam_alt.o
snd-soc-tegra30-alt-spdif-objs := tegra30_spdif_alt.o
snd-soc-tegra114-alt-amx-objs := tegra114_amx_alt.o
snd-soc-tegra114-alt-adx-objs := tegra114_adx_alt.o
snd-soc-tegra124-alt-afc-objs := tegra124_afc_alt.o
snd-soc-tegra210-alt-admaif-objs := tegra210_admaif_alt.o
snd-soc-tegra210-alt-xbar-objs := tegra210_xbar_alt.o
snd-soc-tegra210-alt-i2s-objs := tegra210_i2s_alt.o
snd-soc-tegra210-alt-dmic-objs := tegra210_dmic_alt.o
snd-soc-tegra210-alt-amx-objs := tegra210_amx_alt.o
snd-soc-tegra210-alt-adx-objs := tegra210_adx_alt.o
snd-soc-tegra210-alt-mixer-objs := tegra210_mixer_alt.o
snd-soc-tegra210-alt-sfc-objs := tegra210_sfc_alt.o
snd-soc-tegra210-alt-afc-objs := tegra210_afc_alt.o
snd-soc-tegra210-alt-mvc-objs := tegra210_mvc_alt.o
snd-soc-tegra210-alt-iqc-objs := tegra210_iqc_alt.o
snd-soc-tegra210-alt-spdif-objs := tegra210_spdif_alt.o
snd-soc-tegra210-alt-ope-objs := tegra210_ope_alt.o
snd-soc-tegra210-alt-peq-objs := tegra210_peq_alt.o
snd-soc-tegra210-alt-mbdrc-objs := tegra210_mbdrc_alt.o
snd-soc-tegra210-alt-adsp-objs := tegra210_adsp_alt.o
snd-soc-tegra210-alt-fpga-clock-objs := ahub_unit_fpga_clock.o

obj-$(CONFIG_SND_SOC_TEGRA_ALT) += snd-soc-tegra-alt-utils.o
obj-$(CONFIG_SND_SOC_TEGRA_ALT) += snd-soc-tegra-alt-pcm.o
obj-$(CONFIG_SND_SOC_TEGRA_ALT) += snd-soc-tegra210-alt-fpga-clock.o
obj-$(CONFIG_SND_SOC_TEGRA_ALT) += snd-soc-tegra-alt-isomgr.o
obj-$(CONFIG_SND_SOC_TEGRA_ASOC_HWDEP_ALT) += snd-soc-tegra-alt-hwdep.o
obj-$(CONFIG_SND_SOC_TEGRA_ASOC_MACHINE_ALT) += snd-soc-tegra-alt-machine.o
obj-$(CONFIG_SND_SOC_TEGRA30_APBIF_ALT) += snd-soc-tegra30-alt-apbif.o
obj-$(CONFIG_SND_SOC_TEGRA30_XBAR_ALT) += snd-soc-tegra30-alt-xbar.o
obj-$(CONFIG_SND_SOC_TEGRA30_I2S_ALT) += snd-soc-tegra30-alt-i2s.o
obj-$(CONFIG_SND_SOC_TEGRA30_DAM_ALT) += snd-soc-tegra30-alt-dam.o
obj-$(CONFIG_SND_SOC_TEGRA30_SPDIF_ALT) += snd-soc-tegra30-alt-spdif.o
obj-$(CONFIG_SND_SOC_TEGRA114_AMX_ALT) += snd-soc-tegra114-alt-amx.o
obj-$(CONFIG_SND_SOC_TEGRA114_ADX_ALT) += snd-soc-tegra114-alt-adx.o
obj-$(CONFIG_SND_SOC_TEGRA124_AFC_ALT) += snd-soc-tegra124-alt-afc.o
obj-$(CONFIG_SND_SOC_TEGRA210_ADMAIF_ALT) += snd-soc-tegra210-alt-admaif.o
obj-$(CONFIG_SND_SOC_TEGRA210_XBAR_ALT) += snd-soc-tegra210-alt-xbar.o
obj-$(CONFIG_SND_SOC_TEGRA210_I2S_ALT) += snd-soc-tegra210-alt-i2s.o
obj-$(CONFIG_SND_SOC_TEGRA210_DMIC_ALT) += snd-soc-tegra210-alt-dmic.o
obj-$(CONFIG_SND_SOC_TEGRA210_AMX_ALT) += snd-soc-tegra210-alt-amx.o
obj-$(CONFIG_SND_SOC_TEGRA210_ADX_ALT) += snd-soc-tegra210-alt-adx.o
obj-$(CONFIG_SND_SOC_TEGRA210_MIXER_ALT) += snd-soc-tegra210-alt-mixer.o
obj-$(CONFIG_SND_SOC_TEGRA210_SFC_ALT) += snd-soc-tegra210-alt-sfc.o
obj-$(CONFIG_SND_SOC_TEGRA210_AFC_ALT) += snd-soc-tegra210-alt-afc.o
obj-$(CONFIG_SND_SOC_TEGRA210_MVC_ALT) += snd-soc-tegra210-alt-mvc.o
obj-$(CONFIG_SND_SOC_TEGRA210_IQC_ALT) += snd-soc-tegra210-alt-iqc.o
obj-$(CONFIG_SND_SOC_TEGRA210_SPDIF_ALT) += snd-soc-tegra210-alt-spdif.o
obj-$(CONFIG_SND_SOC_TEGRA210_OPE_ALT) += snd-soc-tegra210-alt-ope.o
obj-$(CONFIG_SND_SOC_TEGRA210_OPE_ALT) += snd-soc-tegra210-alt-peq.o
obj-$(CONFIG_SND_SOC_TEGRA210_OPE_ALT) += snd-soc-tegra210-alt-mbdrc.o
obj-$(CONFIG_SND_SOC_TEGRA210_ADSP_ALT) += snd-soc-tegra210-alt-adsp.o

# Tegra machine Support
snd-soc-tegra-alt-vcm30t124-objs := tegra_vcm30t124_alt.o
snd-soc-tegra-alt-grenada-objs := tegra_grenada_alt.o
snd-soc-tegra-alt-p1889ref-objs := tegra_p1889ref_alt.o
snd-soc-tegra-alt-t210ref-mobile-objs := tegra_t210ref_mobile_rt565x_alt.o
snd-soc-tegra-alt-t210ref-mobile-es755-objs := tegra_t210ref_mobile_es755_alt.o
snd-soc-tegra-alt-t210ref-objs := tegra_t210ref_alt.o
snd-soc-tegra-alt-t210ref-p2382-objs := tegra_t210ref_p2382_alt.o \
						tegra_maui_alt.o

obj-$(CONFIG_SND_SOC_TEGRA_VCM30T124_ALT) += snd-soc-tegra-alt-vcm30t124.o
obj-$(CONFIG_SND_SOC_TEGRA_VCM30T124_ALT) += snd-soc-tegra-alt-machine.o
obj-$(CONFIG_SND_SOC_TEGRA_P1889REF_ALT) += snd-soc-tegra-alt-p1889ref.o
obj-$(CONFIG_SND_SOC_TEGRA_P1889REF_ALT) += snd-soc-tegra-alt-machine.o
obj-$(CONFIG_SND_SOC_TEGRA_GRENADA_ALT) += snd-soc-tegra-alt-grenada.o
obj-$(CONFIG_SND_SOC_TEGRA_GRENADA_ALT) += snd-soc-tegra-alt-machine.o
obj-$(CONFIG_SND_SOC_TEGRA_T210REF_MOBILE_ALT) += snd-soc-tegra-alt-t210ref-mobile.o
obj-$(CONFIG_SND_SOC_TEGRA_T210REF_MOBILE_ES755_ALT) += snd-soc-tegra-alt-t210ref-mobile-es755.o
obj-$(CONFIG_SND_SOC_TEGRA_T210REF_MOBILE_ES755_ALT) += snd-soc-tegra-alt-machine.o
obj-$(CONFIG_SND_SOC_TEGRA_T210REF_ALT) += snd-soc-tegra-alt-t210ref.o
obj-$(CONFIG_SND_SOC_TEGRA_T210REF_ALT) += snd-soc-tegra-alt-machine.o
obj-$(CONFIG_SND_SOC_TEGRA_T210REF_P2382_ALT) += snd-soc-tegra-alt-t210ref-p2382.o
obj-$(CONFIG_SND_SOC_TEGRA_T210REF_P2382_ALT) += snd-soc-tegra-alt-machine.o
