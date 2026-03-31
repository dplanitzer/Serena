#============================================================================
# libc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

HOST_C_SOURCES := $(wildcard $(HOST_SOURCES_DIR)/*.c)

HOST_OBJS := $(patsubst $(HOST_SOURCES_DIR)/%.c,$(HOST_OBJS_DIR)/%.o,$(HOST_C_SOURCES))
HOST_DEPS := $(HOST_OBJS:.o=.d)

HOST_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(HOST_SOURCES_DIR)

#HOST_GENERATE_DEPS = -deps -depfile=$(patsubst $(HOST_OBJS_DIR)/%.o,$(HOST_OBJS_DIR)/%.d,$@)
HOST_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(HOST_OBJS): | $(HOST_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(HOST_OBJS_DIR):
	$(call mkdir_if_needed,$(HOST_OBJS_DIR))

-include $(HOST_DEPS)

$(HOST_OBJS_DIR)/%.o : $(HOST_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREHOST_DEFS) $(HOST_C_INCLUDES) $(LIBC_CC_DONTWARN) $(HOST_GENERATE_DEPS) -o $@ $<
