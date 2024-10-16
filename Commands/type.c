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
#include <stdlib.h>
#include <string.h>
#include <System/Error.h>
#include <System/Types.h>

#if SIZE_WIDTH == 32
#define ADDR_FMT "%.8zx"
#elif SIZE_WIDTH == 64
#define ADDR_FMT "%.16zx"
#else
#error "unknown SIZE_WIDTH"
#endif



static void print_hex_line(size_t addr, const uint8_t* buf, size_t nbytes, size_t ncolumns)
{
    printf(ADDR_FMT"   ", addr);

    for (size_t i = 0; i < nbytes; i++) {
        printf("%.2hhx ", buf[i]);
    }
    for (size_t i = nbytes; i < ncolumns; i++) {
        fputs("   ", stdout);
    }
    
    fputs("  ", stdout);
    for (size_t i = 0; i < nbytes; i++) {
        const int ch = (isprint(buf[i])) ? buf[i] : '.';

        fputc(ch, stdout);
    }
    for (size_t i = nbytes; i < ncolumns; i++) {
        fputc(' ', stdout);
    }
}

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

static errno_t type_hex(const char* _Nonnull path)
{
    decl_try_err();
    FILE* fp = NULL;
    size_t addr = 0;
    uint8_t buf[16];
    
    try_null(fp, fopen(path, "rb"), errno);

    while (!feof(fp)) {
        const size_t r = fread(buf, 1, sizeof(buf), fp);

        if (r == 0) {
            if (ferror(fp)) {
                throw(EIO);
            }
        }

        print_hex_line(addr, buf, r, sizeof(buf));
        addr += sizeof(buf);
#if 0
    //XXX disabled for now because the Console I/O channel doesn't have a way yet
    //XXX to switch from blocking to non-blocking mode 
        if (should_quit()) {
            fputc('\n', stdout);
            break;
        }
#endif
        fputc('\n', stdout);
    }

catch:
    if (fp) {
        fclose(fp);
    }

    return err;
}

static errno_t type_text(const char* _Nonnull path)
{
    decl_try_err();
    FILE* fp = NULL;
    uint8_t buf[128];
    
    try_null(fp, fopen(path, "r"), errno);

    while (!feof(fp)) {
        const size_t r = fread(buf, 1, sizeof(buf) - 1, fp);

        if (r == 0) {
            if (ferror(fp)) {
                throw(EIO);
            }
        }

        buf[r] = '\0';
        fputs(buf, stdout);
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

catch:
    if (fp) {
        fclose(fp);
    }

    return err;
}


////////////////////////////////////////////////////////////////////////////////

const char* path;
bool is_hex = false;

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("type [--hex] <path>"),

    CLAP_BOOL('\0', "hex", &is_hex, "Type the file contents as columns of hexadecimal numbers"),
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
