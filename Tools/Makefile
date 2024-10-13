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
OBJS_DIR := ../build/tools-objs
LIBSYSTEM_HEADERS_DIR := ..\Library\libsystem\Headers
LIBCLAP_HEADERS_DIR := ..\Library\libclap\Headers


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

all: $(OBJS_DIR) $(TOOLS_DIR) $(TOOLS_DIR)/libtool $(TOOLS_DIR)/keymap $(TOOLS_DIR)/makerom $(TOOLS_DIR)/diskimage
diskimage: $(OBJS_DIR) $(TOOLS_DIR) $(TOOLS_DIR)/diskimage
keymap: $(OBJS_DIR) $(TOOLS_DIR) $(TOOLS_DIR)/keymap
libtool: $(OBJS_DIR) $(TOOLS_DIR) $(TOOLS_DIR)/libtool
makerom: $(OBJS_DIR) $(TOOLS_DIR) $(TOOLS_DIR)/makerom
libclap: $(OBJS_DIR) $(OBJS_DIR)/clap.lib


$(TOOLS_DIR):
	$(call mkdir_if_needed,$(TOOLS_DIR))

$(OBJS_DIR):
	$(call mkdir_if_needed,$(OBJS_DIR))


# --------------------------------------------------------------------------
# libclap
#

LIBCLAP_DIR := ../Library/libclap
LIBCLAP_SOURCES_DIR := $(LIBCLAP_DIR)/Sources
LIBCLAP_SRCS := $(LIBCLAP_SOURCES_DIR)/clap.c $(LIBCLAP_SOURCES_DIR)/clap_error.c $(LIBCLAP_SOURCES_DIR)/clap_help.c

$(OBJS_DIR)/clap.lib: $(LIBCLAP_SRCS) $(LIBCLAP_HEADERS_DIR)/clap.h $(LIBCLAP_SOURCES_DIR)/clap_priv.h
	cl /c /I $(LIBCLAP_HEADERS_DIR) /I $(LIBCLAP_SOURCES_DIR) $(DEBUG_FLAGS) /Fo:"$(OBJS_DIR)/" $(filter %.c,$^)
	link /lib /out:"$@" $(OBJS_DIR)\clap_error.obj $(OBJS_DIR)\clap_help.obj $(OBJS_DIR)\clap.obj


# --------------------------------------------------------------------------
# libtool
#

LIBTOOL_SRCS := libtool.c

$(TOOLS_DIR)/libtool: $(LIBTOOL_SRCS) $(OBJS_DIR)/clap.lib
	cl "/D_CRT_SECURE_NO_WARNINGS" "/D_CRT_NONSTDC_NO_WARNINGS" /I $(LIBCLAP_HEADERS_DIR) $(DEBUG_FLAGS) /Fe:"$@" /Fo:"$(OBJS_DIR)/" $^


# --------------------------------------------------------------------------
# keymap
#

KEYMAP_SRCS := keymap.c

$(TOOLS_DIR)/keymap: $(KEYMAP_SRCS) $(OBJS_DIR)/clap.lib
	cl /I $(LIBCLAP_HEADERS_DIR) $(DEBUG_FLAGS) /Fe:"$@" /Fo:"$(OBJS_DIR)/" $^


# --------------------------------------------------------------------------
# makerom
#

MAKEROM_SRCS := makerom.c

$(TOOLS_DIR)/makerom: $(MAKEROM_SRCS) $(OBJS_DIR)/clap.lib
	cl /I $(LIBCLAP_HEADERS_DIR) $(DEBUG_FLAGS) /Fe:"$@" /Fo:"$(OBJS_DIR)/" $^


# --------------------------------------------------------------------------
# diskimage
#

DISKIMAGE_SRCS := diskimage/klib/klib.c diskimage/dispatcher/dispatcher.c
DISKIMAGE_SRCS += diskimage/driver/driver.c diskimage/RamFSContainer.c
DISKIMAGE_SRCS += ../Kernel/Sources/klib/Array.c ../Kernel/Sources/klib/List.c
DISKIMAGE_SRCS += ../Kernel/Sources/kobj/Class.c ../Kernel/Sources/kobj/Any.c ../Kernel/Sources/kobj/Object.c ../Kernel/Sources/kobj/ObjectArray.c
DISKIMAGE_SRCS += ../Kernel/Sources/User.c
DISKIMAGE_SRCS += ../Kernel/Sources/filesystem/FSContainer.c
DISKIMAGE_SRCS += ../Kernel/Sources/filesystem/ContainerFilesystem.c
DISKIMAGE_SRCS += ../Kernel/Sources/filesystem/Filesystem.c
DISKIMAGE_SRCS += ../Kernel/Sources/filesystem/Inode.c
DISKIMAGE_SRCS += ../Kernel/Sources/filesystem/PathComponent.c
DISKIMAGE_SRCS += ../Kernel/Sources/filesystem/serenafs/SerenaFS_Alloc.c ../Kernel/Sources/filesystem/serenafs/SerenaFS_Inode.c
DISKIMAGE_SRCS += ../Kernel/Sources/filesystem/serenafs/SerenaFS_Directory.c ../Kernel/Sources/filesystem/serenafs/SerenaFS_File.c
DISKIMAGE_SRCS += ../Kernel/Sources/filesystem/serenafs/SerenaFS.c
DISKIMAGE_SRCS += diskimage/diskimage.c diskimage/diskimage_win32.c

$(TOOLS_DIR)/diskimage: $(DISKIMAGE_SRCS) $(OBJS_DIR)/clap.lib
	cl /I diskimage\ /I $(LIBSYSTEM_HEADERS_DIR) /I $(LIBCLAP_HEADERS_DIR) /I ..\Kernel\Sources\ /D__SYSTEM_SHIM__=1 /D__KERNEL__=1 /D__DISKIMAGE__=1 $(DEBUG_FLAGS) /Fe:"$@" /Fo:"$(OBJS_DIR)/" $^


clean:
	$(call rm_if_exists,$(OBJS_DIR))
