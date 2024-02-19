# --------------------------------------------------------------------------
# Build variables
#

M68K_C_SOURCES := $(wildcard $(M68K_SOURCES_DIR)/*.c)
M68K_ASM_SOURCES := $(wildcard $(M68K_SOURCES_DIR)/*.s)

M68K_OBJS := $(patsubst $(M68K_SOURCES_DIR)/%.c,$(M68K_OBJS_DIR)/%.o,$(M68K_C_SOURCES))
M68K_DEPS := $(M68K_OBJS:.o=.d)
M68K_OBJS += $(patsubst $(M68K_SOURCES_DIR)/%.s,$(M68K_OBJS_DIR)/%.o,$(M68K_ASM_SOURCES))

M68K_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(M68K_SOURCES_DIR)
M68K_ASM_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(M68K_SOURCES_DIR)

#M68K_GENERATE_DEPS = -deps -depfile=$(patsubst $(M68K_OBJS_DIR)/%.o,$(M68K_OBJS_DIR)/%.d,$@)
M68K_GENERATE_DEPS := 
M68K_AS_DONTWARN :=
M68K_CC_DONTWARN :=


# --------------------------------------------------------------------------
# Build rules
#

$(M68K_OBJS): | $(M68K_OBJS_DIR)

$(M68K_OBJS_DIR):
	$(call mkdir_if_needed,$(M68K_OBJS_DIR))

-include $(M68K_DEPS)

$(M68K_OBJS_DIR)/%.o : $(M68K_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(USER_CC_PREPROC_DEFS) $(M68K_C_INCLUDES) $(M68K_CC_DONTWARN) $(M68K_GENERATE_DEPS) -o $@ $<

$(M68K_OBJS_DIR)/%.o : $(M68K_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(USER_ASM_CONFIG) $(M68K_ASM_INCLUDES) $(M68K_AS_DONTWARN) -o $@ $<
