#
# Expects that the following environment variables are defined:
#
# VBCC  path to the root directory of the VBCC toolchain
# SERENA_SDK path to the Serena SDK root directory
#

ifndef SERENA_SDK
	SERENA_SDK := $(CURDIR)/../..
endif


CC = $(VBCC)/bin/vc
LD = $(VBCC)/bin/vlink

ifeq ($(OS),Windows_NT)
	SDK_BIN_DIR := $(SERENA_SDK)/bin/x86_64-windows
else
	SDK_BIN_DIR := $(SERENA_SDK)/bin/arm64-macos
endif
SDK_CONFIG_DIR := $(SERENA_SDK)/conf
SDK_INCLUDE_DIR := $(SERENA_SDK)/include
SDK_LIB_DIR := $(SERENA_SDK)/lib


ifeq ($(OS),Windows_NT)
	VC_CFG := $(SDK_CONFIG_DIR)/m68k-app-windows.vcfg
	DISKIMAGE := $(SDK_BIN_DIR)/diskimage.exe
	RM = del /Q /F
else
	VC_CFG := $(SDK_CONFIG_DIR)/m68k-app-posix.vcfg
	DISKIMAGE := $(SDK_BIN_DIR)/diskimage
	RM = rm -f
endif


TARGET := hellodispatch
ADF_NAME := hello_disk.adf

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))


.PHONY: adf clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@$(LD) -bataritos -T $(SDK_CONFIG_DIR)/app.ld -s -L$(SDK_LIB_DIR) -lc -ldispatch -o $@ $(SDK_LIB_DIR)/_cstart.o $^

%.o : %.c
	@$(CC) +$(VC_CFG) -c -c99 -cpp-comments -O3 -size -I$(SDK_INCLUDE_DIR) -o $@ $<

adf:
	$(DISKIMAGE) create adf-dd $(ADF_NAME)
	$(DISKIMAGE) format sefs --label "Hello FD" $(ADF_NAME)
	$(DISKIMAGE) push -m=rwxr-xr-x -o=1000:1000 ./$(TARGET) / $(ADF_NAME)

clean:
	-$(RM) $(OBJS) $(TARGET)
