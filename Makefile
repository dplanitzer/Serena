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
TOOLS_DIR := $(BUILD_DIR)/tools
OBJS_DIR := $(BUILD_DIR)/objs
PRODUCT_DIR := $(BUILD_DIR)/product
SDK_DIR := $(PRODUCT_DIR)/serena-sdk
SDK_LIB_DIR := $(SDK_DIR)/lib

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
# OS
#

KERNEL_PROJECT_DIR := $(WORKSPACE_DIR)/kern
KERNEL_HEADERS_DIR := $(KERNEL_PROJECT_DIR)/h
KERNEL_TESTS_PROJECT_DIR := $(WORKSPACE_DIR)/kern/test
KERNEL_OBJS_DIR := $(OBJS_DIR)/kern
KERNEL_TESTS_OBJS_DIR := $(OBJS_DIR)/kern-test
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


DISKTOOL_PROJECT_DIR := $(CMD_DIR)/disk
DISKTOOL_OBJS_DIR := $(CMD_OBJS_DIR)/disk
DISKTOOL_FILE := $(DISKTOOL_OBJS_DIR)/disk

SH_PROJECT_DIR := $(CMD_DIR)/shell
SH_OBJS_DIR := $(CMD_OBJS_DIR)/shell
SH_FILE := $(SH_OBJS_DIR)/shell


SYSTEMD_PROJECT_DIR := $(CMD_DIR)/systemd
SYSTEMD_OBJS_DIR := $(CMD_OBJS_DIR)/systemd
SYSTEMD_FILE := $(SYSTEMD_OBJS_DIR)/systemd


CMD_BIN_PROJECT_DIR := $(CMD_DIR)/bin
COPY_FILE := $(CMD_OBJS_DIR)/copy
DELETE_FILE := $(CMD_OBJS_DIR)/delete
DISK_FILE := $(CMD_OBJS_DIR)/disk
ID_FILE := $(CMD_OBJS_DIR)/id
LIST_FILE := $(CMD_OBJS_DIR)/list
LOGIN_FILE := $(CMD_OBJS_DIR)/login
MAKEDIR_FILE := $(CMD_OBJS_DIR)/makedir
RENAME_FILE := $(CMD_OBJS_DIR)/rename
SHUTDOWN_FILE := $(CMD_OBJS_DIR)/shutdown
STATUS_FILE := $(CMD_OBJS_DIR)/status
TOUCH_FILE := $(CMD_OBJS_DIR)/touch
TYPE_FILE := $(CMD_OBJS_DIR)/type
UPTIME_FILE := $(CMD_OBJS_DIR)/uptime
WAIT_FILE := $(CMD_OBJS_DIR)/wait


LIBC_PROJECT_DIR := $(LIB_DIR)/libc
LIBC_HEADERS_DIR := $(LIBC_PROJECT_DIR)/h
LIBC_OBJS_DIR := $(LIB_OBJS_DIR)/libc
LIBC_FILE := $(SDK_LIB_DIR)/libc.a
CSTART_FILE := $(SDK_LIB_DIR)/_cstart.o

LIBSC_OBJS_DIR := $(LIB_OBJS_DIR)/libsc
LIBSC_FILE := $(SDK_LIB_DIR)/libsc.a


LIBCLAP_PROJECT_DIR := $(LIB_DIR)/libclap
LIBCLAP_HEADERS_DIR := $(LIBCLAP_PROJECT_DIR)/h
LIBCLAP_OBJS_DIR := $(LIB_OBJS_DIR)/libclap
LIBCLAP_FILE := $(SDK_LIB_DIR)/libclap.a


LIBDISPATCH_PROJECT_DIR := $(LIB_DIR)/libdispatch
LIBDISPATCH_HEADERS_DIR := $(LIBDISPATCH_PROJECT_DIR)/h
LIBDISPATCH_OBJS_DIR := $(LIB_OBJS_DIR)/libdispatch
LIBDISPATCH_FILE := $(SDK_LIB_DIR)/libdispatch.a


LIBM_PROJECT_DIR := $(LIB_DIR)/libm
LIBM_HEADERS_DIR := $(LIBM_PROJECT_DIR)/h
LIBM_OBJS_DIR := $(LIB_OBJS_DIR)/libm
LIBM_FILE := $(SDK_LIB_DIR)/libm.a


#---------------------------------------------------------------------------
# Demos
#

SNAKE_PROJECT_DIR := $(DEMOS_DIR)/snake
SNAKE_OBJS_DIR := $(DEMOS_OBJS_DIR)/snake
SNAKE_FILE := $(SNAKE_OBJS_DIR)/snake


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

CC_PREPROC_DEFS := -DDEBUG=1 -DTARGET_CPU_68020=1 -D__SERENA__

#XXX vbcc always defines -D__STDC_HOSTED__=1 and we can't override it for the kernel (which should define -D__STDC_HOSTED__=0)
KERNEL_STDC_PREPROC_DEFS := -D__STDC_UTF_16__=1 -D__STDC_UTF_32__=1 -D__STDC_NO_ATOMICS__=1 -D__STDC_NO_COMPLEX__=1 -D__STDC_NO_THREADS__=1 -D__STDC_WANT_LIB_EXT1__=1
USER_STDC_PREPROC_DEFS := -D__STDC_UTF_16__=1 -D__STDC_UTF_32__=1 -D__STDC_NO_ATOMICS__=1 -D__STDC_NO_COMPLEX__=1 -D__STDC_NO_THREADS__=1 -D___STDC_HOSTED__=1


ifeq ($(OS),Windows_NT)
	VC_CONFIG := $(SCRIPTS_DIR)/vc_windows_host.config
else
	VC_CONFIG := $(SCRIPTS_DIR)/vc_posix_host.config
endif


KERNEL_ASM_CONFIG := -Felf -quiet -nosym -spaces -m68060 -DTARGET_CPU_68020 -DMACHINE_AMIGA
KERNEL_CC_CONFIG := +$(VC_CONFIG) -c -c99 -cpp-comments -cpu=68020 -D_POSIX_SOURCE=1 -D_OPEN_SYS_ITOA_EXT=1 -D__TRY_BANG_LOD=1 -D__ASSERT_LOD=1 -DMACHINE_AMIGA $(KERNEL_STDC_PREPROC_DEFS)

USER_ASM_CONFIG := -Felf -quiet -nosym -spaces -m68060 -DTARGET_CPU_68020
USER_CC_CONFIG := +$(VC_CONFIG) -c -c99 -cpp-comments -cpu=68020 -D_POSIX_SOURCE=1 -D_OPEN_SYS_ITOA_EXT=1 $(USER_STDC_PREPROC_DEFS)

KERNEL_LD_CONFIG := -brawbin1 -T $(SCRIPTS_DIR)/kernel_linker.script -M$(KERNEL_OBJS_DIR)/kernel_mappings.txt
USER_LD_CONFIG := -bataritos -T $(SCRIPTS_DIR)/user_linker.script


# --------------------------------------------------------------------------
# Project includes
#

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

include $(SNAKE_PROJECT_DIR)/project.mk


# --------------------------------------------------------------------------
# Build rules
#

.SUFFIXES:
.PHONY: clean


all: build-rom build-boot-dmg
	@echo Done (Configuration: $(BUILD_CONFIGURATION))


build-rom: $(SDK_LIB_DIR) $(ROM_FILE)


build-boot-dmg: $(SDK_LIB_DIR) $(BOOT_DMG_FILE)

$(SDK_LIB_DIR):
	$(call mkdir_if_needed,$(SDK_LIB_DIR))


$(BOOT_DMG_FILE): $(SNAKE_FILE) $(SH_FILE) $(SYSTEMD_FILE) $(DISKTOOL_FILE) \
				  $(COPY_FILE) $(DELETE_FILE) $(ID_FILE) $(LIST_FILE) \
				  $(LOGIN_FILE) $(MAKEDIR_FILE) $(RENAME_FILE) \
				  $(SHUTDOWN_FILE) $(STATUS_FILE) $(TOUCH_FILE) $(TYPE_FILE) \
				  $(UPTIME_FILE) $(WAIT_FILE) $(KERNEL_TESTS_FILE) | $(PRODUCT_DIR)
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
clean-rom: clean-kernel clean-kernel-tests clean-cmds clean-sh clean-libc
	@echo Done
else
clean-rom: clean-kernel
	@echo Done
endif

clean-boot-disk:
	$(call rm_if_exists,$(BOOT_DMG_FILE))
	@echo Done
