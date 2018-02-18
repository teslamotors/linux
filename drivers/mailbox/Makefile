# Generic MAILBOX API
GCOV_PROFILE_tegra-xusb-mailbox.o := y

obj-$(CONFIG_MAILBOX)		+= mailbox.o

obj-$(CONFIG_PL320_MBOX)	+= pl320-ipc.o

obj-$(CONFIG_OMAP2PLUS_MBOX)	+= omap-mailbox.o

CFLAGS_tegra-xusb-mailbox.o += -Werror
obj-$(CONFIG_TEGRA_XUSB_MBOX)	+= tegra-xusb-mailbox.o
