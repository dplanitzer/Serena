# --------------------------------------------------------------------------
# Build variables
#

KERNEL_SOURCES_DIR := $(KERNEL_PROJECT_DIR)/Sources

CONSOLE_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/console
CONSOLE_BUILD_DIR := $(KERNEL_BUILD_DIR)/console

KLIB_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/klib
KLIB_BUILD_DIR := $(KERNEL_BUILD_DIR)/klib

KRT_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/krt
KRT_BUILD_DIR := $(KERNEL_BUILD_DIR)/krt

KERNEL_C_SOURCES := $(wildcard $(KERNEL_SOURCES_DIR)/*.c)
KERNEL_ASM_SOURCES := $(wildcard $(KERNEL_SOURCES_DIR)/*.s)

KERNEL_OBJS := $(patsubst $(KERNEL_SOURCES_DIR)/%.c,$(KERNEL_BUILD_DIR)/%.o,$(KERNEL_C_SOURCES))
KERNEL_DEPS := $(KERNEL_OBJS:.o=.d)
KERNEL_OBJS += $(patsubst $(KERNEL_SOURCES_DIR)/%.s,$(KERNEL_BUILD_DIR)/%.o,$(KERNEL_ASM_SOURCES))

KERNEL_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR)
KERNEL_ASM_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR)

#KERNEL_GENERATE_DEPS = -deps -depfile=$(patsubst $(KERNEL_BUILD_DIR)/%.o,$(KERNEL_BUILD_DIR)/%.d,$@)
KERNEL_GENERATE_DEPS := 
KERNEL_AS_DONTWARN := -nowarn=62
KERNEL_CC_DONTWARN := -dontwarn=51 -dontwarn=148 -dontwarn=208
KERNEL_LD_DONTWARN := -nowarn=22

KERNEL_CC_PREPROCESSOR_DEFINITIONS := $(CC_PREPROCESSOR_DEFINITIONS) -D__KERNEL__=1


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


-include $(KRT_SOURCES_DIR)/package.mk
-include $(KLIB_SOURCES_DIR)/package.mk
-include $(CONSOLE_SOURCES_DIR)/package.mk

$(KERNEL_BIN_FILE): $(KRT_OBJS) $(KLIB_OBJS) $(CONSOLE_OBJS) $(KERNEL_OBJS) | $(KERNEL_PRODUCT_DIR)
	@echo Linking Kernel
	@$(LD) -s -brawbin1 -T $(KERNEL_SOURCES_DIR)/linker.script $(KERNEL_LD_DONTWARN) -o $@ $^


-include $(KERNEL_DEPS)

$(KERNEL_BUILD_DIR)/%.o : $(KERNEL_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(KERNEL_CC_PREPROCESSOR_DEFINITIONS) $(KERNEL_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(KERNEL_GENERATE_DEPS) -o $@ $<

$(KERNEL_BUILD_DIR)/%.o : $(KERNEL_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_INCLUDES) $(KERNEL_AS_DONTWARN) -o $@ $<


clean-kernel:
	$(call rm_if_exists,$(KERNEL_BUILD_DIR))
