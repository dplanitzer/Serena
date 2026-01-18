# --------------------------------------------------------------------------
# Build variables
#

BUILTINS_C_SOURCES := $(wildcard $(BUILTINS_SOURCES_DIR)/*.c)

BUILTINS_OBJS := $(patsubst $(BUILTINS_SOURCES_DIR)/%.c,$(BUILTINS_OBJS_DIR)/%.o,$(BUILTINS_C_SOURCES))
BUILTINS_DEPS := $(BUILTINS_OBJS:.o=.d)

BUILTINS_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBCLAP_HEADERS_DIR) -I$(BUILTINS_SOURCES_DIR) -I$(SH_SOURCES_DIR)

#BUILTINS_GENERATE_DEPS = -deps -depfile=$(patsubst $(BUILTINS_OBJS_DIR)/%.o,$(BUILTINS_OBJS_DIR)/%.d,$@)
BUILTINS_GENERATE_DEPS := 
BUILTINS_CC_DONTWARN :=


# --------------------------------------------------------------------------
# Build rules
#

$(BUILTINS_OBJS): | $(BUILTINS_OBJS_DIR)

$(BUILTINS_OBJS_DIR):
	$(call mkdir_if_needed,$(BUILTINS_OBJS_DIR))

-include $(BUILTINS_DEPS)

$(BUILTINS_OBJS_DIR)/%.o : $(BUILTINS_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(BUILTINS_C_INCLUDES) $(BUILTINS_CC_DONTWARN) $(BUILTINS_GENERATE_DEPS) -o $@ $<
