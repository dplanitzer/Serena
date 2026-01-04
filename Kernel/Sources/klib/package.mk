# --------------------------------------------------------------------------
# Build variables
#

KLIB_C_SOURCES := $(wildcard $(KLIB_SOURCES_DIR)/*.c)

KLIB_OBJS := $(patsubst $(KLIB_SOURCES_DIR)/%.c,$(KLIB_OBJS_DIR)/%.o,$(KLIB_C_SOURCES))
KLIB_DEPS := $(KLIB_OBJS:.o=.d)

KLIB_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(KLIB_SOURCES_DIR)

#KLIB_GENERATE_DEPS = -deps -depfile=$(patsubst $(KLIB_OBJS_DIR)/%.o,$(KLIB_OBJS_DIR)/%.d,$@)
KLIB_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(KLIB_OBJS): | $(KLIB_OBJS_DIR)

$(KLIB_OBJS_DIR):
	$(call mkdir_if_needed,$(KLIB_OBJS_DIR))

-include $(KLIB_DEPS)

$(KLIB_OBJS_DIR)/%.o : $(KLIB_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(KLIB_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(KLIB_GENERATE_DEPS) -o $@ $<
