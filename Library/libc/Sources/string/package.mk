# --------------------------------------------------------------------------
# Build variables
#

STRING_C_SOURCES := $(wildcard $(STRING_SOURCES_DIR)/*.c)

STRING_OBJS := $(patsubst $(STRING_SOURCES_DIR)/%.c,$(STRING_OBJS_DIR)/%.o,$(STRING_C_SOURCES))
STRING_DEPS := $(STRING_OBJS:.o=.d)

STRING_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(LIBC_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(STRING_SOURCES_DIR)

#STRING_GENERATE_DEPS = -deps -depfile=$(patsubst $(STRING_OBJS_DIR)/%.o,$(STRING_OBJS_DIR)/%.d,$@)
STRING_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(STRING_OBJS): | $(STRING_OBJS_DIR)

$(STRING_OBJS_DIR):
	$(call mkdir_if_needed,$(STRING_OBJS_DIR))

-include $(STRING_DEPS)

$(STRING_OBJS_DIR)/%.o : $(STRING_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(STRING_C_INCLUDES) $(LIBC_CC_DONTWARN) $(STRING_GENERATE_DEPS) -o $@ $<
