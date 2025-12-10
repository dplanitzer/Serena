//
//  type.c
//  cmds
//
//  Created by Dietmar Planitzer on 3/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/limits.h>
#include <sys/types.h>

#if SIZE_WIDTH == 32
#define ADDR_WIDTH  8
#elif SIZE_WIDTH == 64
#define ADDR_WIDTH  16
#else
#error "unknown SIZE_WIDTH"
#endif


#define HEX_COLUMNS 16
#define HEX_LINE_BUF_SIZE (ADDR_WIDTH + 3 + 3*HEX_COLUMNS + 2 + HEX_COLUMNS + 1 + 1)
static char hex_col_buf[HEX_COLUMNS];
static char hex_line_buf[HEX_LINE_BUF_SIZE];

#define TEXT_BUF_SIZE   512
static char text_buf[TEXT_BUF_SIZE];


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

static int type_hex(const char* _Nonnull path)
{
    FILE* fp = fopen(path, "rb");
    size_t nBytesRead;
    size_t addr = 0;
    int hasError = 0;
    
    if (fp == NULL) {
        return -1;
    }

    for (;;) {
        nBytesRead = fread(hex_col_buf, 1, sizeof(hex_col_buf), fp);
        if (feof(fp) || (hasError = ferror(fp))) {
            break;
        }

        format_hex_line(addr, hex_col_buf, nBytesRead, sizeof(hex_col_buf), hex_line_buf);
        fwrite(hex_line_buf, sizeof(hex_line_buf), 1, stdout);
        if (feof(stdout) || (hasError = ferror(stdout))) {
            break;
        }

        addr += sizeof(hex_col_buf);
#if 0
        //XXX disabled for now because the Console I/O channel doesn't have a way yet
        //XXX to switch from blocking to non-blocking mode 
        if (should_quit()) {
            fputc('\n', stdout);
            break;
        }
#endif
    }
    fclose(fp);

    return (hasError) ? -1 : 0;
}

static int type_text(const char* _Nonnull path)
{
    FILE* fp = fopen(path, "r");
    size_t nBytesRead;
    int hasError = 0;
    
    if (fp == NULL) {
        return -1;
    }

    for (;;) {
        nBytesRead = fread(text_buf, 1, sizeof(text_buf), fp);
        if (feof(fp) || (hasError = ferror(fp))) {
            break;
        }

        fwrite(text_buf, nBytesRead, 1, stdout);
        if (feof(stdout) || (hasError = ferror(stdout))) {
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
    fclose(fp);

    return (hasError) ? -1 : 0;
}


////////////////////////////////////////////////////////////////////////////////

const char* path = "";
bool is_hex = false;

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("type [-x | --hex] <path>"),

    CLAP_BOOL('x', "hex", &is_hex, "Type the file contents as columns of hexadecimal numbers"),
    CLAP_REQUIRED_POSITIONAL_STRING(&path, "expected a file to type")
);


static int do_type(const char* _Nonnull path, bool isHex)
{
    if (isHex) {
        return type_hex(path);
    }
    else {
        return type_text(path);
    }
}

int main(int argc, char* argv[])
{
    clap_parse(0, params, argc, argv);
    
    if (do_type(path, is_hex) == 0) {
        return EXIT_SUCCESS;
    }
    else {
        clap_error(argv[0], "%s: %s", path, strerror(errno));
        return EXIT_FAILURE;
    }
}
