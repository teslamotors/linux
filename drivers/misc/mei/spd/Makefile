#
# Makefile for the Storage Proxy device driver.
#

obj-$(CONFIG_INTEL_MEI_SPD) += mei_spd.o
mei_spd-objs := main.o
mei_spd-objs += cmd.o
mei_spd-objs += gpp.o
mei_spd-$(CONFIG_DEBUG_FS) += debugfs.o

ccflags-y += -D__CHECK_ENDIAN__
