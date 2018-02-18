#
# nvdumper code.
#

GCOV_PROFILE := y

obj-${CONFIG_TEGRA_NVDUMPER}            += nvdumper.o
obj-${CONFIG_TEGRA_NVDUMPER}            += nvdumper_regdump.o
obj-${CONFIG_TEGRA_NVDUMPER}            += nvdumper-footprint.o
obj-${CONFIG_TEGRA_NVDUMPER}            += nvdumper_config.o

ifneq ($(CONFIG_ARM64),)

obj-${CONFIG_TEGRA_NVDUMPER}            += nvdumper_reg_t13x.o

else

obj-${CONFIG_TEGRA_NVDUMPER}            += nvdumper_reg_t12x.o

endif
