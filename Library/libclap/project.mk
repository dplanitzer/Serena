# --------------------------------------------------------------------------
# Build variables
#

LIBCLAP_SOURCES_DIR := $(LIBCLAP_PROJECT_DIR)/Sources

LIBCLAP_C_SOURCES := $(wildcard $(LIBCLAP_SOURCES_DIR)/*.c)

LIBCLAP_OBJS := $(patsubst $(LIBCLAP_SOURCES_DIR)/%.c, $(LIBCLAP_OBJS_DIR)/%.o, $(LIBCLAP_C_SOURCES))
LIBCLAP_DEPS := $(LIBCLAP_OBJS:.o=.d)

LIBCLAP_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(LIBCLAP_HEADERS_DIR) -I$(LIBCLAP_SOURCES_DIR)

#LIBCLAP_GENERATE_DEPS = -deps -depfile=$(patsubst $(LIBCLAP_OBJS_DIR)/%.o,$(LIBCLAP_OBJS_DIR)/%.d,$@)
LIBCLAP_GENERATE_DEPS := 
LIBCLAP_CC_DONTWARN :=


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-libclap $(LIBCLAP_OBJS_DIR)


build-libclap: $(LIBCLAP_FILE)

$(LIBCLAP_OBJS): | $(LIBCLAP_OBJS_DIR)

$(LIBCLAP_OBJS_DIR):
	$(call mkdir_if_needed,$(LIBCLAP_OBJS_DIR))


$(LIBCLAP_FILE): $(LIBCLAP_OBJS)
	@echo Making libclap.a
	$(LIBTOOL) create $@ $^


-include $(LIBCLAP_DEPS)

$(LIBCLAP_OBJS_DIR)/%.o : $(LIBCLAP_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(LIBCLAP_C_INCLUDES) $(LIBCLAP_CC_DONTWARN) $(LIBCLAP_GENERATE_DEPS) -o $@ $<


clean-libclap:
	$(call rm_if_exists,$(LIBCLAP_OBJS_DIR))
