# --------------------------------------------------------------------------
# Build variables
#

C_TEST_SOURCES_DIR := $(C_TEST_PROJECT_DIR)
C_TEST_OBJS_DIR := $(LIB_OBJS_DIR)/libc/test

C_TEST_C_SOURCES := $(wildcard $(C_TEST_SOURCES_DIR)/*.c)
C_TEST_OBJS := $(patsubst $(C_TEST_SOURCES_DIR)/%.c, $(C_TEST_OBJS_DIR)/%.o, $(C_TEST_C_SOURCES))
C_TEST_C_INCLUDES := -I$(LIBDISPATCH_HEADERS_DIR) -I$(LIBC_HEADERS_DIR) -I$(LIBM_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(C_TEST_SOURCES_DIR)

C_TEST_CC_DONTWARN := -dontwarn=208,214


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-c-tests $(C_TEST_OBJS_DIR)


build-c-tests: $(C_TEST_FILE)

$(C_TEST_OBJS): | $(C_TEST_OBJS_DIR)

$(C_TEST_OBJS_DIR):
	$(call mkdir_if_needed,$(C_TEST_OBJS_DIR))


$(C_TEST_FILE): $(CSTART_FILE) $(C_TEST_OBJS) $(LIBC_FILE) $(LIBM_FILE) $(LIBDISPATCH_FILE)
	@echo Linking libc Tests
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(C_TEST_OBJS_DIR)/%.o : $(C_TEST_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(C_TEST_C_INCLUDES) $(C_TEST_CC_DONTWARN) -o $@ $<


clean-c-tests:
	$(call rm_if_exists,$(C_TEST_OBJS_DIR))
