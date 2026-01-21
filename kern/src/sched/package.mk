# --------------------------------------------------------------------------
# Build variables
#

SCHED_C_SOURCES := $(wildcard $(SCHED_SOURCES_DIR)/*.c)
SCHED_ASM_SOURCES := $(wildcard $(SCHED_SOURCES_DIR)/*.s)

SCHED_OBJS := $(patsubst $(SCHED_SOURCES_DIR)/%.c,$(SCHED_OBJS_DIR)/%.o,$(SCHED_C_SOURCES))
SCHED_DEPS := $(SCHED_OBJS:.o=.d)
SCHED_OBJS += $(patsubst $(SCHED_SOURCES_DIR)/%.s,$(SCHED_OBJS_DIR)/%.o,$(SCHED_ASM_SOURCES))

SCHED_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(SCHED_SOURCES_DIR)
SCHED_ASM_INCLUDES := $(KERNEL_ASM_INCLUDES) -I$(SCHED_SOURCES_DIR)

#SCHED_GENERATE_DEPS = -deps -depfile=$(patsubst $(SCHED_OBJS_DIR)/%.o,$(SCHED_OBJS_DIR)/%.d,$@)
SCHED_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(SCHED_OBJS): | $(SCHED_OBJS_DIR) $(PRODUCT_DIR)

$(SCHED_OBJS_DIR):
	$(call mkdir_if_needed,$(SCHED_OBJS_DIR))

-include $(SCHED_DEPS)

$(SCHED_OBJS_DIR)/%.o : $(SCHED_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(SCHED_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(SCHED_GENERATE_DEPS) -o $@ $<

$(SCHED_OBJS_DIR)/%.o : $(SCHED_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_CONFIG) $(SCHED_ASM_INCLUDES) $(KERNEL_AS_DONTWARN) -o $@ $<
