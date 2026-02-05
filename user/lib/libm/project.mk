# --------------------------------------------------------------------------
# Build variables
#

LIBM_SOURCES_DIR := $(LIBM_PROJECT_DIR)/src
LIBM_OBJS_DIR := $(LIB_OBJS_DIR)/libm

M68K_SOURCES_DIR := $(LIBM_SOURCES_DIR)/arch/m68k
M68K_OBJS_DIR := $(LIBM_OBJS_DIR)/arch/m68k

LIBM_C_SOURCES := $(wildcard $(LIBM_SOURCES_DIR)/*.c)
LIBM_ASM_SOURCES := $(wildcard $(LIBM_SOURCES_DIR)/*.s)

LIBM_OBJS := $(patsubst $(LIBM_SOURCES_DIR)/%.c, $(LIBM_OBJS_DIR)/%.o, $(LIBM_C_SOURCES))
LIBM_DEPS := $(LIBM_OBJS:.o=.d)
LIBM_OBJS += $(patsubst $(LIBM_SOURCES_DIR)/%.s, $(LIBM_OBJS_DIR)/%.o, $(LIBM_ASM_SOURCES))

LIBM_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(LIBM_HEADERS_DIR) -I$(LIBM_SOURCES_DIR)
LIBM_ASM_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(LIBM_HEADERS_DIR) -I$(LIBM_SOURCES_DIR)

#LIBM_GENERATE_DEPS = -deps -depfile=$(patsubst $(LIBM_OBJS_DIR)/%.o,$(LIBM_OBJS_DIR)/%.d,$@)
LIBM_GENERATE_DEPS := 
LIBM_CC_DONTWARN :=
LIBM_AS_DONTWARN :=


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-libm $(LIBM_OBJS_DIR)


build-libm: $(LIBM_FILE)

$(LIBM_OBJS): | $(LIBM_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(LIBM_OBJS_DIR):
	$(call mkdir_if_needed,$(LIBM_OBJS_DIR))


-include $(M68K_SOURCES_DIR)/package.mk

$(LIBM_FILE): $(M68K_OBJS) $(LIBM_OBJS)
	@echo Making libm.a
	$(LIBTOOL) create $@ $^


-include $(LIBM_DEPS)

$(LIBM_OBJS_DIR)/%.o : $(LIBM_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(LIBM_C_INCLUDES) $(LIBM_CC_DONTWARN) $(LIBM_GENERATE_DEPS) -o $@ $<

$(LIBM_OBJS_DIR)/%.o : $(LIBM_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(USER_ASM_CONFIG) $(LIBM_ASM_INCLUDES) -o $@ $<


clean-libm:
	$(call rm_if_exists,$(LIBM_OBJS_DIR))
