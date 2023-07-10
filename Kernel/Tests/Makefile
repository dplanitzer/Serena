# --------------------------------------------------------------------------
# Build variables
#

SOURCES_DIR = .
INCLUDE_DIR := .
RUNTIME_INCLUDE_DIR := $(PROJECT_DIR)/Library/Runtime/Sources
SYSTEM_INCLUDE_DIR := $(PROJECT_DIR)/Library/System/Sources

TESTS_BUILD_DIR := $(BUILD_DIR)/Kernel/Tests
RUNTIME_BUILD_DIR := $(BUILD_DIR)/Library/Runtime
SYSTEM_BUILD_DIR := $(BUILD_DIR)/Library/System

RUNTIME_LIB_FILE := $(RUNTIME_BUILD_DIR)/librt.a
SYSTEM_LIB_FILE := $(SYSTEM_BUILD_DIR)/libsystem.a

C_SOURCES := $(wildcard $(SOURCES_DIR)/*.c)
ASM_SOURCES := $(wildcard $(SOURCES_DIR)/*.s)

C_OBJECTS	:= $(patsubst $(SOURCES_DIR)/%.c, $(TESTS_BUILD_DIR)/%.o, $(C_SOURCES))
ASM_OBJECTS	:= $(patsubst $(SOURCES_DIR)/%.s, $(TESTS_BUILD_DIR)/%.o, $(ASM_SOURCES))

BIN_FILE := $(TESTS_BUILD_DIR)/rom_tests.bin


# --------------------------------------------------------------------------
# Includes
#

include $(PROJECT_DIR)/common.mk


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean


build: build_dir $(BIN_FILE) $(SYSTEM_LIB_FILE)

build_dir:
	$(call mkdir_if_needed,$(TESTS_BUILD_DIR))


$(BIN_FILE): $(C_OBJECTS) $(ASM_OBJECTS)
	@echo Linking
	@$(LD) -s -brawbin1 -Ttext 0xfe0000 -o $@ $^ $(RUNTIME_LIB_FILE) $(SYSTEM_LIB_FILE)


$(C_OBJECTS): $(TESTS_BUILD_DIR)/%.o : $(SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) -I$(INCLUDE_DIR) -I$(RUNTIME_INCLUDE_DIR) -I$(SYSTEM_INCLUDE_DIR) -o $@ $<

$(ASM_OBJECTS): $(TESTS_BUILD_DIR)/%.o : $(SOURCES_DIR)/%.s
	@echo $<
	@$(AS) -I$(INCLUDE_DIR) -o $@ $<


clean:
	$(call rm_if_exists,$(TESTS_BUILD_DIR))
