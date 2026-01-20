#============================================================================
# libc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

VCPU_C_SOURCES := $(wildcard $(VCPU_SOURCES_DIR)/*.c)

VCPU_OBJS := $(patsubst $(VCPU_SOURCES_DIR)/%.c,$(VCPU_OBJS_DIR)/%.o,$(VCPU_C_SOURCES))
VCPU_DEPS := $(VCPU_OBJS:.o=.d)

VCPU_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(VCPU_SOURCES_DIR)

#VCPU_GENERATE_DEPS = -deps -depfile=$(patsubst $(VCPU_OBJS_DIR)/%.o,$(VCPU_OBJS_DIR)/%.d,$@)
VCPU_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(VCPU_OBJS): | $(VCPU_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(VCPU_OBJS_DIR):
	$(call mkdir_if_needed,$(VCPU_OBJS_DIR))

-include $(VCPU_DEPS)

$(VCPU_OBJS_DIR)/%.o : $(VCPU_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(VCPU_C_INCLUDES) $(LIBC_CC_DONTWARN) $(VCPU_GENERATE_DEPS) -o $@ $<
