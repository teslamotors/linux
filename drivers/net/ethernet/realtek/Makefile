#
# Makefile for the Realtek network device drivers.
# r8169: Realtek upstream driver
# r8168: Realtek vendor driver
#

obj-$(CONFIG_8139CP) += 8139cp.o
obj-$(CONFIG_8139TOO) += 8139too.o
obj-$(CONFIG_ATP) += atp.o
obj-$(CONFIG_R8169) += r8169.o
obj-$(CONFIG_R8168) += r8168.o
r8168-y := r8168_n.o r8168_asf.o rtl_eeprom.o rtltool.o pgdrv.o
