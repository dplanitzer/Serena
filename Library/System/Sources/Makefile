# --------------------------------------------------------------------------
# Build variables
#

INCLUDE_DIR := .

C_SOURCES := $(wildcard *.c)
ASM_SOURCES := $(wildcard *.s)

OBJS := $(patsubst %.c, $(SYSTEM_BUILD_DIR)/%.o, $(C_SOURCES))
$(eval OBJS += $(patsubst %.s, $(SYSTEM_BUILD_DIR)/%.o, $(ASM_SOURCES)))


# --------------------------------------------------------------------------
# Includes
#

include $(PROJECT_DIR)/common.mk


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean


build: build_dir $(SYSTEM_LIB_FILE)

build_dir:
	$(call mkdir_if_needed,$(SYSTEM_BUILD_DIR))


$(SYSTEM_LIB_FILE): $(OBJS)
	@echo Linking
	@$(LD) -baoutnull -r -o $@ $^


$(SYSTEM_BUILD_DIR)/%.o : %.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) -I$. -o $@ $<

$(SYSTEM_BUILD_DIR)/%.o : %.s
	@echo $<
	@$(AS) -I. -I$(KERNEL_INCLUDE_DIR) -o $@ $<


clean:
	$(call rm_if_exists,$(SYSTEM_BUILD_DIR))
