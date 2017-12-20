# compat26/tegra26/Makefile
#
# Tegra specific 2.6 compatilibty functions (from original 2.6 source code)

obj-y	+= devices.o
obj-y	+= clock.o
obj-y	+= dvfs.o
obj-y	+= pageattr.o

obj-y	+= nv/

obj-$(CONFIG_ARCH_TEGRA_2x_SOC)		+= board-p852-panel.o

include $(srctree)/compat26/Makefile.inc
