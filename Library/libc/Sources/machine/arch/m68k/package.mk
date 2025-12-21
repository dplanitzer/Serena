# --------------------------------------------------------------------------
# Build variables
#

ARCH_M68K_ASM_SOURCES := $(wildcard $(ARCH_M68K_SOURCES_DIR)/*.s)

ARCH_M68K_OBJS := $(patsubst $(ARCH_M68K_SOURCES_DIR)/%.s,$(ARCH_M68K_OBJS_DIR)/%.o,$(ARCH_M68K_ASM_SOURCES))
ARCH_M68K_DEPS := $(ARCH_M68K_OBJS:.o=.d)

ARCH_M68K_ASM_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(ARCH_M68K_SOURCES_DIR)


# --------------------------------------------------------------------------
# Build rules
#

$(ARCH_M68K_OBJS): | $(ARCH_M68K_OBJS_DIR)

$(ARCH_M68K_OBJS_DIR):
	$(call mkdir_if_needed,$(ARCH_M68K_OBJS_DIR))


$(ARCH_M68K_OBJS_DIR)/%.o : $(ARCH_M68K_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(USER_ASM_CONFIG) $(ARCH_M68K_ASM_INCLUDES) -o $@ $<
