# --------------------------------------------------------------------------
# Build variables
#

CMDS_SOURCES_DIR := $(CMDS_PROJECT_DIR)
CMDS_C_SOURCES := $(wildcard $(CMDS_SOURCES_DIR)/*.c)
CMDS_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(LIBC_HEADERS_DIR) -I$(LIBCLAP_HEADERS_DIR) -I$(CMDS_SOURCES_DIR)
CMDS_OBJS := $(patsubst $(CMDS_SOURCES_DIR)/%.c, $(CMDS_OBJS_DIR)/%.o, $(CMDS_C_SOURCES))


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-cmds $(CMDS_OBJS_DIR)


build-cmds: $(LOGIN_FILE)

$(CMDS_OBJS_DIR):
	$(call mkdir_if_needed,$(CMDS_OBJS_DIR))


$(LOGIN_FILE): $(ASTART_FILE) $(CMDS_OBJS_DIR)/login.o $(LIBSYSTEM_FILE) $(LIBC_FILE)
	@echo Linking cmds
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(CMDS_OBJS_DIR)/%.o : $(CMDS_SOURCES_DIR)/%.c | $(CMDS_OBJS_DIR)
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(CMDS_C_INCLUDES) -o $@ $<


clean-cmds:
	$(call rm_if_exists,$(CMDS_OBJS_DIR))
