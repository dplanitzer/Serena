# --------------------------------------------------------------------------
# Build variables
#

FILESYSTEM_C_SOURCES := $(wildcard $(FILESYSTEM_SOURCES_DIR)/*.c)

FILESYSTEM_OBJS := $(patsubst $(FILESYSTEM_SOURCES_DIR)/%.c,$(FILESYSTEM_OBJS_DIR)/%.o,$(FILESYSTEM_C_SOURCES))
FILESYSTEM_DEPS := $(FILESYSTEM_OBJS:.o=.d)

FILESYSTEM_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(FILESYSTEM_SOURCES_DIR)

#FILESYSTEM_GENERATE_DEPS = -deps -depfile=$(patsubst $(FILESYSTEM_OBJS_DIR)/%.o,$(FILESYSTEM_OBJS_DIR)/%.d,$@)
FILESYSTEM_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(FILESYSTEM_OBJS): | $(FILESYSTEM_OBJS_DIR)

$(FILESYSTEM_OBJS_DIR):
	$(call mkdir_if_needed,$(FILESYSTEM_OBJS_DIR))

-include $(FILESYSTEM_DEPS)

$(FILESYSTEM_OBJS_DIR)/%.o : $(FILESYSTEM_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(FILESYSTEM_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(FILESYSTEM_GENERATE_DEPS) -o $@ $<
