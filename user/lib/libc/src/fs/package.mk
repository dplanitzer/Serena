#============================================================================
# libc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

FS_C_SOURCES := $(wildcard $(FS_SOURCES_DIR)/*.c)

FS_OBJS := $(patsubst $(FS_SOURCES_DIR)/%.c,$(FS_OBJS_DIR)/%.o,$(FS_C_SOURCES))
FS_DEPS := $(FS_OBJS:.o=.d)

FS_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(FS_SOURCES_DIR)

#FS_GENERATE_DEPS = -deps -depfile=$(patsubst $(FS_OBJS_DIR)/%.o,$(FS_OBJS_DIR)/%.d,$@)
FS_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(FS_OBJS): | $(FS_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(FS_OBJS_DIR):
	$(call mkdir_if_needed,$(FS_OBJS_DIR))

-include $(FS_DEPS)

$(FS_OBJS_DIR)/%.o : $(FS_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(FS_C_INCLUDES) $(LIBC_CC_DONTWARN) $(FS_GENERATE_DEPS) -o $@ $<
