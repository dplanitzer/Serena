# --------------------------------------------------------------------------
# Build variables
#

KERNEL_SOURCES_DIR := $(KERNEL_PROJECT_DIR)/Sources


BOOT_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/boot
BOOT_OBJS_DIR := $(KERNEL_OBJS_DIR)/boot

CONSOLE_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/console
CONSOLE_OBJS_DIR := $(KERNEL_OBJS_DIR)/console

DISK_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/disk
DISK_OBJS_DIR := $(KERNEL_OBJS_DIR)/disk

DISPATCHER_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/dispatcher
DISPATCHER_OBJS_DIR := $(KERNEL_OBJS_DIR)/dispatcher

DISPATCHQUEUE_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/dispatchqueue
DISPATCHQUEUE_OBJS_DIR := $(KERNEL_OBJS_DIR)/dispatchqueue

DRIVER_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/driver
DRIVER_OBJS_DIR := $(KERNEL_OBJS_DIR)/driver

DRIVER_AMIGA_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/driver/amiga
DRIVER_AMIGA_OBJS_DIR := $(KERNEL_OBJS_DIR)/driver/amiga

DRIVER_AMIGA_FLOPPY_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/driver/amiga/floppy
DRIVER_AMIGA_FLOPPY_OBJS_DIR := $(KERNEL_OBJS_DIR)/driver/amiga/floppy

DRIVER_AMIGA_GRAPHICS_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/driver/amiga/graphics
DRIVER_AMIGA_GRAPHICS_OBJS_DIR := $(KERNEL_OBJS_DIR)/driver/amiga/graphics

DRIVER_AMIGA_ZORRO_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/driver/amiga/zorro
DRIVER_AMIGA_ZORRO_OBJS_DIR := $(KERNEL_OBJS_DIR)/driver/amiga/zorro

DRIVER_DISK_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/driver/disk
DRIVER_DISK_OBJS_DIR := $(KERNEL_OBJS_DIR)/driver/disk

DRIVER_HID_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/driver/hid
DRIVER_HID_OBJS_DIR := $(KERNEL_OBJS_DIR)/driver/hid

FILEMANAGER_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/filemanager
FILEMANAGER_OBJS_DIR := $(KERNEL_OBJS_DIR)/filemanager

FILESYSTEM_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/filesystem
FILESYSTEM_OBJS_DIR := $(KERNEL_OBJS_DIR)/filesystem

FILESYSTEM_DEVFS_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/filesystem/devfs
FILESYSTEM_DEVFS_OBJS_DIR := $(KERNEL_OBJS_DIR)/filesystem/devfs

FILESYSTEM_SERENAFS_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/filesystem/serenafs
FILESYSTEM_SERENAFS_OBJS_DIR := $(KERNEL_OBJS_DIR)/filesystem/serenafs

HAL_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/hal
HAL_OBJS_DIR := $(KERNEL_OBJS_DIR)/hal

KLIB_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/klib
KLIB_OBJS_DIR := $(KERNEL_OBJS_DIR)/klib

KOBJ_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/kobj
KOBJ_OBJS_DIR := $(KERNEL_OBJS_DIR)/kobj

KRT_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/krt
KRT_OBJS_DIR := $(KERNEL_OBJS_DIR)/krt

PROCESS_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/process
PROCESS_OBJS_DIR := $(KERNEL_OBJS_DIR)/process

SECURITY_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/security
SECURITY_OBJS_DIR := $(KERNEL_OBJS_DIR)/security


KERNEL_C_SOURCES := $(wildcard $(KERNEL_SOURCES_DIR)/*.c)
KERNEL_ASM_SOURCES := $(wildcard $(KERNEL_SOURCES_DIR)/*.s)

KERNEL_OBJS := $(patsubst $(KERNEL_SOURCES_DIR)/%.c,$(KERNEL_OBJS_DIR)/%.o,$(KERNEL_C_SOURCES))
KERNEL_DEPS := $(KERNEL_OBJS:.o=.d)
KERNEL_OBJS += $(patsubst $(KERNEL_SOURCES_DIR)/%.s,$(KERNEL_OBJS_DIR)/%.o,$(KERNEL_ASM_SOURCES))

KERNEL_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR)
KERNEL_ASM_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR)

#KERNEL_GENERATE_DEPS = -deps -depfile=$(patsubst $(KERNEL_OBJS_DIR)/%.o,$(KERNEL_OBJS_DIR)/%.d,$@)
KERNEL_GENERATE_DEPS := 
KERNEL_AS_DONTWARN := -nowarn=62
KERNEL_CC_DONTWARN := -dontwarn=51,148,208
KERNEL_LD_DONTWARN := -nowarn=22

KERNEL_CC_PREPROC_DEFS := $(CC_PREPROC_DEFS) -D__KERNEL__=1


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-kernel $(KERNEL_OBJS_DIR)


build-kernel: $(KERNEL_FILE)

$(KERNEL_OBJS): | $(KERNEL_OBJS_DIR)

$(KERNEL_OBJS_DIR):
	$(call mkdir_if_needed,$(KERNEL_OBJS_DIR))


-include $(BOOT_SOURCES_DIR)/package.mk
-include $(CONSOLE_SOURCES_DIR)/package.mk
-include $(DISK_SOURCES_DIR)/package.mk
-include $(DISPATCHER_SOURCES_DIR)/package.mk
-include $(DISPATCHQUEUE_SOURCES_DIR)/package.mk
-include $(DRIVER_SOURCES_DIR)/package.mk
-include $(DRIVER_AMIGA_SOURCES_DIR)/package.mk
-include $(DRIVER_AMIGA_FLOPPY_SOURCES_DIR)/package.mk
-include $(DRIVER_AMIGA_GRAPHICS_SOURCES_DIR)/package.mk
-include $(DRIVER_AMIGA_ZORRO_SOURCES_DIR)/package.mk
-include $(DRIVER_DISK_SOURCES_DIR)/package.mk
-include $(DRIVER_HID_SOURCES_DIR)/package.mk
-include $(FILEMANAGER_SOURCES_DIR)/package.mk
-include $(FILESYSTEM_SOURCES_DIR)/package.mk
-include $(FILESYSTEM_DEVFS_SOURCES_DIR)/package.mk
-include $(FILESYSTEM_SERENAFS_SOURCES_DIR)/package.mk
-include $(HAL_SOURCES_DIR)/package.mk
-include $(KRT_SOURCES_DIR)/package.mk
-include $(KOBJ_SOURCES_DIR)/package.mk
-include $(KLIB_SOURCES_DIR)/package.mk
-include $(PROCESS_SOURCES_DIR)/package.mk
-include $(SECURITY_SOURCES_DIR)/package.mk


$(KERNEL_FILE):	$(BOOT_OBJS) \
				$(KRT_OBJS) $(KOBJ_OBJS) $(KLIB_OBJS) $(CONSOLE_OBJS) \
				$(DISK_OBJS) $(DISPATCHER_OBJS) $(DISPATCHQUEUE_OBJS) \
				$(DRIVER_AMIGA_OBJS) \
				$(DRIVER_AMIGA_FLOPPY_OBJS) $(DRIVER_AMIGA_GRAPHICS_OBJS) \
				$(DRIVER_AMIGA_ZORRO_OBJS) \
				$(DRIVER_OBJS) $(DRIVER_DISK_OBJS) $(DRIVER_HID_OBJS) \
				$(FILEMANAGER_OBJS) $(FILESYSTEM_OBJS) \
				$(FILESYSTEM_DEVFS_OBJS) $(FILESYSTEM_SERENAFS_OBJS) \
				$(HAL_OBJS) $(PROCESS_OBJS) $(SECURITY_OBJS) $(KERNEL_OBJS)
	@echo Linking Kernel
	@$(LD) $(KERNEL_LD_CONFIG) $(KERNEL_LD_DONTWARN) -s -o $@ $^


-include $(KERNEL_DEPS)

$(KERNEL_OBJS_DIR)/%.o : $(KERNEL_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(KERNEL_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(KERNEL_GENERATE_DEPS) -o $@ $<

$(KERNEL_OBJS_DIR)/%.o : $(KERNEL_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_CONFIG) $(KERNEL_ASM_INCLUDES) $(KERNEL_AS_DONTWARN) -o $@ $<


clean-kernel:
	$(call rm_if_exists,$(KERNEL_OBJS_DIR))
