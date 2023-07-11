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

export CC_PREPROCESSOR_DEFINITIONS := -DDEBUG=1 -D__BIG_ENDIAN__=1 -D__LP32__=1 -DTARGET_CPU_68030=1


# --------------------------------------------------------------------------
# Common Directories
#

export PROJECT_DIR := $(CURDIR)
export BUILD_DIR := $(PROJECT_DIR)/build


# --------------------------------------------------------------------------
# Build tools
#

ifeq ($(OS),Windows_NT)
VC_CONFIG := $(PROJECT_DIR)/vc_windows_host.config
else
VC_CONFIG := $(PROJECT_DIR)/vc_posix_host.config
endif
export VC_CONFIG

export AS = $(VBCC)/bin/vasmm68k_mot -Faout -quiet -nosym -spaces -m68060 -DTARGET_CPU_68030
export CC = $(VBCC)/bin/vc +$(VC_CONFIG) -c -c99 -cpp-comments -cpu=68030
export LD = $(VBCC)/bin/vlink
export PY = python


# --------------------------------------------------------------------------
# Project definitions
#

KERNEL_PROJECT_DIR := $(PROJECT_DIR)/Kernel/Sources
KERNEL_TESTS_PROJECT_DIR := $(PROJECT_DIR)/Kernel/Tests
export KERNEL_INCLUDE_DIR := $(PROJECT_DIR)/Kernel/Sources
export KERNEL_BUILD_DIR := $(BUILD_DIR)/Kernel
export KERNEL_TESTS_BUILD_DIR := $(BUILD_DIR)/Kernel/Tests

ROM_FILE := $(KERNEL_BUILD_DIR)/Boot.rom
export KERNEL_BIN_FILE := $(KERNEL_BUILD_DIR)/rom_kernel.bin
export KERNEL_TESTS_BIN_FILE := $(KERNEL_TESTS_BUILD_DIR)/rom_tests.bin

RUNTIME_PROJECT_DIR := $(PROJECT_DIR)/Library/Runtime/Sources
export RUNTIME_INCLUDE_DIR := $(RUNTIME_PROJECT_DIR)
export RUNTIME_BUILD_DIR := $(BUILD_DIR)/Library/Runtime
export RUNTIME_LIB_FILE := $(RUNTIME_BUILD_DIR)/librt.a

SYSTEM_PROJECT_DIR := $(PROJECT_DIR)/Library/System/Sources
export SYSTEM_INCLUDE_DIR := $(SYSTEM_PROJECT_DIR)
export SYSTEM_BUILD_DIR := $(BUILD_DIR)/Library/System
export SYSTEM_LIB_FILE := $(SYSTEM_BUILD_DIR)/libsystem.a


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: build build_all_kernel_projects clean
.POSIX: build


build: build_all_kernel_projects $(ROM_FILE)

$(ROM_FILE): $(KERNEL_BIN_FILE) $(KERNEL_TESTS_BIN_FILE) finalizerom.py
	@echo Making ROM
	$(PY) ./finalizerom.py $(KERNEL_BIN_FILE) $(KERNEL_TESTS_BIN_FILE) $(ROM_FILE)

build_all_kernel_projects:
	@echo Building ($(BUILD_CONFIGURATION))
	+$(MAKE) build -C $(RUNTIME_PROJECT_DIR)
	+$(MAKE) build -C $(SYSTEM_PROJECT_DIR)
	+$(MAKE) build -C $(KERNEL_PROJECT_DIR)
	+$(MAKE) build -C $(KERNEL_TESTS_PROJECT_DIR)
	@echo Done

clean:
	@echo Cleaning
	+$(MAKE) clean -C $(KERNEL_PROJECT_DIR)
	+$(MAKE) clean -C $(KERNEL_TESTS_PROJECT_DIR)
	+$(MAKE) clean -C $(SYSTEM_PROJECT_DIR)
	+$(MAKE) clean -C $(RUNTIME_PROJECT_DIR)
	@echo Done
	