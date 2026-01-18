//
//  utils.c
//  snake
//
//  Created by Dietmar Planitzer on 3/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <ext/string.h>


void cursor_on(bool onOff)
{
    if (onOff) {
        puts("\033[?25h");
    }
    else {
        puts("\033[?25l");
    }
}

char* cls(char* dst)
{
    return strcpy_x(dst, "\033[2J\033[H");
}

char* mv_by_precomp(char* dst, const char* leb)
{
    *dst++ = 033;
    *dst++ = '[';
    dst = strcpy_x(dst, leb);
    *dst++ = 'C';

    return dst;
}

char* mv_to(char* dst, int x, int y)
{
    *dst++ = 033;
    *dst++ = '[';
    itoa(y + 1, dst, 10);
    dst = strcat_x(dst, ";");
    itoa(x + 1, dst, 10);
    dst = strcat_x(dst, "f");

    return dst;
}

char* h_line(char* dst, char ch, int count)
{
    while (count-- > 0) {
        *dst++ = ch;
    }
    return dst;
}
