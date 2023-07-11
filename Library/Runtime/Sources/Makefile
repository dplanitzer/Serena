# --------------------------------------------------------------------------
# Build variables
#

C_SOURCES := $(wildcard *.c)
ASM_SOURCES := $(wildcard *.s)

C_OBJECTS	:= $(patsubst %.c, $(RUNTIME_BUILD_DIR)/%.o, $(C_SOURCES))
ASM_OBJECTS	:= $(patsubst %.s, $(RUNTIME_BUILD_DIR)/%.o, $(ASM_SOURCES))


# --------------------------------------------------------------------------
# Includes
#

include $(PROJECT_DIR)/common.mk


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean


build: build_dir $(RUNTIME_LIB_FILE)

build_dir:
	$(call mkdir_if_needed,$(RUNTIME_BUILD_DIR))


$(RUNTIME_LIB_FILE): $(C_OBJECTS) $(ASM_OBJECTS)
	@echo Linking
	@$(LD) -baoutnull -r -o $@ $^


$(C_OBJECTS): $(RUNTIME_BUILD_DIR)/%.o : %.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) -I. -o $@ $<

$(ASM_OBJECTS): $(RUNTIME_BUILD_DIR)/%.o : %.s
	@echo $<
	@$(AS) -I. -o $@ $<


clean:
	$(call rm_if_exists,$(RUNTIME_BUILD_DIR))
