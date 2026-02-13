# --------------------------------------------------------------------------
# Build variables
#

LD80_C_SOURCES := $(wildcard $(LD80_SOURCES_DIR)/*.c)

LD80_OBJS := $(patsubst $(LD80_SOURCES_DIR)/%.c,$(LD80_OBJS_DIR)/%.o,$(LD80_C_SOURCES))
LD80_DEPS := $(LD80_OBJS:.o=.d)

#LD80_GENERATE_DEPS = -deps -depfile=$(patsubst $(LD80_OBJS_DIR)/%.o,$(LD80_OBJS_DIR)/%.d,$@)
LD80_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(LD80_OBJS): | $(LD80_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(LD80_OBJS_DIR):
	$(call mkdir_if_needed,$(LD80_OBJS_DIR))

-include $(LD80_DEPS)

$(LD80_OBJS_DIR)/%.o : $(LD80_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(LIBM_C_INCLUDES) -I$(LD80_SOURCES_DIR) $(LIBM_CC_DONTWARN) $(LD80_GENERATE_DEPS) -o $@ $<
