# --------------------------------------------------------------------------
# Builds:
#	libtool
#	keymap
#
# and puts them inside the build/tools folder.
#


# --------------------------------------------------------------------------
# Common Directories
#

TOOLS_DIR := ../build/tools


# --------------------------------------------------------------------------
# Includes
#

include ../common.mk


# --------------------------------------------------------------------------
# Build rules
#

.SUFFIXES:
.PHONY: clean


default: all

all: $(TOOLS_DIR) $(TOOLS_DIR)/libtool $(TOOLS_DIR)/keymap $(TOOLS_DIR)/makerom

$(TOOLS_DIR):
	$(call mkdir_if_needed,$(TOOLS_DIR))

$(TOOLS_DIR)/libtool: libtool.c
	cl "/D_CRT_SECURE_NO_WARNINGS" "/D_CRT_NONSTDC_NO_WARNINGS" /Fe"$@" /Fo"$(TOOLS_DIR)/" $^

$(TOOLS_DIR)/keymap: keymap.c
	cl /Fe"$@" /Fo"$(TOOLS_DIR)/" $^

$(TOOLS_DIR)/makerom: makerom.c
	cl /Fe"$@" /Fo"$(TOOLS_DIR)/" $^

clean:
	$(call rm_if_exists,$(TOOLS_DIR))
