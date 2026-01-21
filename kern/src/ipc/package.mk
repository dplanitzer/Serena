# --------------------------------------------------------------------------
# Build variables
#

IPC_C_SOURCES := $(wildcard $(IPC_SOURCES_DIR)/*.c)

IPC_OBJS := $(patsubst $(IPC_SOURCES_DIR)/%.c,$(IPC_OBJS_DIR)/%.o,$(IPC_C_SOURCES))
IPC_DEPS := $(IPC_OBJS:.o=.d)

IPC_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(IPC_SOURCES_DIR)

#IPC_GENERATE_DEPS = -deps -depfile=$(patsubst $(IPC_OBJS_DIR)/%.o,$(IPC_OBJS_DIR)/%.d,$@)
IPC_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(IPC_OBJS): | $(IPC_OBJS_DIR) $(PRODUCT_DIR)

$(IPC_OBJS_DIR):
	$(call mkdir_if_needed,$(IPC_OBJS_DIR))

-include $(IPC_DEPS)

$(IPC_OBJS_DIR)/%.o : $(IPC_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(IPC_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(IPC_GENERATE_DEPS) -o $@ $<
