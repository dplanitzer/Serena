# --------------------------------------------------------------------------
# Build variables
#

FLOWTERM_TEST_SOURCES_DIR := $(FLOWTERM_TEST_PROJECT_DIR)
FLOWTERM_TEST_OBJS_DIR := $(LIB_OBJS_DIR)/libflowterm/test

FLOWTERM_TEST_C_SOURCES := $(wildcard $(FLOWTERM_TEST_SOURCES_DIR)/*.c)
FLOWTERM_TEST_OBJS := $(patsubst $(FLOWTERM_TEST_SOURCES_DIR)/%.c, $(FLOWTERM_TEST_OBJS_DIR)/%.o, $(FLOWTERM_TEST_C_SOURCES))
FLOWTERM_TEST_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBFLOWTERM_HEADERS_DIR) -I$(FLOWTERM_TEST_SOURCES_DIR)

FLOWTERM_TEST_CC_DONTWARN := -dontwarn=208,214


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-flowterm-tests $(FLOWTERM_TEST_OBJS_DIR)


build-flowterm-tests: $(FLOWTERM_TEST_FILE)

$(FLOWTERM_TEST_OBJS): | $(FLOWTERM_TEST_OBJS_DIR)

$(FLOWTERM_TEST_OBJS_DIR):
	$(call mkdir_if_needed,$(FLOWTERM_TEST_OBJS_DIR))


$(FLOWTERM_TEST_FILE): $(CSTART_FILE) $(FLOWTERM_TEST_OBJS) $(LIBC_FILE) $(LIBFLOWTERM_FILE)
	@echo Linking libflowterm Tests
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(FLOWTERM_TEST_OBJS_DIR)/%.o : $(FLOWTERM_TEST_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(FLOWTERM_TEST_C_INCLUDES) $(FLOWTERM_TEST_CC_DONTWARN) -o $@ $<


clean-flowterm-tests:
	$(call rm_if_exists,$(FLOWTERM_TEST_OBJS_DIR))
