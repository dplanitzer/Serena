# --------------------------------------------------------------------------
# Build variables
#

STDLIB_C_SOURCES := $(wildcard $(STDLIB_SOURCES_DIR)/*.c)

STDLIB_OBJS := $(patsubst $(STDLIB_SOURCES_DIR)/%.c,$(STDLIB_OBJS_DIR)/%.o,$(STDLIB_C_SOURCES))
STDLIB_DEPS := $(STDLIB_OBJS:.o=.d)

STDLIB_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(STDLIB_SOURCES_DIR)

#STDLIB_GENERATE_DEPS = -deps -depfile=$(patsubst $(STDLIB_OBJS_DIR)/%.o,$(STDLIB_OBJS_DIR)/%.d,$@)
STDLIB_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(STDLIB_OBJS): | $(STDLIB_OBJS_DIR)

$(STDLIB_OBJS_DIR):
	$(call mkdir_if_needed,$(STDLIB_OBJS_DIR))

-include $(STDLIB_DEPS)

$(STDLIB_OBJS_DIR)/%.o : $(STDLIB_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(STDLIB_C_INCLUDES) $(LIBC_CC_DONTWARN) $(STDLIB_GENERATE_DEPS) -o $@ $<
