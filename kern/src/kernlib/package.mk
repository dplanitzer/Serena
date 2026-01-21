# --------------------------------------------------------------------------
# Build variables
#

KERNLIB_C_SOURCES := $(wildcard $(KERNLIB_SOURCES_DIR)/*.c)

KERNLIB_OBJS := $(patsubst $(KERNLIB_SOURCES_DIR)/%.c,$(KERNLIB_OBJS_DIR)/%.o,$(KERNLIB_C_SOURCES))
KERNLIB_DEPS := $(KERNLIB_OBJS:.o=.d)

KERNLIB_C_INCLUDES := $(KERNEL_C_INCLUDES) -I$(KERNLIB_SOURCES_DIR)

#KERNLIB_GENERATE_DEPS = -deps -depfile=$(patsubst $(KERNLIB_OBJS_DIR)/%.o,$(KERNLIB_OBJS_DIR)/%.d,$@)
KERNLIB_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(KERNLIB_OBJS): | $(KERNLIB_OBJS_DIR) $(PRODUCT_DIR)

$(KERNLIB_OBJS_DIR):
	$(call mkdir_if_needed,$(KERNLIB_OBJS_DIR))

-include $(KERNLIB_DEPS)

$(KERNLIB_OBJS_DIR)/%.o : $(KERNLIB_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_KOPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(KERNLIB_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(KERNLIB_GENERATE_DEPS) -o $@ $<
