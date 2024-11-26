//
//  type.c
//  sh
//
//  Created by Dietmar Planitzer on 3/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#define _OPEN_SYS_ITOA_EXT
#include <stdlib.h>
#include <string.h>
#include <System/Error.h>
#include <System/Types.h>

#if SIZE_WIDTH == 32
#define ADDR_WIDTH  8
#elif SIZE_WIDTH == 64
#define ADDR_WIDTH  16
#else
#error "unknown SIZE_WIDTH"
#endif



static bool should_quit(void)
{
    bool isPausing = false;

    do {
        const int ch = fgetc(stdin);

        switch (ch) {
            case EOF:   return true;
            case 0x03:  return true;    // Ctrl-C
            case ' ':   isPausing = !isPausing; break;  // XXX switch stdin to blocking while in pause mode
            default:    break;
        }
    } while (isPausing);

    return false;
}

static void format_hex_line(size_t addr, const uint8_t* buf, size_t nbytes, size_t ncolumns, char* line)
{
    static const char* digits = "0123456789abcdef";

#if ADDR_WIDTH == 8
    char* p = ultoa(addr, &line[8], 16);
#else
    char* p = ulltoa(addr, &line[16], 16);
#endif
    const size_t addrLen = strlen(p);

    for (size_t i = 0; i < ADDR_WIDTH - addrLen; i++) {
        *line++ = '0';
    }
    for (size_t i = 0; i < addrLen; i++) {
        *line++ = *p++;
    }

    *line++ = ' ';
    *line++ = ' ';
    *line++ = ' ';

    for (size_t i = 0; i < nbytes; i++) {
        *line++ = digits[buf[i] >> 4];
        *line++ = digits[buf[i] & 0x0f];
        *line++ = ' ';
    }
    for (size_t i = nbytes; i < ncolumns; i++) {
        *line++ = ' ';
        *line++ = ' ';
        *line++ = ' ';
    }
    
    *line++ = ' ';
    *line++ = ' ';
    for (size_t i = 0; i < nbytes; i++) {
        *line++ = isprint(buf[i]) ? buf[i] : '.';
    }
    for (size_t i = nbytes; i < ncolumns; i++) {
        *line++ = ' ';
    }

    *line++ = '\n';
    *line   = '\0';
}

#define HEX_COLUMNS 16
static errno_t type_hex(const char* _Nonnull path)
{
    decl_try_err();
    FILE* fp = NULL;
    uint8_t* buf = NULL;
    char* line = NULL;
    const size_t bufSize = HEX_COLUMNS;
    const size_t lineLength = ADDR_WIDTH + 3 + 3*HEX_COLUMNS + 2 + HEX_COLUMNS + 1 + 1;
    size_t addr = 0;
    
    errno = 0;
    try_null(buf, malloc(bufSize), ENOMEM);
    try_null(line, malloc(lineLength), ENOMEM);
    try_null(fp, fopen(path, "rb"), errno);

    for (;;) {
        const size_t nBytesRead = fread(buf, 1, bufSize, fp);
        if (feof(fp) || ferror(fp)) {
            break;
        }

        format_hex_line(addr, buf, nBytesRead, bufSize, line);
        fwrite(line, lineLength, 1, stdout);
        if (feof(stdout) || ferror(stdout)) {
            break;
        }

        addr += bufSize;
#if 0
    //XXX disabled for now because the Console I/O channel doesn't have a way yet
    //XXX to switch from blocking to non-blocking mode 
        if (should_quit()) {
            fputc('\n', stdout);
            break;
        }
#endif
    }
    err = errno;

catch:
    if (fp) {
        fclose(fp);
    }
    free(line);
    free(buf);

    return err;
}

#define TEXT_BUF_SIZE   512
static errno_t type_text(const char* _Nonnull path)
{
    decl_try_err();
    FILE* fp = NULL;
    uint8_t* buf = NULL;
    
    errno = 0;
    try_null(buf, malloc(TEXT_BUF_SIZE), ENOMEM);
    try_null(fp, fopen(path, "r"), errno);

    for (;;) {
        const size_t nBytesRead = fread(buf, 1, TEXT_BUF_SIZE, fp);
        if (feof(fp) || ferror(fp)) {
            break;
        }

        const size_t r = fwrite(buf, 1, nBytesRead, stdout);
        if (feof(stdout) || ferror(stdout)) {
            break;
        }

#if 0
    //XXX disabled for now because the Console I/O channel doesn't have a way yet
    //XXX to switch from blocking to non-blocking mode 
        if (should_quit()) {
            fputc('\n', stdout);
            break;
        }
#endif
    }
    fputc('\n', stdout);
    err = errno;

catch:
    if (fp) {
        fclose(fp);
    }
    free(buf);

    return err;
}


////////////////////////////////////////////////////////////////////////////////

const char* path;
bool is_hex = false;

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("type [-x | --hex] <path>"),

    CLAP_BOOL('x', "hex", &is_hex, "Type the file contents as columns of hexadecimal numbers"),
    CLAP_REQUIRED_POSITIONAL_STRING(&path, "expected a file to type")
);


static errno_t do_type(const char* _Nonnull path, bool isHex, const char* _Nonnull proc_name)
{
    decl_try_err();

    if (isHex) {
        err = type_hex(path);
    }
    else {
        err = type_text(path);
    }

    if (err != EOK) {
        clap_error(proc_name, "%s: %s", path, strerror(err));
    }

    return err;
}

int main(int argc, char* argv[])
{
    is_hex = false;
    
    clap_parse(0, params, argc, argv);
    
    return do_type(path, is_hex, argv[0]) == EOK ? EXIT_SUCCESS : EXIT_FAILURE;
}
