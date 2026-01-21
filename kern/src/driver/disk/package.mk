# --------------------------------------------------------------------------
# Build variables
#

DRIVER_DISK_C_SOURCES := $(wildcard $(DRIVER_DISK_SOURCES_DIR)/*.c)

DRIVER_DISK_OBJS := $(patsubst $(DRIVER_DISK_SOURCES_DIR)/%.c,$(DRIVER_DISK_OBJS_DIR)/%.o,$(DRIVER_DISK_C_SOURCES))
DRIVER_DISK_DEPS := $(DRIVER_DISK_OBJS:.o=.d)

DRIVER_DISK_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(DRIVER_DISK_SOURCES_DIR)

#DRIVER_DISK_GENERATE_DEPS = -deps -depfile=$(patsubst $(DRIVER_DISK_OBJS_DIR)/%.o,$(DRIVER_DISK_OBJS_DIR)/%.d,$@)
DRIVER_DISK_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(DRIVER_DISK_OBJS): | $(DRIVER_DISK_OBJS_DIR) $(PRODUCT_DIR)

$(DRIVER_DISK_OBJS_DIR):
	$(call mkdir_if_needed,$(DRIVER_DISK_OBJS_DIR))

-include $(DRIVER_DISK_DEPS)

$(DRIVER_DISK_OBJS_DIR)/%.o : $(DRIVER_DISK_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(DRIVER_DISK_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(DRIVER_DISK_GENERATE_DEPS) -o $@ $<
