# --------------------------------------------------------------------------
# Build variables
#

CMDS_SOURCES_DIR := $(CMD_BIN_PROJECT_DIR)
CMDS_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBCLAP_HEADERS_DIR) -I$(CMDS_SOURCES_DIR)

CMDS_C_SOURCES := $(wildcard $(CMDS_SOURCES_DIR)/*.c)

CMDS_OBJS := $(patsubst $(CMDS_SOURCES_DIR)/%.c,$(CMDS_OBJS_DIR)/%.o,$(CMDS_C_SOURCES))
CMDS_DEPS := $(CMDS_OBJS:.o=.d)

#CMDS_GENERATE_DEPS = -deps -depfile=$(patsubst $(CMDS_OBJS_DIR)/%.o,$(CMDS_OBJS_DIR)/%.d,$@)
CMDS_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-cmds $(CMD_OBJS_DIR)


build-cmds: $(COPY_FILE) $(CPU_FILE) $(DELETE_FILE) $(LIST_FILE) \
			$(LOGIN_FILE) $(MAKEDIR_FILE) $(RENAME_FILE) $(SHUTDOWN_FILE) \
			$(TOUCH_FILE) $(TYPE_FILE)

$(CMD_OBJS_DIR):
	$(call mkdir_if_needed,$(CMD_OBJS_DIR))


$(COPY_FILE): $(CSTART_FILE) $(CMD_OBJS_DIR)/copy.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(CPU_FILE): $(CSTART_FILE) $(CMD_OBJS_DIR)/cpu.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(DELETE_FILE): $(CSTART_FILE) $(CMD_OBJS_DIR)/delete.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(LIST_FILE): $(CSTART_FILE) $(CMD_OBJS_DIR)/list.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(LOGIN_FILE): $(CSTART_FILE) $(CMD_OBJS_DIR)/login.o $(LIBC_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(MAKEDIR_FILE): $(CSTART_FILE) $(CMD_OBJS_DIR)/makedir.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(RENAME_FILE): $(CSTART_FILE) $(CMD_OBJS_DIR)/rename.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(SHUTDOWN_FILE): $(CSTART_FILE) $(CMD_OBJS_DIR)/shutdown.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(TOUCH_FILE): $(CSTART_FILE) $(CMD_OBJS_DIR)/touch.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(TYPE_FILE): $(CSTART_FILE) $(CMD_OBJS_DIR)/type.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(CMDS_OBJS): | $(CMDS_OBJS_DIR) $(PRODUCT_CMD_DIR)

-include $(CMDS_DEPS)

$(CMD_OBJS_DIR)/%.o : $(CMDS_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(CMDS_C_INCLUDES) $(CMDS_GENERATE_DEPS) -o $@ $<


clean-cmds:
	$(call rm_if_exists,$(CMD_OBJS_DIR))
