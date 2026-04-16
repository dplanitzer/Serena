# --------------------------------------------------------------------------
# Build variables
#

STATUS_SOURCES_DIR := $(STATUS_PROJECT_DIR)
STATUS_OBJS_DIR := $(CMD_OBJS_DIR)/status

STATUS_C_SOURCES := $(wildcard $(STATUS_SOURCES_DIR)/*.c)
STATUS_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBCLAP_HEADERS_DIR) -I$(STATUS_SOURCES_DIR)
STATUS_OBJS := $(patsubst $(STATUS_SOURCES_DIR)/%.c, $(STATUS_OBJS_DIR)/%.o, $(STATUS_C_SOURCES))


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-status $(STATUS_OBJS_DIR)


build-status: $(STATUS_FILE)

$(STATUS_OBJS): | $(STATUS_OBJS_DIR) $(PRODUCT_CMD_DIR)

$(STATUS_OBJS_DIR):
	$(call mkdir_if_needed,$(STATUS_OBJS_DIR))


$(STATUS_FILE): $(CSTART_FILE) $(STATUS_OBJS) $(LIBC_FILE) $(LIBCLAP_FILE)
	@echo Linking status
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(STATUS_OBJS_DIR)/%.o : $(STATUS_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(STATUS_C_INCLUDES) -o $@ $<


clean-status:
	$(call rm_if_exists,$(STATUS_OBJS_DIR))
