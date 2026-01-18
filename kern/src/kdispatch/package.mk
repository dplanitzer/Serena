# --------------------------------------------------------------------------
# Build variables
#

KDISPATCH_C_SOURCES := $(wildcard $(KDISPATCH_SOURCES_DIR)/*.c)

KDISPATCH_OBJS := $(patsubst $(KDISPATCH_SOURCES_DIR)/%.c,$(KDISPATCH_OBJS_DIR)/%.o,$(KDISPATCH_C_SOURCES))
KDISPATCH_DEPS := $(KDISPATCH_OBJS:.o=.d)

KDISPATCH_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(KDISPATCH_SOURCES_DIR)

#KDISPATCH_GENERATE_DEPS = -deps -depfile=$(patsubst $(KDISPATCH_OBJS_DIR)/%.o,$(KDISPATCH_OBJS_DIR)/%.d,$@)
KDISPATCH_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(KDISPATCH_OBJS): | $(KDISPATCH_OBJS_DIR)

$(KDISPATCH_OBJS_DIR):
	$(call mkdir_if_needed,$(KDISPATCH_OBJS_DIR))

-include $(KDISPATCH_DEPS)

$(KDISPATCH_OBJS_DIR)/%.o : $(KDISPATCH_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(KDISPATCH_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(KDISPATCH_GENERATE_DEPS) -o $@ $<
