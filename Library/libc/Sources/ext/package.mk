#============================================================================
# libc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

EXT_C_SOURCES := $(wildcard $(EXT_SOURCES_DIR)/*.c)

EXT_OBJS := $(patsubst $(EXT_SOURCES_DIR)/%.c,$(EXT_OBJS_DIR)/%.o,$(EXT_C_SOURCES))
EXT_DEPS := $(EXT_OBJS:.o=.d)

EXT_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(EXT_SOURCES_DIR)

#EXT_GENERATE_DEPS = -deps -depfile=$(patsubst $(EXT_OBJS_DIR)/%.o,$(EXT_OBJS_DIR)/%.d,$@)
EXT_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(EXT_OBJS): | $(EXT_OBJS_DIR)

$(EXT_OBJS_DIR):
	$(call mkdir_if_needed,$(EXT_OBJS_DIR))

-include $(EXT_DEPS)

$(EXT_OBJS_DIR)/%.o : $(EXT_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(EXT_C_INCLUDES) $(LIBC_CC_DONTWARN) $(EXT_GENERATE_DEPS) -o $@ $<


#============================================================================
# libsc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

EXT_SC_SOURCES := $(EXT_SOURCES_DIR)/hash.c \
				  $(EXT_SOURCES_DIR)/ilog2.c \
				  $(EXT_SOURCES_DIR)/ipow2.c \
				  $(EXT_SOURCES_DIR)/timespec.c \
				  $(EXT_SOURCES_DIR)/queue.c \
				  $(EXT_SOURCES_DIR)/__fmt.c

EXT_SC_OBJS := $(patsubst $(EXT_SOURCES_DIR)/%.c,$(EXT_SC_OBJS_DIR)/%.o,$(EXT_SC_SOURCES))
EXT_SC_DEPS := $(EXT_SC_OBJS:.o=.d)

#EXT_SC_GENERATE_DEPS = -deps -depfile=$(patsubst $(EXT_SC_OBJS_DIR)/%.o,$(EXT_SC_OBJS_DIR)/%.d,$@)
EXT_SC_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(EXT_SC_OBJS): | $(EXT_SC_OBJS_DIR)

$(EXT_SC_OBJS_DIR):
	$(call mkdir_if_needed,$(EXT_SC_OBJS_DIR))

-include $(EXT_SC_DEPS)

$(EXT_SC_OBJS_DIR)/%.o : $(EXT_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(EXT_C_INCLUDES) $(LIBC_CC_DONTWARN) $(EXT_SC_GENERATE_DEPS) -o $@ $<
