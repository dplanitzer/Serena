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


int putchar(int ch)
{
    if (ch == EOF) {
        return EOF;
    }

    unsigned char buf = (unsigned char) ch;

    if (__syscall(SC_write, (const char*)&buf, 1) == 0) {
        return ch;
    } else {
        return 0;
    }
}

int puts(const char *str)
{
    if (__syscall(SC_write, str, strlen(str)) != 0) {
        return EOF;
    }

    return putchar('\n');
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
