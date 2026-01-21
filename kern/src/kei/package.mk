# --------------------------------------------------------------------------
# Build variables
#

KEI_C_SOURCES := $(wildcard $(KEI_SOURCES_DIR)/*.c)

KEI_OBJS := $(patsubst $(KEI_SOURCES_DIR)/%.c,$(KEI_OBJS_DIR)/%.o,$(KEI_C_SOURCES))
KEI_DEPS := $(KEI_OBJS:.o=.d)

KEI_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(KEI_SOURCES_DIR)

#KEI_GENERATE_DEPS = -deps -depfile=$(patsubst $(KEI_OBJS_DIR)/%.o,$(KEI_OBJS_DIR)/%.d,$@)
KEI_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(KEI_OBJS): | $(KEI_OBJS_DIR) $(PRODUCT_DIR)

$(KEI_OBJS_DIR):
	$(call mkdir_if_needed,$(KEI_OBJS_DIR))

-include $(KEI_DEPS)

$(KEI_OBJS_DIR)/%.o : $(KEI_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(KEI_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(KEI_GENERATE_DEPS) -o $@ $<
