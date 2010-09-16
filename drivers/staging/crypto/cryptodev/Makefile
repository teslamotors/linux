KERNEL_DIR = /lib/modules/$(shell uname -r)/build
VERSION = 0.6

cryptodev-objs = cryptodev_main.o cryptodev_cipher.o

obj-m += cryptodev.o

build:
	@echo "#define VERSION \"$(VERSION)\"" > version.h
	make -C $(KERNEL_DIR) SUBDIRS=`pwd` modules

install:
	make -C $(KERNEL_DIR) SUBDIRS=`pwd` modules_install
	@echo "Installing cryptodev.h in /usr/include/crypto ..."
	@install -D cryptodev.h /usr/include/crypto/cryptodev.h

clean:
	make -C $(KERNEL_DIR) SUBDIRS=`pwd` clean
	rm -f $(hostprogs) *~
	KERNEL_DIR=$(KERNEL_DIR) make -C examples clean

check:
	KERNEL_DIR=$(KERNEL_DIR) make -C examples check

FILEBASE = cryptodev-linux-$(VERSION)
TMPDIR ?= /tmp
OUTPUT = $(FILEBASE).tar.gz

dist: clean
	@echo Packing
	@rm -f *.tar.gz
	@mkdir $(TMPDIR)/$(FILEBASE)
	@cp -ar extras examples Makefile *.c *.h README NEWS \
		AUTHORS COPYING $(TMPDIR)/$(FILEBASE)
	@rm -rf $(TMPDIR)/$(FILEBASE)/.git* $(TMPDIR)/$(FILEBASE)/releases $(TMPDIR)/$(FILEBASE)/scripts
	@tar -C /tmp -czf ./$(OUTPUT) $(FILEBASE)
	@rm -rf $(TMPDIR)/$(FILEBASE)
	@echo Signing $(OUTPUT)
	@gpg --output $(OUTPUT).sig -sb $(OUTPUT)
	@gpg --verify $(OUTPUT).sig $(OUTPUT)
	@mv $(OUTPUT) $(OUTPUT).sig releases/
