# --------------------------------------------------------------------------
# Build variables
#

KOBJ_C_SOURCES := $(wildcard $(KOBJ_SOURCES_DIR)/*.c)
KOBJ_ASM_SOURCES := $(wildcard $(KOBJ_SOURCES_DIR)/*.s)

KOBJ_OBJS := $(patsubst $(KOBJ_SOURCES_DIR)/%.c,$(KOBJ_OBJS_DIR)/%.o,$(KOBJ_C_SOURCES))
KOBJ_DEPS := $(KOBJ_OBJS:.o=.d)
KOBJ_OBJS += $(patsubst $(KOBJ_SOURCES_DIR)/%.s,$(KOBJ_OBJS_DIR)/%.o,$(KOBJ_ASM_SOURCES))

KOBJ_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(KOBJ_SOURCES_DIR)
KOBJ_ASM_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(KOBJ_SOURCES_DIR)

#KOBJ_GENERATE_DEPS = -deps -depfile=$(patsubst $(KOBJ_OBJS_DIR)/%.o,$(KOBJ_OBJS_DIR)/%.d,$@)
KOBJ_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(KOBJ_OBJS): | $(KOBJ_OBJS_DIR)

$(KOBJ_OBJS_DIR):
	$(call mkdir_if_needed,$(KOBJ_OBJS_DIR))

-include $(KOBJ_DEPS)

$(KOBJ_OBJS_DIR)/%.o : $(KOBJ_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(KOBJ_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(KOBJ_GENERATE_DEPS) -o $@ $<

$(KOBJ_OBJS_DIR)/%.o : $(KOBJ_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_CONFIG) $(KOBJ_ASM_INCLUDES) $(KERNEL_AS_DONTWARN) -o $@ $<
