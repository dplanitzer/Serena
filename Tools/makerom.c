//
//  makerom.c
//  makerom
//
//  Created by Dietmar Planitzer on 2/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <clap.h>


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

static FILE* open_require(const char* filename, const char* mode)
{
    FILE* s = fopen(filename, mode);

    if (s == NULL) {
        clap_error("Unable to open '%s'", filename);
        // NOT REACHED
    }
    return s;
}

static void fwrite_require(const void* data, size_t size, FILE* s)
{
    if (fwrite(data, size, 1, s) < 1) {
        clap_error("I/O error");
        // NOT REACHED
    }
}

static void fputc_require(int ch, FILE* s)
{
    if (fputc(ch, s) == EOF) {
        clap_error("I/O error");
        // NOT REACHED
    }
}

static size_t getFileSize(FILE* s)
{
    const long origpos = ftell(s);
    const int r0 = fseek(s, 0, SEEK_END);
    const long r = ftell(s);

    if (r == -1 || r0 != 0 || origpos == -1) {
        clap_error("I/O error");
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
            clap_error("I/O error");
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

clap_string_array_t paths = {NULL, 0};

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("makerom <romFile> <binaryImagePath ...>"),
    CLAP_PROLOG(
        "Creates a ROM image file for use in Amiga computers. Takes a path to a kernel image file <binaryImagePath> as input"
        " plus optional additional image files and packages all those files up into a ROM image file which will be stored at <romFile>."
    ),

    CLAP_VARARG(&paths)
);


int main(int argc, char* argv[])
{
    clap_parse(params, argc, argv);

    if (paths.count < 2) {
        clap_error("missing image file paths");
        // NOT REACHED
    }


    // Write one file after the other
    // Align each file at a 4 byte boundary
    // Write 16 bytes of IRQ autovec generation data at the very end of the ROM
    const char autovec[] = {0x00, 0x18, 0x00, 0x19, 0x00, 0x1a, 0x00, 0x1b, 0x00, 0x1c, 0x00, 0x1d, 0x00, 0x1e, 0x00, 0x1f};
    const size_t romCapacity = SIZE_KB(256) - sizeof(autovec);
    size_t romSize = 0;

    FILE* romFile = open_require(paths.strings[0], "wb");
    setvbuf(romFile, NULL, _IOFBF, 8192);


    // Add the input files to the ROM
    for (int i = 1; i < paths.count; i++) {
        FILE* fp = open_require(paths.strings[i], "rb");
        const size_t fileSize = getFileSize(fp);
        const size_t nb = 4 - (fileSize & 0x03);

        if (romSize + fileSize + nb > romCapacity) {
            clap_error("ROM image too big");
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
