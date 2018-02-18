GCOV_PROFILE := y
subdir-ccflags-y := -Werror
EXTRA_CFLAGS += -I../nvhost/drivers/video/tegra/host
EXTRA_CFLAGS += -Idrivers/video/tegra/host
ccflags-$(CONFIG_ADF_TEGRA) += -Idrivers/video/tegra -Idrivers/staging/android
ifeq ($(CONFIG_ARCH_TEGRA_18x_SOC),y)
ccflags-y += -I$(srctree)/../t18x/drivers/video/tegra/dc
ccflags-y += -I$(srctree)/drivers/video/tegra/dc
endif
obj-y += dc.o mode.o clock.o window.o
ifneq ($(CONFIG_TEGRA_NVDISPLAY),y)
obj-y += bandwidth.o
endif
obj-$(CONFIG_TEGRA_CSC) += csc.o
obj-$(CONFIG_TEGRA_LUT) += lut.o
obj-y += cursor.o
obj-y += rgb.o
obj-y += of_dc.o
obj-$(CONFIG_TEGRA_HDMI) += hdmi.o
obj-$(CONFIG_TEGRA_HDMI) += hdmi_state_machine.o
obj-$(CONFIG_TEGRA_HDMI2FPD_DS90UH949) += hdmi2fpd_ds90uh949.o
#obj-y += dsi2lvds.o
obj-$(CONFIG_TEGRA_DSI2EDP_TC358767) += tc358767_dsi2edp.o
obj-$(CONFIG_TEGRA_DSI2EDP_SN65DSI86) += sn65dsi86_dsi2edp.o
obj-$(CONFIG_TEGRA_DSI2LVDS_SN65DSI85) += sn65dsi85_dsi2lvds.o
obj-$(CONFIG_TEGRA_LVDS2FPDL_DS90UB947) += ds90ub947-q1.o
ifeq ($(CONFIG_ARCH_TEGRA_21x_SOC)$(CONFIG_ARCH_TEGRA_18x_SOC), y)
ccflags-$(CONFIG_TEGRA_MIPI_CAL) +=-DCOMMON_MIPICAL_SUPPORTED\
				-Idrivers/media/platform/tegra
else
obj-$(CONFIG_TEGRA_MIPI_CAL) += mipi_cal.o
endif

obj-$(CONFIG_TEGRA_NVHDCP) += nvhdcp.o
ifneq ($(CONFIG_ARCH_TEGRA_18x_SOC),y)
obj-$(CONFIG_TEGRA_HDMIHDCP) += hdmihdcp.o
obj-$(CONFIG_TEGRA_HDMIHDCP) += nvhdcp_hdcp22_methods.o
ccflags-$(CONFIG_TEGRA_HDMIHDCP) += -DCONFIG_HDCP
endif
ifeq ($(CONFIG_ARCH_TEGRA_18x_SOC),y)
ifeq ($(CONFIG_TRUSTED_LITTLE_KERNEL),y)
obj-$(CONFIG_TEGRA_HDMIHDCP) += hdmihdcp.o
obj-$(CONFIG_TEGRA_HDMIHDCP) += nvhdcp_hdcp22_methods.o
ccflags-$(CONFIG_TEGRA_HDMIHDCP) += -DCONFIG_HDCP
endif
endif
#endif
obj-y += edid.o edid_quirks.o
ifneq ($(CONFIG_TEGRA_NVDISPLAY),y)
obj-$(CONFIG_TEGRA_NVSD) += nvsd.o
endif
obj-y += dsi.o
obj-$(CONFIG_DEBUG_FS) += dsi_debug.o
obj-$(CONFIG_DEBUG_FS) += tegra_dsi_csi_test.o
obj-y += dc_sysfs.o
obj-y += dc_config.o
obj-y += sor.o
obj-y += dpaux.o
obj-$(CONFIG_DEBUG_FS) += dp_debug.o
obj-$(CONFIG_TEGRA_DP) += dp.o
obj-$(CONFIG_TEGRA_DP) += dp_lt.o
obj-$(CONFIG_TEGRA_DP) += dp_auto.o
obj-$(CONFIG_TEGRA_DP) += hpd.o
obj-$(CONFIG_TEGRA_LVDS) += lvds.o
obj-$(CONFIG_TEGRA_NVSR) += nvsr.o
obj-$(CONFIG_TEGRA_DC_EXTENSIONS) += ext/
obj-$(CONFIG_TEGRA_HDMI2_0) += hdmi2.0.o
obj-$(CONFIG_TEGRA_HDMI2GMSL_MAX929x) += max929x_hdmi2gmsl.o
obj-$(CONFIG_TEGRA_HDA_DC) += hda_dc.o
obj-$(CONFIG_TEGRA_DC_FAKE_PANEL_SUPPORT) += fake_panel.o null_or.o
obj-$(CONFIG_TEGRA_VRR) += vrr.o
obj-$(CONFIG_TEGRA_NVDISPLAY) += dsi_padctrl.o
obj-$(CONFIG_TEGRA_HDMIVRR) += hdmivrr.o
ifndef CONFIG_ARCH_TEGRA_210_SOC
obj-y += panel/
endif
