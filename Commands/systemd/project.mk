# --------------------------------------------------------------------------
# Build variables
#

SYSTEMD_SOURCES_DIR := $(SYSTEMD_PROJECT_DIR)
SYSTEMD_C_SOURCES := $(wildcard $(SYSTEMD_SOURCES_DIR)/*.c)
SYSTEMD_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(LIBC_HEADERS_DIR) -I$(SYSTEMD_SOURCES_DIR)
SYSTEMD_OBJS := $(patsubst $(SYSTEMD_SOURCES_DIR)/%.c, $(SYSTEMD_OBJS_DIR)/%.o, $(SYSTEMD_C_SOURCES))


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-systemd $(SYSTEMD_OBJS_DIR)


build-systemd: $(SYSTEMD_FILE)

$(SYSTEMD_OBJS): | $(SYSTEMD_OBJS_DIR)

$(SYSTEMD_OBJS_DIR):
	$(call mkdir_if_needed,$(SYSTEMD_OBJS_DIR))


$(SYSTEMD_FILE): $(ASTART_FILE) $(SYSTEMD_OBJS) $(LIBC_FILE)
	@echo Linking systemd
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(SYSTEMD_OBJS_DIR)/%.o : $(SYSTEMD_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(SYSTEMD_C_INCLUDES) -o $@ $<


clean-systemd:
	$(call rm_if_exists,$(SYSTEMD_OBJS_DIR))
