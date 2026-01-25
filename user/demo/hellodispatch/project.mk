# --------------------------------------------------------------------------
# Build variables
#

HELLODISPATCH_SOURCES_DIR := $(HELLODISPATCH_PROJECT_DIR)
HELLODISPATCH_OBJS_DIR := $(DEMOS_OBJS_DIR)/hellodispatch

HELLODISPATCH_C_SOURCES := $(wildcard $(HELLODISPATCH_SOURCES_DIR)/*.c)
HELLODISPATCH_C_INCLUDES := -I$(LIBDISPATCH_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_HEADERS_DIR) -I$(HELLODISPATCH_SOURCES_DIR)
HELLODISPATCH_OBJS := $(patsubst $(HELLODISPATCH_SOURCES_DIR)/%.c, $(HELLODISPATCH_OBJS_DIR)/%.o, $(HELLODISPATCH_C_SOURCES))


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-hellodispatch $(HELLODISPATCH_OBJS_DIR)


build-hellodispatch: $(HELLODISPATCH_FILE)

$(HELLODISPATCH_OBJS): | $(HELLODISPATCH_OBJS_DIR) $(PRODUCT_DEMO_DIR)

$(HELLODISPATCH_OBJS_DIR):
	$(call mkdir_if_needed,$(HELLODISPATCH_OBJS_DIR))


$(HELLODISPATCH_FILE): $(CSTART_FILE) $(HELLODISPATCH_OBJS) $(LIBC_FILE) $(LIBDISPATCH_FILE)
	@echo Linking hellodispatch
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(HELLODISPATCH_OBJS_DIR)/%.o : $(HELLODISPATCH_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(HELLODISPATCH_C_INCLUDES) -o $@ $<


clean-hellodispatch:
	$(call rm_if_exists,$(HELLODISPATCH_OBJS_DIR))
