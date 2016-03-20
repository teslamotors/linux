# SPDX-License-Identifier: GPL-2.0
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
PKG_CONFIG = $(CROSS_COMPILE)pkg-config

ifeq ($(srctree),)
srctree := $(patsubst %/,%,$(dir $(shell pwd)))
srctree := $(patsubst %/,%,$(dir $(srctree)))
#$(info Determined 'srctree' to be $(srctree))
endif

INSTALL = install
prefix ?= /usr/local
bindir = $(prefix)/bin


CFLAGS += $(HOSTCFLAGS)
CFLAGS += -D__EXPORTED_HEADERS__ -g
ifdef RPMB_STATIC
LDFLAGS += -pthread -static
CFLAGS +=  -pthread -static
PKG_STATIC = --static
endif
CFLAGS += -I$(srctree)/include/uapi -I$(srctree)/include
LDLIBS += $(shell $(PKG_CONFIG) --libs $(PKG_STATIC) libcrypto)

prog := rpmb

all : $(prog)

$(prog): rpmb.o

clean :
	$(RM) $(prog) *.o

install: $(prog)
	$(INSTALL) -m755 -d $(DESTDIR)$(bindir)
	$(INSTALL) $(prog) $(DESTDIR)$(bindir)
