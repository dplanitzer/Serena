# --------------------------------------------------------------------------
# Build variables
#

DRIVER_AMIGA_FLOPPY_C_SOURCES := $(wildcard $(DRIVER_AMIGA_FLOPPY_SOURCES_DIR)/*.c)
DRIVER_AMIGA_FLOPPY_ASM_SOURCES := $(wildcard $(DRIVER_AMIGA_FLOPPY_SOURCES_DIR)/*.s)

DRIVER_AMIGA_FLOPPY_OBJS := $(patsubst $(DRIVER_AMIGA_FLOPPY_SOURCES_DIR)/%.c,$(DRIVER_AMIGA_FLOPPY_OBJS_DIR)/%.o,$(DRIVER_AMIGA_FLOPPY_C_SOURCES))
DRIVER_AMIGA_FLOPPY_DEPS := $(DRIVER_AMIGA_FLOPPY_OBJS:.o=.d)
DRIVER_AMIGA_FLOPPY_OBJS += $(patsubst $(DRIVER_AMIGA_FLOPPY_SOURCES_DIR)/%.s,$(DRIVER_AMIGA_FLOPPY_OBJS_DIR)/%.o,$(DRIVER_AMIGA_FLOPPY_ASM_SOURCES))

DRIVER_AMIGA_FLOPPY_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(DRIVER_AMIGA_FLOPPY_SOURCES_DIR)
DRIVER_AMIGA_FLOPPY_ASM_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(DRIVER_AMIGA_FLOPPY_SOURCES_DIR)

#DRIVER_AMIGA_FLOPPY_GENERATE_DEPS = -deps -depfile=$(patsubst $(DRIVER_AMIGA_FLOPPY_OBJS_DIR)/%.o,$(DRIVER_AMIGA_FLOPPY_OBJS_DIR)/%.d,$@)
DRIVER_AMIGA_FLOPPY_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(DRIVER_AMIGA_FLOPPY_OBJS): | $(DRIVER_AMIGA_FLOPPY_OBJS_DIR)

$(DRIVER_AMIGA_FLOPPY_OBJS_DIR):
	$(call mkdir_if_needed,$(DRIVER_AMIGA_FLOPPY_OBJS_DIR))

-include $(DRIVER_AMIGA_FLOPPY_DEPS)

$(DRIVER_AMIGA_FLOPPY_OBJS_DIR)/%.o : $(DRIVER_AMIGA_FLOPPY_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(DRIVER_AMIGA_FLOPPY_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(DRIVER_AMIGA_FLOPPY_GENERATE_DEPS) -o $@ $<

$(DRIVER_AMIGA_FLOPPY_OBJS_DIR)/%.o : $(DRIVER_AMIGA_FLOPPY_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_CONFIG) $(DRIVER_AMIGA_FLOPPY_ASM_INCLUDES) $(KERNEL_AS_DONTWARN) -o $@ $<
