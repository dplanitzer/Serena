# --------------------------------------------------------------------------
# Build variables
#

KRT_C_SOURCES := $(wildcard $(KRT_SOURCES_DIR)/*.c)
KRT_ASM_SOURCES := $(wildcard $(KRT_SOURCES_DIR)/*.s)

KRT_OBJS := $(patsubst $(KRT_SOURCES_DIR)/%.c,$(KRT_OBJS_DIR)/%.o,$(KRT_C_SOURCES))
KRT_DEPS := $(KRT_OBJS:.o=.d)
KRT_OBJS += $(patsubst $(KRT_SOURCES_DIR)/%.s,$(KRT_OBJS_DIR)/%.o,$(KRT_ASM_SOURCES))

KRT_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(KRT_SOURCES_DIR)
KRT_ASM_INCLUDES := $(KERNEL_ASM_INCLUDES) -I$(KRT_SOURCES_DIR)

#KRT_GENERATE_DEPS = -deps -depfile=$(patsubst $(KRT_OBJS_DIR)/%.o,$(KRT_OBJS_DIR)/%.d,$@)
KRT_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(KRT_OBJS): | $(KRT_OBJS_DIR)

$(KRT_OBJS_DIR):
	$(call mkdir_if_needed,$(KRT_OBJS_DIR))

-include $(KRT_DEPS)

$(KRT_OBJS_DIR)/%.o : $(KRT_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(KRT_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(KRT_GENERATE_DEPS) -o $@ $<

$(KRT_OBJS_DIR)/%.o : $(KRT_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_CONFIG) $(KRT_ASM_INCLUDES) $(KERNEL_AS_DONTWARN) -o $@ $<
