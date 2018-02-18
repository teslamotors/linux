GCOV_PROFILE := y

# Reference the mach-tegra from arch/arm for now
ccflags-y += -I$(srctree)/arch/arm/mach-tegra/include \
             -I$(srctree)/arch/arm/mach-tegra \
             -I$(srctree)/../display/drivers/video/tegra/dc/panel
asflags-y += -I$(srctree)/arch/arm/mach-tegra/include \
             -I$(srctree)/arch/arm/mach-tegra
subdir-ccflags-y := -Werror

# Assemblies
obj-y += reset-handler.o trampoline.o

ifeq ($(CONFIG_SMP),y)
obj-$(CONFIG_ARCH_TEGRA_13x_SOC)      += platsmp.o
endif

obj-${CONFIG_MACH_LOKI}               += board-loki-power.o
obj-${CONFIG_MACH_LOKI}               += board-loki-memory.o
obj-${CONFIG_MACH_LOKI}               += board-loki-sdhci.o
obj-${CONFIG_MACH_LOKI}               += board-loki-sensors.o
obj-${CONFIG_MACH_LOKI}               += board-loki-kbc.o
obj-${CONFIG_MACH_LOKI}               += board-loki-panel.o

obj-${CONFIG_MACH_T132REF}              += board-t132ref.o
obj-${CONFIG_MACH_T132REF}              += board-t132ref-laguna-power.o
obj-${CONFIG_MACH_T132REF}              += board-t132ref-memory.o
obj-${CONFIG_MACH_T132REF}              += board-t132ref-sensors.o
obj-${CONFIG_MACH_T132REF}              += board-t132ref-norrin-power.o
obj-${CONFIG_MACH_T132REF}              += board-t132ref-norrin-memory.o
obj-${CONFIG_MACH_T132REF}              += board-t132ref-power.o
obj-${CONFIG_MACH_T132REF}              += board-t132ref-sdhci.o
obj-${CONFIG_MACH_T132REF}              += board-t132ref-tn8-sysedp.o

obj-y                                   += board-touch-maxim_sti-spi.o

obj-${CONFIG_ARCH_TEGRA_21x_SOC}      += board-t210ref.o
obj-${CONFIG_ARCH_TEGRA_21x_SOC}      += board-t210ref-camera.o
obj-${CONFIG_ARCH_TEGRA_21x_SOC}      += tegra21_init.o

ifeq ($(CONFIG_PM_SLEEP),y)
obj-y                                 += pm.o
obj-y                                 += sleep.o
endif

# To override the "-march=armv7-xxx" in arm/mach-tegra/Makefile
AFLAGS_sleep.o :=
