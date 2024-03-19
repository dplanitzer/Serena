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
    const long origpos = ftell(s);
    const int r0 = fseek(s, 0, SEEK_END);
    const long r = ftell(s);

    if (r == -1 || r0 != 0 || origpos == -1) {
        failed("I/O error");
        // NOT REACHED
    }

    fseek(s, origpos, SEEK_SET);
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
    printf("makerom <romFile> <kernelFile> ... \n");

    exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        help();
        // NOT REACHED
    }


    // Write one file after the other
    // Align each file at a 4 byte boundary
    // Write 16 bytes of IRQ autovec generation data at the very end of the ROM
    const char autovec[] = {0x00, 0x18, 0x00, 0x19, 0x00, 0x1a, 0x00, 0x1b, 0x00, 0x1c, 0x00, 0x1d, 0x00, 0x1e, 0x00, 0x1f};
    const size_t romCapacity = SIZE_KB(256) - sizeof(autovec);
    size_t romSize = 0;

    FILE* romFile = open_require(argv[1], "wb");
    setvbuf(romFile, NULL, _IOFBF, 8192);


    // Add the input files to the ROM
    for (int i = 2; i < argc; i++) {
        FILE* fp = open_require(argv[i], "rb");
        const size_t fileSize = getFileSize(fp);
        const size_t nb = 4 - (fileSize & 0x03);

        if (romSize + fileSize + nb > romCapacity) {
            failed("ROM too big");
            // NOT REACHED
        }

        setvbuf(fp, NULL, _IONBF, 0);
        appendContentsOfFile(fp, romFile);
        if (nb > 0) {
            appendByFilling(0, nb, romFile);
        }
        fclose(fp);

        romSize += fileSize;
        romSize += nb;
    }


    // 68k IRQ auto-vector generation support
    appendByFilling(0, romCapacity - romSize, romFile);
    appendBytes(autovec, sizeof(autovec), romFile);

    fclose(romFile);

    return EXIT_SUCCESS;
}
