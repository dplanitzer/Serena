# --------------------------------------------------------------------------
# Build variables
#

LOG_C_SOURCES := $(wildcard $(LOG_SOURCES_DIR)/*.c)

LOG_OBJS := $(patsubst $(LOG_SOURCES_DIR)/%.c,$(LOG_OBJS_DIR)/%.o,$(LOG_C_SOURCES))
LOG_DEPS := $(LOG_OBJS:.o=.d)

LOG_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(LOG_SOURCES_DIR)

#LOG_GENERATE_DEPS = -deps -depfile=$(patsubst $(LOG_OBJS_DIR)/%.o,$(LOG_OBJS_DIR)/%.d,$@)
LOG_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(LOG_OBJS): | $(LOG_OBJS_DIR)

$(LOG_OBJS_DIR):
	$(call mkdir_if_needed,$(LOG_OBJS_DIR))

-include $(LOG_DEPS)

$(LOG_OBJS_DIR)/%.o : $(LOG_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(LOG_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(LOG_GENERATE_DEPS) -o $@ $<
