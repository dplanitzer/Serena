# Expects that an environment variable 'VBCC' exists and that it points to the vbcc tool chain directory.
# Eg VBCC='C:\Program Files\vbcc'
#
# Expects that the vbcc tool chain 'bin' folder is listed in the PATH environment variable.
#


# --------------------------------------------------------------------------
# Build settings
#

# Supported build configs:
# 'release'  compile with optimizations turned on and do not generate debug info
# 'debug'    compile without optimizations and generate debug info
#ifndef BUILD_CONFIGURATION
	BUILD_CONFIGURATION = release
#endif

#ifeq ($(BUILD_CONFIGURATION), "release")
#	echo "release build"

	CC_OPT_SETTING = -O=23999 -schedule
	CC_GENERATE_DEBUG_INFO =
#else ifeq ($(BUILD_CONFIGURATION), "debug")
#	echo "debug build"

#	CC_OPT_SETTING = -O0
#	CC_GENERATE_DEBUG_INFO = -g
#endif

CC_PREPROCESSOR_DEFINITIONS = -DDEBUG=1 -D__BIG_ENDIAN__=1 -D__LP32__=1 -DTARGET_CPU_68030=1


# --------------------------------------------------------------------------
# Build variables
#

SOURCES_DIR = .
#ifndef BUILD_DIR
	BUILD_DIR = build
#endif

INCLUDE_DIR = .

C_SOURCES = $(wildcard $(SOURCES_DIR)/*.c)
ASM_SOURCES = $(wildcard $(SOURCES_DIR)/*.s)

C_OBJECTS	= $(patsubst $(SOURCES_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
ASM_OBJECTS	= $(patsubst $(SOURCES_DIR)/%.s,$(BUILD_DIR)/%.o,$(ASM_SOURCES))

ROM_FILE = $(BUILD_DIR)/Boot.rom
BIN_FILE = $(BUILD_DIR)/rom.bin

ifeq ($(OS),Windows_NT)
VC_CONFIG = vc_windows_host.config
else
VC_CONFIG = vc_posix_host.config
endif


# --------------------------------------------------------------------------
# Functions
#

# Create a directory if it doesn't exist
ifeq ($(OS),Windows_NT)
define mkdir_if_needed
	powershell If (!(Test-Path '$(1)')) { [void](New-Item -ItemType Directory -Path '$(1)') }
endef
else
define mkdir_if_needed
	mkdir -p '$(1)'
endef
endif


# Delete a file if it exists and do nothing otherwise
ifeq ($(OS),Windows_NT)
define rm_if_exists
	powershell If (Test-Path '$(1)') { Remove-Item -Recurse -Force '$(1)' }
endef
else
define rm_if_exists
	rm -rf '$(1)'
endef
endif


# --------------------------------------------------------------------------
# Build tools
#

AS = $(VBCC)/bin/vasmm68k_mot -Fhunk -spaces -nosym -quiet -m68060 -DTARGET_CPU_68030
CC = $(VBCC)/bin/vc +$(VC_CONFIG)
LD = $(VBCC)/bin/vlink
PY = python


# --------------------------------------------------------------------------
# Build rules
#

build:	build_dir $(ROM_FILE)

build_dir:
	$(call mkdir_if_needed,$(BUILD_DIR))

$(ROM_FILE):	$(BIN_FILE) ../finalizerom.py
	$(PY) ../finalizerom.py $(BIN_FILE) $(ROM_FILE)

$(BIN_FILE):	$(C_OBJECTS) $(ASM_OBJECTS)
	@$(LD) -s -brawbin1 -Ttext 0xfc0000 -o $@ $(BUILD_DIR)/traps.o $(filter-out $(BUILD_DIR)/traps.o, $^)

$(C_OBJECTS): $(BUILD_DIR)/%.o : $(SOURCES_DIR)/%.c
	@echo $<
	@$(CC) -c -c99 -cpp-comments -nostdlib -cpu=68030 $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) -I$(INCLUDE_DIR) -o $@ $<

$(ASM_OBJECTS): $(BUILD_DIR)/%.o : $(SOURCES_DIR)/%.s
	@echo $<
	@$(AS) -I$(INCLUDE_DIR) -o $@ $<

clean:
	$(call rm_if_exists,$(BUILD_DIR))
