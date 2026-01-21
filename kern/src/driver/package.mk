# --------------------------------------------------------------------------
# Build variables
#

DRIVER_C_SOURCES := $(wildcard $(DRIVER_SOURCES_DIR)/*.c)

DRIVER_OBJS := $(patsubst $(DRIVER_SOURCES_DIR)/%.c,$(DRIVER_OBJS_DIR)/%.o,$(DRIVER_C_SOURCES))
DRIVER_DEPS := $(DRIVER_OBJS:.o=.d)

DRIVER_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(DRIVER_SOURCES_DIR)

#DRIVER_GENERATE_DEPS = -deps -depfile=$(patsubst $(DRIVER_OBJS_DIR)/%.o,$(DRIVER_OBJS_DIR)/%.d,$@)
DRIVER_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(DRIVER_OBJS): | $(DRIVER_OBJS_DIR) $(PRODUCT_DIR)

$(DRIVER_OBJS_DIR):
	$(call mkdir_if_needed,$(DRIVER_OBJS_DIR))

-include $(DRIVER_DEPS)

$(DRIVER_OBJS_DIR)/%.o : $(DRIVER_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(DRIVER_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(DRIVER_GENERATE_DEPS) -o $@ $<
