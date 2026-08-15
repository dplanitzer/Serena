# --------------------------------------------------------------------------
# Build variables
#

LIBGD_SOURCES_DIR := $(LIBGD_PROJECT_DIR)/src
LIBGD_OBJS_DIR := $(LIB_OBJS_DIR)/libgd

LIBGD_C_SOURCES := $(wildcard $(LIBGD_SOURCES_DIR)/*.c)

LIBGD_OBJS := $(patsubst $(LIBGD_SOURCES_DIR)/%.c, $(LIBGD_OBJS_DIR)/%.o, $(LIBGD_C_SOURCES))
LIBGD_DEPS := $(LIBGD_OBJS:.o=.d)

LIBGD_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBGD_HEADERS_DIR) -I$(LIBGD_SOURCES_DIR)

#LIBGD_GENERATE_DEPS = -deps -depfile=$(patsubst $(LIBGD_OBJS_DIR)/%.o,$(LIBGD_OBJS_DIR)/%.d,$@)
LIBGD_GENERATE_DEPS := 
LIBGD_CC_DONTWARN :=


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-libgd $(LIBGD_OBJS_DIR)


build-libgd: $(LIBGD_FILE)

$(LIBGD_OBJS): | $(LIBGD_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(LIBGD_OBJS_DIR):
	$(call mkdir_if_needed,$(LIBGD_OBJS_DIR))


$(LIBGD_FILE): $(LIBGD_OBJS)
	@echo Making libgd.a
	$(LIBTOOL) create $@ $^


-include $(LIBGD_DEPS)

$(LIBGD_OBJS_DIR)/%.o : $(LIBGD_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(LIBGD_C_INCLUDES) $(LIBGD_CC_DONTWARN) $(LIBGD_GENERATE_DEPS) -o $@ $<


clean-libgd:
	$(call rm_if_exists,$(LIBGD_OBJS_DIR))
