# --------------------------------------------------------------------------
# Build variables
#

CONSOLE_C_SOURCES := $(wildcard $(CONSOLE_SOURCES_DIR)/*.c)

CONSOLE_OBJS := $(patsubst $(CONSOLE_SOURCES_DIR)/%.c,$(CONSOLE_BUILD_DIR)/%.o,$(CONSOLE_C_SOURCES))
CONSOLE_DEPS := $(CONSOLE_OBJS:.o=.d)

CONSOLE_C_INCLUDES := -I$(LIBABI_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(CONSOLE_SOURCES_DIR)

#CONSOLE_GENERATE_DEPS = -deps -depfile=$(patsubst $(CONSOLE_BUILD_DIR)/%.o,$(CONSOLE_BUILD_DIR)/%.d,$@)
CONSOLE_GENERATE_DEPS := 
CONSOLE_CC_DONTWARN := -dontwarn=51


# --------------------------------------------------------------------------
# Build rules
#

$(CONSOLE_OBJS): | $(CONSOLE_BUILD_DIR)

$(CONSOLE_BUILD_DIR):
	$(call mkdir_if_needed,$(CONSOLE_BUILD_DIR))

-include $(CONSOLE_DEPS)

$(CONSOLE_BUILD_DIR)/%.o : $(CONSOLE_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) $(CONSOLE_C_INCLUDES) $(CONSOLE_CC_DONTWARN) $(CONSOLE_GENERATE_DEPS) -o $@ $<
