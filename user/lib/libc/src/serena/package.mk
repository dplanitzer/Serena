#============================================================================
# libc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

SERENA_C_SOURCES := $(wildcard $(SERENA_SOURCES_DIR)/*.c)

SERENA_OBJS := $(patsubst $(SERENA_SOURCES_DIR)/%.c,$(SERENA_OBJS_DIR)/%.o,$(SERENA_C_SOURCES))
SERENA_DEPS := $(SERENA_OBJS:.o=.d)

SERENA_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(SERENA_SOURCES_DIR)

#SERENA_GENERATE_DEPS = -deps -depfile=$(patsubst $(SERENA_OBJS_DIR)/%.o,$(SERENA_OBJS_DIR)/%.d,$@)
SERENA_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(SERENA_OBJS): | $(SERENA_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(SERENA_OBJS_DIR):
	$(call mkdir_if_needed,$(SERENA_OBJS_DIR))

-include $(SERENA_DEPS)

$(SERENA_OBJS_DIR)/%.o : $(SERENA_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(SERENA_C_INCLUDES) $(LIBC_CC_DONTWARN) $(SERENA_GENERATE_DEPS) -o $@ $<
