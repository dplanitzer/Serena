# --------------------------------------------------------------------------
# Build variables
#

CMDS_SOURCES_DIR := $(CMDS_PROJECT_DIR)
CMDS_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(LIBC_HEADERS_DIR) -I$(LIBCLAP_HEADERS_DIR) -I$(CMDS_SOURCES_DIR) -I$(KERNEL_SOURCES_DIR)

KERNEL_SOURCES_DIR := $(KERNEL_PROJECT_DIR)/Sources
SERENAFS_TOOLS_DIR = $(KERNEL_SOURCES_DIR)/filesystem/serenafs/tools
SERENAFS_TOOLS_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(LIBC_HEADERS_DIR) -I$(LIBCLAP_HEADERS_DIR) -I$(CMDS_SOURCES_DIR) -I$(KERNEL_SOURCES_DIR)

SERENAFS_TOOLS_OBJS_DIR := $(KERNEL_OBJS_DIR)/filesystem/serenafs/tools


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-cmds $(CMDS_OBJS_DIR) $(SERENAFS_TOOLS_OBJS_DIR)


build-cmds: $(DISKUTIL_FILE) $(FSID_FILE) $(INFO_FILE) $(LOGIN_FILE) $(TYPE_FILE)

$(CMDS_OBJS_DIR):
	$(call mkdir_if_needed,$(CMDS_OBJS_DIR))

$(SERENAFS_TOOLS_OBJS_DIR):
	$(call mkdir_if_needed,$(SERENAFS_TOOLS_OBJS_DIR))


$(DISKUTIL_FILE): $(CSTART_FILE) $(SERENAFS_TOOLS_OBJS_DIR)/format.o $(CMDS_OBJS_DIR)/diskutil.o $(LIBSYSTEM_FILE) $(LIBC_FILE) $(LIBCLAP_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(FSID_FILE): $(CSTART_FILE) $(CMDS_OBJS_DIR)/fsid.o $(LIBSYSTEM_FILE) $(LIBC_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(INFO_FILE): $(CSTART_FILE) $(CMDS_OBJS_DIR)/info.o $(LIBSYSTEM_FILE) $(LIBC_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(LOGIN_FILE): $(ASTART_FILE) $(CMDS_OBJS_DIR)/login.o $(LIBSYSTEM_FILE) $(LIBC_FILE)
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(TYPE_FILE): $(CSTART_FILE) $(CMDS_OBJS_DIR)/type.o $(LIBSYSTEM_FILE) $(LIBC_FILE) $(LIBCLAP_FILE)
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
