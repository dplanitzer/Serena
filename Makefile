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
	CC_OPT_SETTING := -O3
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
export PRODUCT_DIR := $(WORKSPACE_DIR)/product


# --------------------------------------------------------------------------
# Build tools
#

ifeq ($(OS),Windows_NT)
VC_CONFIG := $(WORKSPACE_DIR)/vc_windows_host.config
else
VC_CONFIG := $(WORKSPACE_DIR)/vc_posix_host.config
endif
export VC_CONFIG

export AS = $(VBCC)/bin/vasmm68k_mot -Faout -quiet -nosym -spaces -m68060 -DTARGET_CPU_68030
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
export KERNEL_BUILD_DIR := $(BUILD_DIR)/Kernel
export KERNEL_PRODUCT_DIR := $(PRODUCT_DIR)/Kernel
export KERNEL_TESTS_BUILD_DIR := $(BUILD_DIR)/Kernel/Tests

ROM_FILE := $(KERNEL_PRODUCT_DIR)/Apollo.rom
export KERNEL_BIN_FILE := $(KERNEL_BUILD_DIR)/Kernel.bin
export KERNEL_TESTS_BIN_FILE := $(KERNEL_TESTS_BUILD_DIR)/KernelTests.bin

LIBC_PROJECT_DIR := $(WORKSPACE_DIR)/Library/C.framework
export LIBC_HEADERS_DIR := $(LIBC_PROJECT_DIR)/Headers
export LIBC_BUILD_DIR := $(BUILD_DIR)/Library/C.framework
export LIBC_PRODUCT_DIR := $(PRODUCT_DIR)/Library/C.framework
export LIBC_LIB_FILE := $(LIBC_PRODUCT_DIR)/libc.a
export LIBC_ASTART_FILE := $(LIBC_PRODUCT_DIR)/_astart.o
export LIBC_CSTART_FILE := $(LIBC_PRODUCT_DIR)/_cstart.o


# --------------------------------------------------------------------------
# Build rules
#

.SUFFIXES:
.PHONY: all clean


all:
	@echo Building all [$(BUILD_CONFIGURATION)]
	$(MAKE) build-libc
	$(MAKE) build-kernel
	$(MAKE) build-kernel-tests
	$(MAKE) build-kernel-rom
	@echo Done

include $(LIBC_PROJECT_DIR)/project.mk

include $(KERNEL_PROJECT_DIR)/project.mk

include $(KERNEL_TESTS_PROJECT_DIR)/project.mk

build-kernel-rom: $(ROM_FILE)

$(ROM_FILE): $(KERNEL_BIN_FILE) $(KERNEL_TESTS_BIN_FILE) finalizerom.py
	@echo Making ROM
	$(PY) ./finalizerom.py $(KERNEL_BIN_FILE) $(KERNEL_TESTS_BIN_FILE) $(ROM_FILE)


clean-kernel-rom:
	$(MAKE) clean-kernel
	$(MAKE) clean-kernel-tests
	
clean:
	@echo Cleaning...
	$(call rm_if_exists,$(BUILD_DIR))
	$(call rm_if_exists,$(PRODUCT_DIR))
	@echo Done
