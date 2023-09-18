# --------------------------------------------------------------------------
# Build variables
#

KERNEL_TESTS_SOURCES_DIR := $(WORKSPACE_DIR)/Kernel/Tests

KERNEL_TESTS_C_SOURCES := $(wildcard $(KERNEL_TESTS_SOURCES_DIR)/*.c)
KERNEL_TESTS_ASM_SOURCES := $(wildcard $(KERNEL_TESTS_SOURCES_DIR)/*.s)

KERNEL_TESTS_OBJS := $(patsubst $(KERNEL_TESTS_SOURCES_DIR)/%.c, $(KERNEL_TESTS_BUILD_DIR)/%.o, $(KERNEL_TESTS_C_SOURCES))
KERNEL_TESTS_OBJS += $(patsubst $(KERNEL_TESTS_SOURCES_DIR)/%.s, $(KERNEL_TESTS_BUILD_DIR)/%.o, $(KERNEL_TESTS_ASM_SOURCES))

KERNEL_TESTS_C_INCLUDES := -I$(LIBABI_HEADERS_DIR) -I$(LIBC_HEADERS_DIR) -I$(KERNEL_TESTS_SOURCES_DIR)
KERNEL_TESTS_ASM_INCLUDES := -I$(LIBABI_HEADERS_DIR) -I$(KERNEL_TESTS_SOURCES_DIR)


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-kernel-tests $(KERNEL_TESTS_BUILD_DIR)


build-kernel-tests: $(KERNEL_TESTS_BIN_FILE)

$(KERNEL_TESTS_OBJS): | $(KERNEL_TESTS_BUILD_DIR)

$(KERNEL_TESTS_BUILD_DIR):
	$(call mkdir_if_needed,$(KERNEL_TESTS_BUILD_DIR))


$(KERNEL_TESTS_BIN_FILE): $(KERNEL_TESTS_OBJS) $(LIBC_LIB_FILE) | $(KERNEL_PRODUCT_DIR)
	@echo Linking Kernel Tests
	@$(LD) -s -bataritos -T $(KERNEL_TESTS_SOURCES_DIR)/linker.script -o $@ $(LIBC_ASTART_FILE) $^


$(KERNEL_TESTS_BUILD_DIR)/%.o : $(KERNEL_TESTS_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) $(KERNEL_TESTS_C_INCLUDES) -o $@ $<

$(KERNEL_TESTS_BUILD_DIR)/%.o : $(KERNEL_TESTS_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_TESTS_ASM_INCLUDES) -o $@ $<


clean-kernel-tests:
	$(call rm_if_exists,$(KERNEL_TESTS_BUILD_DIR))
