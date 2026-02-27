# --------------------------------------------------------------------------
# Build variables
#

LIBM_SOURCES_DIR := $(LIBM_PROJECT_DIR)/src
LIBM_OBJS_DIR := $(LIB_OBJS_DIR)/libm

M68K_SOURCES_DIR := $(LIBM_SOURCES_DIR)/arch/m68k
M68K_OBJS_DIR := $(LIBM_OBJS_DIR)/arch/m68k

BSDEXT_SOURCES_DIR := $(LIBM_SOURCES_DIR)/bsdext
BSDEXT_OBJS_DIR := $(LIBM_OBJS_DIR)/bsdext

COMPLEX_SOURCES_DIR := $(LIBM_SOURCES_DIR)/complex
COMPLEX_OBJS_DIR := $(LIBM_OBJS_DIR)/complex

CORE_SOURCES_DIR := $(LIBM_SOURCES_DIR)/core
CORE_OBJS_DIR := $(LIBM_OBJS_DIR)/core

LD128_SOURCES_DIR := $(LIBM_SOURCES_DIR)/ld128
LD128_OBJS_DIR := $(LIBM_OBJS_DIR)/ld128

LD80_SOURCES_DIR := $(LIBM_SOURCES_DIR)/ld80
LD80_OBJS_DIR := $(LIBM_OBJS_DIR)/ld80


LIBM_C_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(LIBM_HEADERS_DIR) -I$(CORE_SOURCES_DIR)
LIBM_ASM_INCLUDES := -I$(LIBC_HEADERS_DIR) -I$(LIBM_HEADERS_DIR) -I$(CORE_SOURCES_DIR)

LIBM_CC_DONTWARN := -dontwarn=51
LIBM_AS_DONTWARN :=


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: clean-libm $(LIBM_OBJS_DIR)


build-libm: $(LIBM_FILE)

$(LIBM_OBJS): | $(LIBM_OBJS_DIR) $(PRODUCT_LIB_DIR)

$(LIBM_OBJS_DIR):
	$(call mkdir_if_needed,$(LIBM_OBJS_DIR))


-include $(M68K_SOURCES_DIR)/package.mk
-include $(BSDEXT_SOURCES_DIR)/package.mk
-include $(COMPLEX_SOURCES_DIR)/package.mk
-include $(CORE_SOURCES_DIR)/package.mk
-include $(LD128_SOURCES_DIR)/package.mk
-include $(LD80_SOURCES_DIR)/package.mk

$(LIBM_FILE): $(M68K_OBJS) $(BSDEXT_OBJS) $(COMPLEX_OBJS) $(CORE_OBJS) \
			  $(LD128_OBJS) $(LD80_OBJS) $(LIBM_OBJS)
	@echo Making libm.a
	$(LIBTOOL) create $@ $^


clean-libm:
	$(call rm_if_exists,$(LIBM_OBJS_DIR))
