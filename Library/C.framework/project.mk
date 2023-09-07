# --------------------------------------------------------------------------
# Build variables
#

LIBC_SOURCES_DIR := $(LIBC_PROJECT_DIR)/Sources

LIBC_ASTART_C_SOURCE := $(LIBC_SOURCES_DIR)/_astart.c
LIBC_CSTART_C_SOURCE := $(LIBC_SOURCES_DIR)/_cstart.c

LIBC_C_SOURCES := $(filter-out $(LIBC_SOURCES_DIR)/_astart.c $(LIBC_SOURCES_DIR)/_cstart.c, $(wildcard $(LIBC_SOURCES_DIR)/*.c))
LIBC_ASM_SOURCES := $(wildcard $(LIBC_SOURCES_DIR)/*.s)

LIBC_OBJS := $(patsubst $(LIBC_SOURCES_DIR)/%.c, $(LIBC_BUILD_DIR)/%.o, $(LIBC_C_SOURCES))
LIBC_DEPS := $(LIBC_OBJS:.o=.d)
LIBC_OBJS += $(patsubst $(LIBC_SOURCES_DIR)/%.s, $(LIBC_BUILD_DIR)/%.o, $(LIBC_ASM_SOURCES))

LIBC_C_INCLUDES := -I$(LIBABI_HEADERS_DIR) -I$(LIBC_HEADERS_DIR) -I$(LIBC_SOURCES_DIR)
LIBC_ASM_INCLUDES := -I$(LIBABI_HEADERS_DIR) -I$(LIBC_HEADERS_DIR) -I$(LIBC_SOURCES_DIR)


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-libc $(LIBC_BUILD_DIR) $(LIBC_PRODUCT_DIR)


build-libc: $(LIBC_LIB_FILE) $(LIBC_ASTART_FILE) $(LIBC_CSTART_FILE)

$(LIBC_OBJS): | $(LIBC_BUILD_DIR)

$(LIBC_BUILD_DIR):
	$(call mkdir_if_needed,$(LIBC_BUILD_DIR))

$(LIBC_PRODUCT_DIR):
	$(call mkdir_if_needed,$(LIBC_PRODUCT_DIR))


$(LIBC_LIB_FILE): $(LIBC_OBJS) | $(LIBC_PRODUCT_DIR)
	@echo Linking libc.a
	@$(LD) -baoutnull -r -o $@ $^


-include $(LIBC_DEPS)

$(LIBC_BUILD_DIR)/%.o : $(LIBC_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) $(LIBC_C_INCLUDES) -dontwarn=214 -deps -depfile=$(patsubst $(LIBC_BUILD_DIR)/%.o,$(LIBC_BUILD_DIR)/%.d,$@) -o $@ $<

$(LIBC_BUILD_DIR)/%.o : $(LIBC_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(LIBC_ASM_INCLUDES) -o $@ $<


# start() function(s)
$(LIBC_ASTART_FILE) : $(LIBC_ASTART_C_SOURCE) | $(LIBC_PRODUCT_DIR)
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_PREPROCESSOR_DEFINITIONS) $(LIBC_C_INCLUDES) -deps -depfile=$(patsubst $(LIBC_BUILD_DIR)/%.o,$(LIBC_BUILD_DIR)/%.d,$@) -o $@ $<

$(LIBC_CSTART_FILE) : $(LIBC_CSTART_C_SOURCE) | $(LIBC_PRODUCT_DIR)
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_PREPROCESSOR_DEFINITIONS) $(LIBC_C_INCLUDES) -deps -depfile=$(patsubst $(LIBC_BUILD_DIR)/%.o,$(LIBC_BUILD_DIR)/%.d,$@) -o $@ $<


clean-libc:
	$(call rm_if_exists,$(LIBC_BUILD_DIR))
