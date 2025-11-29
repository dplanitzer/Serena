# --------------------------------------------------------------------------
# Build variables
#

DISPATCH_C_SOURCES := $(wildcard $(DISPATCH_SOURCES_DIR)/*.c)

DISPATCH_OBJS := $(patsubst $(DISPATCH_SOURCES_DIR)/%.c,$(DISPATCH_OBJS_DIR)/%.o,$(DISPATCH_C_SOURCES))
DISPATCH_DEPS := $(DISPATCH_OBJS:.o=.d)

DISPATCH_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(DISPATCH_SOURCES_DIR)

#DISPATCH_GENERATE_DEPS = -deps -depfile=$(patsubst $(DISPATCH_OBJS_DIR)/%.o,$(DISPATCH_OBJS_DIR)/%.d,$@)
DISPATCH_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(DISPATCH_OBJS): | $(DISPATCH_OBJS_DIR)

$(DISPATCH_OBJS_DIR):
	$(call mkdir_if_needed,$(DISPATCH_OBJS_DIR))

-include $(DISPATCH_DEPS)

$(DISPATCH_OBJS_DIR)/%.o : $(DISPATCH_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(DISPATCH_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(DISPATCH_GENERATE_DEPS) -o $@ $<
