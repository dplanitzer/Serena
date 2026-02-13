# --------------------------------------------------------------------------
# Build variables
#

BSDEXT_C_SOURCES := $(wildcard $(BSDEXT_SOURCES_DIR)/*.c)

BSDEXT_OBJS := $(patsubst $(BSDEXT_SOURCES_DIR)/%.c,$(BSDEXT_OBJS_DIR)/%.o,$(BSDEXT_C_SOURCES))
BSDEXT_DEPS := $(BSDEXT_OBJS:.o=.d)

#BSDEXT_GENERATE_DEPS = -deps -depfile=$(patsubst $(BSDEXT_OBJS_DIR)/%.o,$(BSDEXT_OBJS_DIR)/%.d,$@)
BSDEXT_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(BSDEXT_OBJS): | $(BSDEXT_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(BSDEXT_OBJS_DIR):
	$(call mkdir_if_needed,$(BSDEXT_OBJS_DIR))

-include $(BSDEXT_DEPS)

$(BSDEXT_OBJS_DIR)/%.o : $(BSDEXT_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(LIBM_C_INCLUDES) -I$(BSDEXT_SOURCES_DIR) $(LIBM_CC_DONTWARN) $(BSDEXT_GENERATE_DEPS) -o $@ $<
