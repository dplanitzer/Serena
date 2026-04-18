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

char* _Nonnull fmt_duration(const nanotime_t* _Nonnull dur, char* _Nonnull buf)
{
    time_t secs = dur->tv_sec;
    time_t hours = secs / 3600;
    secs -= hours * 3600;
    time_t mins = secs / 60;
    secs -= mins * 60;

    if (hours > 99) {
        hours = 99; //XXX for now
    }

    const div_t hd = div(hours, 10);
    const div_t md = div(mins, 10);
    const div_t sd = div(secs, 10);

    buf[0] = '0' + hd.quot;
    buf[1] = '0' + hd.rem;
    buf[2] = ':';
    buf[3] = '0' + md.quot;
    buf[4] = '0' + md.rem;
    buf[5] = '.';
    buf[6] = '0' + sd.quot;
    buf[7] = '0' + sd.rem;
    buf[8] = '\0';

    return buf;
}
