# --------------------------------------------------------------------------
# Build variables
#

IEEEFP_C_SOURCES := $(wildcard $(IEEEFP_SOURCES_DIR)/*.c)

IEEEFP_OBJS := $(patsubst $(IEEEFP_SOURCES_DIR)/%.c,$(IEEEFP_OBJS_DIR)/%.o,$(IEEEFP_C_SOURCES))
IEEEFP_DEPS := $(IEEEFP_OBJS:.o=.d)

IEEEFP_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(LIBM_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_SOURCES_DIR) -I$(IEEEFP_SOURCES_DIR)

#IEEEFP_GENERATE_DEPS = -deps -depfile=$(patsubst $(IEEEFP_OBJS_DIR)/%.o,$(IEEEFP_OBJS_DIR)/%.d,$@)
IEEEFP_GENERATE_DEPS := 


# --------------------------------------------------------------------------
# Build rules
#

$(IEEEFP_OBJS): | $(IEEEFP_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(IEEEFP_OBJS_DIR):
	$(call mkdir_if_needed,$(IEEEFP_OBJS_DIR))

-include $(IEEEFP_DEPS)

$(IEEEFP_OBJS_DIR)/%.o : $(IEEEFP_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) -D__IEEE_BIG_ENDIAN=1 -DNO_BF96=1 -DMULTIPLE_THREADS=1 $(IEEEFP_C_INCLUDES) $(LIBC_CC_DONTWARN) $(IEEEFP_GENERATE_DEPS) -o $@ $<
