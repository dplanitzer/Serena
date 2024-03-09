# --------------------------------------------------------------------------
# Build variables
#

DRIVER_HID_C_SOURCES := $(wildcard $(DRIVER_HID_SOURCES_DIR)/*.c)

DRIVER_HID_OBJS := $(patsubst $(DRIVER_HID_SOURCES_DIR)/%.c,$(DRIVER_HID_OBJS_DIR)/%.o,$(DRIVER_HID_C_SOURCES))
DRIVER_HID_DEPS := $(DRIVER_HID_OBJS:.o=.d)

DRIVER_HID_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR) -I$(DRIVER_HID_SOURCES_DIR)

#DRIVER_HID_GENERATE_DEPS = -deps -depfile=$(patsubst $(DRIVER_HID_OBJS_DIR)/%.o,$(DRIVER_HID_OBJS_DIR)/%.d,$@)
DRIVER_HID_GENERATE_DEPS := 
DRIVER_HID_CC_DONTWARN := -dontwarn=51


# --------------------------------------------------------------------------
# Build rules
#

$(DRIVER_HID_OBJS): | $(DRIVER_HID_OBJS_DIR)

$(DRIVER_HID_OBJS_DIR):
	$(call mkdir_if_needed,$(DRIVER_HID_OBJS_DIR))

-include $(DRIVER_HID_DEPS)

$(DRIVER_HID_OBJS_DIR)/%.o : $(DRIVER_HID_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(DRIVER_HID_C_INCLUDES) $(DRIVER_HID_CC_DONTWARN) $(DRIVER_HID_GENERATE_DEPS) -o $@ $<
