#============================================================================
# libsc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

ARCH_M68K_VBCC_SC_SOURCES := $(ARCH_M68K_VBCC_SOURCES_DIR)/divmod64.c

ARCH_M68K_VBCC_SC_ASM_SOURCES := $(ARCH_M68K_VBCC_SOURCES_DIR)/bits64_m68k.s \
								 $(ARCH_M68K_VBCC_SOURCES_DIR)/mul64_m68k.s

ARCH_M68K_VBCC_SC_OBJS := $(patsubst $(ARCH_M68K_VBCC_SOURCES_DIR)/%.c,$(ARCH_M68K_VBCC_SC_OBJS_DIR)/%.o,$(ARCH_M68K_VBCC_SC_SOURCES))
ARCH_M68K_VBCC_SC_DEPS := $(ARCH_M68K_VBCC_SC_OBJS:.o=.d)
ARCH_M68K_VBCC_SC_OBJS += $(patsubst $(ARCH_M68K_VBCC_SOURCES_DIR)/%.s,$(ARCH_M68K_VBCC_SC_OBJS_DIR)/%.o,$(ARCH_M68K_VBCC_SC_ASM_SOURCES))

ARCH_M68K_VBCC_C_INCLUDES := -I$(ARCH_M68K_VBCC_SOURCES_DIR)
ARCH_M68K_VBCC_ASM_INCLUDES := -I$(ARCH_M68K_VBCC_SOURCES_DIR)

#ARCH_M68K_VBCC_SC_GENERATE_DEPS = -deps -depfile=$(patsubst $(ARCH_M68K_VBCC_SC_OBJS_DIR)/%.o,$(ARCH_M68K_VBCC_SC_OBJS_DIR)/%.d,$@)
ARCH_M68K_VBCC_SC_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(ARCH_M68K_VBCC_SC_OBJS): | $(ARCH_M68K_VBCC_SC_OBJS_DIR)

$(ARCH_M68K_VBCC_SC_OBJS_DIR):
	$(call mkdir_if_needed,$(ARCH_M68K_VBCC_SC_OBJS_DIR))

-include $(ARCH_M68K_VBCC_SC_DEPS)

$(ARCH_M68K_VBCC_SC_OBJS_DIR)/%.o : $(ARCH_M68K_VBCC_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(ARCH_M68K_VBCC_C_INCLUDES) $(LIBC_CC_DONTWARN) $(ARCH_M68K_VBCC_SC_GENERATE_DEPS) -o $@ $<

$(ARCH_M68K_VBCC_SC_OBJS_DIR)/%.o : $(ARCH_M68K_VBCC_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_CONFIG) $(ARCH_M68K_VBCC_SC_ASM_INCLUDES) -o $@ $<
