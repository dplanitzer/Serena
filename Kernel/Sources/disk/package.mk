# --------------------------------------------------------------------------
# Build variables
#

DISK_C_SOURCES := $(wildcard $(DISK_SOURCES_DIR)/*.c)
DISK_ASM_SOURCES := $(wildcard $(DISK_SOURCES_DIR)/*.s)

DISK_OBJS := $(patsubst $(DISK_SOURCES_DIR)/%.c,$(DISK_OBJS_DIR)/%.o,$(DISK_C_SOURCES))
DISK_DEPS := $(DISK_OBJS:.o=.d)
DISK_OBJS += $(patsubst $(DISK_SOURCES_DIR)/%.s,$(DISK_OBJS_DIR)/%.o,$(DISK_ASM_SOURCES))

DISK_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(DISK_SOURCES_DIR)
DISK_ASM_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(DISK_SOURCES_DIR)

#DISK_GENERATE_DEPS = -deps -depfile=$(patsubst $(DISK_OBJS_DIR)/%.o,$(DISK_OBJS_DIR)/%.d,$@)
DISK_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(DISK_OBJS): | $(DISK_OBJS_DIR)

$(DISK_OBJS_DIR):
	$(call mkdir_if_needed,$(DISK_OBJS_DIR))

-include $(DISK_DEPS)

$(DISK_OBJS_DIR)/%.o : $(DISK_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(DISK_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(DISK_GENERATE_DEPS) -o $@ $<

$(DISK_OBJS_DIR)/%.o : $(DISK_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_CONFIG) $(DISK_ASM_INCLUDES) $(KERNEL_AS_DONTWARN) -o $@ $<
