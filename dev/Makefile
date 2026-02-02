# --------------------------------------------------------------------------
# Builds:
#   diskimage
#	libtool
#	keymap
#   makerom
#   libclap
#
# and puts them inside the build/host folder.
#

UNAME_S := $(shell uname -s)

.PHONY: diskimage libtool keymap makerom clean

default: all

all diskimage libtool keymap makerom:
ifeq ($(OS),Windows_NT)
	$(MAKE) -f Makefile-windows --no-print-directory $(MAKECMDGOALS)
else
ifeq ($(UNAME_S),Linux)
    $(MAKE) -f Makefile-posix --no-print-directory $(MAKECMDGOALS)
endif
ifeq ($(UNAME_S),Darwin)
    $(MAKE) -f Makefile-posix --no-print-directory $(MAKECMDGOALS)
endif
endif
