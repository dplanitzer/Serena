#============================================================================
# libc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

MALLOC_C_SOURCES := $(wildcard $(MALLOC_SOURCES_DIR)/*.c)

MALLOC_OBJS := $(patsubst $(MALLOC_SOURCES_DIR)/%.c,$(MALLOC_OBJS_DIR)/%.o,$(MALLOC_C_SOURCES))
MALLOC_DEPS := $(MALLOC_OBJS:.o=.d)

MALLOC_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(MALLOC_SOURCES_DIR)

#MALLOC_GENERATE_DEPS = -deps -depfile=$(patsubst $(MALLOC_OBJS_DIR)/%.o,$(MALLOC_OBJS_DIR)/%.d,$@)
MALLOC_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(MALLOC_OBJS): | $(MALLOC_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(MALLOC_OBJS_DIR):
	$(call mkdir_if_needed,$(MALLOC_OBJS_DIR))

-include $(MALLOC_DEPS)

$(MALLOC_OBJS_DIR)/%.o : $(MALLOC_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(MALLOC_C_INCLUDES) $(LIBC_CC_DONTWARN) $(MALLOC_GENERATE_DEPS) -o $@ $<


#============================================================================
# libsc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

MALLOC_SC_SOURCES := $(MALLOC_SOURCES_DIR)/__lsta.c

MALLOC_SC_OBJS := $(patsubst $(MALLOC_SOURCES_DIR)/%.c,$(MALLOC_SC_OBJS_DIR)/%.o,$(MALLOC_SC_SOURCES))
MALLOC_SC_DEPS := $(MALLOC_SC_OBJS:.o=.d)

#MALLOC_SC_GENERATE_DEPS = -deps -depfile=$(patsubst $(MALLOC_SC_OBJS_DIR)/%.o,$(MALLOC_SC_OBJS_DIR)/%.d,$@)
MALLOC_SC_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(MALLOC_SC_OBJS): | $(MALLOC_SC_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(MALLOC_SC_OBJS_DIR):
	$(call mkdir_if_needed,$(MALLOC_SC_OBJS_DIR))

-include $(MALLOC_SC_DEPS)

$(MALLOC_SC_OBJS_DIR)/%.o : $(MALLOC_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(MALLOC_C_INCLUDES) $(LIBC_CC_DONTWARN) $(MALLOC_SC_GENERATE_DEPS) -o $@ $<
