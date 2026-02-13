# --------------------------------------------------------------------------
# Build variables
#

CORE_C_SOURCES := $(wildcard $(CORE_SOURCES_DIR)/*.c)

CORE_OBJS := $(patsubst $(CORE_SOURCES_DIR)/%.c,$(CORE_OBJS_DIR)/%.o,$(CORE_C_SOURCES))
CORE_DEPS := $(CORE_OBJS:.o=.d)

#CORE_GENERATE_DEPS = -deps -depfile=$(patsubst $(CORE_OBJS_DIR)/%.o,$(CORE_OBJS_DIR)/%.d,$@)
CORE_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(CORE_OBJS): | $(CORE_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(CORE_OBJS_DIR):
	$(call mkdir_if_needed,$(CORE_OBJS_DIR))

-include $(CORE_DEPS)

$(CORE_OBJS_DIR)/%.o : $(CORE_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(LIBM_C_INCLUDES) -I$(CORE_SOURCES_DIR) $(LIBM_CC_DONTWARN) $(CORE_GENERATE_DEPS) -o $@ $<
