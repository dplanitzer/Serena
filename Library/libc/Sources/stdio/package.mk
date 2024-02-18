# --------------------------------------------------------------------------
# Build variables
#

STDIO_C_SOURCES := $(wildcard $(STDIO_SOURCES_DIR)/*.c)

STDIO_OBJS := $(patsubst $(STDIO_SOURCES_DIR)/%.c,$(STDIO_OBJS_DIR)/%.o,$(STDIO_C_SOURCES))
STDIO_DEPS := $(STDIO_OBJS:.o=.d)

STDIO_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(LIBC_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(STDIO_SOURCES_DIR)

#STDIO_GENERATE_DEPS = -deps -depfile=$(patsubst $(STDIO_OBJS_DIR)/%.o,$(STDIO_OBJS_DIR)/%.d,$@)
STDIO_GENERATE_DEPS := 
STDIO_CC_DONTWARN := -dontwarn=214


# --------------------------------------------------------------------------
# Build rules
#

$(STDIO_OBJS): | $(STDIO_OBJS_DIR)

$(STDIO_OBJS_DIR):
	$(call mkdir_if_needed,$(STDIO_OBJS_DIR))

-include $(STDIO_DEPS)

$(STDIO_OBJS_DIR)/%.o : $(STDIO_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(STDIO_C_INCLUDES) $(STDIO_CC_DONTWARN) $(STDIO_GENERATE_DEPS) -o $@ $<
