# --------------------------------------------------------------------------
# Build variables
#

DRIVER_C_SOURCES := $(wildcard $(DRIVER_SOURCES_DIR)/*.c)
DRIVER_ASM_SOURCES := $(wildcard $(DRIVER_SOURCES_DIR)/*.s)

DRIVER_OBJS := $(patsubst $(DRIVER_SOURCES_DIR)/%.c,$(DRIVER_OBJS_DIR)/%.o,$(DRIVER_C_SOURCES))
DRIVER_DEPS := $(DRIVER_OBJS:.o=.d)
DRIVER_OBJS += $(patsubst $(DRIVER_SOURCES_DIR)/%.s,$(DRIVER_OBJS_DIR)/%.o,$(DRIVER_ASM_SOURCES))

DRIVER_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(DRIVER_SOURCES_DIR)
DRIVER_ASM_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(DRIVER_SOURCES_DIR)

#DRIVER_GENERATE_DEPS = -deps -depfile=$(patsubst $(DRIVER_OBJS_DIR)/%.o,$(DRIVER_OBJS_DIR)/%.d,$@)
DRIVER_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(DRIVER_OBJS): | $(DRIVER_OBJS_DIR)

$(DRIVER_OBJS_DIR):
	$(call mkdir_if_needed,$(DRIVER_OBJS_DIR))

-include $(DRIVER_DEPS)

$(DRIVER_OBJS_DIR)/%.o : $(DRIVER_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(DRIVER_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(DRIVER_GENERATE_DEPS) -o $@ $<

$(DRIVER_OBJS_DIR)/%.o : $(DRIVER_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_CONFIG) $(DRIVER_ASM_INCLUDES) $(KERNEL_AS_DONTWARN) -o $@ $<
