#============================================================================
# libc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

STRING_C_SOURCES := $(wildcard $(STRING_SOURCES_DIR)/*.c)

STRING_OBJS := $(patsubst $(STRING_SOURCES_DIR)/%.c,$(STRING_OBJS_DIR)/%.o,$(STRING_C_SOURCES))
STRING_DEPS := $(STRING_OBJS:.o=.d)

STRING_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(STRING_SOURCES_DIR)

#STRING_GENERATE_DEPS = -deps -depfile=$(patsubst $(STRING_OBJS_DIR)/%.o,$(STRING_OBJS_DIR)/%.d,$@)
STRING_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(STRING_OBJS): | $(STRING_OBJS_DIR)

$(STRING_OBJS_DIR):
	$(call mkdir_if_needed,$(STRING_OBJS_DIR))

-include $(STRING_DEPS)

$(STRING_OBJS_DIR)/%.o : $(STRING_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(STRING_C_INCLUDES) $(LIBC_CC_DONTWARN) $(STRING_GENERATE_DEPS) -o $@ $<


#============================================================================
# libsc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

STRING_SC_SOURCES := $(STRING_SOURCES_DIR)/ctype.c \
					 $(STRING_SOURCES_DIR)/memchr.c \
					 $(STRING_SOURCES_DIR)/memcmp.c \
					 $(STRING_SOURCES_DIR)/memcpy.c \
					 $(STRING_SOURCES_DIR)/memmove.c \
					 $(STRING_SOURCES_DIR)/memset.c \
					 $(STRING_SOURCES_DIR)/strcat.c \
					 $(STRING_SOURCES_DIR)/strchr.c \
					 $(STRING_SOURCES_DIR)/strncat.c \
					 $(STRING_SOURCES_DIR)/strcmp.c \
					 $(STRING_SOURCES_DIR)/strncmp.c \
					 $(STRING_SOURCES_DIR)/strcpy.c \
					 $(STRING_SOURCES_DIR)/strncpy.c \
					 $(STRING_SOURCES_DIR)/strlen.c \
					 $(STRING_SOURCES_DIR)/strnlen_s.c \
					 $(STRING_SOURCES_DIR)/strrchr.c \
					 $(STRING_SOURCES_DIR)/strrchr.c

STRING_SC_OBJS := $(patsubst $(STRING_SOURCES_DIR)/%.c,$(STRING_SC_OBJS_DIR)/%.o,$(STRING_SC_SOURCES))
STRING_SC_DEPS := $(STRING_SC_OBJS:.o=.d)

#STRING_SC_GENERATE_DEPS = -deps -depfile=$(patsubst $(STRING_SC_OBJS_DIR)/%.o,$(STRING_SC_OBJS_DIR)/%.d,$@)
STRING_SC_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(STRING_SC_OBJS): | $(STRING_SC_OBJS_DIR)

$(STRING_SC_OBJS_DIR):
	$(call mkdir_if_needed,$(STRING_SC_OBJS_DIR))

-include $(STRING_SC_DEPS)

$(STRING_SC_OBJS_DIR)/%.o : $(STRING_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(STRING_C_INCLUDES) $(LIBC_CC_DONTWARN) $(STRING_SC_GENERATE_DEPS) -o $@ $<
