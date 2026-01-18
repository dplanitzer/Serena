//
//  Utilities.c
//  sh
//
//  Created by Dietmar Planitzer on 5/20/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Utilities.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <clap.h>
#include <unistd.h>
#include <ext/limits.h>
#include <sys/stat.h>


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

int read_contents_of_file(const char* _Nonnull path, char* _Nullable * _Nonnull pOutText, size_t* _Nullable pOutLength)
{
    struct stat st;
    
    const int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return EOF;
    }

    if (fstat(fd, &st) < 0) {
        close(fd);
        return EOF;
    }

    if (!S_ISREG(st.st_mode)) {
        errno = EINVAL;
        close(fd);
        return EOF;
    }
    if (st.st_size > (off_t)SSIZE_MAX) {
        errno = E2BIG;
        close(fd);
        return EOF;
    }

    const size_t fileSize = (size_t)st.st_size;
    char* text = malloc(fileSize + 1);
    int r;

    if (read(fd, text, fileSize) == fileSize) {
        text[fileSize] = '\0';

        *pOutText = text;
        if (pOutLength) {
            *pOutLength = fileSize + 1;
        }
        r = 0;
    }
    else {
        free(text);
        r = EOF;
    }
    close(fd);

    return r;
}
