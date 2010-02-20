KERNEL_DIR = /lib/modules/$(shell uname -r)/build

cryptodev-objs = cryptodev_main.o cryptodev_cipher.o

obj-m += cryptodev.o

build:
	make -C $(KERNEL_DIR) SUBDIRS=`pwd` modules

install:
	make -C $(KERNEL_DIR) SUBDIRS=`pwd` modules_install
	@echo "Installing cryptodev.h in /usr/include/crypto ..."
	@install -D cryptodev.h /usr/include/crypto/cryptodev.h

clean:
	make -C $(KERNEL_DIR) SUBDIRS=`pwd` clean
	rm -f $(hostprogs)

check:
	KERNEL_DIR=$(KERNEL_DIR) make -C examples check
