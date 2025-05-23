//
//  Utilities.c
//  sh
//
//  Created by Dietmar Planitzer on 5/20/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Utilities.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <clap.h>


void print_error(const char* _Nonnull proc_name, const char* _Nullable path, errno_t err)
{
    const char* errstr = shell_strerror(err);

    if (path && *path != '\0') {
        clap_error(proc_name, "%s: %s", path, errstr);
    }
    else {
        clap_error(proc_name, "%s", errstr);
    }
}

const char* char_to_ascii(char ch, char buf[3])
{
    if (isprint(ch)) {
        buf[0] = ch;
        buf[1] = '\0';
    }
    else {
        buf[0] = '^';
        buf[1] = ch + 64;
        buf[2] = '\0';
    }

    return buf;
}

size_t hash_cstring(const char* _Nonnull str)
{
    size_t h = 5381;
    char c;

    while (c = *str++) {
        h = ((h << 5) + h) + c;
    }

    return h;
}

size_t hash_string(const char* _Nonnull str, size_t len)
{
    size_t h = 5381;
    char c;

    while (len-- > 0) {
        h = ((h << 5) + h) + *str++;
    }

    return h;
}

int read_contents_of_file(const char* _Nonnull path, char* _Nullable * _Nonnull pOutText, size_t* _Nullable pOutLength)
{
    FILE* fp = fopen(path, "r");

    if (fp == NULL) {
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    const size_t fileSize = ftell(fp);
    rewind(fp);

    char* text = malloc(fileSize + 1);
    fread(text, fileSize, 1, fp);
    text[fileSize] = '\0';
    fclose(fp);

    if (ferror(fp)) {
        free(text);
        *pOutText = NULL;
        if (pOutLength) {
            *pOutLength = 0;
        }
    }
    else {
        *pOutText = text;
        if (pOutLength) {
            *pOutLength = fileSize + 1;
        }
    }
    
    return 0;
}
