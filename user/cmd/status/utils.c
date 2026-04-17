//
//  utils.c
//  status
//
//  Created by Dietmar Planitzer on 4/15/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "utils.h"
#include <stdio.h>
#include <ext/stdlib.h>
#include <ext/string.h>


void term_cursor_on(bool onOff)
{
    if (onOff) {
        puts("\033[?25h");
    }
    else {
        puts("\033[?25l");
    }
}

void term_cls(void)
{
    fputs("\033[2J\033[H", stdout);
}

void term_move_to(int x, int y)
{
    static char buf[2 + 9 + 1 + 9 + 1 + 1];
    char* p = buf;

    *p++ = 033;
    *p++ = '[';
    itoa(y + 1, p, 10);
    p = strcat_x(p, ";");
    itoa(x + 1, p, 10);
    p = strcat_x(p, "f");

    fputs(p, stdout);
}

char* fmt_mem_size(uint64_t msize, char* _Nonnull buf)
{
#define N_UNITS 4
    static const char* postfix[N_UNITS] = { "", "K", "M", "G" };
    int pi = 0;

    for (;;) {
        if (msize < 1024ull || pi == N_UNITS) {
            ulltoa(msize, buf, 10);
            strcat(buf, postfix[pi]);
            break;
        }

        msize >>= 10ull;
        pi++;
    }

    return buf;
}
