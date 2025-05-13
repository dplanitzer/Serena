# --------------------------------------------------------------------------
# Build variables
#

DRIVER_AMIGA_C_SOURCES := $(wildcard $(DRIVER_AMIGA_SOURCES_DIR)/*.c)

DRIVER_AMIGA_OBJS := $(patsubst $(DRIVER_AMIGA_SOURCES_DIR)/%.c,$(DRIVER_AMIGA_OBJS_DIR)/%.o,$(DRIVER_AMIGA_C_SOURCES))
DRIVER_AMIGA_DEPS := $(DRIVER_AMIGA_OBJS:.o=.d)

DRIVER_AMIGA_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(DRIVER_AMIGA_SOURCES_DIR)

#DRIVER_AMIGA_GENERATE_DEPS = -deps -depfile=$(patsubst $(DRIVER_AMIGA_OBJS_DIR)/%.o,$(DRIVER_AMIGA_OBJS_DIR)/%.d,$@)
DRIVER_AMIGA_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(DRIVER_AMIGA_OBJS): | $(DRIVER_AMIGA_OBJS_DIR)

$(DRIVER_AMIGA_OBJS_DIR):
	$(call mkdir_if_needed,$(DRIVER_AMIGA_OBJS_DIR))

-include $(DRIVER_AMIGA_DEPS)

$(DRIVER_AMIGA_OBJS_DIR)/%.o : $(DRIVER_AMIGA_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(DRIVER_AMIGA_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(DRIVER_AMIGA_GENERATE_DEPS) -o $@ $<
