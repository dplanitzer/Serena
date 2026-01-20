#============================================================================
# libc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

SYNCH_C_SOURCES := $(wildcard $(SYNCH_SOURCES_DIR)/*.c)

SYNCH_OBJS := $(patsubst $(SYNCH_SOURCES_DIR)/%.c,$(SYNCH_OBJS_DIR)/%.o,$(SYNCH_C_SOURCES))
SYNCH_DEPS := $(SYNCH_OBJS:.o=.d)

SYNCH_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(SYNCH_SOURCES_DIR)

#SYNCH_GENERATE_DEPS = -deps -depfile=$(patsubst $(SYNCH_OBJS_DIR)/%.o,$(SYNCH_OBJS_DIR)/%.d,$@)
SYNCH_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(SYNCH_OBJS): | $(SYNCH_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(SYNCH_OBJS_DIR):
	$(call mkdir_if_needed,$(SYNCH_OBJS_DIR))

-include $(SYNCH_DEPS)

$(SYNCH_OBJS_DIR)/%.o : $(SYNCH_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(SYNCH_C_INCLUDES) $(LIBC_CC_DONTWARN) $(SYNCH_GENERATE_DEPS) -o $@ $<
