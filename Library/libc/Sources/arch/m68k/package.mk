#============================================================================
# libc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

ARCH_M68K_C_SOURCES := $(wildcard $(ARCH_M68K_SOURCES_DIR)/*.c)
ARCH_M68K_ASM_SOURCES := $(wildcard $(ARCH_M68K_SOURCES_DIR)/*.s)

ARCH_M68K_OBJS := $(patsubst $(ARCH_M68K_SOURCES_DIR)/%.c,$(ARCH_M68K_OBJS_DIR)/%.o,$(ARCH_M68K_C_SOURCES))
ARCH_M68K_DEPS := $(ARCH_M68K_OBJS:.o=.d)
ARCH_M68K_OBJS += $(patsubst $(ARCH_M68K_SOURCES_DIR)/%.s,$(ARCH_M68K_OBJS_DIR)/%.o,$(ARCH_M68K_ASM_SOURCES))

ARCH_M68K_ASM_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(ARCH_M68K_SOURCES_DIR)
ARCH_M68K_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(ARCH_M68K_SOURCES_DIR)


# --------------------------------------------------------------------------
# Build rules
#

$(ARCH_M68K_OBJS): | $(ARCH_M68K_OBJS_DIR)

$(ARCH_M68K_OBJS_DIR):
	$(call mkdir_if_needed,$(ARCH_M68K_OBJS_DIR))

-include $(ARCH_M68K_DEPS)

$(ARCH_M68K_OBJS_DIR)/%.o : $(ARCH_M68K_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(ARCH_M68K_C_INCLUDES) $(LIBC_CC_DONTWARN) $(ARCH_M68K_GENERATE_DEPS) -o $@ $<

$(ARCH_M68K_OBJS_DIR)/%.o : $(ARCH_M68K_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(USER_ASM_CONFIG) $(ARCH_M68K_ASM_INCLUDES) -o $@ $<


#============================================================================
# libsc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

ARCH_M68K_SC_ASM_SOURCES := $(ARCH_M68K_SOURCES_DIR)/abs.s \
							$(ARCH_M68K_SOURCES_DIR)/llabs.s \
							$(ARCH_M68K_SOURCES_DIR)/div.s \
							$(ARCH_M68K_SOURCES_DIR)/udiv.s

ARCH_M68K_SC_OBJS := $(patsubst $(ARCH_M68K_SOURCES_DIR)/%.s,$(ARCH_M68K_SC_OBJS_DIR)/%.o,$(ARCH_M68K_SC_ASM_SOURCES))


# --------------------------------------------------------------------------
# Build rules
#

$(ARCH_M68K_SC_OBJS): | $(ARCH_M68K_SC_OBJS_DIR)

$(ARCH_M68K_SC_OBJS_DIR):
	$(call mkdir_if_needed,$(ARCH_M68K_SC_OBJS_DIR))

$(ARCH_M68K_SC_OBJS_DIR)/%.o : $(ARCH_M68K_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_CONFIG) $(ARCH_M68K_ASM_INCLUDES) -o $@ $<
