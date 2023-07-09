# Expects that an environment variable 'VBCC' exists and that it points to the vbcc tool chain directory.
# Eg VBCC='C:\Program Files\vbcc'
#
# Expects that the vbcc tool chain 'bin' folder is listed in the PATH environment variable.
#


# --------------------------------------------------------------------------
# Build Configuration
#

# Supported build configs:
# 'release'  compile with optimizations turned on and do not generate debug info
# 'debug'    compile without optimizations and generate debug info
ifndef BUILD_CONFIGURATION
	BUILD_CONFIGURATION := release
endif
export BUILD_CONFIGURATION

ifeq ($(BUILD_CONFIGURATION), release)
	CC_OPT_SETTING := -O3
	CC_GENERATE_DEBUG_INFO :=
else
	CC_OPT_SETTING := -O0
	CC_GENERATE_DEBUG_INFO := -g
endif
export CC_OPT_SETTING
export CC_GENERATE_DEBUG_INFO

export CC_PREPROCESSOR_DEFINITIONS := -DDEBUG=1 -D__BIG_ENDIAN__=1 -D__LP32__=1 -DTARGET_CPU_68030=1


# --------------------------------------------------------------------------
# Common Directories
#

export PROJECT_DIR := $(CURDIR)
export BUILD_DIR := $(PROJECT_DIR)/build


# --------------------------------------------------------------------------
# Build tools
#

ifeq ($(OS),Windows_NT)
VC_CONFIG := $(PROJECT_DIR)/vc_windows_host.config
else
VC_CONFIG := $(PROJECT_DIR)/vc_posix_host.config
endif
export VC_CONFIG

export AS = $(VBCC)/bin/vasmm68k_mot -Faout -quiet -nosym -spaces -m68060 -DTARGET_CPU_68030
export CC = $(VBCC)/bin/vc +$(VC_CONFIG) -c -c99 -cpp-comments -cpu=68030
export LD = $(VBCC)/bin/vlink
export PY = python


# --------------------------------------------------------------------------
# Build rules
#

.PHONY: build clean

build:
	@echo Building ($(BUILD_CONFIGURATION))
	+$(MAKE) build -C ./Kernel/Sources/
	@echo Done

clean:
	@echo Cleaning
	+$(MAKE) clean -C ./Kernel/Sources/
	@echo Done
	