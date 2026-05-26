# --------------------------------------------------------------------------
# Build variables
#

DQ_TEST_SOURCES_DIR := $(DQ_TEST_PROJECT_DIR)
DQ_TEST_OBJS_DIR := $(LIB_OBJS_DIR)/libdispatch/test

DQ_TEST_C_SOURCES := $(wildcard $(DQ_TEST_SOURCES_DIR)/*.c)
DQ_TEST_OBJS := $(patsubst $(DQ_TEST_SOURCES_DIR)/%.c, $(DQ_TEST_OBJS_DIR)/%.o, $(DQ_TEST_C_SOURCES))
DQ_TEST_C_INCLUDES := -I$(LIBDISPATCH_HEADERS_DIR) -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(DQ_TEST_SOURCES_DIR)

DQ_TEST_CC_DONTWARN := -dontwarn=208,214


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-dispatch-tests $(DQ_TEST_OBJS_DIR)


build-dispatch-tests: $(DQ_TEST_FILE)

$(DQ_TEST_OBJS): | $(DQ_TEST_OBJS_DIR)

$(DQ_TEST_OBJS_DIR):
	$(call mkdir_if_needed,$(DQ_TEST_OBJS_DIR))


$(DQ_TEST_FILE): $(CSTART_FILE) $(DQ_TEST_OBJS) $(LIBC_FILE) $(LIBDISPATCH_FILE)
	@echo Linking libdispatch tests
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(DQ_TEST_OBJS_DIR)/%.o : $(DQ_TEST_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(DQ_TEST_C_INCLUDES) $(DQ_TEST_CC_DONTWARN) -o $@ $<


clean-dispatch-tests:
	$(call rm_if_exists,$(DQ_TEST_OBJS_DIR))
