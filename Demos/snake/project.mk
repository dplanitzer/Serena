# --------------------------------------------------------------------------
# Build variables
#

SNAKE_SOURCES_DIR := $(SNAKE_PROJECT_DIR)
SNAKE_C_SOURCES := $(wildcard $(SNAKE_SOURCES_DIR)/*.c)
SNAKE_C_INCLUDES := -I$(LIBDISPATCH_HEADERS_DIR) -I$(LIBC_HEADERS_DIR) -I$(SNAKE_SOURCES_DIR)
SNAKE_OBJS := $(patsubst $(SNAKE_SOURCES_DIR)/%.c, $(SNAKE_OBJS_DIR)/%.o, $(SNAKE_C_SOURCES))


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-snake $(SNAKE_OBJS_DIR)


build-snake: $(SNAKE_FILE)

$(SNAKE_OBJS): | $(SNAKE_OBJS_DIR)

$(SNAKE_OBJS_DIR):
	$(call mkdir_if_needed,$(SNAKE_OBJS_DIR))


$(SNAKE_FILE): $(CSTART_FILE) $(SNAKE_OBJS) $(LIBC_FILE) $(LIBDISPATCH_FILE)
	@echo Linking snake
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(SNAKE_OBJS_DIR)/%.o : $(SNAKE_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(SNAKE_C_INCLUDES) -o $@ $<


clean-snake:
	$(call rm_if_exists,$(SNAKE_OBJS_DIR))
