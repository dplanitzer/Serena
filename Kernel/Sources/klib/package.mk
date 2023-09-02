# --------------------------------------------------------------------------
# Build variables
#

KLIB_C_SOURCES := $(wildcard $(KLIB_SOURCES_DIR)/*.c)
KLIB_ASM_SOURCES := $(wildcard $(KLIB_SOURCES_DIR)/*.s)

KLIB_OBJS := $(patsubst $(KLIB_SOURCES_DIR)/%.c,$(KLIB_BUILD_DIR)/%.o,$(KLIB_C_SOURCES))
KLIB_DEPS := $(KLIB_OBJS:.o=.d)
KLIB_OBJS += $(patsubst $(KLIB_SOURCES_DIR)/%.s,$(KLIB_BUILD_DIR)/%.o,$(KLIB_ASM_SOURCES))

KLIB_C_INCLUDES := -I$(RUNTIME_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(KLIB_SOURCES_DIR)
KLIB_ASM_INCLUDES := -I$(KERNEL_SOURCES_DIR) -I$(KLIB_SOURCES_DIR)


# --------------------------------------------------------------------------
# Build rules
#

$(KLIB_OBJS): | $(KLIB_BUILD_DIR)

$(KLIB_BUILD_DIR):
	$(call mkdir_if_needed,$(KLIB_BUILD_DIR))

-include $(KLIB_DEPS)

$(KLIB_BUILD_DIR)/%.o : $(KLIB_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) $(KLIB_C_INCLUDES) -dontwarn=51 -dontwarn=148 -dontwarn=208 -deps -depfile=$(patsubst $(KLIB_BUILD_DIR)/%.o,$(KLIB_BUILD_DIR)/%.d,$@) -o $@ $<

$(KLIB_BUILD_DIR)/%.o : $(KLIB_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KLIB_ASM_INCLUDES) -nowarn=62 -o $@ $<
