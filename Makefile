# Expects that an environment variable 'VBCC' exists and that it points to the vbcc tool chain directory.
# Eg VBCC='C:\Program Files\vbcc'
#
# Expects that the vbcc tool chain 'bin' folder is listed in the PATH environment variable.
#


# --------------------------------------------------------------------------
# Common Directories
#

WORKSPACE_DIR := $(CURDIR)
SCRIPTS_DIR := $(WORKSPACE_DIR)/Tools
BUILD_DIR := $(WORKSPACE_DIR)/build
TOOLS_DIR := $(BUILD_DIR)/tools
DEMOS_DIR := $(WORKSPACE_DIR)/Demos
OBJS_DIR := $(BUILD_DIR)/objs
PRODUCT_DIR := $(BUILD_DIR)/product


# --------------------------------------------------------------------------
# Build tools
#

DISKIMAGE = $(TOOLS_DIR)/diskimage
LIBTOOL = $(TOOLS_DIR)/libtool
MAKEROM = $(TOOLS_DIR)/makerom
AS = $(VBCC)/bin/vasmm68k_mot
CC = $(VBCC)/bin/vc
LD = $(VBCC)/bin/vlink


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
	CC_OPT_SETTING := -O3 -size
	CC_KOPT_SETTING := -O3 -size -use-framepointer
	CC_GEN_DEBUG_INFO :=
else
	CC_OPT_SETTING := -O0
	CC_KOPT_SETTING := -O0 -use-framepointer
	CC_GEN_DEBUG_INFO := -g
endif

# XXX comment out to build a system that boots from floppy disk
#BOOT_FROM_ROM := 1

CC_PREPROC_DEFS := -DDEBUG=1 -DTARGET_CPU_68030=1 -D__SERENA__

#XXX vbcc always defines -D__STDC_HOSTED__=1 and we can't override it for the kernel (which should define -D__STDC_HOSTED__=0)
KERNEL_STDC_PREPROC_DEFS := -D__STDC_UTF_16__=1 -D__STDC_UTF_32__=1 -D__STDC_NO_ATOMICS__=1 -D__STDC_NO_COMPLEX__=1 -D__STDC_NO_THREADS__=1
USER_STDC_PREPROC_DEFS := -D__STDC_UTF_16__=1 -D__STDC_UTF_32__=1 -D__STDC_NO_ATOMICS__=1 -D__STDC_NO_COMPLEX__=1 -D__STDC_NO_THREADS__=1


ifeq ($(OS),Windows_NT)
	VC_CONFIG := $(SCRIPTS_DIR)/vc_windows_host.config
else
	VC_CONFIG := $(SCRIPTS_DIR)/vc_posix_host.config
endif


KERNEL_ASM_CONFIG := -Felf -quiet -nosym -spaces -m68060 -DTARGET_CPU_68030
KERNEL_CC_CONFIG := +$(VC_CONFIG) -c -c99 -cpp-comments -cpu=68030 $(KERNEL_STDC_PREPROC_DEFS)

USER_ASM_CONFIG := -Felf -quiet -nosym -spaces -m68060 -DTARGET_CPU_68030
USER_CC_CONFIG := +$(VC_CONFIG) -c -c99 -cpp-comments -cpu=68030 $(USER_STDC_PREPROC_DEFS)

KERNEL_LD_CONFIG := -brawbin1 -T $(SCRIPTS_DIR)/kernel_linker.script
USER_LD_CONFIG := -bataritos -T $(SCRIPTS_DIR)/user_linker.script


# --------------------------------------------------------------------------
# Common Includes
#

include $(WORKSPACE_DIR)/common.mk


# --------------------------------------------------------------------------
# Project definitions
#

KERNEL_PROJECT_DIR := $(WORKSPACE_DIR)/Kernel
KERNEL_TESTS_PROJECT_DIR := $(WORKSPACE_DIR)/Kernel/Tests
KERNEL_OBJS_DIR := $(OBJS_DIR)/Kernel
KERNEL_TESTS_OBJS_DIR := $(OBJS_DIR)/KernelTests
KERNEL_FILE := $(KERNEL_OBJS_DIR)/Kernel.bin
KERNEL_TESTS_FILE := $(KERNEL_TESTS_OBJS_DIR)/test


ROM_FILE := $(PRODUCT_DIR)/Serena.rom

BOOT_DISK_DIR := $(BUILD_DIR)/bootdisk
ifdef BOOT_FROM_ROM
BOOT_DMG_FILE := $(PRODUCT_DIR)/boot_disk.smg
BOOT_DMG_CONFIG := --disk=smg --size=256k
BOOT_DMG_FILE_FOR_ROM := $(BOOT_DMG_FILE)
else
BOOT_DMG_FILE := $(PRODUCT_DIR)/boot_disk.adf
BOOT_DMG_CONFIG := --disk=adf-dd
BOOT_DMG_FILE_FOR_ROM :=
endif


SH_PROJECT_DIR := $(WORKSPACE_DIR)/Commands/shell
SH_OBJS_DIR := $(OBJS_DIR)/Commands/shell
SH_FILE := $(SH_OBJS_DIR)/shell


CMDS_PROJECT_DIR := $(WORKSPACE_DIR)/Commands
CMDS_OBJS_DIR := $(OBJS_DIR)/Commands
LOGIN_FILE := $(CMDS_OBJS_DIR)/login
TYPE_FILE := $(CMDS_OBJS_DIR)/type


LIBSYSTEM_PROJECT_DIR := $(WORKSPACE_DIR)/Library/libsystem
LIBSYSTEM_HEADERS_DIR := $(LIBSYSTEM_PROJECT_DIR)/Headers
LIBSYSTEM_OBJS_DIR := $(OBJS_DIR)/Library/libsystem
LIBSYSTEM_FILE := $(LIBSYSTEM_OBJS_DIR)/libsystem.a


LIBC_PROJECT_DIR := $(WORKSPACE_DIR)/Library/libc
LIBC_HEADERS_DIR := $(LIBC_PROJECT_DIR)/Headers
LIBC_OBJS_DIR := $(OBJS_DIR)/Library/libc
LIBC_FILE := $(LIBC_OBJS_DIR)/libc.a
ASTART_FILE := $(LIBC_OBJS_DIR)/_astart.o
CSTART_FILE := $(LIBC_OBJS_DIR)/_cstart.o


LIBM_PROJECT_DIR := $(WORKSPACE_DIR)/Library/libm
LIBM_HEADERS_DIR := $(LIBM_PROJECT_DIR)/Headers
LIBM_OBJS_DIR := $(OBJS_DIR)/Library/libm
LIBM_FILE := $(LIBM_OBJS_DIR)/libm.a


LIBCLAP_PROJECT_DIR := $(WORKSPACE_DIR)/Library/libclap
LIBCLAP_HEADERS_DIR := $(LIBCLAP_PROJECT_DIR)/Headers
LIBCLAP_OBJS_DIR := $(OBJS_DIR)/Library/libclap
LIBCLAP_FILE := $(LIBCLAP_OBJS_DIR)/libclap.a


# --------------------------------------------------------------------------
# Project includes
#

include $(LIBSYSTEM_PROJECT_DIR)/project.mk
include $(LIBC_PROJECT_DIR)/project.mk
include $(LIBM_PROJECT_DIR)/project.mk
include $(LIBCLAP_PROJECT_DIR)/project.mk

include $(KERNEL_PROJECT_DIR)/project.mk
include $(KERNEL_TESTS_PROJECT_DIR)/project.mk

include $(SH_PROJECT_DIR)/project.mk
include $(CMDS_PROJECT_DIR)/project.mk


# --------------------------------------------------------------------------
# Build rules
#

.SUFFIXES:
.PHONY: clean


all: build-rom build-boot-dmg
	@echo Done (Configuration: $(BUILD_CONFIGURATION))

build-rom: $(ROM_FILE)

build-boot-dmg: $(BOOT_DMG_FILE)

build-boot-disk: $(LOGIN_FILE) $(SH_FILE) $(TYPE_FILE) $(KERNEL_TESTS_FILE)
	$(call mkdir_if_needed,$(BOOT_DISK_DIR))
	$(call mkdir_if_needed,$(BOOT_DISK_DIR)/System/Commands)
	$(call mkdir_if_needed,$(BOOT_DISK_DIR)/System/Devices)
	$(call mkdir_if_needed,$(BOOT_DISK_DIR)/Users/Administrator)
	$(call copy,$(LOGIN_FILE),$(BOOT_DISK_DIR)/System/Commands/)
	$(call copy,$(SH_FILE),$(BOOT_DISK_DIR)/System/Commands/)
	$(call copy,$(TYPE_FILE),$(BOOT_DISK_DIR)/System/Commands/)
	$(call copy,$(KERNEL_TESTS_FILE),$(BOOT_DISK_DIR)/Users/Administrator/)
	$(call copy,$(DEMOS_DIR)/fibonacci.sh,$(BOOT_DISK_DIR)/Users/Administrator/)
	$(call copy,$(DEMOS_DIR)/helloworld.sh,$(BOOT_DISK_DIR)/Users/Administrator/)
	$(call copy,$(DEMOS_DIR)/prime.sh,$(BOOT_DISK_DIR)/Users/Administrator/)
	$(call copy,$(DEMOS_DIR)/while.sh,$(BOOT_DISK_DIR)/Users/Administrator/)

$(BOOT_DMG_FILE): build-boot-disk | $(PRODUCT_DIR)
	@echo Making boot_disk.adf
	$(DISKIMAGE) create $(BOOT_DMG_CONFIG) $(BOOT_DISK_DIR) $(BOOT_DMG_FILE)

$(ROM_FILE): $(KERNEL_FILE) $(BOOT_DMG_FILE_FOR_ROM) | $(PRODUCT_DIR)
	@echo Making ROM
	$(MAKEROM) $(ROM_FILE) $(KERNEL_FILE) $(BOOT_DMG_FILE_FOR_ROM)

$(PRODUCT_DIR):
	$(call mkdir_if_needed,$(PRODUCT_DIR))

	
clean:
	@echo Cleaning...
	$(call rm_if_exists,$(OBJS_DIR))
	$(call rm_if_exists,$(PRODUCT_DIR))
	$(call rm_if_exists,$(BOOT_DISK_DIR))
	@echo Done

ifdef BOOT_FROM_ROM
clean-rom: clean-kernel clean-kernel-tests clean-cmds clean-sh clean-libc clean-libsystem
	@echo Done
else
clean-rom: clean-kernel
	@echo Done
endif

clean-boot-disk:
	$(call rm_if_exists,$(BOOT_DISK_DIR))
	$(call rm_if_exists,$(BOOT_DMG_FILE))
	@echo Done
