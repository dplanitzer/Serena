# --------------------------------------------------------------------------
# Build variables
#

PROCESS_C_SOURCES := $(wildcard $(PROCESS_SOURCES_DIR)/*.c)

PROCESS_OBJS := $(patsubst $(PROCESS_SOURCES_DIR)/%.c,$(PROCESS_OBJS_DIR)/%.o,$(PROCESS_C_SOURCES))
PROCESS_DEPS := $(PROCESS_OBJS:.o=.d)

PROCESS_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(PROCESS_SOURCES_DIR)

#PROCESS_GENERATE_DEPS = -deps -depfile=$(patsubst $(PROCESS_OBJS_DIR)/%.o,$(PROCESS_OBJS_DIR)/%.d,$@)
PROCESS_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(PROCESS_OBJS): | $(PROCESS_OBJS_DIR)

$(PROCESS_OBJS_DIR):
	$(call mkdir_if_needed,$(PROCESS_OBJS_DIR))

-include $(PROCESS_DEPS)

$(PROCESS_OBJS_DIR)/%.o : $(PROCESS_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(PROCESS_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(PROCESS_GENERATE_DEPS) -o $@ $<
