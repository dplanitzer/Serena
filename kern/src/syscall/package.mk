# --------------------------------------------------------------------------
# Build variables
#

SYSCALL_C_SOURCES := $(wildcard $(SYSCALL_SOURCES_DIR)/*.c)

SYSCALL_OBJS := $(patsubst $(SYSCALL_SOURCES_DIR)/%.c,$(SYSCALL_OBJS_DIR)/%.o,$(SYSCALL_C_SOURCES))
SYSCALL_DEPS := $(SYSCALL_OBJS:.o=.d)

SYSCALL_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(SYSCALL_SOURCES_DIR)

#SYSCALL_GENERATE_DEPS = -deps -depfile=$(patsubst $(SYSCALL_OBJS_DIR)/%.o,$(SYSCALL_OBJS_DIR)/%.d,$@)
SYSCALL_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(SYSCALL_OBJS): | $(SYSCALL_OBJS_DIR) $(PRODUCT_DIR)

$(SYSCALL_OBJS_DIR):
	$(call mkdir_if_needed,$(SYSCALL_OBJS_DIR))

-include $(SYSCALL_DEPS)

$(SYSCALL_OBJS_DIR)/%.o : $(SYSCALL_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(SYSCALL_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(SYSCALL_GENERATE_DEPS) -o $@ $<
