# Expects that an environment variable 'VBCC' exists and that it points to the vbcc tool chain directory.
# Eg VBCC='C:\Program Files\vbcc'
#
# Expects that the vbcc tool chain 'bin' folder is listed in the PATH environment variable.
#


# --------------------------------------------------------------------------
# Common Directories
#

WORKSPACE_DIR := $(CURDIR)
SCRIPTS_DIR := $(WORKSPACE_DIR)/dev/etc
BUILD_DIR := $(WORKSPACE_DIR)/build
TOOLS_DIR := $(BUILD_DIR)/host
OBJS_DIR := $(BUILD_DIR)/objs
PRODUCT_DIR := $(BUILD_DIR)/product
PRODUCT_CMD_DIR := $(PRODUCT_DIR)/cmd
PRODUCT_DEMO_DIR := $(PRODUCT_DIR)/demo
PRODUCT_LIB_DIR := $(PRODUCT_DIR)/lib

USER_DIR := $(WORKSPACE_DIR)/user
CMD_DIR := $(USER_DIR)/cmd
CMD_OBJS_DIR := $(OBJS_DIR)/cmd
DEMOS_DIR := $(USER_DIR)/demo
DEMOS_OBJS_DIR := $(OBJS_DIR)/demo
LIB_DIR := $(USER_DIR)/lib
LIB_OBJS_DIR := $(OBJS_DIR)/lib


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
# Common Includes
#

include $(WORKSPACE_DIR)/common.mk


# --------------------------------------------------------------------------
# Kernel
#

KERNEL_PROJECT_DIR := $(WORKSPACE_DIR)/kern
KERNEL_HEADERS_DIR := $(KERNEL_PROJECT_DIR)/h
KERNEL_FILE := $(PRODUCT_DIR)/kernel.bin


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


#---------------------------------------------------------------------------
# Libraries
#

LIBC_PROJECT_DIR := $(LIB_DIR)/libc
LIBC_HEADERS_DIR := $(LIBC_PROJECT_DIR)/h
LIBC_FILE := $(PRODUCT_LIB_DIR)/libc.a
CSTART_FILE := $(PRODUCT_LIB_DIR)/_cstart.o

LIBSC_FILE := $(PRODUCT_LIB_DIR)/libsc.a


LIBCLAP_PROJECT_DIR := $(LIB_DIR)/libclap
LIBCLAP_HEADERS_DIR := $(LIBCLAP_PROJECT_DIR)/h
LIBCLAP_FILE := $(PRODUCT_LIB_DIR)/libclap.a


LIBDISPATCH_PROJECT_DIR := $(LIB_DIR)/libdispatch
LIBDISPATCH_HEADERS_DIR := $(LIBDISPATCH_PROJECT_DIR)/h
LIBDISPATCH_FILE := $(PRODUCT_LIB_DIR)/libdispatch.a


LIBM_PROJECT_DIR := $(LIB_DIR)/libm
LIBM_HEADERS_DIR := $(LIBM_PROJECT_DIR)/h
LIBM_FILE := $(PRODUCT_LIB_DIR)/libm.a


#---------------------------------------------------------------------------
# Commands
#

DISKTOOL_PROJECT_DIR := $(CMD_DIR)/disk
DISKTOOL_FILE := $(PRODUCT_CMD_DIR)/disk

SH_PROJECT_DIR := $(CMD_DIR)/shell
SH_FILE := $(PRODUCT_CMD_DIR)/shell


SYSTEMD_PROJECT_DIR := $(CMD_DIR)/systemd
SYSTEMD_FILE := $(PRODUCT_CMD_DIR)/systemd


CMD_BIN_PROJECT_DIR := $(CMD_DIR)/bin
COPY_FILE := $(PRODUCT_CMD_DIR)/copy
DELETE_FILE := $(PRODUCT_CMD_DIR)/delete
DISK_FILE := $(PRODUCT_CMD_DIR)/disk
ID_FILE := $(PRODUCT_CMD_DIR)/id
LIST_FILE := $(PRODUCT_CMD_DIR)/list
LOGIN_FILE := $(PRODUCT_CMD_DIR)/login
MAKEDIR_FILE := $(PRODUCT_CMD_DIR)/makedir
RENAME_FILE := $(PRODUCT_CMD_DIR)/rename
SHUTDOWN_FILE := $(PRODUCT_CMD_DIR)/shutdown
STATUS_FILE := $(PRODUCT_CMD_DIR)/status
TOUCH_FILE := $(PRODUCT_CMD_DIR)/touch
TYPE_FILE := $(PRODUCT_CMD_DIR)/type
UPTIME_FILE := $(PRODUCT_CMD_DIR)/uptime
WAIT_FILE := $(PRODUCT_CMD_DIR)/wait


#---------------------------------------------------------------------------
# Demos
#

HELLODISPATCH_PROJECT_DIR := $(DEMOS_DIR)/hellodispatch
HELLODISPATCH_FILE := $(PRODUCT_DEMO_DIR)/hellodispatch

SNAKE_PROJECT_DIR := $(DEMOS_DIR)/snake
SNAKE_FILE := $(PRODUCT_DEMO_DIR)/snake

KERNEL_TESTS_PROJECT_DIR := $(WORKSPACE_DIR)/kern/test
KERNEL_TESTS_FILE := $(PRODUCT_DEMO_DIR)/test


# --------------------------------------------------------------------------
# SDK
#

ifeq ($(OS),Windows_NT)
HOST_TARGET_NAME := x86_64-windows
else
HOST_TARGET_NAME := arm64-macos
endif

SDK_DIR := $(PRODUCT_DIR)/serena-sdk
SDK_BIN_HOST_DIR := $(SDK_DIR)/bin/$(HOST_TARGET_NAME)
SDK_CONF_DIR := $(SDK_DIR)/conf
SDK_EXAMPLE_DIR := $(SDK_DIR)/example
SDK_INCLUDE_DIR := $(SDK_DIR)/include
SDK_LIB_DIR := $(SDK_DIR)/lib


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
	CC_KOPT_SETTING := -O3 -size
	CC_GEN_DEBUG_INFO :=
else
	CC_OPT_SETTING := -O0
	CC_KOPT_SETTING := -O0
	CC_GEN_DEBUG_INFO := -g
endif

# XXX comment out to build a system that boots from floppy disk
#BOOT_FROM_ROM := 1

CC_PREPROC_DEFS := -DDEBUG=1 -DTARGET_CPU_68020=1


#XXX the vbcc config file always defines -D__STDC_HOSTED__=1 and we can't override it for the kernel (which should define -D__STDC_HOSTED__=0)
ifeq ($(OS),Windows_NT)
	VC_CFG := $(SCRIPTS_DIR)/m68k-app-windows.vcfg
	VC_CFG_KERN := $(SCRIPTS_DIR)/m68k-kern-windows.vcfg
else
	VC_CFG := $(SCRIPTS_DIR)/m68k-app-posix.vcfg
	VC_CFG_KERN := $(SCRIPTS_DIR)/m68k-kern-posix.vcfg
endif


KERNEL_ASM_CONFIG := -Felf -quiet -nosym -spaces -m68060 -DTARGET_CPU_68020 -DMACHINE_AMIGA
KERNEL_CC_CONFIG := +$(VC_CFG_KERN) -c -c99 -cpp-comments -DMACHINE_AMIGA -D_POSIX_SOURCE=1 -D__TRY_BANG_LOD=1 -D__ASSERT_LOD=1

USER_ASM_CONFIG := -Felf -quiet -nosym -spaces -m68060 -DTARGET_CPU_68020
USER_CC_CONFIG := +$(VC_CFG) -c -c99 -cpp-comments -fpu=68881 -D_POSIX_SOURCE=1 -D_OPEN_SYS_ITOA_EXT=1

KERNEL_LD_CONFIG := -brawbin1 -T $(SCRIPTS_DIR)/m68k-amiga-kern.ld -M$(PRODUCT_DIR)/kernel_mappings.txt
USER_LD_CONFIG := -bataritos -T $(SCRIPTS_DIR)/app.ld


# --------------------------------------------------------------------------
# Build rules
#

.SUFFIXES:
.PHONY: clean $(PRODUCT_DIR) $(PRODUCT_CMD_DIR) $(PRODUCT_DEMO_DIR) $(PRODUCT_LIB_DIR)


all: build-rom build-boot-dmg
	@echo Done (Configuration: $(BUILD_CONFIGURATION))

build-rom: $(ROM_FILE)

build-boot-dmg: $(BOOT_DMG_FILE)

$(PRODUCT_DIR):
	$(call mkdir_if_needed,$(PRODUCT_DIR))

$(PRODUCT_CMD_DIR):
	$(call mkdir_if_needed,$(PRODUCT_CMD_DIR))

$(PRODUCT_DEMO_DIR):
	$(call mkdir_if_needed,$(PRODUCT_DEMO_DIR))

$(PRODUCT_LIB_DIR):
	$(call mkdir_if_needed,$(PRODUCT_LIB_DIR))


include $(LIBC_PROJECT_DIR)/project.mk
include $(LIBM_PROJECT_DIR)/project.mk
include $(LIBCLAP_PROJECT_DIR)/project.mk
include $(LIBDISPATCH_PROJECT_DIR)/project.mk

include $(KERNEL_PROJECT_DIR)/project.mk
include $(KERNEL_TESTS_PROJECT_DIR)/project.mk

include $(DISKTOOL_PROJECT_DIR)/project.mk
include $(SH_PROJECT_DIR)/project.mk
include $(SYSTEMD_PROJECT_DIR)/project.mk
include $(CMD_BIN_PROJECT_DIR)/project.mk

include $(HELLODISPATCH_PROJECT_DIR)/project.mk
include $(SNAKE_PROJECT_DIR)/project.mk


build-all-libs: $(LIBC_FILE) $(LIBM_FILE) $(LIBCLAP_FILE) $(LIBDISPATCH_FILE)

build-all-cmds:	$(SH_FILE) $(SYSTEMD_FILE) $(DISKTOOL_FILE) \
				$(COPY_FILE) $(DELETE_FILE) $(ID_FILE) $(LIST_FILE) \
				$(LOGIN_FILE) $(MAKEDIR_FILE) $(RENAME_FILE) \
				$(SHUTDOWN_FILE) $(STATUS_FILE) $(TOUCH_FILE) $(TYPE_FILE) \
				$(UPTIME_FILE) $(WAIT_FILE)

build-all-demos: $(HELLODISPATCH_FILE) $(SNAKE_FILE) $(KERNEL_TESTS_FILE)

$(BOOT_DMG_FILE): build-all-libs build-all-cmds build-all-demos
	@echo Making boot_disk.adf
	$(DISKIMAGE) create $(BOOT_DMG_CONFIG) $(BOOT_DMG_FILE)
	$(DISKIMAGE) format sefs --label "Serena FD" $(BOOT_DMG_FILE)

	$(DISKIMAGE) makedir -p /System/Commands $(BOOT_DMG_FILE)
	$(DISKIMAGE) makedir -m=rwxr-xr-x /dev $(BOOT_DMG_FILE)
	$(DISKIMAGE) makedir -m=rwxr-xr-x /fs $(BOOT_DMG_FILE)
	$(DISKIMAGE) makedir -m=rwxr-xr-x /proc $(BOOT_DMG_FILE)
	$(DISKIMAGE) makedir -m=rwxrwxrwx /tmp $(BOOT_DMG_FILE)
	$(DISKIMAGE) makedir -m=rwxr-xr-x /Users $(BOOT_DMG_FILE)
	$(DISKIMAGE) makedir -m=rwxrwxrwx /Volumes $(BOOT_DMG_FILE)
	$(DISKIMAGE) makedir -m=rwxrwxrwx /Volumes/FD1 $(BOOT_DMG_FILE)
	$(DISKIMAGE) makedir -m=rwxr-x--- -o=1000:1000 -p /Users/admin $(BOOT_DMG_FILE)

	$(DISKIMAGE) push -m=rwxr-xr-x $(SYSTEMD_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(LOGIN_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(SH_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(COPY_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(DELETE_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(DISKTOOL_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(ID_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(LIST_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(MAKEDIR_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(RENAME_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(SHUTDOWN_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(STATUS_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(TOUCH_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(TYPE_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(UPTIME_FILE) /System/Commands/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-xr-x $(WAIT_FILE) /System/Commands/ $(BOOT_DMG_FILE)

	$(DISKIMAGE) push -m=rwxr-x--- -o=1000:1000 $(KERNEL_TESTS_FILE) /Users/admin/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rw-r----- -o=1000:1000 $(DEMOS_DIR)/fibonacci.sh /Users/admin/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rw-r----- -o=1000:1000 $(DEMOS_DIR)/helloworld.sh /Users/admin/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rw-r----- -o=1000:1000 $(DEMOS_DIR)/prime.sh /Users/admin/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rw-r----- -o=1000:1000 $(DEMOS_DIR)/while.sh /Users/admin/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-x--- -o=1000:1000 $(HELLODISPATCH_FILE) /Users/admin/ $(BOOT_DMG_FILE)
	$(DISKIMAGE) push -m=rwxr-x--- -o=1000:1000 $(SNAKE_FILE) /Users/admin/ $(BOOT_DMG_FILE)

$(ROM_FILE): $(KERNEL_FILE) $(BOOT_DMG_FILE_FOR_ROM)
	@echo Making ROM
	$(MAKEROM) $(ROM_FILE) $(KERNEL_FILE) $(BOOT_DMG_FILE_FOR_ROM)


build-sdk: build-all-libs
	@echo Making SDK
	$(call mkdir_if_needed,$(SDK_BIN_HOST_DIR))
	$(call copy_executable,$(TOOLS_DIR)/diskimage,$(SDK_BIN_HOST_DIR)/)
	$(call copy_executable,$(TOOLS_DIR)/libtool,$(SDK_BIN_HOST_DIR)/)
	$(call copy_executable,$(TOOLS_DIR)/keymap,$(SDK_BIN_HOST_DIR)/)
	$(call copy_executable,$(TOOLS_DIR)/makerom,$(SDK_BIN_HOST_DIR)/)

	$(call mkdir_if_needed,$(SDK_CONF_DIR))
	$(call copy,$(SCRIPTS_DIR)/app.ld,$(SDK_CONF_DIR)/)
	$(call copy,$(SCRIPTS_DIR)/m68k-app-posix.vcfg,$(SDK_CONF_DIR)/)
	$(call copy,$(SCRIPTS_DIR)/m68k-app-windows.vcfg,$(SDK_CONF_DIR)/)

	$(call mkdir_if_needed,$(SDK_EXAMPLE_DIR))
	$(call copy,$(HELLODISPATCH_PROJECT_DIR)/,$(SDK_EXAMPLE_DIR)/)
# XXX not great. The copy above should really filter out the 'project.mk' file
	$(call rm_if_exists,$(SDK_EXAMPLE_DIR)/hellodispatch/project.mk)

# XXX all these copy functions should filter out files of the form '__*.h'
	$(call mkdir_if_needed,$(SDK_INCLUDE_DIR))
	$(call copy_contents_of_dir,$(LIBC_HEADERS_DIR),$(SDK_INCLUDE_DIR)/)
	$(call copy_contents_of_dir,$(LIBCLAP_HEADERS_DIR),$(SDK_INCLUDE_DIR)/)
	$(call copy_contents_of_dir,$(LIBDISPATCH_HEADERS_DIR),$(SDK_INCLUDE_DIR)/)
	$(call copy_contents_of_dir,$(LIBM_HEADERS_DIR),$(SDK_INCLUDE_DIR)/)
	$(call copy,$(KERNEL_HEADERS_DIR)/kpi,$(SDK_INCLUDE_DIR)/)
	$(call copy,$(KERNEL_HEADERS_DIR)/machine,$(SDK_INCLUDE_DIR)/)

	$(call copy,$(PRODUCT_LIB_DIR),$(SDK_LIB_DIR))
# XXX not great. The copy above should really filter out the 'libsc.a' file
	$(call rm_if_exists,$(SDK_LIB_DIR)/libsc.a)

clean-sdk:
	$(call rm_if_exists,$(SDK_DIR))
	@echo Done

	
clean:
	@echo Cleaning...
	$(call rm_if_exists,$(OBJS_DIR))
	$(call rm_if_exists,$(PRODUCT_DIR))
	@echo Done

ifdef BOOT_FROM_ROM
clean-rom: clean-kernel clean-kernel-tests clean-cmds clean-sh clean-libc
	@echo Done
else
clean-rom: clean-kernel
	@echo Done
endif

clean-boot-disk:
	$(call rm_if_exists,$(BOOT_DMG_FILE))
	@echo Done
