# --------------------------------------------------------------------------
# Build variables
#

SOURCES_DIR = .
INCLUDE_DIR := .
KERNEL_INCLUDE_DIR := $(PROJECT_DIR)/Kernel/Sources

SYSTEM_BUILD_DIR := $(BUILD_DIR)/Library/System

C_SOURCES := $(wildcard $(SOURCES_DIR)/*.c)
ASM_SOURCES := $(wildcard $(SOURCES_DIR)/*.s)

C_OBJECTS	:= $(patsubst $(SOURCES_DIR)/%.c, $(SYSTEM_BUILD_DIR)/%.o, $(C_SOURCES))
ASM_OBJECTS	:= $(patsubst $(SOURCES_DIR)/%.s, $(SYSTEM_BUILD_DIR)/%.o, $(ASM_SOURCES))

LIB_FILE := $(SYSTEM_BUILD_DIR)/libsystem.a


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
	$(call mkdir_if_needed,$(SYSTEM_BUILD_DIR))


$(LIB_FILE): $(C_OBJECTS) $(ASM_OBJECTS)
	@echo Linking
	@$(LD) -baoutnull -r -o $@ $^


$(C_OBJECTS): $(SYSTEM_BUILD_DIR)/%.o : $(SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) -I$(INCLUDE_DIR) -o $@ $<

$(ASM_OBJECTS): $(SYSTEM_BUILD_DIR)/%.o : $(SOURCES_DIR)/%.s
	@echo $<
	@$(AS) -I$(INCLUDE_DIR) -I$(KERNEL_INCLUDE_DIR) -o $@ $<


clean:
	$(call rm_if_exists,$(SYSTEM_BUILD_DIR))
