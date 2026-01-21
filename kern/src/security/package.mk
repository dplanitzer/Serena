# --------------------------------------------------------------------------
# Build variables
#

SECURITY_C_SOURCES := $(wildcard $(SECURITY_SOURCES_DIR)/*.c)

SECURITY_OBJS := $(patsubst $(SECURITY_SOURCES_DIR)/%.c,$(SECURITY_OBJS_DIR)/%.o,$(SECURITY_C_SOURCES))
SECURITY_DEPS := $(SECURITY_OBJS:.o=.d)

SECURITY_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(SECURITY_SOURCES_DIR)

#SECURITY_GENERATE_DEPS = -deps -depfile=$(patsubst $(SECURITY_OBJS_DIR)/%.o,$(SECURITY_OBJS_DIR)/%.d,$@)
SECURITY_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(SECURITY_OBJS): | $(SECURITY_OBJS_DIR) $(PRODUCT_DIR)

$(SECURITY_OBJS_DIR):
	$(call mkdir_if_needed,$(SECURITY_OBJS_DIR))

-include $(SECURITY_DEPS)

$(SECURITY_OBJS_DIR)/%.o : $(SECURITY_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(SECURITY_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(SECURITY_GENERATE_DEPS) -o $@ $<
