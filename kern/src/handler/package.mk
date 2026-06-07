# --------------------------------------------------------------------------
# Build variables
#

HND_C_SOURCES := $(wildcard $(HND_SOURCES_DIR)/*.c)

HND_OBJS := $(patsubst $(HND_SOURCES_DIR)/%.c,$(HND_OBJS_DIR)/%.o,$(HND_C_SOURCES))
HND_DEPS := $(HND_OBJS:.o=.d)

HND_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(HND_SOURCES_DIR)

#HND_GENERATE_DEPS = -deps -depfile=$(patsubst $(HND_OBJS_DIR)/%.o,$(HND_OBJS_DIR)/%.d,$@)
HND_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(HND_OBJS): | $(HND_OBJS_DIR) $(PRODUCT_DIR)

$(HND_OBJS_DIR):
	$(call mkdir_if_needed,$(HND_OBJS_DIR))

-include $(HND_DEPS)

$(HND_OBJS_DIR)/%.o : $(HND_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(HND_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(HND_GENERATE_DEPS) -o $@ $<
