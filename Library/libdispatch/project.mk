# --------------------------------------------------------------------------
# Build variables
#

LIBDISPATCH_SOURCES_DIR := $(LIBDISPATCH_PROJECT_DIR)/Sources

LIBDISPATCH_C_SOURCES := $(wildcard $(LIBDISPATCH_SOURCES_DIR)/*.c)

LIBDISPATCH_OBJS := $(patsubst $(LIBDISPATCH_SOURCES_DIR)/%.c, $(LIBDISPATCH_OBJS_DIR)/%.o, $(LIBDISPATCH_C_SOURCES))
LIBDISPATCH_DEPS := $(LIBDISPATCH_OBJS:.o=.d)

LIBDISPATCH_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(LIBDISPATCH_HEADERS_DIR) -I$(LIBDISPATCH_SOURCES_DIR)

#LIBDISPATCH_GENERATE_DEPS = -deps -depfile=$(patsubst $(LIBDISPATCH_OBJS_DIR)/%.o,$(LIBDISPATCH_OBJS_DIR)/%.d,$@)
LIBDISPATCH_GENERATE_DEPS := 
LIBDISPATCH_CC_DONTWARN :=


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-libdispatch $(LIBDISPATCH_OBJS_DIR)


build-libdispatch: $(LIBDISPATCH_FILE)

$(LIBDISPATCH_OBJS): | $(LIBDISPATCH_OBJS_DIR)

$(LIBDISPATCH_OBJS_DIR):
	$(call mkdir_if_needed,$(LIBDISPATCH_OBJS_DIR))


$(LIBDISPATCH_FILE): $(LIBDISPATCH_OBJS)
	@echo Making libdispatch.a
	$(LIBTOOL) create $@ $^


-include $(LIBDISPATCH_DEPS)

$(LIBDISPATCH_OBJS_DIR)/%.o : $(LIBDISPATCH_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(LIBDISPATCH_C_INCLUDES) $(LIBDISPATCH_CC_DONTWARN) $(LIBDISPATCH_GENERATE_DEPS) -o $@ $<


clean-libdispatch:
	$(call rm_if_exists,$(LIBDISPATCH_OBJS_DIR))
