# Expects that an environment variable 'VBCC' exists and that it points to the vbcc tool chain directory.
# Eg VBCC='C:\Program Files\vbcc'
#
# Expects that the vbcc tool chain 'bin' folder is listed in the PATH environment variable.
#


# --------------------------------------------------------------------------
# Build Configuration
#

# Supported build configs:
# 'release'  compile with optimizations turned on and do not generate debug info
# 'debug'    compile without optimizations and generate debug info
ifndef BUILD_CONFIGURATION
	BUILD_CONFIGURATION := release
endif
export BUILD_CONFIGURATION

ifeq ($(BUILD_CONFIGURATION), release)
	CC_OPT_SETTING := -O=17407
	CC_GENERATE_DEBUG_INFO :=
else
	CC_OPT_SETTING := -O0
	CC_GENERATE_DEBUG_INFO := -g
endif
export CC_OPT_SETTING
export CC_GENERATE_DEBUG_INFO

export CC_PREPROCESSOR_DEFINITIONS := -DDEBUG=1 -D__BIG_ENDIAN__=1 -D__ILP32__=1 -DTARGET_CPU_68030=1


# --------------------------------------------------------------------------
# Common Directories
#

export WORKSPACE_DIR := $(CURDIR)
export BUILD_DIR := $(WORKSPACE_DIR)/build
export OBJS_DIR := $(BUILD_DIR)/objs
export PRODUCT_DIR := $(BUILD_DIR)/product


# --------------------------------------------------------------------------
# Build tools
#

ifeq ($(OS),Windows_NT)
VC_CONFIG := $(WORKSPACE_DIR)/Tools/vc_windows_host.config
else
VC_CONFIG := $(WORKSPACE_DIR)/Tools/vc_posix_host.config
endif
export VC_CONFIG

export LIBTOOL = $(VBCC)/bin/libtool
export AS = $(VBCC)/bin/vasmm68k_mot -Felf -quiet -nosym -spaces -m68060 -DTARGET_CPU_68030
export CC = $(VBCC)/bin/vc +$(VC_CONFIG) -c -c99 -cpp-comments -cpu=68030
export LD = $(VBCC)/bin/vlink
export PY = python


# --------------------------------------------------------------------------
# Includes
#

include $(WORKSPACE_DIR)/common.mk


# --------------------------------------------------------------------------
# Project definitions
#

KERNEL_PROJECT_DIR := $(WORKSPACE_DIR)/Kernel
KERNEL_TESTS_PROJECT_DIR := $(WORKSPACE_DIR)/Kernel/Tests
export KERNEL_OBJS_DIR := $(OBJS_DIR)/Kernel
export KERNEL_PRODUCT_DIR := $(PRODUCT_DIR)/Kernel
export KERNEL_TESTS_OBJS_DIR := $(OBJS_DIR)/Kernel/Tests

SH_PROJECT_DIR := $(WORKSPACE_DIR)/Commands/sh
export SH_OBJS_DIR := $(OBJS_DIR)/Commands/sh

ROM_FILE := $(KERNEL_PRODUCT_DIR)/Apollo.rom
export KERNEL_FILE := $(KERNEL_OBJS_DIR)/Kernel.bin
export KERNEL_TESTS_FILE := $(KERNEL_TESTS_OBJS_DIR)/KernelTests
export SH_FILE := $(SH_OBJS_DIR)/sh

LIBSYSTEM_PROJECT_DIR := $(WORKSPACE_DIR)/Library/libsystem
export LIBSYSTEM_HEADERS_DIR := $(LIBSYSTEM_PROJECT_DIR)/Headers
export LIBSYSTEM_PRODUCT_DIR := $(PRODUCT_DIR)/Library/libsystem
export LIBSYSTEM_FILE := $(LIBSYSTEM_PRODUCT_DIR)/libsystem.a

LIBC_PROJECT_DIR := $(WORKSPACE_DIR)/Library/libc
export LIBC_HEADERS_DIR := $(LIBC_PROJECT_DIR)/Headers
export LIBC_PRODUCT_DIR := $(PRODUCT_DIR)/Library/libc
export LIBC_FILE := $(LIBC_PRODUCT_DIR)/libc.a
export ASTART_FILE := $(LIBC_PRODUCT_DIR)/_astart.o
export CSTART_FILE := $(LIBC_PRODUCT_DIR)/_cstart.o

LIBM_PROJECT_DIR := $(WORKSPACE_DIR)/Library/libm
export LIBM_HEADERS_DIR := $(LIBM_PROJECT_DIR)/Headers
export LIBM_PRODUCT_DIR := $(PRODUCT_DIR)/Library/libm
export LIBM_FILE := $(LIBM_PRODUCT_DIR)/libm.a


# --------------------------------------------------------------------------
# Build rules
#

.SUFFIXES:
.PHONY: all clean


all:
	@echo Building all [$(BUILD_CONFIGURATION)]
	$(MAKE) build-libsystem
	$(MAKE) build-libc
	$(MAKE) build-libm
	$(MAKE) build-kernel
	$(MAKE) build-kernel-tests
	$(MAKE) build-sh
	$(MAKE) build-kernel-rom
	@echo Done

include $(LIBSYSTEM_PROJECT_DIR)/project.mk
include $(LIBC_PROJECT_DIR)/project.mk
include $(LIBM_PROJECT_DIR)/project.mk

include $(KERNEL_PROJECT_DIR)/project.mk
include $(KERNEL_TESTS_PROJECT_DIR)/project.mk

include $(SH_PROJECT_DIR)/project.mk

build-kernel-rom: $(ROM_FILE)

#$(ROM_FILE): $(KERNEL_FILE) $(KERNEL_TESTS_FILE) ./Tools/finalizerom.py
#	@echo Making ROM
#	$(PY) ./Tools/finalizerom.py $(KERNEL_FILE) $(KERNEL_TESTS_FILE) $(ROM_FILE)

$(ROM_FILE): $(KERNEL_FILE) $(SH_FILE) ./Tools/finalizerom.py
	@echo Making ROM
	$(PY) ./Tools/finalizerom.py $(KERNEL_FILE) $(SH_FILE) $(ROM_FILE)


clean-kernel-rom:
	@echo Cleaning Kernel...
	$(MAKE) clean-kernel
	$(MAKE) clean-kernel-tests
	$(MAKE) clean-sh
	
clean:
	@echo Cleaning...
	$(call rm_if_exists,$(OBJS_DIR))
	$(call rm_if_exists,$(PRODUCT_DIR))
	@echo Done
