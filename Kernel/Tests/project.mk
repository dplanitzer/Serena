# --------------------------------------------------------------------------
# Build variables
#

KERNEL_TESTS_SOURCES_DIR := $(WORKSPACE_DIR)/Kernel/Tests

KERNEL_TESTS_C_SOURCES := $(wildcard $(KERNEL_TESTS_SOURCES_DIR)/*.c)
KERNEL_TESTS_ASM_SOURCES := $(wildcard $(KERNEL_TESTS_SOURCES_DIR)/*.s)

KERNEL_TESTS_OBJS := $(patsubst $(KERNEL_TESTS_SOURCES_DIR)/%.c, $(KERNEL_TESTS_BUILD_DIR)/%.o, $(KERNEL_TESTS_C_SOURCES))
KERNEL_TESTS_OBJS += $(patsubst $(KERNEL_TESTS_SOURCES_DIR)/%.s, $(KERNEL_TESTS_BUILD_DIR)/%.o, $(KERNEL_TESTS_ASM_SOURCES))


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean_kernel_tests


$(KERNEL_TESTS_OBJS): | $(KERNEL_TESTS_BUILD_DIR) $(KERNEL_PRODUCT_DIR)

$(KERNEL_TESTS_BUILD_DIR):
	$(call mkdir_if_needed,$(KERNEL_TESTS_BUILD_DIR))


$(KERNEL_TESTS_BIN_FILE): $(CLIB_LIB_FILE) $(SYSTEM_LIB_FILE) $(KERNEL_TESTS_OBJS)
	@echo Linking Kernel Tests
	@$(LD) -s -bataritos -T $(KERNEL_TESTS_SOURCES_DIR)/linker.script -o $@ $(CLIB_ASTART_FILE) $^


$(KERNEL_TESTS_BUILD_DIR)/%.o : $(KERNEL_TESTS_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) -I$(KERNEL_TESTS_SOURCES_DIR) -I$(CLIB_HEADERS_DIR) -I$(SYSTEM_HEADERS_DIR) -o $@ $<

$(KERNEL_TESTS_BUILD_DIR)/%.o : $(KERNEL_TESTS_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) -I$(KERNEL_TESTS_SOURCES_DIR) -o $@ $<


clean_kernel_tests:
	$(call rm_if_exists,$(KERNEL_TESTS_BUILD_DIR))
