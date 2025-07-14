# --------------------------------------------------------------------------
# Build variables
#

CMDS_SOURCES_DIR := $(CMDS_PROJECT_DIR)
CMDS_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(LIBCLAP_HEADERS_DIR) -I$(CMDS_SOURCES_DIR) -I$(KERNEL_SOURCES_DIR)

KERNEL_SOURCES_DIR := $(KERNEL_PROJECT_DIR)/Sources
SERENAFS_TOOLS_DIR = $(KERNEL_SOURCES_DIR)/filesystem/serenafs/tools
SERENAFS_TOOLS_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(LIBCLAP_HEADERS_DIR) -I$(CMDS_SOURCES_DIR) -I$(KERNEL_SOURCES_DIR)

SERENAFS_TOOLS_OBJS_DIR := $(KERNEL_OBJS_DIR)/filesystem/serenafs/tools


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-cmds $(CMDS_OBJS_DIR) $(SERENAFS_TOOLS_OBJS_DIR)


build-cmds: $(COPY_FILE) $(DELETE_FILE) $(DISK_FILE) $(ID_FILE) $(LIST_FILE) $(LOGIN_FILE) $(MAKEDIR_FILE) $(RENAME_FILE) $(SHUTDOWN_FILE) $(STATUS_FILE) $(TOUCH_FILE) $(TYPE_FILE) $(UPTIME_FILE) $(WAIT_FILE)

$(CMDS_OBJS_DIR):
	$(call mkdir_if_needed,$(CMDS_OBJS_DIR))

$(SERENAFS_TOOLS_OBJS_DIR):
	$(call mkdir_if_needed,$(SERENAFS_TOOLS_OBJS_DIR))


$(COPY_FILE): $(CSTART_FILE) $(CMDS_OBJS_DIR)/copy.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(DELETE_FILE): $(CSTART_FILE) $(CMDS_OBJS_DIR)/delete.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(DISK_FILE): $(CSTART_FILE) $(SERENAFS_TOOLS_OBJS_DIR)/format.o $(CMDS_OBJS_DIR)/disk.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(ID_FILE): $(CSTART_FILE) $(CMDS_OBJS_DIR)/id.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(LIST_FILE): $(CSTART_FILE) $(CMDS_OBJS_DIR)/list.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(LOGIN_FILE): $(CSTART_FILE) $(CMDS_OBJS_DIR)/login.o $(LIBC_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(MAKEDIR_FILE): $(CSTART_FILE) $(CMDS_OBJS_DIR)/makedir.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(RENAME_FILE): $(CSTART_FILE) $(CMDS_OBJS_DIR)/rename.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(SHUTDOWN_FILE): $(CSTART_FILE) $(CMDS_OBJS_DIR)/shutdown.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(STATUS_FILE): $(CSTART_FILE) $(CMDS_OBJS_DIR)/status.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(TOUCH_FILE): $(CSTART_FILE) $(CMDS_OBJS_DIR)/touch.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(TYPE_FILE): $(CSTART_FILE) $(CMDS_OBJS_DIR)/type.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(UPTIME_FILE): $(CSTART_FILE) $(CMDS_OBJS_DIR)/uptime.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(WAIT_FILE): $(CSTART_FILE) $(CMDS_OBJS_DIR)/wait.o $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(CMDS_OBJS_DIR)/%.o : $(CMDS_SOURCES_DIR)/%.c | $(CMDS_OBJS_DIR)
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(CMDS_C_INCLUDES) -o $@ $<


$(SERENAFS_TOOLS_OBJS_DIR)/format.o : $(SERENAFS_TOOLS_DIR)/format.c | $(SERENAFS_TOOLS_OBJS_DIR)
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(SERENAFS_TOOLS_C_INCLUDES) -o $@ $<


clean-cmds:
	$(call rm_if_exists,$(CMDS_OBJS_DIR))
	$(call rm_if_exists,$(SERENAFS_TOOLS_OBJS_DIR))
