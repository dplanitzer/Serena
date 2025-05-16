# --------------------------------------------------------------------------
# Build variables
#

SYS_C_SOURCES := $(wildcard $(SYS_SOURCES_DIR)/*.c)

SYS_OBJS := $(patsubst $(SYS_SOURCES_DIR)/%.c,$(SYS_OBJS_DIR)/%.o,$(SYS_C_SOURCES))
SYS_DEPS := $(SYS_OBJS:.o=.d)

SYS_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(SYS_SOURCES_DIR)

#SYS_GENERATE_DEPS = -deps -depfile=$(patsubst $(SYS_OBJS_DIR)/%.o,$(SYS_OBJS_DIR)/%.d,$@)
SYS_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(SYS_OBJS): | $(SYS_OBJS_DIR)

$(SYS_OBJS_DIR):
	$(call mkdir_if_needed,$(SYS_OBJS_DIR))

-include $(SYS_DEPS)

$(SYS_OBJS_DIR)/%.o : $(SYS_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(SYS_C_INCLUDES) $(LIBC_CC_DONTWARN) $(SYS_GENERATE_DEPS) -o $@ $<
