# --------------------------------------------------------------------------
# Builds:
#   diskimage
#	libtool
#	keymap
#   makerom
#   libclap
#
# and puts them inside the build/tools folder.
#


# --------------------------------------------------------------------------
# Common Directories
#

BUILD_DIR := ../build
TOOLS_DIR := $(BUILD_DIR)/tools
OBJS_DIR := $(BUILD_DIR)/tools-objs
LIBS_DIR := ../Library
LIBCLAP_DIR := $(LIBS_DIR)/libclap
LIBCLAP_HEADERS_DIR := $(LIBCLAP_DIR)/Headers
LIBCLAP_SOURCES_DIR := $(LIBCLAP_DIR)/Sources


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

all: libclap makerom libtool keymap diskimage
diskimage: $(TOOLS_DIR)/diskimage
keymap: $(TOOLS_DIR)/keymap
libtool: $(TOOLS_DIR)/libtool
makerom: $(TOOLS_DIR)/makerom
libclap: $(OBJS_DIR)/clap.lib

clean:
	$(call rm_if_exists,$(OBJS_DIR))


$(TOOLS_DIR):
	$(call mkdir_if_needed,$(TOOLS_DIR))

$(OBJS_DIR):
	$(call mkdir_if_needed,$(OBJS_DIR))


# --------------------------------------------------------------------------
# libclap
#

LIBCLAP_H_SOURCES := $(wildcard $(LIBCLAP_SOURCES_DIR)/*.h)
LIBCLAP_C_SOURCES := $(wildcard $(LIBCLAP_SOURCES_DIR)/*.c)
LIBCLAP_OBJS := $(patsubst $(LIBCLAP_SOURCES_DIR)/%.c, $(OBJS_DIR)/%.obj, $(LIBCLAP_C_SOURCES))

$(OBJS_DIR)/clap.lib: $(LIBCLAP_C_SOURCES) $(LIBCLAP_H_SOURCES) | $(OBJS_DIR)
	cl /c /I $(LIBCLAP_HEADERS_DIR) /I $(LIBCLAP_SOURCES_DIR) $(DEBUG_FLAGS) /Fo:"$(OBJS_DIR)/" $(filter %.c,$^)
	link /lib /out:"$@" $(LIBCLAP_OBJS)


# --------------------------------------------------------------------------
# diskimage
#

LIBC_SOURCES_DIR := ../Library/libc/Sources
LIBC_C_SOURCES += $(LIBC_SOURCES_DIR)/ext/deque.c
LIBC_C_SOURCES += $(LIBC_SOURCES_DIR)/ext/hash.c
LIBC_C_SOURCES += $(LIBC_SOURCES_DIR)/ext/log2_ul.c
LIBC_C_SOURCES += $(LIBC_SOURCES_DIR)/ext/log2_ull.c
LIBC_C_SOURCES += $(LIBC_SOURCES_DIR)/ext/pow2_ul.c
LIBC_C_SOURCES += $(LIBC_SOURCES_DIR)/ext/pow2_ull.c
LIBC_C_SOURCES += $(LIBC_SOURCES_DIR)/ext/queue.c
LIBC_C_SOURCES += $(LIBC_SOURCES_DIR)/ext/_Try_bang_failed.c

KERNEL_SOURCES_DIR := ../Kernel/Sources
KERNEL_C_SOURCES += $(KERNEL_SOURCES_DIR)/filemanager/FileHierarchy.c
KERNEL_C_SOURCES += $(KERNEL_SOURCES_DIR)/filemanager/FileManager_Directory.c
KERNEL_C_SOURCES += $(KERNEL_SOURCES_DIR)/filemanager/FileManager_File.c
KERNEL_C_SOURCES += $(KERNEL_SOURCES_DIR)/filemanager/FileManager.c
KERNEL_C_SOURCES += $(KERNEL_SOURCES_DIR)/filemanager/ResolvedPath.c
KERNEL_C_SOURCES += $(KERNEL_SOURCES_DIR)/filesystem/FSContainer.c
KERNEL_C_SOURCES += $(KERNEL_SOURCES_DIR)/filesystem/ContainerFilesystem.c
KERNEL_C_SOURCES += $(KERNEL_SOURCES_DIR)/filesystem/Filesystem.c
KERNEL_C_SOURCES += $(KERNEL_SOURCES_DIR)/filesystem/FSChannel.c
KERNEL_C_SOURCES += $(KERNEL_SOURCES_DIR)/filesystem/IOChannel.c
KERNEL_C_SOURCES += $(KERNEL_SOURCES_DIR)/filesystem/Inode.c
KERNEL_C_SOURCES += $(KERNEL_SOURCES_DIR)/filesystem/InodeChannel.c
KERNEL_C_SOURCES += $(KERNEL_SOURCES_DIR)/filesystem/PathComponent.c
KERNEL_C_SOURCES += $(KERNEL_SOURCES_DIR)/security/SecurityManager.c

KOBJ_C_SOURCES := $(wildcard $(KERNEL_SOURCES_DIR)/kobj/*.c)
SERENAFS_C_SOURCES := $(wildcard $(KERNEL_SOURCES_DIR)/filesystem/serenafs/*.c)
SERENAFS_TOOLS_C_SOURCES := $(wildcard $(KERNEL_SOURCES_DIR)/filesystem/serenafs/tools/*.c)

DISKIMAGE_C_SOURCES := $(wildcard diskimage/*.c)

ALL_DISKIMAGE_C_SOURCES := $(LIBC_C_SOURCES) \
						   $(KERNEL_C_SOURCES) \
						   $(KOBJ_C_SOURCES) \
						   $(SERENAFS_C_SOURCES) \
						   $(SERENAFS_TOOLS_C_SOURCES) \
						   $(DISKIMAGE_C_SOURCES)

$(TOOLS_DIR)/diskimage: $(ALL_DISKIMAGE_C_SOURCES) $(OBJS_DIR)/clap.lib | $(OBJS_DIR) $(TOOLS_DIR)
	cl /I diskimage /I $(LIBCLAP_HEADERS_DIR) /I $(KERNEL_SOURCES_DIR) /D__KERNEL__=1 /D__DISKIMAGE__=1 /D__TRY_BANG_LOD=2 $(DEBUG_FLAGS) /std:c11 /experimental:c11atomics /Fe:"$@" /Fo:"$(OBJS_DIR)/" $^


# --------------------------------------------------------------------------
# keymap
#

KEYMAP_C_SOURCES := bin/keymap.c

$(TOOLS_DIR)/keymap: $(KEYMAP_C_SOURCES) $(OBJS_DIR)/clap.lib | $(OBJS_DIR) $(TOOLS_DIR)
	cl /I $(LIBCLAP_HEADERS_DIR) $(DEBUG_FLAGS) /Fe:"$@" /Fo:"$(OBJS_DIR)/" $^


# --------------------------------------------------------------------------
# libtool
#

LIBTOOL_C_SOURCES := bin/libtool.c

$(TOOLS_DIR)/libtool: $(LIBTOOL_C_SOURCES) $(OBJS_DIR)/clap.lib | $(OBJS_DIR) $(TOOLS_DIR)
	cl "/D_CRT_SECURE_NO_WARNINGS" "/D_CRT_NONSTDC_NO_WARNINGS" /I $(LIBCLAP_HEADERS_DIR) $(DEBUG_FLAGS) /Fe:"$@" /Fo:"$(OBJS_DIR)/" $^


# --------------------------------------------------------------------------
# makerom
#

MAKEROM_C_SOURCES := bin/makerom.c

$(TOOLS_DIR)/makerom: $(MAKEROM_C_SOURCES) $(OBJS_DIR)/clap.lib | $(OBJS_DIR) $(TOOLS_DIR)
	cl /I $(LIBCLAP_HEADERS_DIR) $(DEBUG_FLAGS) /Fe:"$@" /Fo:"$(OBJS_DIR)/" $^
