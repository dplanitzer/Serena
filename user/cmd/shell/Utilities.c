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
#include <ext/limits.h>
#include <serena/file.h>
#include <serena/process.h>


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
    fs_attr_t attr;
    
    const int fd = fs_open(path, O_RDONLY);
    if (fd < 0) {
        return EOF;
    }

    if (fd_attr(fd, &attr) < 0) {
        fd_close(fd);
        return EOF;
    }

    if (attr.file_type != FS_FTYPE_REG) {
        errno = EINVAL;
        fd_close(fd);
        return EOF;
    }
    if (attr.size > (off_t)SSIZE_MAX) {
        errno = E2BIG;
        fd_close(fd);
        return EOF;
    }

    const size_t fileSize = (size_t)attr.size;
    char* text = malloc(fileSize + 1);
    int r;

    if (fd_read(fd, text, fileSize) == fileSize) {
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
    fd_close(fd);

    return r;
}
