# --------------------------------------------------------------------------
# Build variables
#

COMPLEX_C_SOURCES := $(wildcard $(COMPLEX_SOURCES_DIR)/*.c)

COMPLEX_OBJS := $(patsubst $(COMPLEX_SOURCES_DIR)/%.c,$(COMPLEX_OBJS_DIR)/%.o,$(COMPLEX_C_SOURCES))
COMPLEX_DEPS := $(COMPLEX_OBJS:.o=.d)

#COMPLEX_GENERATE_DEPS = -deps -depfile=$(patsubst $(COMPLEX_OBJS_DIR)/%.o,$(COMPLEX_OBJS_DIR)/%.d,$@)
COMPLEX_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(COMPLEX_OBJS): | $(COMPLEX_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(COMPLEX_OBJS_DIR):
	$(call mkdir_if_needed,$(COMPLEX_OBJS_DIR))

-include $(COMPLEX_DEPS)

$(COMPLEX_OBJS_DIR)/%.o : $(COMPLEX_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(LIBM_C_INCLUDES) -I$(COMPLEX_SOURCES_DIR) $(LIBM_CC_DONTWARN) $(COMPLEX_GENERATE_DEPS) -o $@ $<
