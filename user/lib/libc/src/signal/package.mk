#============================================================================
# libc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

SIGNAL_C_SOURCES := $(wildcard $(SIGNAL_SOURCES_DIR)/*.c)

SIGNAL_OBJS := $(patsubst $(SIGNAL_SOURCES_DIR)/%.c,$(SIGNAL_OBJS_DIR)/%.o,$(SIGNAL_C_SOURCES))
SIGNAL_DEPS := $(SIGNAL_OBJS:.o=.d)

SIGNAL_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(SIGNAL_SOURCES_DIR)

#SIGNAL_GENERATE_DEPS = -deps -depfile=$(patsubst $(SIGNAL_OBJS_DIR)/%.o,$(SIGNAL_OBJS_DIR)/%.d,$@)
SIGNAL_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(SIGNAL_OBJS): | $(SIGNAL_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(SIGNAL_OBJS_DIR):
	$(call mkdir_if_needed,$(SIGNAL_OBJS_DIR))

-include $(SIGNAL_DEPS)

$(SIGNAL_OBJS_DIR)/%.o : $(SIGNAL_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(SIGNAL_C_INCLUDES) $(LIBC_CC_DONTWARN) $(SIGNAL_GENERATE_DEPS) -o $@ $<
