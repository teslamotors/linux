#
# Since version 1.6 the asynchronous mode has been
# disabled by default. To re-enable it uncomment the
# corresponding CFLAG.
#
CRYPTODEV_CFLAGS ?= #-DENABLE_ASYNC
KBUILD_CFLAGS += -I$(src) $(CRYPTODEV_CFLAGS)
KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
VERSION = 1.6
PREFIX ?=

cryptodev-objs = ioctl.o main.o cryptlib.o authenc.o zc.o util.o

obj-m += cryptodev.o

build: version.h
	make -C $(KERNEL_DIR) SUBDIRS=`pwd` modules

version.h: Makefile
	@echo "#define VERSION \"$(VERSION)\"" > version.h

install: modules_install

modules_install:
	make -C $(KERNEL_DIR) SUBDIRS=`pwd` modules_install
	@echo "Installing cryptodev.h in $(PREFIX)/usr/include/crypto ..."
	@install -D crypto/cryptodev.h $(PREFIX)/usr/include/crypto/cryptodev.h

clean:
	make -C $(KERNEL_DIR) SUBDIRS=`pwd` clean
	rm -f $(hostprogs) *~
	CFLAGS=$(CRYPTODEV_CFLAGS) KERNEL_DIR=$(KERNEL_DIR) make -C tests clean

check:
	CFLAGS=$(CRYPTODEV_CFLAGS) KERNEL_DIR=$(KERNEL_DIR) make -C tests check

CPOPTS =
ifneq (${SHOW_TYPES},)
CPOPTS += --show-types
endif
ifneq (${IGNORE_TYPES},)
CPOPTS += --ignore ${IGNORE_TYPES}
endif

checkpatch:
	$(KERNEL_DIR)/scripts/checkpatch.pl ${CPOPTS} --file *.c *.h

FILEBASE = cryptodev-linux-$(VERSION)
TMPDIR ?= /tmp
OUTPUT = $(FILEBASE).tar.gz

dist: clean
	@echo Packing
	@rm -f *.tar.gz
	@mkdir $(TMPDIR)/$(FILEBASE)
	@cp -ar crypto extras tests examples Makefile *.c *.h README NEWS \
		INSTALL AUTHORS COPYING $(TMPDIR)/$(FILEBASE)
	@rm -rf $(TMPDIR)/$(FILEBASE)/.git* $(TMPDIR)/$(FILEBASE)/releases $(TMPDIR)/$(FILEBASE)/scripts
	@tar -C /tmp -czf ./$(OUTPUT) $(FILEBASE)
	@rm -rf $(TMPDIR)/$(FILEBASE)
	@echo Signing $(OUTPUT)
	@gpg --output $(OUTPUT).sig -sb $(OUTPUT)
	@gpg --verify $(OUTPUT).sig $(OUTPUT)
	@mv $(OUTPUT) $(OUTPUT).sig releases/
