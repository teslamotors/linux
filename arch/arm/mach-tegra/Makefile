obj-y                                   += board-info.o
obj-y                                   += prod-info.o
obj-y                                   += serial-info.o
obj-y                                   += common.o
obj-y                                   += apbio.o
obj-y                                   += io.o
obj-y                                   += irq.o legacy_irq.o
obj-y                                   += syncpt.o
obj-y                                   += clock.o
obj-y                                   += dvfs.o
obj-y                                   += timer.o
obj-y                                   += gpio.o
obj-y                                   += pinmux.o
obj-y                                   += devices.o
obj-y                                   += delay.o
obj-y                                   += powergate.o
obj-y                                   += suspend.o
obj-y                                   += fuse.o
obj-y                                   += kfuse.o
ifeq ($(CONFIG_TEGRA_ALSA),y)
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += tegra2_i2s.o
else
ifeq ($(CONFIG_EMBEDDED_TEGRA_ALSA),y)
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += tegra2_i2s.o
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += tegra2_i2s_tdm.o
else
obj-y                                   += tegra_i2s_audio.o
endif
endif
obj-y                                   += tegra_spdif_audio.o
obj-y                                   += tegra_das.o
obj-y                                   += mc.o
obj-$(CONFIG_USB_SUPPORT)               += usb_phy.o
obj-$(CONFIG_FIQ)                       += fiq.o
obj-$(CONFIG_TEGRA_FIQ_DEBUGGER)        += tegra_fiq_debugger.o
obj-$(CONFIG_TEGRA_PWM)                 += pwm.o
obj-$(CONFIG_TEGRA_ARB_SEMAPHORE)	+= arb_sema.o

obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += clock.o
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += tegra2_clocks.o
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += tegra2_dvfs.o
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += tegra2_fuse.o
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += tegra2_speedo.o
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += suspend-t2.o
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)		+= tegra2_save.o
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)		+= tegra2_emc.o
obj-$(CONFIG_CPU_V7)			+= cortex-a9.o

obj-$(CONFIG_ARCH_TEGRA_2x_SOC)		+= pinmux-t2-tables.o
ifeq ($(CONFIG_SMP),y)
obj-y                                   += localtimer.o platsmp.o headsmp.o
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += headsmp-t2.o
endif

obj-$(CONFIG_TEGRA_SYSTEM_DMA)          += dma.o
obj-$(CONFIG_CPU_FREQ)                  += cpu-tegra.o
obj-$(CONFIG_CPU_IDLE)                  += cpuidle.o
obj-$(CONFIG_TEGRA_IOVMM)               += iovmm.o
obj-$(CONFIG_TEGRA_IOVMM_GART)          += iovmm-gart.o

obj-y                                   += nv/

obj-${CONFIG_MACH_HARMONY}              += board-harmony.o
obj-${CONFIG_MACH_HARMONY}              += board-harmony-pinmux.o
obj-${CONFIG_MACH_HARMONY}              += board-harmony-panel.o
obj-${CONFIG_MACH_HARMONY}              += board-harmony-sdhci.o
obj-${CONFIG_MACH_HARMONY}              += board-harmony-power.o
obj-${CONFIG_MACH_HARMONY}              += tegra_rfkill_harmony.o

obj-${CONFIG_MACH_P852}                 += p852/

obj-${CONFIG_MACH_VENTANA}              += board-ventana.o
obj-${CONFIG_MACH_VENTANA}              += board-ventana-pinmux.o
obj-${CONFIG_MACH_VENTANA}              += board-ventana-sdhci.o
obj-${CONFIG_MACH_VENTANA}              += board-ventana-power.o
obj-${CONFIG_MACH_VENTANA}              += board-ventana-panel.o
obj-${CONFIG_MACH_VENTANA}              += board-ventana-sensors.o
obj-${CONFIG_MACH_VENTANA}              += board-ventana-kbc.o
obj-${CONFIG_MACH_VENTANA}              += board-ventana-memory.o

obj-${CONFIG_MACH_WHISTLER}             += board-whistler.o
obj-${CONFIG_MACH_WHISTLER}             += board-whistler-pinmux.o
obj-${CONFIG_MACH_WHISTLER}             += board-whistler-sdhci.o
obj-${CONFIG_MACH_WHISTLER}             += board-whistler-power.o
obj-${CONFIG_MACH_WHISTLER}             += board-whistler-panel.o
obj-${CONFIG_MACH_WHISTLER}             += board-whistler-sensors.o
obj-${CONFIG_MACH_WHISTLER}             += board-whistler-kbc.o

obj-$(CONFIG_TEGRA_PCI)                 += pcie.o
