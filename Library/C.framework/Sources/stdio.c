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
#include <System.h>


int putchar(int ch)
{
    char buf[2];

    buf[0] = (char)ch;
    buf[1] = '\0';

    if (__write(buf) == 0) {
        return ch;
    } else {
        return 0;
    }
}

int puts(const char *str)
{
    if (__write(str) == 0) {
        return 0;
    } else {
        return EOF;
    }
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
