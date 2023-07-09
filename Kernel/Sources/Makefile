# --------------------------------------------------------------------------
# Build variables
#

SOURCES_DIR = .
INCLUDE_DIR := .
RUNTIME_INCLUDE_DIR := $(PROJECT_DIR)/Library/Runtime/Sources

KERNEL_BUILD_DIR := $(BUILD_DIR)/Kernel
RUNTIME_BUILD_DIR := $(BUILD_DIR)/Library/Runtime

RUNTIME_LIB_FILE := $(RUNTIME_BUILD_DIR)/librt.a

C_SOURCES := $(wildcard $(SOURCES_DIR)/*.c)
ASM_SOURCES := $(wildcard $(SOURCES_DIR)/*.s)

C_OBJECTS	:= $(patsubst $(SOURCES_DIR)/%.c, $(KERNEL_BUILD_DIR)/%.o, $(C_SOURCES))
ASM_OBJECTS	:= $(patsubst $(SOURCES_DIR)/%.s, $(KERNEL_BUILD_DIR)/%.o, $(ASM_SOURCES))

ROM_FILE := $(KERNEL_BUILD_DIR)/Boot.rom
BIN_FILE := $(KERNEL_BUILD_DIR)/rom.bin


# --------------------------------------------------------------------------
# Includes
#

include $(PROJECT_DIR)/common.mk


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean


build: build_dir $(ROM_FILE)

build_dir:
	$(call mkdir_if_needed,$(KERNEL_BUILD_DIR))


$(ROM_FILE): $(BIN_FILE) ../finalizerom.py
	$(PY) ../finalizerom.py $(BIN_FILE) $(ROM_FILE)

$(BIN_FILE): $(RUNTIME_LIB_FILE) $(C_OBJECTS) $(ASM_OBJECTS)
	@echo Linking
	@$(LD) -s -brawbin1 -Ttext 0xfc0000 -o $@ $(KERNEL_BUILD_DIR)/traps.o $(filter-out $(KERNEL_BUILD_DIR)/traps.o, $^)


$(C_OBJECTS): $(KERNEL_BUILD_DIR)/%.o : $(SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) -I$(INCLUDE_DIR) -I$(RUNTIME_INCLUDE_DIR) -o $@ $<

$(ASM_OBJECTS): $(KERNEL_BUILD_DIR)/%.o : $(SOURCES_DIR)/%.s
	@echo $<
	@$(AS) -I$(INCLUDE_DIR) -o $@ $<


$(RUNTIME_LIB_FILE):
	+$(MAKE) build -C $(RUNTIME_INCLUDE_DIR)


clean:
	$(call rm_if_exists,$(KERNEL_BUILD_DIR))
	+$(MAKE) clean -C $(RUNTIME_INCLUDE_DIR)
