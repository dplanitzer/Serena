# --------------------------------------------------------------------------
# Build variables
#

LOCALE_C_SOURCES := $(wildcard $(LOCALE_SOURCES_DIR)/*.c)

LOCALE_OBJS := $(patsubst $(LOCALE_SOURCES_DIR)/%.c,$(LOCALE_OBJS_DIR)/%.o,$(LOCALE_C_SOURCES))
LOCALE_DEPS := $(LOCALE_OBJS:.o=.d)

LOCALE_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(LOCALE_SOURCES_DIR)

#LOCALE_GENERATE_DEPS = -deps -depfile=$(patsubst $(LOCALE_OBJS_DIR)/%.o,$(LOCALE_OBJS_DIR)/%.d,$@)
LOCALE_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(LOCALE_OBJS): | $(LOCALE_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(LOCALE_OBJS_DIR):
	$(call mkdir_if_needed,$(LOCALE_OBJS_DIR))

-include $(LOCALE_DEPS)

$(LOCALE_OBJS_DIR)/%.o : $(LOCALE_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(LOCALE_C_INCLUDES) $(LIBC_CC_DONTWARN) $(LOCALE_GENERATE_DEPS) -o $@ $<
