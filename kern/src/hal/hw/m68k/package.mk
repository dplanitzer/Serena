# --------------------------------------------------------------------------
# Build variables
#

HAL_M68K_C_SOURCES := $(wildcard $(HAL_M68K_SOURCES_DIR)/*.c)
HAL_M68K_ASM_SOURCES := $(wildcard $(HAL_M68K_SOURCES_DIR)/*.s)

HAL_M68K_OBJS := $(patsubst $(HAL_M68K_SOURCES_DIR)/%.c,$(HAL_M68K_OBJS_DIR)/%.o,$(HAL_M68K_C_SOURCES))
HAL_M68K_DEPS := $(HAL_M68K_OBJS:.o=.d)
HAL_M68K_OBJS += $(patsubst $(HAL_M68K_SOURCES_DIR)/%.s,$(HAL_M68K_OBJS_DIR)/%.o,$(HAL_M68K_ASM_SOURCES))

HAL_M68K_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(HAL_M68K_SOURCES_DIR)
HAL_M68K_ASM_INCLUDES := $(KERNEL_ASM_INCLUDES) -I$(HAL_M68K_SOURCES_DIR)

#HAL_M68K_GENERATE_DEPS = -deps -depfile=$(patsubst $(HAL_M68K_OBJS_DIR)/%.o,$(HAL_M68K_OBJS_DIR)/%.d,$@)
HAL_M68K_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(HAL_M68K_OBJS): | $(HAL_M68K_OBJS_DIR) $(PRODUCT_DIR)

$(HAL_M68K_OBJS_DIR):
	$(call mkdir_if_needed,$(HAL_M68K_OBJS_DIR))

-include $(HAL_M68K_DEPS)

$(HAL_M68K_OBJS_DIR)/%.o : $(HAL_M68K_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(HAL_M68K_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(HAL_M68K_GENERATE_DEPS) -o $@ $<

$(HAL_M68K_OBJS_DIR)/%.o : $(HAL_M68K_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_CONFIG) $(HAL_M68K_ASM_INCLUDES) $(KERNEL_AS_DONTWARN) -o $@ $<
