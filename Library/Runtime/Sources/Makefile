# --------------------------------------------------------------------------
# Build variables
#

SOURCES_DIR = .
INCLUDE_DIR := .

RUNTIME_BUILD_DIR := $(BUILD_DIR)/Library/Runtime

C_SOURCES := $(wildcard $(SOURCES_DIR)/*.c)
ASM_SOURCES := $(wildcard $(SOURCES_DIR)/*.s)

C_OBJECTS	:= $(patsubst $(SOURCES_DIR)/%.c, $(RUNTIME_BUILD_DIR)/%.o, $(C_SOURCES))
ASM_OBJECTS	:= $(patsubst $(SOURCES_DIR)/%.s, $(RUNTIME_BUILD_DIR)/%.o, $(ASM_SOURCES))

LIB_FILE := $(RUNTIME_BUILD_DIR)/librt.a


# --------------------------------------------------------------------------
# Includes
#

include $(PROJECT_DIR)/common.mk


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean


build: build_dir $(LIB_FILE)

build_dir:
	$(call mkdir_if_needed,$(RUNTIME_BUILD_DIR))


$(LIB_FILE): $(C_OBJECTS) $(ASM_OBJECTS)
	@echo Linking
	@$(LD) -baoutnull -r -o $@ $^


$(C_OBJECTS): $(RUNTIME_BUILD_DIR)/%.o : $(SOURCES_DIR)/%.c
	@echo $<
	@$(CC) -nostdlib $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) -I$(INCLUDE_DIR) -o $@ $<

$(ASM_OBJECTS): $(RUNTIME_BUILD_DIR)/%.o : $(SOURCES_DIR)/%.s
	@echo $<
	@$(AS) -I$(INCLUDE_DIR) -o $@ $<


clean:
	$(call rm_if_exists,$(RUNTIME_BUILD_DIR))
