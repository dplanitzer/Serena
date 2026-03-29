#============================================================================
# libc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

PROC_C_SOURCES := $(wildcard $(PROC_SOURCES_DIR)/*.c)

PROC_OBJS := $(patsubst $(PROC_SOURCES_DIR)/%.c,$(PROC_OBJS_DIR)/%.o,$(PROC_C_SOURCES))
PROC_DEPS := $(PROC_OBJS:.o=.d)

PROC_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(PROC_SOURCES_DIR)

#PROC_GENERATE_DEPS = -deps -depfile=$(patsubst $(PROC_OBJS_DIR)/%.o,$(PROC_OBJS_DIR)/%.d,$@)
PROC_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(PROC_OBJS): | $(PROC_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(PROC_OBJS_DIR):
	$(call mkdir_if_needed,$(PROC_OBJS_DIR))

-include $(PROC_DEPS)

$(PROC_OBJS_DIR)/%.o : $(PROC_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(PROC_C_INCLUDES) $(LIBC_CC_DONTWARN) $(PROC_GENERATE_DEPS) -o $@ $<
