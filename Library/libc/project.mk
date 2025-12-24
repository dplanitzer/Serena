# --------------------------------------------------------------------------
# Build variables
#

LIBC_SOURCES_DIR := $(LIBC_PROJECT_DIR)/Sources

ARCH_M68K_SOURCES_DIR := $(LIBC_SOURCES_DIR)/machine/arch/m68k
ARCH_M68K_OBJS_DIR := $(LIBC_OBJS_DIR)/machine/arch/m68k

LOCALE_SOURCES_DIR := $(LIBC_SOURCES_DIR)/locale
LOCALE_OBJS_DIR := $(LIBC_OBJS_DIR)/locale

MALLOC_SOURCES_DIR := $(LIBC_SOURCES_DIR)/malloc
MALLOC_OBJS_DIR := $(LIBC_OBJS_DIR)/malloc

STDIO_SOURCES_DIR := $(LIBC_SOURCES_DIR)/stdio
STDIO_OBJS_DIR := $(LIBC_OBJS_DIR)/stdio

STDLIB_SOURCES_DIR := $(LIBC_SOURCES_DIR)/stdlib
STDLIB_OBJS_DIR := $(LIBC_OBJS_DIR)/stdlib

STRING_SOURCES_DIR := $(LIBC_SOURCES_DIR)/string
STRING_OBJS_DIR := $(LIBC_OBJS_DIR)/string
STRING_SC_OBJS_DIR := $(LIBSC_OBJS_DIR)/string

SYS_SOURCES_DIR := $(LIBC_SOURCES_DIR)/sys
SYS_OBJS_DIR := $(LIBC_OBJS_DIR)/sys

TIME_SOURCES_DIR := $(LIBC_SOURCES_DIR)/time
TIME_OBJS_DIR := $(LIBC_OBJS_DIR)/time

CSTART_C_SOURCE := $(LIBC_SOURCES_DIR)/_cstart.c

LIBC_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(KERNEL_HEADERS_DIR) -I$(LIBC_SOURCES_DIR)


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-libc clean-libsc $(LIBC_OBJS_DIR) $(LIBSC_OBJS_DIR)


build-libc: $(LIBC_FILE) $(CSTART_FILE) | $(LIBC_OBJS_DIR)

$(LIBC_OBJS_DIR):
	$(call mkdir_if_needed,$(LIBC_OBJS_DIR))


build-libsc: $(LIBSC_FILE) | $(LIBSC_OBJS_DIR)

$(LIBSC_OBJS_DIR):
	$(call mkdir_if_needed,$(LIBSC_OBJS_DIR))


-include $(ARCH_M68K_SOURCES_DIR)/package.mk
-include $(LOCALE_SOURCES_DIR)/package.mk
-include $(MALLOC_SOURCES_DIR)/package.mk
-include $(STDIO_SOURCES_DIR)/package.mk
-include $(STDLIB_SOURCES_DIR)/package.mk
-include $(STRING_SOURCES_DIR)/package.mk
-include $(SYS_SOURCES_DIR)/package.mk
-include $(TIME_SOURCES_DIR)/package.mk


# libc (hosted)
$(LIBC_FILE): $(ARCH_M68K_OBJS) $(LOCALE_OBJS) $(MALLOC_OBJS) $(STDIO_OBJS) $(STDLIB_OBJS) $(STRING_OBJS) $(SYS_OBJS) $(TIME_OBJS)
	@echo Making libc.a
	$(LIBTOOL) create $@ $^


# _start() function
$(CSTART_FILE) : $(CSTART_C_SOURCE) | $(LIBC_OBJS_DIR)
	@echo $<
	@$(CC) $(USER_CC_CONFIG) $(CC_OPT_SETTING) $(CC_PREPROC_DEFS) $(LIBC_C_INCLUDES) -o $@ $<


#libsc (freestanding)
$(LIBSC_FILE): $(STRING_SC_OBJS)
	@echo Making libsc.a
	$(LIBTOOL) create $@ $^


clean-libc:
	$(call rm_if_exists,$(LIBC_OBJS_DIR))

clean-libsc:
	$(call rm_if_exists,$(LIBSC_OBJS_DIR))
