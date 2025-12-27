#============================================================================
# libc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

STDLIB_C_SOURCES := $(wildcard $(STDLIB_SOURCES_DIR)/*.c)

STDLIB_OBJS := $(patsubst $(STDLIB_SOURCES_DIR)/%.c,$(STDLIB_OBJS_DIR)/%.o,$(STDLIB_C_SOURCES))
STDLIB_DEPS := $(STDLIB_OBJS:.o=.d)

STDLIB_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(STDLIB_SOURCES_DIR)

#STDLIB_GENERATE_DEPS = -deps -depfile=$(patsubst $(STDLIB_OBJS_DIR)/%.o,$(STDLIB_OBJS_DIR)/%.d,$@)
STDLIB_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(STDLIB_OBJS): | $(STDLIB_OBJS_DIR)

$(STDLIB_OBJS_DIR):
	$(call mkdir_if_needed,$(STDLIB_OBJS_DIR))

-include $(STDLIB_DEPS)

$(STDLIB_OBJS_DIR)/%.o : $(STDLIB_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(STDLIB_C_INCLUDES) $(LIBC_CC_DONTWARN) $(STDLIB_GENERATE_DEPS) -o $@ $<


#============================================================================
# libsc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

STDLIB_SC_SOURCES := $(STDLIB_SOURCES_DIR)/abs.c \
					 $(STDLIB_SOURCES_DIR)/bsearch.c \
					 $(STDLIB_SOURCES_DIR)/div.c \
					 $(STDLIB_SOURCES_DIR)/qsort.c \
					 $(STDLIB_SOURCES_DIR)/__i32toa.c \
					 $(STDLIB_SOURCES_DIR)/__i64toa.c \
					 $(STDLIB_SOURCES_DIR)/__strtoi64.c \
					 $(STDLIB_SOURCES_DIR)/__u32toa.c \
					 $(STDLIB_SOURCES_DIR)/__u64toa.c

STDLIB_SC_OBJS := $(patsubst $(STDLIB_SOURCES_DIR)/%.c,$(STDLIB_SC_OBJS_DIR)/%.o,$(STDLIB_SC_SOURCES))
STDLIB_SC_DEPS := $(STDLIB_SC_OBJS:.o=.d)

#STDLIB_SC_GENERATE_DEPS = -deps -depfile=$(patsubst $(STDLIB_SC_OBJS_DIR)/%.o,$(STDLIB_SC_OBJS_DIR)/%.d,$@)
STDLIB_SC_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(STDLIB_SC_OBJS): | $(STDLIB_SC_OBJS_DIR)

$(STDLIB_SC_OBJS_DIR):
	$(call mkdir_if_needed,$(STDLIB_SC_OBJS_DIR))

-include $(STDLIB_SC_DEPS)

$(STDLIB_SC_OBJS_DIR)/%.o : $(STDLIB_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(STDLIB_C_INCLUDES) $(LIBC_CC_DONTWARN) $(STDLIB_SC_GENERATE_DEPS) -o $@ $<
