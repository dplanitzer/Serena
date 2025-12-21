# --------------------------------------------------------------------------
# Build variables
#

TIME_C_SOURCES := $(wildcard $(TIME_SOURCES_DIR)/*.c)

TIME_OBJS := $(patsubst $(TIME_SOURCES_DIR)/%.c,$(TIME_OBJS_DIR)/%.o,$(TIME_C_SOURCES))
TIME_DEPS := $(TIME_OBJS:.o=.d)

TIME_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(TIME_SOURCES_DIR)

#TIME_GENERATE_DEPS = -deps -depfile=$(patsubst $(TIME_OBJS_DIR)/%.o,$(TIME_OBJS_DIR)/%.d,$@)
TIME_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(TIME_OBJS): | $(TIME_OBJS_DIR)

$(TIME_OBJS_DIR):
	$(call mkdir_if_needed,$(TIME_OBJS_DIR))

-include $(TIME_DEPS)

$(TIME_OBJS_DIR)/%.o : $(TIME_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(TIME_C_INCLUDES) $(LIBC_CC_DONTWARN) $(TIME_GENERATE_DEPS) -o $@ $<
