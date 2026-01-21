# --------------------------------------------------------------------------
# Build variables
#

CONSOLE_C_SOURCES := $(wildcard $(CONSOLE_SOURCES_DIR)/*.c)

CONSOLE_OBJS := $(patsubst $(CONSOLE_SOURCES_DIR)/%.c,$(CONSOLE_OBJS_DIR)/%.o,$(CONSOLE_C_SOURCES))
CONSOLE_DEPS := $(CONSOLE_OBJS:.o=.d)

CONSOLE_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(CONSOLE_SOURCES_DIR)

#CONSOLE_GENERATE_DEPS = -deps -depfile=$(patsubst $(CONSOLE_OBJS_DIR)/%.o,$(CONSOLE_OBJS_DIR)/%.d,$@)
CONSOLE_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(CONSOLE_OBJS): | $(CONSOLE_OBJS_DIR) $(PRODUCT_DIR)

$(CONSOLE_OBJS_DIR):
	$(call mkdir_if_needed,$(CONSOLE_OBJS_DIR))

-include $(CONSOLE_DEPS)

$(CONSOLE_OBJS_DIR)/%.o : $(CONSOLE_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(CONSOLE_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(CONSOLE_GENERATE_DEPS) -o $@ $<
