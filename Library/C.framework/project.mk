# --------------------------------------------------------------------------
# Build variables
#

CLIB_SOURCES_DIR := $(CLIB_PROJECT_DIR)/Sources

CLIB_ASTART_C_SOURCE := $(CLIB_SOURCES_DIR)/_astart.c
CLIB_CSTART_C_SOURCE := $(CLIB_SOURCES_DIR)/_cstart.c

CLIB_C_SOURCES := $(filter-out $(CLIB_SOURCES_DIR)/_astart.c $(CLIB_SOURCES_DIR)/_cstart.c, $(wildcard $(CLIB_SOURCES_DIR)/*.c))
CLIB_ASM_SOURCES := $(wildcard $(CLIB_SOURCES_DIR)/*.s)

CLIB_OBJS := $(patsubst $(CLIB_SOURCES_DIR)/%.c, $(CLIB_BUILD_DIR)/%.o, $(CLIB_C_SOURCES))
CLIB_DEPS := $(CLIB_OBJS:.o=.d)
CLIB_OBJS += $(patsubst $(CLIB_SOURCES_DIR)/%.s, $(CLIB_BUILD_DIR)/%.o, $(CLIB_ASM_SOURCES))


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean_clib


$(CLIB_OBJS): | $(CLIB_BUILD_DIR) $(CLIB_PRODUCT_DIR) $(CLIB_ASTART_FILE) $(CLIB_CSTART_FILE)

$(CLIB_BUILD_DIR):
	$(call mkdir_if_needed,$(CLIB_BUILD_DIR))

$(CLIB_PRODUCT_DIR):
	$(call mkdir_if_needed,$(CLIB_PRODUCT_DIR))


$(CLIB_LIB_FILE): $(CLIB_OBJS)
	@echo Linking libc.a
	@$(LD) -baoutnull -r -o $@ $^


-include $(CLIB_DEPS)

$(CLIB_BUILD_DIR)/%.o : $(CLIB_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) -I$(CLIB_HEADERS_DIR) -I$(CLIB_SOURCES_DIR) -I$(SYSTEM_HEADERS_DIR) -dontwarn=214 -deps -depfile=$(patsubst $(CLIB_BUILD_DIR)/%.o,$(CLIB_BUILD_DIR)/%.d,$@) -o $@ $<

$(CLIB_BUILD_DIR)/%.o : $(CLIB_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) -I$(CLIB_SOURCES_DIR) -o $@ $<


# start() function(s)
$(CLIB_ASTART_FILE) : $(CLIB_ASTART_C_SOURCE)
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_PREPROCESSOR_DEFINITIONS) -I$(CLIB_HEADERS_DIR) -I$(CLIB_SOURCES_DIR) -I$(SYSTEM_HEADERS_DIR) -deps -depfile=$(patsubst $(CLIB_BUILD_DIR)/%.o,$(CLIB_BUILD_DIR)/%.d,$@) -o $@ $<

$(CLIB_CSTART_FILE) : $(CLIB_CSTART_C_SOURCE)
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_PREPROCESSOR_DEFINITIONS) -I$(CLIB_HEADERS_DIR) -I$(CLIB_SOURCES_DIR) -I$(SYSTEM_HEADERS_DIR) -deps -depfile=$(patsubst $(CLIB_BUILD_DIR)/%.o,$(CLIB_BUILD_DIR)/%.d,$@) -o $@ $<


clean_clib:
	$(call rm_if_exists,$(CLIB_BUILD_DIR))
