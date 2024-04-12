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
# Common Settings
#

#DEBUG_FLAGS := /Od /Zi
DEBUG_FLAGS :=


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

all: $(TOOLS_DIR) $(TOOLS_DIR)/libtool $(TOOLS_DIR)/keymap $(TOOLS_DIR)/makerom $(TOOLS_DIR)/diskimage
diskimage: $(TOOLS_DIR) $(TOOLS_DIR)/diskimage
keymap: $(TOOLS_DIR) $(TOOLS_DIR)/keymap
libtool: $(TOOLS_DIR) $(TOOLS_DIR)/libtool
makerom: $(TOOLS_DIR) $(TOOLS_DIR)/makerom


$(TOOLS_DIR):
	$(call mkdir_if_needed,$(TOOLS_DIR))


$(TOOLS_DIR)/libtool: libtool.c
	cl "/D_CRT_SECURE_NO_WARNINGS" "/D_CRT_NONSTDC_NO_WARNINGS" /Fe"$@" /Fo"$(TOOLS_DIR)/" $^


$(TOOLS_DIR)/keymap: keymap.c
	cl /Fe"$@" /Fo"$(TOOLS_DIR)/" $^


$(TOOLS_DIR)/makerom: makerom.c
	cl /Fe"$@" /Fo"$(TOOLS_DIR)/" $^


DISKIMAGE_SRCS := diskimage/klib/klib.c diskimage/dispatcher/dispatcher.c
DISKIMAGE_SRCS += diskimage/driver/driver.c diskimage/driver/DiskDriver.c
DISKIMAGE_SRCS += ../Kernel/Sources/klib/Array.c ../Kernel/Sources/klib/List.c
DISKIMAGE_SRCS += ../Kernel/Sources/kobj/Any.c ../Kernel/Sources/kobj/Object.c ../Kernel/Sources/kobj/ObjectArray.c ../Kernel/Sources/kobj/Runtime.c
DISKIMAGE_SRCS += ../Kernel/Sources/User.c
DISKIMAGE_SRCS += ../Kernel/Sources/filesystem/Filesystem.c
DISKIMAGE_SRCS += ../Kernel/Sources/filesystem/FilesystemManager.c
DISKIMAGE_SRCS += ../Kernel/Sources/filesystem/Inode.c
DISKIMAGE_SRCS += ../Kernel/Sources/filesystem/PathComponent.c
DISKIMAGE_SRCS += ../Kernel/Sources/filesystem/serenafs/SerenaFS.c
DISKIMAGE_SRCS += diskimage/diskimage.c diskimage/diskimage_win32.c

$(TOOLS_DIR)/diskimage: $(DISKIMAGE_SRCS)
	cl /I diskimage\ /I ..\Library\libsystem\Headers\ /I ..\Kernel\Sources\ /D__SYSTEM_SHIM__=1 /D__KERNEL__=1 /D__DISKIMAGE__=1 $(DEBUG_FLAGS) /Fe"$@" /Fo"$(TOOLS_DIR)/" $^


clean:
	$(call rm_if_exists,$(TOOLS_DIR))
