# --------------------------------------------------------------------------
# Build variables
#

SH_SOURCES_DIR := $(SH_PROJECT_DIR)
SH_C_SOURCES := $(wildcard $(SH_SOURCES_DIR)/*.c)
SH_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(LIBC_HEADERS_DIR) -I$(LIBCLAP_HEADERS_DIR) -I$(SH_SOURCES_DIR)
SH_OBJS := $(patsubst $(SH_SOURCES_DIR)/%.c, $(SH_OBJS_DIR)/%.o, $(SH_C_SOURCES))

BUILTINS_SOURCES_DIR := $(SH_SOURCES_DIR)/builtins
BUILTINS_OBJS_DIR := $(SH_OBJS_DIR)/builtins


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-sh $(SH_OBJS_DIR)


build-sh: $(SH_FILE)

$(SH_OBJS): | $(SH_OBJS_DIR)

$(SH_OBJS_DIR):
	$(call mkdir_if_needed,$(SH_OBJS_DIR))


-include $(BUILTINS_SOURCES_DIR)/package.mk

$(SH_FILE): $(ASTART_FILE) $(SH_OBJS) $(BUILTINS_OBJS) $(LIBC_FILE) $(LIBCLAP_FILE)
	@echo Linking sh
	@$(LD) $(USER_LD_CONFIG) -s -o $@ $^


$(SH_OBJS_DIR)/%.o : $(SH_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(SH_C_INCLUDES) -o $@ $<


clean-sh:
	$(call rm_if_exists,$(SH_OBJS_DIR))
