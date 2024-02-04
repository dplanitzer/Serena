# --------------------------------------------------------------------------
# Build variables
#

KRT_C_SOURCES := $(wildcard $(KRT_SOURCES_DIR)/*.c)
KRT_ASM_SOURCES := $(wildcard $(KRT_SOURCES_DIR)/*.s)

KRT_OBJS := $(patsubst $(KRT_SOURCES_DIR)/%.c,$(KRT_BUILD_DIR)/%.o,$(KRT_C_SOURCES))
KRT_DEPS := $(KRT_OBJS:.o=.d)
KRT_OBJS += $(patsubst $(KRT_SOURCES_DIR)/%.s,$(KRT_BUILD_DIR)/%.o,$(KRT_ASM_SOURCES))

KRT_C_INCLUDES := -I$(KRT_SOURCES_DIR)
KRT_ASM_INCLUDES := -I$(KRT_SOURCES_DIR)

#KRT_GENERATE_DEPS = -deps -depfile=$(patsubst $(KRT_BUILD_DIR)/%.o,$(KRT_BUILD_DIR)/%.d,$@)
KRT_GENERATE_DEPS := 
KRT_AS_DONTWARN :=
KRT_CC_DONTWARN :=


# --------------------------------------------------------------------------
# Build rules
#

$(KRT_OBJS): | $(KRT_BUILD_DIR)

$(KRT_BUILD_DIR):
	$(call mkdir_if_needed,$(KRT_BUILD_DIR))

-include $(KRT_DEPS)

$(KRT_BUILD_DIR)/%.o : $(KRT_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) $(KRT_C_INCLUDES) $(KRT_CC_DONTWARN) $(KRT_GENERATE_DEPS) -o $@ $<

$(KRT_BUILD_DIR)/%.o : $(KRT_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KRT_ASM_INCLUDES) $(KRT_AS_DONTWARN) -o $@ $<
