# --------------------------------------------------------------------------
# Build variables
#

BOOT_C_SOURCES := $(wildcard $(BOOT_SOURCES_DIR)/*.c)
BOOT_ASM_SOURCES := $(wildcard $(BOOT_SOURCES_DIR)/*.s)

BOOT_OBJS := $(patsubst $(BOOT_SOURCES_DIR)/%.c,$(BOOT_OBJS_DIR)/%.o,$(BOOT_C_SOURCES))
BOOT_DEPS := $(BOOT_OBJS:.o=.d)
BOOT_OBJS += $(patsubst $(BOOT_SOURCES_DIR)/%.s,$(BOOT_OBJS_DIR)/%.o,$(BOOT_ASM_SOURCES))

BOOT_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(BOOT_SOURCES_DIR)
BOOT_ASM_INCLUDES := $(KERNEL_ASM_INCLUDES) -I$(BOOT_SOURCES_DIR)

#BOOT_GENERATE_DEPS = -deps -depfile=$(patsubst $(BOOT_OBJS_DIR)/%.o,$(BOOT_OBJS_DIR)/%.d,$@)
BOOT_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(BOOT_OBJS): | $(BOOT_OBJS_DIR)

$(BOOT_OBJS_DIR):
	$(call mkdir_if_needed,$(BOOT_OBJS_DIR))

-include $(BOOT_DEPS)

$(BOOT_OBJS_DIR)/%.o : $(BOOT_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(BOOT_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(BOOT_GENERATE_DEPS) -o $@ $<

$(BOOT_OBJS_DIR)/%.o : $(BOOT_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_CONFIG) $(BOOT_ASM_INCLUDES) $(KERNEL_AS_DONTWARN) -o $@ $<
