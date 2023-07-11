# --------------------------------------------------------------------------
# Build variables
#

TESTS_SOURCES_DIR := $(PROJECT_DIR)/Kernel/Tests

KERNEL_BUILD_DIR := $(BUILD_DIR)/Kernel
TESTS_BUILD_DIR := $(BUILD_DIR)/Kernel/Tests

TESTS_BIN_FILE := $(TESTS_BUILD_DIR)/rom_tests.bin

C_SOURCES := $(wildcard *.c)
ASM_SOURCES := $(wildcard *.s)

OBJS := $(patsubst %.c, $(KERNEL_BUILD_DIR)/%.o, $(C_SOURCES))
$(eval OBJS += $(patsubst %.s, $(KERNEL_BUILD_DIR)/%.o, $(ASM_SOURCES)))

ROM_FILE := $(KERNEL_BUILD_DIR)/Boot.rom
BIN_FILE := $(KERNEL_BUILD_DIR)/rom_kernel.bin


# --------------------------------------------------------------------------
# Includes
#

include $(PROJECT_DIR)/common.mk


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean


build: $(ROM_FILE)

$(OBJS): | $(KERNEL_BUILD_DIR)

$(KERNEL_BUILD_DIR):
	$(call mkdir_if_needed,$(KERNEL_BUILD_DIR))


$(ROM_FILE): $(BIN_FILE) $(TESTS_BIN_FILE) ../finalizerom.py
	$(PY) ../finalizerom.py $(BIN_FILE) $(TESTS_BIN_FILE) $(ROM_FILE)


$(BIN_FILE): $(RUNTIME_LIB_FILE) $(OBJS)
	@echo Linking
	@$(LD) -s -brawbin1 -Ttext 0xfc0000 -o $@ $(KERNEL_BUILD_DIR)/traps.o $(filter-out $(KERNEL_BUILD_DIR)/traps.o, $^)

$(TESTS_BIN_FILE):
	+$(MAKE) build -C $(TESTS_SOURCES_DIR)


$(KERNEL_BUILD_DIR)/%.o : %.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) -I. -I$(RUNTIME_INCLUDE_DIR) -o $@ $<

$(KERNEL_BUILD_DIR)/%.o : %.s
	@echo $<
	@$(AS) -I. -o $@ $<


clean:
	$(call rm_if_exists,$(KERNEL_BUILD_DIR))
	+$(MAKE) clean -C $(TESTS_SOURCES_DIR)
