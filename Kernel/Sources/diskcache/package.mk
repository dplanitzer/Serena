# --------------------------------------------------------------------------
# Build variables
#

DISKCACHE_C_SOURCES := $(wildcard $(DISKCACHE_SOURCES_DIR)/*.c)

DISKCACHE_OBJS := $(patsubst $(DISKCACHE_SOURCES_DIR)/%.c,$(DISKCACHE_OBJS_DIR)/%.o,$(DISKCACHE_C_SOURCES))
DISKCACHE_DEPS := $(DISKCACHE_OBJS:.o=.d)

DISKCACHE_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(DISKCACHE_SOURCES_DIR)

#DISKCACHE_GENERATE_DEPS = -deps -depfile=$(patsubst $(DISKCACHE_OBJS_DIR)/%.o,$(DISKCACHE_OBJS_DIR)/%.d,$@)
DISKCACHE_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(DISKCACHE_OBJS): | $(DISKCACHE_OBJS_DIR)

$(DISKCACHE_OBJS_DIR):
	$(call mkdir_if_needed,$(DISKCACHE_OBJS_DIR))

-include $(DISKCACHE_DEPS)

$(DISKCACHE_OBJS_DIR)/%.o : $(DISKCACHE_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(DISKCACHE_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(DISKCACHE_GENERATE_DEPS) -o $@ $<
