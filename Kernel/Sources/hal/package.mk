# --------------------------------------------------------------------------
# Build variables
#

HAL_C_SOURCES := $(wildcard $(HAL_SOURCES_DIR)/*.c)
HAL_ASM_SOURCES := $(wildcard $(HAL_SOURCES_DIR)/*.s)

HAL_OBJS := $(patsubst $(HAL_SOURCES_DIR)/%.c,$(HAL_OBJS_DIR)/%.o,$(HAL_C_SOURCES))
HAL_DEPS := $(HAL_OBJS:.o=.d)
HAL_OBJS += $(patsubst $(HAL_SOURCES_DIR)/%.s,$(HAL_OBJS_DIR)/%.o,$(HAL_ASM_SOURCES))

HAL_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(HAL_SOURCES_DIR)
HAL_ASM_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(HAL_SOURCES_DIR)

#HAL_GENERATE_DEPS = -deps -depfile=$(patsubst $(HAL_OBJS_DIR)/%.o,$(HAL_OBJS_DIR)/%.d,$@)
HAL_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(HAL_OBJS): | $(HAL_OBJS_DIR)

$(HAL_OBJS_DIR):
	$(call mkdir_if_needed,$(HAL_OBJS_DIR))

-include $(HAL_DEPS)

$(HAL_OBJS_DIR)/%.o : $(HAL_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(HAL_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(HAL_GENERATE_DEPS) -o $@ $<

$(HAL_OBJS_DIR)/%.o : $(HAL_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_CONFIG) $(HAL_ASM_INCLUDES) $(KERNEL_AS_DONTWARN) -o $@ $<
