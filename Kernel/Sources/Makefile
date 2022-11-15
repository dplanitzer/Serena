# --------------------------------------------------------------------------
# SDK paths
#

export VBCC = $(HOME)/Applications/Emulators/Amiga/SDK/vbcc
export PATH := $(VBCC)/bin:$(PATH)


# --------------------------------------------------------------------------
# Build tools
#

AS = $(VBCC)/bin/vasmm68k_mot -Fhunk -spaces -nosym -quiet -m68060
CC = $(VBCC)/bin/vc
LD = $(VBCC)/bin/vlink
RM = rm -f
PY = python


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

# we keep Common Subexpression Elimnation turned off because the compiler has a bug where
# this optimization breaks code if you nest structs more than one level deep

	CC_OPT_SETTING = -O=1163 -schedule
	CC_GENERATE_DEBUG_INFO =
	CC_PREPROCESSOR_DEFINITIONS = -DDEBUG=1 -D__BIG_ENDIAN__=1 -D__LP32__=1
#else ifeq ($(BUILD_CONFIGURATION), "debug")
#	echo "debug build"
#
#	CC_OPT_SETTING =
#	CC_GENERATE_DEBUG_INFO = -g
#	CC_PREPROCESSOR_DEFINITIONS = -DDEBUG=1
#endif


# --------------------------------------------------------------------------
# Build variables
#

SOURCES_DIR = .
# Allow Xcode to specify the build directory location
# XXX using the Xcode build directory suddenly stopped at one point and leads to a compile error...
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


# --------------------------------------------------------------------------
# Build rules
#

run:	build_dir $(ROM_FILE)

build_dir:
	mkdir -p $(BUILD_DIR)

$(ROM_FILE):	$(BIN_FILE) ../finalizerom.py
	$(PY) ../finalizerom.py $(BIN_FILE) $(ROM_FILE)

$(BIN_FILE):	$(C_OBJECTS) $(ASM_OBJECTS)
	@$(LD) -s -brawbin1 -Ttext 0xfc0000 -o $@ $(BUILD_DIR)/traps.o $(filter-out $(BUILD_DIR)/traps.o, $^)

$(C_OBJECTS): $(BUILD_DIR)/%.o : $(SOURCES_DIR)/%.c
	@echo $<
	@$(CC) -c -c99 -cpp-comments -nostdlib -cpu=68020 $(CC_OPT_SETTING) $(CC_GENERATE_DEBUG_INFO) $(CC_PREPROCESSOR_DEFINITIONS) -I$(INCLUDE_DIR) -o $@ $<

$(ASM_OBJECTS): $(BUILD_DIR)/%.o : $(SOURCES_DIR)/%.s
	@echo $<
	@$(AS) -I$(INCLUDE_DIR) -o $@ $<

clean:
	$(RM) -rf $(BUILD_DIR)
