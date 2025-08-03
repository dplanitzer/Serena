# --------------------------------------------------------------------------
# Build variables
#

HANDLER_C_SOURCES := $(wildcard $(HANDLER_SOURCES_DIR)/*.c)

HANDLER_OBJS := $(patsubst $(HANDLER_SOURCES_DIR)/%.c,$(HANDLER_OBJS_DIR)/%.o,$(HANDLER_C_SOURCES))
HANDLER_DEPS := $(HANDLER_OBJS:.o=.d)

HANDLER_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(HANDLER_SOURCES_DIR)

#HANDLER_GENERATE_DEPS = -deps -depfile=$(patsubst $(HANDLER_OBJS_DIR)/%.o,$(HANDLER_OBJS_DIR)/%.d,$@)
HANDLER_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(HANDLER_OBJS): | $(HANDLER_OBJS_DIR)

$(HANDLER_OBJS_DIR):
	$(call mkdir_if_needed,$(HANDLER_OBJS_DIR))

-include $(HANDLER_DEPS)

$(HANDLER_OBJS_DIR)/%.o : $(HANDLER_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(HANDLER_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(HANDLER_GENERATE_DEPS) -o $@ $<
