//
//  stdio.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <syscall.h>
#include <__stddef.h>


int getchar(void)
{
    char buf;
    const ssize_t r = __syscall(SC_read, &buf, 1);

    if (r < 0) {
        return EOF;
    } else {
        return (int)(unsigned char)buf;
    }
}

char *gets(char *str)
{
    if (str == NULL) {
        return NULL;
    }

    char* p = str;

    while (true) {
        const int ch = getchar();

        if (ch == EOF || ch == '\n') {
            break;
        }

        *p++ = (char)ch;
    }
    *p = '\0';
    
    return str;
}

int putchar(int ch)
{
    if (ch == EOF) {
        return EOF;
    }

    unsigned char uch = (unsigned char) ch;

    if (__write((const char*)&uch, 1) == 0) {
        return uch;
    } else {
        return 0;
    }
}

int puts(const char *str)
{
    if (__write(str, strlen(str)) != 0) {
        return EOF;
    }

    return putchar('\n');
}

errno_t __write(const char* _Nonnull pBuffer, size_t nBytes)
{
    ssize_t nBytesWritten = 0;

    while (nBytesWritten < nBytes) {
        const ssize_t r = __syscall(SC_write, pBuffer, nBytes);
        
        if (r < 0) {
            return (errno_t) -r;
        }
        nBytesWritten += nBytes;
    }

    return 0;
}

void perror(const char *str)
{
    if (str && *str != '\0') {
        puts(str);
        puts(": ");
    }

    puts(strerror(errno));
    putchar('\n');
}
