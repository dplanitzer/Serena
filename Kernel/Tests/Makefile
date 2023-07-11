# --------------------------------------------------------------------------
# Build variables
#

TESTS_BUILD_DIR := $(BUILD_DIR)/Kernel/Tests

C_SOURCES := $(wildcard *.c)
ASM_SOURCES := $(wildcard *.s)

C_OBJECTS	:= $(patsubst %.c, $(TESTS_BUILD_DIR)/%.o, $(C_SOURCES))
ASM_OBJECTS	:= $(patsubst %.s, $(TESTS_BUILD_DIR)/%.o, $(ASM_SOURCES))

BIN_FILE := $(TESTS_BUILD_DIR)/rom_tests.bin


# --------------------------------------------------------------------------
# Includes
#

include $(PROJECT_DIR)/common.mk


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean


build: build_dir $(BIN_FILE)

build_dir:
	$(call mkdir_if_needed,$(TESTS_BUILD_DIR))


$(BIN_FILE): $(RUNTIME_LIB_FILE) $(SYSTEM_LIB_FILE) $(C_OBJECTS) $(ASM_OBJECTS)
	@echo Linking
	@$(LD) -s -brawbin1 -Ttext 0xfe0000 -o $@ $(TESTS_BUILD_DIR)/HelloWorld.o $(filter-out $(TESTS_BUILD_DIR)/HelloWorld.o, $^)


$(C_OBJECTS): $(TESTS_BUILD_DIR)/%.o : %.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) -I. -I$(RUNTIME_INCLUDE_DIR) -I$(SYSTEM_INCLUDE_DIR) -o $@ $<

$(ASM_OBJECTS): $(TESTS_BUILD_DIR)/%.o : %.s
	@echo $<
	@$(AS) -I. -o $@ $<


clean:
	$(call rm_if_exists,$(TESTS_BUILD_DIR))
