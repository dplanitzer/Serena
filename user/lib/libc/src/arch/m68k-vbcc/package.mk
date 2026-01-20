#============================================================================
# libc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

ARCH_M68K_VBCC_C_SOURCES := $(ARCH_M68K_VBCC_SOURCES_DIR)/divmod64_kei.c \
							$(ARCH_M68K_VBCC_SOURCES_DIR)/mul64_kei.c \
							$(ARCH_M68K_VBCC_SOURCES_DIR)/rshlsh64_kei.c

ARCH_M68K_VBCC_OBJS := $(patsubst $(ARCH_M68K_VBCC_SOURCES_DIR)/%.c,$(ARCH_M68K_VBCC_OBJS_DIR)/%.o,$(ARCH_M68K_VBCC_C_SOURCES))
ARCH_M68K_VBCC_DEPS := $(ARCH_M68K_VBCC_OBJS:.o=.d)

ARCH_M68K_VBCC_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(ARCH_M68K_VBCC_SOURCES_DIR)

#ARCH_M68K_VBCC_GENERATE_DEPS = -deps -depfile=$(patsubst $(ARCH_M68K_VBCC_OBJS_DIR)/%.o,$(ARCH_M68K_VBCC_OBJS_DIR)/%.d,$@)
ARCH_M68K_VBCC_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(ARCH_M68K_VBCC_OBJS): | $(ARCH_M68K_VBCC_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(ARCH_M68K_VBCC_OBJS_DIR):
	$(call mkdir_if_needed,$(ARCH_M68K_VBCC_OBJS_DIR))

-include $(ARCH_M68K_VBCC_DEPS)

$(ARCH_M68K_VBCC_OBJS_DIR)/%.o : $(ARCH_M68K_VBCC_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(ARCH_M68K_VBCC_C_INCLUDES) $(LIBC_CC_DONTWARN) $(ARCH_M68K_VBCC_GENERATE_DEPS) -o $@ $<


#============================================================================
# libsc
#============================================================================
# --------------------------------------------------------------------------
# Build variables
#

ARCH_M68K_VBCC_SC_ASM_SOURCES := $(ARCH_M68K_VBCC_SOURCES_DIR)/__divsint64_020.s \
								 $(ARCH_M68K_VBCC_SOURCES_DIR)/__divsint64_060.s \
								 $(ARCH_M68K_VBCC_SOURCES_DIR)/__divuint64_020.s \
								 $(ARCH_M68K_VBCC_SOURCES_DIR)/__divuint64_060.s \
								 $(ARCH_M68K_VBCC_SOURCES_DIR)/__lshint64.s \
								 $(ARCH_M68K_VBCC_SOURCES_DIR)/__modsint64_020.s \
								 $(ARCH_M68K_VBCC_SOURCES_DIR)/__modsint64_060.s \
								 $(ARCH_M68K_VBCC_SOURCES_DIR)/__moduint64_020.s \
								 $(ARCH_M68K_VBCC_SOURCES_DIR)/__moduint64_060.s \
								 $(ARCH_M68K_VBCC_SOURCES_DIR)/__mulint64.s \
								 $(ARCH_M68K_VBCC_SOURCES_DIR)/__rshsint64.s \
								 $(ARCH_M68K_VBCC_SOURCES_DIR)/__rshuint64.s

ARCH_M68K_VBCC_SC_OBJS := $(patsubst $(ARCH_M68K_VBCC_SOURCES_DIR)/%.s,$(ARCH_M68K_VBCC_SC_OBJS_DIR)/%.o,$(ARCH_M68K_VBCC_SC_ASM_SOURCES))


# --------------------------------------------------------------------------
# Build rules
#

$(ARCH_M68K_VBCC_SC_OBJS): | $(ARCH_M68K_VBCC_SC_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(ARCH_M68K_VBCC_SC_OBJS_DIR):
	$(call mkdir_if_needed,$(ARCH_M68K_VBCC_SC_OBJS_DIR))

$(ARCH_M68K_VBCC_SC_OBJS_DIR)/%.o : $(ARCH_M68K_VBCC_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_CONFIG) $(ARCH_M68K_VBCC_ASM_INCLUDES) -o $@ $<
