# --------------------------------------------------------------------------
# Build variables
#

KERNEL_SOURCES_DIR := $(KERNEL_PROJECT_DIR)/Sources

CONSOLE_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/console
CONSOLE_OBJS_DIR := $(KERNEL_OBJS_DIR)/console

KLIB_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/klib
KLIB_OBJS_DIR := $(KERNEL_OBJS_DIR)/klib

KRT_SOURCES_DIR := $(KERNEL_SOURCES_DIR)/krt
KRT_OBJS_DIR := $(KERNEL_OBJS_DIR)/krt

KERNEL_C_SOURCES := $(wildcard $(KERNEL_SOURCES_DIR)/*.c)
KERNEL_ASM_SOURCES := $(wildcard $(KERNEL_SOURCES_DIR)/*.s)

KERNEL_OBJS := $(patsubst $(KERNEL_SOURCES_DIR)/%.c,$(KERNEL_OBJS_DIR)/%.o,$(KERNEL_C_SOURCES))
KERNEL_DEPS := $(KERNEL_OBJS:.o=.d)
KERNEL_OBJS += $(patsubst $(KERNEL_SOURCES_DIR)/%.s,$(KERNEL_OBJS_DIR)/%.o,$(KERNEL_ASM_SOURCES))

KERNEL_C_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR)
KERNEL_ASM_INCLUDES := -I$(LIBSYSTEM_HEADERS_DIR) -I$(KERNEL_SOURCES_DIR)

#KERNEL_GENERATE_DEPS = -deps -depfile=$(patsubst $(KERNEL_OBJS_DIR)/%.o,$(KERNEL_OBJS_DIR)/%.d,$@)
KERNEL_GENERATE_DEPS := 
KERNEL_AS_DONTWARN := -nowarn=62
KERNEL_CC_DONTWARN := -dontwarn=51 -dontwarn=148 -dontwarn=208
KERNEL_LD_DONTWARN := -nowarn=22

KERNEL_CC_PREPROC_DEFS := $(CC_PREPROC_DEFS) -D__KERNEL__=1


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-kernel $(KERNEL_OBJS_DIR) $(KERNEL_PRODUCT_DIR)


build-kernel: $(KERNEL_FILE)

$(KERNEL_OBJS): | $(KERNEL_OBJS_DIR)

$(KERNEL_OBJS_DIR):
	$(call mkdir_if_needed,$(KERNEL_OBJS_DIR))

$(KERNEL_PRODUCT_DIR):
	$(call mkdir_if_needed,$(KERNEL_PRODUCT_DIR))


-include $(KRT_SOURCES_DIR)/package.mk
-include $(KLIB_SOURCES_DIR)/package.mk
-include $(CONSOLE_SOURCES_DIR)/package.mk

$(KERNEL_FILE): $(KRT_OBJS) $(KLIB_OBJS) $(CONSOLE_OBJS) $(KERNEL_OBJS) | $(KERNEL_PRODUCT_DIR)
	@echo Linking Kernel
	@$(LD) $(KERNEL_LD_CONFIG) $(KERNEL_LD_DONTWARN) -s -o $@ $^


-include $(KERNEL_DEPS)

$(KERNEL_OBJS_DIR)/%.o : $(KERNEL_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(KERNEL_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(KERNEL_CC_PREPROC_DEFS) $(KERNEL_C_INCLUDES) $(KERNEL_CC_DONTWARN) $(KERNEL_GENERATE_DEPS) -o $@ $<

$(KERNEL_OBJS_DIR)/%.o : $(KERNEL_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(KERNEL_ASM_CONFIG) $(KERNEL_ASM_INCLUDES) $(KERNEL_AS_DONTWARN) -o $@ $<


clean-kernel:
	$(call rm_if_exists,$(KERNEL_OBJS_DIR))
