# --------------------------------------------------------------------------
# Build variables
#

DISKTOOL_SOURCES_DIR := $(DISKTOOL_PROJECT_DIR)
DISKTOOL_C_SOURCES := $(wildcard $(DISKTOOL_SOURCES_DIR)/*.c)
DISKTOOL_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBCLAP_HEADERS_DIR) -I$(DISKTOOL_SOURCES_DIR)
DISKTOOL_OBJS := $(patsubst $(DISKTOOL_SOURCES_DIR)/%.c, $(DISKTOOL_OBJS_DIR)/%.o, $(DISKTOOL_C_SOURCES))


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-disktool $(DISKTOOL_OBJS_DIR)


build-disktool: $(DISKTOOL_FILE)

$(DISKTOOL_OBJS): | $(DISKTOOL_OBJS_DIR)

$(DISKTOOL_OBJS_DIR):
	$(call mkdir_if_needed,$(DISKTOOL_OBJS_DIR))


$(DISKTOOL_FILE): $(CSTART_FILE) $(DISKTOOL_OBJS) $(LIBC_FILE) $(LIBCLAP_FILE)
	@echo Linking disktool
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(DISKTOOL_OBJS_DIR)/%.o : $(DISKTOOL_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(DISKTOOL_C_INCLUDES) -o $@ $<


clean-disktool:
	$(call rm_if_exists,$(DISKTOOL_OBJS_DIR))
