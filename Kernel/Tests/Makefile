# --------------------------------------------------------------------------
# Build variables
#


C_SOURCES := $(wildcard *.c)
ASM_SOURCES := $(wildcard *.s)

OBJS := $(patsubst %.c, $(KERNEL_TESTS_BUILD_DIR)/%.o, $(C_SOURCES))
$(eval OBJS += $(patsubst %.s, $(KERNEL_TESTS_BUILD_DIR)/%.o, $(ASM_SOURCES)))


# --------------------------------------------------------------------------
# Includes
#

include $(PROJECT_DIR)/common.mk


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean


build: $(KERNEL_TESTS_BIN_FILE)

$(OBJS): | $(KERNEL_TESTS_BUILD_DIR)

$(KERNEL_TESTS_BUILD_DIR):
	$(call mkdir_if_needed,$(KERNEL_TESTS_BUILD_DIR))


$(KERNEL_TESTS_BIN_FILE): $(RUNTIME_LIB_FILE) $(SYSTEM_LIB_FILE) $(OBJS)
	@echo Linking
	@$(LD) -s -brawbin1 -Ttext 0xfe0000 -o $@ $(KERNEL_TESTS_BUILD_DIR)/HelloWorld.o $(filter-out $(KERNEL_TESTS_BUILD_DIR)/HelloWorld.o, $^)


$(KERNEL_TESTS_BUILD_DIR)/%.o : %.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) -I. -I$(RUNTIME_INCLUDE_DIR) -I$(SYSTEM_INCLUDE_DIR) -o $@ $<

$(KERNEL_TESTS_BUILD_DIR)/%.o : %.s
	@echo $<
	@$(AS) -I. -o $@ $<


clean:
	$(call rm_if_exists,$(KERNEL_TESTS_BUILD_DIR))
