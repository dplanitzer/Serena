# --------------------------------------------------------------------------
# Build variables
#

KERNEL_SOURCES_DIR := $(KERNEL_PROJECT_DIR)/Sources

KLIB_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/klib
KLIB_BUILD_DIR := $(KERNEL_BUILD_DIR)/klib

KERNEL_C_SOURCES := $(wildcard $(KERNEL_SOURCES_DIR)/*.c)
KERNEL_ASM_SOURCES := $(wildcard $(KERNEL_SOURCES_DIR)/*.s)

KERNEL_OBJS := $(patsubst $(KERNEL_SOURCES_DIR)/%.c,$(KERNEL_BUILD_DIR)/%.o,$(KERNEL_C_SOURCES))
KERNEL_DEPS := $(KERNEL_OBJS:.o=.d)
KERNEL_OBJS += $(patsubst $(KERNEL_SOURCES_DIR)/%.s,$(KERNEL_BUILD_DIR)/%.o,$(KERNEL_ASM_SOURCES))

KERNEL_C_INCLUDES := -I$(LIBABI_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR)
KERNEL_ASM_INCLUDES := -I$(LIBABI_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR)

#KERNEL_GENERATE_DEPS = -deps -depfile=$(patsubst $(KERNEL_BUILD_DIR)/%.o,$(KERNEL_BUILD_DIR)/%.d,$@)
KERNEL_GENERATE_DEPS := 
KERNEL_DONTWARN := -dontwarn=51 -dontwarn=148 -dontwarn=208


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-kernel $(KERNEL_BUILD_DIR) $(KERNEL_PRODUCT_DIR)


build-kernel: $(KERNEL_BIN_FILE)

$(KERNEL_OBJS): | $(KERNEL_BUILD_DIR)

$(KERNEL_BUILD_DIR):
	$(call mkdir_if_needed,$(KERNEL_BUILD_DIR))

$(KERNEL_PRODUCT_DIR):
	$(call mkdir_if_needed,$(KERNEL_PRODUCT_DIR))


-include $(KLIB_SOURCES_DIR)/package.mk

$(KERNEL_BIN_FILE): $(KLIB_OBJS) $(KERNEL_OBJS) | $(KERNEL_PRODUCT_DIR)
	@echo Linking Kernel
	@$(LD) -s -brawbin1 -T $(KERNEL_SOURCES_DIR)/linker.script -o $@ $^


-include $(KERNEL_DEPS)

$(KERNEL_BUILD_DIR)/%.o : $(KERNEL_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) $(KERNEL_C_INCLUDES) $(KERNEL_DONTWARN) $(KERNEL_GENERATE_DEPS) -o $@ $<

$(KERNEL_BUILD_DIR)/%.o : $(KERNEL_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_INCLUDES) -nowarn=62 -o $@ $<


clean-kernel:
	$(call rm_if_exists,$(KERNEL_BUILD_DIR))
