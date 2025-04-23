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
# OS
#

KERNEL_PROJECT_DIR := $(WORKSPACE_DIR)/Kernel
KERNEL_TESTS_PROJECT_DIR := $(WORKSPACE_DIR)/Kernel/Tests
KERNEL_OBJS_DIR := $(OBJS_DIR)/Kernel
KERNEL_TESTS_OBJS_DIR := $(OBJS_DIR)/KernelTests
KERNEL_FILE := $(KERNEL_OBJS_DIR)/Kernel.bin
KERNEL_TESTS_FILE := $(KERNEL_TESTS_OBJS_DIR)/test


ROM_FILE := $(PRODUCT_DIR)/Serena.rom

ifdef BOOT_FROM_ROM
BOOT_DMG_FILE := $(PRODUCT_DIR)/boot_disk.smg
BOOT_DMG_CONFIG := --size=256k smg
BOOT_DMG_FILE_FOR_ROM := $(BOOT_DMG_FILE)
else
BOOT_DMG_FILE := $(PRODUCT_DIR)/boot_disk.adf
BOOT_DMG_CONFIG := adf-dd
BOOT_DMG_FILE_FOR_ROM :=
endif


SH_PROJECT_DIR := $(WORKSPACE_DIR)/Commands/shell
SH_OBJS_DIR := $(OBJS_DIR)/Commands/shell
SH_FILE := $(SH_OBJS_DIR)/shell


SYSTEMD_PROJECT_DIR := $(WORKSPACE_DIR)/Commands/systemd
SYSTEMD_OBJS_DIR := $(OBJS_DIR)/Commands/systemd
SYSTEMD_FILE := $(SYSTEMD_OBJS_DIR)/systemd


CMDS_PROJECT_DIR := $(WORKSPACE_DIR)/Commands
CMDS_OBJS_DIR := $(OBJS_DIR)/Commands
DELETE_FILE := $(CMDS_OBJS_DIR)/delete
DISK_FILE := $(CMDS_OBJS_DIR)/disk
ID_FILE := $(CMDS_OBJS_DIR)/id
LOGIN_FILE := $(CMDS_OBJS_DIR)/login
MAKEDIR_FILE := $(CMDS_OBJS_DIR)/makedir
RENAME_FILE := $(CMDS_OBJS_DIR)/rename
SHUTDOWN_FILE := $(CMDS_OBJS_DIR)/shutdown
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


#---------------------------------------------------------------------------
# Demos
#

SNAKE_PROJECT_DIR := $(WORKSPACE_DIR)/Demos/snake
SNAKE_OBJS_DIR := $(OBJS_DIR)/Demos/snake
SNAKE_FILE := $(SNAKE_OBJS_DIR)/snake


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
include $(SYSTEMD_PROJECT_DIR)/project.mk
include $(CMDS_PROJECT_DIR)/project.mk

include $(SNAKE_PROJECT_DIR)/project.mk


# --------------------------------------------------------------------------
# Build rules
#

.SUFFIXES:
.PHONY: clean


all: build-rom build-boot-dmg
	@echo Done (Configuration: $(BUILD_CONFIGURATION))


build-rom: $(ROM_FILE)


build-boot-dmg: $(BOOT_DMG_FILE)


$(BOOT_DMG_FILE): $(SNAKE_FILE) $(SH_FILE) $(SYSTEMD_FILE) $(DELETE_FILE) $(DISK_FILE) $(ID_FILE) $(LOGIN_FILE) $(MAKEDIR_FILE) $(RENAME_FILE) $(SHUTDOWN_FILE) $(TYPE_FILE) $(KERNEL_TESTS_FILE) | $(PRODUCT_DIR)
	@echo Making boot_disk.adf
	$(DISKIMAGE) create $(BOOT_DMG_CONFIG) $(BOOT_DMG_FILE)
	$(DISKIMAGE) format sefs --label "Serena FD" $(BOOT_DMG_FILE)

	$(DISKIMAGE) makedir -p /System/Commands $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(SYSTEMD_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(LOGIN_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(SH_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(DELETE_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(DISK_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(ID_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(MAKEDIR_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(RENAME_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(SHUTDOWN_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(TYPE_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) makedir -m=rwxr-xr-x /dev $(BOOT_DMG_FILE)
	$(DISKIMAGE) makedir -m=rwxr-xr-x /fs $(BOOT_DMG_FILE)
	$(DISKIMAGE) makedir -m=rwxrwxrwx /tmp $(BOOT_DMG_FILE)
	$(DISKIMAGE) makedir -m=rwxr-xr-x /Users $(BOOT_DMG_FILE)
	$(DISKIMAGE) makedir -m=rwxrwxrwx /Volumes $(BOOT_DMG_FILE)
	$(DISKIMAGE) makedir -m=rwxrwxrwx /Volumes/FD1 $(BOOT_DMG_FILE)

	$(DISKIMAGE) makedir -m=rwxr-x--- -o=1000:1000 -p /Users/admin $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-x--- -o=1000:1000 $(KERNEL_TESTS_FILE) /Users/admin/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rw-r----- -o=1000:1000 $(DEMOS_DIR)/fibonacci.sh /Users/admin/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rw-r----- -o=1000:1000 $(DEMOS_DIR)/helloworld.sh /Users/admin/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rw-r----- -o=1000:1000 $(DEMOS_DIR)/prime.sh /Users/admin/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rw-r----- -o=1000:1000 $(DEMOS_DIR)/while.sh /Users/admin/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-x--- -o=1000:1000 $(SNAKE_FILE) /Users/admin/ $(BOOT_DMG_FILE)


$(ROM_FILE): $(KERNEL_FILE) $(BOOT_DMG_FILE_FOR_ROM) | $(PRODUCT_DIR)
	@echo Making ROM
	$(MAKEROM) $(ROM_FILE) $(KERNEL_FILE) $(BOOT_DMG_FILE_FOR_ROM)


$(PRODUCT_DIR):
	$(call mkdir_if_needed,$(PRODUCT_DIR))

	
clean:
	@echo Cleaning...
	$(call rm_if_exists,$(OBJS_DIR))
	$(call rm_if_exists,$(PRODUCT_DIR))
	@echo Done

ifdef BOOT_FROM_ROM
clean-rom: clean-kernel clean-kernel-tests clean-cmds clean-sh clean-libc clean-libsystem
	@echo Done
else
clean-rom: clean-kernel
	@echo Done
endif

clean-boot-disk:
	$(call rm_if_exists,$(BOOT_DMG_FILE))
	@echo Done
