tar_src_unfiltered = $(wildcard *.c) $(wildcard */*.c)
tar_src_local = $(filter-out cbc-ldisc.mod.c,$(tar_src_unfiltered)) $(wildcard *.h) $(wildcard */*.h) Makefile Kbuild
tar_src = $(addprefix drivers/cbc/,$(tar_src_local))
tar_src += include/linux/cbc-core.h
tar_src += include/uapi/linux/cbc-core.h

KDIR ?= /lib/modules/`uname -r`/build

default:
	$(MAKE) -C $(KDIR) M=$$PWD CONFIG_CBC_LDISC=m EXTRA_CFLAGS=-I$$PWD/../../include

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

tar:
	tar cf cbc_kmod.tar -C ../../ $(tar_src)
