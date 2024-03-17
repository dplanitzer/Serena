# Expects that an environment variable 'VBCC' exists and that it points to the vbcc tool chain directory.
# Eg VBCC='C:\Program Files\vbcc'
#
# Expects that the vbcc tool chain 'bin' folder is listed in the PATH environment variable.
#


# --------------------------------------------------------------------------
# Common Directories
#

export WORKSPACE_DIR := $(CURDIR)
export SCRIPTS_DIR := $(WORKSPACE_DIR)/Tools
export BUILD_DIR := $(WORKSPACE_DIR)/build
export TOOLS_DIR := $(BUILD_DIR)/tools
export OBJS_DIR := $(BUILD_DIR)/objs
export PRODUCT_DIR := $(BUILD_DIR)/product


# --------------------------------------------------------------------------
# Build tools
#

export LIBTOOL = $(TOOLS_DIR)/libtool
export MAKEROM = $(TOOLS_DIR)/makerom
export AS = $(VBCC)/bin/vasmm68k_mot
export CC = $(VBCC)/bin/vc
export LD = $(VBCC)/bin/vlink


# --------------------------------------------------------------------------
# Build Configuration
#

# Supported build configs:
# 'release'  compile with optimizations turned on and do not generate debug info
# 'debug'    compile without optimizations and generate debug info
ifndef BUILD_CONFIGURATION
	BUILD_CONFIGURATION := release
endif

ifeq ($(BUILD_CONFIGURATION), release)
	CC_OPT_SETTING := -O=17407
	CC_GEN_DEBUG_INFO :=
else
	CC_OPT_SETTING := -O0
	CC_GEN_DEBUG_INFO := -g
endif


export CC_OPT_SETTING
export CC_GEN_DEBUG_INFO

export CC_PREPROC_DEFS := -DDEBUG=1 -DTARGET_CPU_68030=1

#XXX vbcc always defines -D__STDC_HOSTED__=1 and we can't override it for the kernel (which should define -D__STDC_HOSTED__=0)
KERNEL_STDC_PREPROC_DEFS := -D__STDC_UTF_16__=1 -D__STDC_UTF_32__=1 -D__STDC_NO_ATOMICS__=1 -D__STDC_NO_COMPLEX__=1 -D__STDC_NO_THREADS__=1
USER_STDC_PREPROC_DEFS := -D__STDC_UTF_16__=1 -D__STDC_UTF_32__=1 -D__STDC_NO_ATOMICS__=1 -D__STDC_NO_COMPLEX__=1 -D__STDC_NO_THREADS__=1


ifeq ($(OS),Windows_NT)
	VC_CONFIG := $(SCRIPTS_DIR)/vc_windows_host.config
else
	VC_CONFIG := $(SCRIPTS_DIR)/vc_posix_host.config
endif


export KERNEL_ASM_CONFIG := -Felf -quiet -nosym -spaces -m68060 -DTARGET_CPU_68030
export KERNEL_CC_CONFIG := +$(VC_CONFIG) -c -c99 -cpp-comments -cpu=68030 $(KERNEL_STDC_PREPROC_DEFS)

export USER_ASM_CONFIG := -Felf -quiet -nosym -spaces -m68060 -DTARGET_CPU_68030
export USER_CC_CONFIG := +$(VC_CONFIG) -c -c99 -cpp-comments -cpu=68030 $(USER_STDC_PREPROC_DEFS)

export KERNEL_LD_CONFIG := -brawbin1 -T $(SCRIPTS_DIR)/kernel_linker.script
export USER_LD_CONFIG := -bataritos -T $(SCRIPTS_DIR)/user_linker.script


# --------------------------------------------------------------------------
# Common Includes
#

include $(WORKSPACE_DIR)/common.mk


# --------------------------------------------------------------------------
# Project definitions
#

KERNEL_PROJECT_DIR := $(WORKSPACE_DIR)/Kernel
KERNEL_TESTS_PROJECT_DIR := $(WORKSPACE_DIR)/Kernel/Tests
export KERNEL_OBJS_DIR := $(OBJS_DIR)/Kernel
export KERNEL_TESTS_OBJS_DIR := $(OBJS_DIR)/Kernel/Tests
export KERNEL_FILE := $(KERNEL_OBJS_DIR)/Kernel.bin
export KERNEL_TESTS_FILE := $(KERNEL_TESTS_OBJS_DIR)/Kernel_Tests

ROM_FILE := $(PRODUCT_DIR)/Serena.rom


SH_PROJECT_DIR := $(WORKSPACE_DIR)/Commands/sh
export SH_OBJS_DIR := $(OBJS_DIR)/Commands/sh
export SH_FILE := $(SH_OBJS_DIR)/sh


LIBSYSTEM_PROJECT_DIR := $(WORKSPACE_DIR)/Library/libsystem
export LIBSYSTEM_HEADERS_DIR := $(LIBSYSTEM_PROJECT_DIR)/Headers
export LIBSYSTEM_OBJS_DIR := $(OBJS_DIR)/Library/libsystem
export LIBSYSTEM_FILE := $(LIBSYSTEM_OBJS_DIR)/libsystem.a


LIBC_PROJECT_DIR := $(WORKSPACE_DIR)/Library/libc
export LIBC_HEADERS_DIR := $(LIBC_PROJECT_DIR)/Headers
export LIBC_OBJS_DIR := $(OBJS_DIR)/Library/libc
export LIBC_FILE := $(LIBC_OBJS_DIR)/libc.a
export ASTART_FILE := $(LIBC_OBJS_DIR)/_astart.o
export CSTART_FILE := $(LIBC_OBJS_DIR)/_cstart.o


LIBM_PROJECT_DIR := $(WORKSPACE_DIR)/Library/libm
export LIBM_HEADERS_DIR := $(LIBM_PROJECT_DIR)/Headers
export LIBM_OBJS_DIR := $(OBJS_DIR)/Library/libm
export LIBM_FILE := $(LIBM_OBJS_DIR)/libm.a


# --------------------------------------------------------------------------
# Project includes
#

include $(LIBSYSTEM_PROJECT_DIR)/project.mk
include $(LIBC_PROJECT_DIR)/project.mk
include $(LIBM_PROJECT_DIR)/project.mk

include $(KERNEL_PROJECT_DIR)/project.mk
include $(KERNEL_TESTS_PROJECT_DIR)/project.mk

include $(SH_PROJECT_DIR)/project.mk


# --------------------------------------------------------------------------
# Build rules
#

.SUFFIXES:
.PHONY: clean


all: build-rom
	@echo Done (Configuration: [$(BUILD_CONFIGURATION)])

build-rom: $(ROM_FILE)

#$(ROM_FILE): $(KERNEL_FILE) $(KERNEL_TESTS_FILE)
#	@echo Making ROM
#	$(MAKEROM) $(KERNEL_FILE) $(KERNEL_TESTS_FILE) $(ROM_FILE)

$(ROM_FILE): $(KERNEL_FILE) $(SH_FILE) | $(PRODUCT_DIR)
	@echo Making ROM
	$(MAKEROM) $(KERNEL_FILE) $(SH_FILE) $(ROM_FILE)

$(PRODUCT_DIR):
	$(call mkdir_if_needed,$(PRODUCT_DIR))

	
clean:
	@echo Cleaning...
	$(call rm_if_exists,$(OBJS_DIR))
	$(call rm_if_exists,$(PRODUCT_DIR))
	@echo Done

clean-rom: clean-kernel clean-kernel-tests clean-sh clean-libc clean-libsystem
	@echo Done
