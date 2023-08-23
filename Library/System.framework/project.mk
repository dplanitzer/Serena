# --------------------------------------------------------------------------
# Build variables
#

SYSTEM_SOURCES_DIR := $(SYSTEM_PROJECT_DIR)/Sources

SYSTEM_C_SOURCES := $(wildcard $(SYSTEM_SOURCES_DIR)/*.c)
SYSTEM_ASM_SOURCES := $(wildcard $(SYSTEM_SOURCES_DIR)/*.s)

SYSTEM_OBJS := $(patsubst $(SYSTEM_SOURCES_DIR)/%.c, $(SYSTEM_BUILD_DIR)/%.o, $(SYSTEM_C_SOURCES))
SYSTEM_DEPS := $(SYSTEM_OBJS:.o=.d)
SYSTEM_OBJS += $(patsubst $(SYSTEM_SOURCES_DIR)/%.s, $(SYSTEM_BUILD_DIR)/%.o, $(SYSTEM_ASM_SOURCES))


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean_system


$(SYSTEM_OBJS): | $(SYSTEM_BUILD_DIR) $(SYSTEM_PRODUCT_DIR)

$(SYSTEM_BUILD_DIR):
	$(call mkdir_if_needed,$(SYSTEM_BUILD_DIR))

$(SYSTEM_PRODUCT_DIR):
	$(call mkdir_if_needed,$(SYSTEM_PRODUCT_DIR))


$(SYSTEM_LIB_FILE): $(SYSTEM_OBJS)
	@echo Linking libSystem.a
	@$(LD) -baoutnull -r -o $@ $^


-include $(SYSTEM_DEPS)

$(SYSTEM_BUILD_DIR)/%.o : $(SYSTEM_SOURCES_DIR)%.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) -I$(SYSTEM_SOURCES_DIR) -deps -depfile=$(patsubst $(SYSTEM_BUILD_DIR)/%.o,$(SYSTEM_BUILD_DIR)/%.d,$@) -o $@ $<

$(SYSTEM_BUILD_DIR)/%.o : $(SYSTEM_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) -I$(SYSTEM_SOURCES_DIR) -o $@ $<


clean_system:
	$(call rm_if_exists,$(SYSTEM_BUILD_DIR))
