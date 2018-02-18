GCOV_PROFILE := y

subdir-ccflags-y += -Werror
obj-$(CONFIG_SYSEDP_FRAMEWORK) += sysedp.o
obj-$(CONFIG_SYSEDP_FRAMEWORK) += sysedp_sysfs.o
obj-$(CONFIG_SYSEDP_FRAMEWORK) += sysedp_dynamic_capping.o
obj-$(CONFIG_SYSEDP_FRAMEWORK) += sysedp_reactive_capping.o
obj-$(CONFIG_SYSEDP_FRAMEWORK) += sysedp_batmon_calc.o
obj-$(CONFIG_SYSEDP_FRAMEWORK) += sysedp_modem.o
ifdef CONFIG_DEBUG_FS
obj-$(CONFIG_SYSEDP_FRAMEWORK) += sysedp_debug.o
endif
