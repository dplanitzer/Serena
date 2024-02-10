# --------------------------------------------------------------------------
# Build variables
#

BUILTINS_C_SOURCES := $(wildcard $(BUILTINS_SOURCES_DIR)/*.c)

BUILTINS_OBJS := $(patsubst $(BUILTINS_SOURCES_DIR)/%.c,$(BUILTINS_BUILD_DIR)/%.o,$(BUILTINS_C_SOURCES))
BUILTINS_DEPS := $(BUILTINS_OBJS:.o=.d)

BUILTINS_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(LIBC_HEADERS_DIR) -I$(BUILTINS_SOURCES_DIR) -I$(SH_SOURCES_DIR)

#BUILTINS_GENERATE_DEPS = -deps -depfile=$(patsubst $(BUILTINS_BUILD_DIR)/%.o,$(BUILTINS_BUILD_DIR)/%.d,$@)
BUILTINS_GENERATE_DEPS := 
BUILTINS_CC_DONTWARN :=


# --------------------------------------------------------------------------
# Build rules
#

$(BUILTINS_OBJS): | $(BUILTINS_BUILD_DIR)

$(BUILTINS_BUILD_DIR):
	$(call mkdir_if_needed,$(BUILTINS_BUILD_DIR))

-include $(BUILTINS_DEPS)

$(BUILTINS_BUILD_DIR)/%.o : $(BUILTINS_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) $(BUILTINS_C_INCLUDES) $(BUILTINS_CC_DONTWARN) $(BUILTINS_GENERATE_DEPS) -o $@ $<
