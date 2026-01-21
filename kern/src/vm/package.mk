# --------------------------------------------------------------------------
# Build variables
#

VM_C_SOURCES := $(wildcard $(VM_SOURCES_DIR)/*.c)

VM_OBJS := $(patsubst $(VM_SOURCES_DIR)/%.c,$(VM_OBJS_DIR)/%.o,$(VM_C_SOURCES))
VM_DEPS := $(VM_OBJS:.o=.d)

VM_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(VM_SOURCES_DIR)

#VM_GENERATE_DEPS = -deps -depfile=$(patsubst $(VM_OBJS_DIR)/%.o,$(VM_OBJS_DIR)/%.d,$@)
VM_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(VM_OBJS): | $(VM_OBJS_DIR) $(PRODUCT_DIR)

$(VM_OBJS_DIR):
	$(call mkdir_if_needed,$(VM_OBJS_DIR))

-include $(VM_DEPS)

$(VM_OBJS_DIR)/%.o : $(VM_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(VM_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(VM_GENERATE_DEPS) -o $@ $<
