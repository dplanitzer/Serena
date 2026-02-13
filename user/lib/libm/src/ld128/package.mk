# --------------------------------------------------------------------------
# Build variables
#

LD128_C_SOURCES := $(wildcard $(LD128_SOURCES_DIR)/*.c)

LD128_OBJS := $(patsubst $(LD128_SOURCES_DIR)/%.c,$(LD128_OBJS_DIR)/%.o,$(LD128_C_SOURCES))
LD128_DEPS := $(LD128_OBJS:.o=.d)

#LD128_GENERATE_DEPS = -deps -depfile=$(patsubst $(LD128_OBJS_DIR)/%.o,$(LD128_OBJS_DIR)/%.d,$@)
LD128_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(LD128_OBJS): | $(LD128_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(LD128_OBJS_DIR):
	$(call mkdir_if_needed,$(LD128_OBJS_DIR))

-include $(LD128_DEPS)

$(LD128_OBJS_DIR)/%.o : $(LD128_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(LIBM_C_INCLUDES) -I$(LD128_SOURCES_DIR) $(LIBM_CC_DONTWARN) $(LD128_GENERATE_DEPS) -o $@ $<
