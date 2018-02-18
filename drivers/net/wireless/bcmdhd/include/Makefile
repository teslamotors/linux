#!/bin/bash
#
# Copyright (C) 1999-2015, Broadcom Corporation
# 
#      Unless you and Broadcom execute a separate written software license
# agreement governing use of this software, this software is licensed to you
# under the terms of the GNU General Public License version 2 (the "GPL"),
# available at http://www.broadcom.com/licenses/GPLv2.php, with the
# following added to such license:
# 
#      As a special exception, the copyright holders of this software give you
# permission to link this software with independent modules, and to copy and
# distribute the resulting executable under terms of your choice, provided that
# you also meet, for each linked independent module, the terms and conditions of
# the license of that module.  An independent module is a module which is not
# derived from this software.  The special exception does not apply to any
# modifications of the software.
# 
#      Notwithstanding the above, under no circumstances may you combine this
# software in any way with any other Broadcom software provided under a license
# other than the GPL, without Broadcom's express prior written consent.
#
# This script serves following purpose:
#
# 1. It generates native version information by querying
#    automerger maintained database to see where src/include
#    came from
# 2. For select components, as listed in compvers.sh
#    it generates component version files
#
# $Id: Makefile 518520 2014-12-02 13:42:31Z $
#

export SRCBASE:=..

TARGETS := epivers.h

ifdef VERBOSE
export VERBOSE
endif

all release: epivers compvers

# Generate epivers.h for native branch url
epivers:
	bash epivers.sh

# Generate component versions based on component url
compvers:
	@if [ -s "compvers.sh" ]; then \
		echo "Generating component versions, if any"; \
		bash compvers.sh; \
	else \
		echo "Skipping component version generation"; \
	fi

# Generate epivers.h for native branch version
clean_compvers:
	@if [ -s "compvers.sh" ]; then \
		echo "bash compvers.sh clean"; \
		bash compvers.sh clean; \
	else \
		echo "Skipping component version clean"; \
	fi

clean:
	rm -f $(TARGETS) *.prev

clean_all: clean clean_compvers

.PHONY: all release clean epivers compvers clean_compvers
