KERNEL_DIR := /lib/modules/$(shell uname -r)/build

obj-m += cryptodev.o

build:
	make -C $(KERNEL_DIR) SUBDIRS=`pwd` modules

install:
	make -C $(KERNEL_DIR) SUBDIRS=`pwd` modules_install
	@echo "Installing cryptodev.h in /usr/include/crypto ..."
	@install -D cryptodev.h /usr/include/crypto

clean:
	make -C $(KERNEL_DIR) SUBDIRS=`pwd` clean
