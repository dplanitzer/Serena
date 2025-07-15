# --------------------------------------------------------------------------
# Build variables
#

LIBC_SOURCES_DIR := $(LIBC_PROJECT_DIR)/Sources

ARCH_M68K_SOURCES_DIR := $(LIBC_SOURCES_DIR)/machine/arch/m68k
ARCH_M68K_OBJS_DIR := $(LIBC_OBJS_DIR)/machine/arch/m68k

MALLOC_SOURCES_DIR := $(LIBC_SOURCES_DIR)/malloc
MALLOC_OBJS_DIR := $(LIBC_OBJS_DIR)/malloc

STDIO_SOURCES_DIR := $(LIBC_SOURCES_DIR)/stdio
STDIO_OBJS_DIR := $(LIBC_OBJS_DIR)/stdio

STDLIB_SOURCES_DIR := $(LIBC_SOURCES_DIR)/stdlib
STDLIB_OBJS_DIR := $(LIBC_OBJS_DIR)/stdlib

STRING_SOURCES_DIR := $(LIBC_SOURCES_DIR)/string
STRING_OBJS_DIR := $(LIBC_OBJS_DIR)/string

SYS_SOURCES_DIR := $(LIBC_SOURCES_DIR)/sys
SYS_OBJS_DIR := $(LIBC_OBJS_DIR)/sys

TIME_SOURCES_DIR := $(LIBC_SOURCES_DIR)/time
TIME_OBJS_DIR := $(LIBC_OBJS_DIR)/time

LIBC_ASTART_C_SOURCE := $(LIBC_SOURCES_DIR)/_astart.c
LIBC_CSTART_C_SOURCE := $(LIBC_SOURCES_DIR)/_cstart.c

LIBC_C_SOURCES := $(filter-out $(LIBC_SOURCES_DIR)/_astart.c $(LIBC_SOURCES_DIR)/_cstart.c, $(wildcard $(LIBC_SOURCES_DIR)/*.c))
LIBC_ASM_SOURCES := $(wildcard $(LIBC_SOURCES_DIR)/*.s)

LIBC_OBJS := $(patsubst $(LIBC_SOURCES_DIR)/%.c, $(LIBC_OBJS_DIR)/%.o, $(LIBC_C_SOURCES))
LIBC_DEPS := $(LIBC_OBJS:.o=.d)
LIBC_OBJS += $(patsubst $(LIBC_SOURCES_DIR)/%.s, $(LIBC_OBJS_DIR)/%.o, $(LIBC_ASM_SOURCES))

LIBC_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(LIBC_SOURCES_DIR)
LIBC_ASM_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(LIBC_SOURCES_DIR)

#LIBC_GENERATE_DEPS = -deps -depfile=$(patsubst $(LIBC_OBJS_DIR)/%.o,$(LIBC_OBJS_DIR)/%.d,$@)
LIBC_GENERATE_DEPS := 
LIBC_CC_DONTWARN := -dontwarn=214


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-libc $(LIBC_OBJS_DIR)


build-libc: $(LIBC_FILE) $(CSTART_FILE)

$(LIBC_OBJS): | $(LIBC_OBJS_DIR)

$(LIBC_OBJS_DIR):
	$(call mkdir_if_needed,$(LIBC_OBJS_DIR))


-include $(ARCH_M68K_SOURCES_DIR)/package.mk
-include $(MALLOC_SOURCES_DIR)/package.mk
-include $(STDIO_SOURCES_DIR)/package.mk
-include $(STDLIB_SOURCES_DIR)/package.mk
-include $(STRING_SOURCES_DIR)/package.mk
-include $(SYS_SOURCES_DIR)/package.mk
-include $(TIME_SOURCES_DIR)/package.mk


$(LIBC_FILE): $(ARCH_M68K_OBJS) $(LIBC_OBJS) $(MALLOC_OBJS) $(STDIO_OBJS) $(STDLIB_OBJS) $(STRING_OBJS) $(SYS_OBJS) $(TIME_OBJS)
	@echo Making libc.a
	$(LIBTOOL) create $@ $^


-include $(LIBC_DEPS)

$(LIBC_OBJS_DIR)/%.o : $(LIBC_SOURCES_DIR)/%.c
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_GEN_DEBUG_INFO) $(CC_PREPROC_DEFS) $(LIBC_C_INCLUDES) $(LIBC_CC_DONTWARN) $(LIBC_GENERATE_DEPS) -o $@ $<

$(LIBC_OBJS_DIR)/%.o : $(LIBC_SOURCES_DIR)/%.s
	@echo $<
	@$(AS) $(USER_ASM_CONFIG) $(LIBC_ASM_INCLUDES) -o $@ $<


# start() function
$(CSTART_FILE) : $(LIBC_CSTART_C_SOURCE) | $(LIBC_OBJS_DIR)
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_PREPROC_DEFS) $(LIBC_C_INCLUDES) -o $@ $<


clean-libc:
	$(call rm_if_exists,$(LIBC_OBJS_DIR))
