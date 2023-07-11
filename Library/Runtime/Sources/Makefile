# --------------------------------------------------------------------------
# Build variables
#

C_SOURCES := $(wildcard *.c)
ASM_SOURCES := $(wildcard *.s)

OBJS := $(patsubst %.c, $(RUNTIME_BUILD_DIR)/%.o, $(C_SOURCES))
$(eval OBJS += $(patsubst %.s, $(RUNTIME_BUILD_DIR)/%.o, $(ASM_SOURCES)))


# --------------------------------------------------------------------------
# Includes
#

include $(PROJECT_DIR)/common.mk


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean


build: $(RUNTIME_LIB_FILE)

$(OBJS): | $(RUNTIME_BUILD_DIR)

$(RUNTIME_BUILD_DIR):
	$(call mkdir_if_needed,$(RUNTIME_BUILD_DIR))


$(RUNTIME_LIB_FILE): $(OBJS)
	@$(LD) -baoutnull -r -o $@ $^


$(RUNTIME_BUILD_DIR)/%.o : %.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) -I. -o $@ $<

$(RUNTIME_BUILD_DIR)/%.o : %.s
	@echo $<
	@$(AS) -I. -o $@ $<


clean:
	$(call rm_if_exists,$(RUNTIME_BUILD_DIR))
