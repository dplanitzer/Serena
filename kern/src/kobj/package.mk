# --------------------------------------------------------------------------
# Build variables
#

KOBJ_C_SOURCES := $(wildcard $(KOBJ_SOURCES_DIR)/*.c)

KOBJ_OBJS := $(patsubst $(KOBJ_SOURCES_DIR)/%.c,$(KOBJ_OBJS_DIR)/%.o,$(KOBJ_C_SOURCES))
KOBJ_DEPS := $(KOBJ_OBJS:.o=.d)

KOBJ_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(KOBJ_SOURCES_DIR)

#KOBJ_GENERATE_DEPS = -deps -depfile=$(patsubst $(KOBJ_OBJS_DIR)/%.o,$(KOBJ_OBJS_DIR)/%.d,$@)
KOBJ_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(KOBJ_OBJS): | $(KOBJ_OBJS_DIR) $(PRODUCT_DIR)

$(KOBJ_OBJS_DIR):
	$(call mkdir_if_needed,$(KOBJ_OBJS_DIR))

-include $(KOBJ_DEPS)

$(KOBJ_OBJS_DIR)/%.o : $(KOBJ_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(KOBJ_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(KOBJ_GENERATE_DEPS) -o $@ $<
