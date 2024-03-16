//
//  makerom.c
//  makerom
//
//  Created by Dietmar Planitzer on 2/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


// To compile on Windows:
// - open a Visual Studio Command Line environment
// - cl makerom.c
//
// To compile on POSIX:
// - open a terminal window
// - gcc makerom.c -o makerom
//


////////////////////////////////////////////////////////////////////////////////
// Utilities
////////////////////////////////////////////////////////////////////////////////

#define SIZE_KB(x) (((size_t)x) * 1024l)

static void failed(const char* msg)
{
    puts(msg);
    exit(EXIT_FAILURE);
}

static void failedf(const char* fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

static FILE* open_require(const char* filename, const char* mode)
{
    FILE* s = fopen(filename, mode);

    if (s == NULL) {
        failedf("Unable to open '%s'", filename);
        // NOT REACHED
    }
    return s;
}

static void fwrite_require(const void* data, size_t size, FILE* s)
{
    if (fwrite(data, size, 1, s) < 1) {
        failed("I/O error");
        // NOT REACHED
    }
}

static void fputc_require(int ch, FILE* s)
{
    if (fputc(ch, s) == EOF) {
        failed("I/O error");
        // NOT REACHED
    }
}

static size_t getFileSize(FILE* s)
{
    const long r = ftell(s);

    if (r == -1) {
        failed("I/O error");
        // NOT REACHED
    }

    return r;
}

#define BLOCK_SIZE  8192
static char gBlock[BLOCK_SIZE];

static void appendByFilling(int ch, size_t size, FILE* s)
{
    memset(gBlock, ch, (size < BLOCK_SIZE) ? size : BLOCK_SIZE);

    while (size >= BLOCK_SIZE) {
        fwrite_require(gBlock, BLOCK_SIZE, s);
        size -= BLOCK_SIZE;
    }
    if (size > 0) {
        fwrite_require(gBlock, size, s);
    }
}

static void appendBytes(const char* bytes, size_t size, FILE* s)
{
    fwrite_require(bytes, size, s);
}

static void appendContentsOfFile(FILE* src_s, FILE* s)
{
    while (!feof(src_s)) {
        const size_t nBytesRead = fread(gBlock, 1, BLOCK_SIZE, src_s);

        if (nBytesRead < BLOCK_SIZE && ferror(src_s)) {
            failed("I/O error");
            // NOT REACHED
        }

        if (nBytesRead > 0) {
            fwrite_require(gBlock, nBytesRead, s);
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
// main
////////////////////////////////////////////////////////////////////////////////

static void help(void)
{
    printf("makerom <kernelFile> [initAppFile] [dmgFile] <romFile>\n");

    exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        help();
        // NOT REACHED
    }


    // Write a split-style Amiga ROM:
    // 128k of Kernel space
    // 48k of init user process space
    // 64k of disk image space
    // 16 bytes of IRQ autovec generation data
    const char* kernelPath = argv[1];
    const char* initAppPath = (argc >= 4) ? argv[2] : "";
    const char* dmgPath = (argc >= 5) ? argv[3] : "";
    const char* romPath = argv[argc - 1];
    FILE* romFile = open_require(romPath, "wb");
    FILE* kernelFile = open_require(kernelPath, "rb");
    FILE* initAppFile = (*initAppPath != '\0') ? open_require(initAppPath, "rb") : NULL;
    FILE* dmgFile = (*dmgPath != '\0') ? open_require(dmgPath, "rb") : NULL;
    const char autovec[] = {0, 24, 0, 25, 0, 26, 0, 27, 0, 28, 0, 29, 0, 30, 0, 31};
    const size_t maxRomSize = SIZE_KB(256) - sizeof(autovec);

    setvbuf(romFile, NULL, _IOFBF, 8192);
    setvbuf(kernelFile, NULL, _IONBF, 0);
    if (initAppFile) {
        setvbuf(initAppFile, NULL, _IONBF, 0);
    }
    if (dmgFile) {
        setvbuf(dmgFile, NULL, _IONBF, 0);
    }


    // Kernel file
    appendContentsOfFile(kernelFile, romFile);
    const size_t maxKernelSize = SIZE_KB(128);
    const size_t kernelSize = getFileSize(romFile);
    if (kernelSize > maxKernelSize) {
        failed("Kernel too big");
        // NOT REACHED
    }
    appendByFilling(0, maxKernelSize - kernelSize, romFile);


    // (Optional) Init App File
    if (initAppFile) {
        appendContentsOfFile(initAppFile, romFile);
        const size_t maxAppSize = SIZE_KB(48);
        const size_t appSize = getFileSize(romFile) - maxKernelSize;
        if (appSize > maxAppSize) {
            failed("App too big");
            // NOT REACHED
        }


        // (Optional) Disk Image File
        if (dmgFile) {
            appendByFilling(0, maxAppSize - appSize, romFile);
            appendContentsOfFile(dmgFile, romFile);
        }
    }


    // 68k IRQ auto-vector generation support
    const size_t romSize = getFileSize(romFile);
    if (romSize > maxRomSize) {
        failed("ROM too big");
        // NOT REACHED
    }

    appendByFilling(0, maxRomSize - romSize, romFile);
    appendBytes(autovec, sizeof(autovec), romFile);


    if (dmgFile) {
        fclose(dmgFile);
    }
    if (initAppFile) {
        fclose(initAppFile);
    }
    fclose(kernelFile);
    fclose(romFile);

    return EXIT_SUCCESS;
}
