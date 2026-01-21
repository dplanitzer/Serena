# --------------------------------------------------------------------------
# Build variables
#

KERNEL_TESTS_SOURCES_DIR := $(KERNEL_TESTS_PROJECT_DIR)
KERNEL_TESTS_OBJS_DIR := $(OBJS_DIR)/kerntest

KERNEL_TESTS_C_SOURCES := $(wildcard $(KERNEL_TESTS_SOURCES_DIR)/*.c)
KERNEL_TESTS_OBJS := $(patsubst $(KERNEL_TESTS_SOURCES_DIR)/%.c, $(KERNEL_TESTS_OBJS_DIR)/%.o, $(KERNEL_TESTS_C_SOURCES))
KERNEL_TESTS_C_INCLUDES := -I$(LIBDISPATCH_HEADERS_DIR) -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(KERNEL_TESTS_SOURCES_DIR)

KERNEL_TESTS_CC_DONTWARN := -dontwarn=208,214


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-kernel-tests $(KERNEL_TESTS_OBJS_DIR)


build-kernel-tests: $(KERNEL_TESTS_FILE)

$(KERNEL_TESTS_OBJS): | $(KERNEL_TESTS_OBJS_DIR)

$(KERNEL_TESTS_OBJS_DIR):
	$(call mkdir_if_needed,$(KERNEL_TESTS_OBJS_DIR))


$(KERNEL_TESTS_FILE): $(CSTART_FILE) $(KERNEL_TESTS_OBJS) $(LIBC_FILE) $(LIBDISPATCH_FILE)
	@echo Linking Kernel Tests
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(KERNEL_TESTS_OBJS_DIR)/%.o : $(KERNEL_TESTS_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(KERNEL_TESTS_C_INCLUDES) $(KERNEL_TESTS_CC_DONTWARN) -o $@ $<


clean-kernel-tests:
	$(call rm_if_exists,$(KERNEL_TESTS_OBJS_DIR))
