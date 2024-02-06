# --------------------------------------------------------------------------
# Build variables
#

SH_SOURCES_DIR := $(WORKSPACE_DIR)/Commands/sh
SH_C_SOURCES := $(wildcard $(SH_SOURCES_DIR)/*.c)
SH_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(LIBC_HEADERS_DIR) -I$(SH_SOURCES_DIR)
SH_OBJS := $(patsubst $(SH_SOURCES_DIR)/%.c, $(SH_BUILD_DIR)/%.o, $(SH_C_SOURCES))


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-sh $(SH_BUILD_DIR)


build-sh: $(SH_BIN_FILE)

$(SH_OBJS): | $(SH_BUILD_DIR)

$(SH_BUILD_DIR):
	$(call mkdir_if_needed,$(SH_BUILD_DIR))


$(SH_BIN_FILE): $(SH_OBJS) $(LIBSYSTEM_LIB_FILE) $(LIBC_LIB_FILE) | $(KERNEL_PRODUCT_DIR)
	@echo Linking sh
	@$(LD) -s -bataritos -T $(SH_SOURCES_DIR)/linker.script -o $@ $(LIBC_ASTART_FILE) $^


$(SH_BUILD_DIR)/%.o : $(SH_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) $(SH_C_INCLUDES) -o $@ $<


clean-sh:
	$(call rm_if_exists,$(SH_BUILD_DIR))
