//
//  ungetc.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


// Expects:
// - 's' direction is in
// - 's' ugbCount > 0
// - 's' is not buffered
int __fget_ugb(char* _Nonnull _Restrict pch, FILE * _Nonnull _Restrict s)
{
    if (s->cb.seek(s->context, 1ll, SEEK_CUR) < 0ll) {
        return EOF;
    }

    s->ugbCount = 0;
    *pch = s->ugb;
    return 1;
}

int __ungetc(int ch, FILE * _Nonnull s)
{
    int r = EOF;

    if (ch == EOF) {
        return EOF;
    }

    const unsigned char ch8 = (const unsigned char)ch;

    if (s->flags.bufferMode > _IONBF) {
        if (s->bufferIndex == 0) {
            return EOF;
        }

        s->bufferIndex--;
        s->buffer[s->bufferIndex] = ch8;
    }
    else {
        if (s->ugbCount > 0) {
            return EOF;
        }

        if (s->cb.seek(s->context, -1ll, SEEK_CUR) < 0ll) {
            return EOF;
        }

        s->ugb = ch8;
        s->ugbCount = 1;
    }
    
    s->flags.hasEof = 0;
    return (int)ch8;
}

int ungetc(int ch, FILE * _Nonnull s)
{
    int r = EOF;

    __flock(s);
    __fensure_no_eof_err(s);
    __fensure_readable(s);
    __fensure_byte_oriented(s);
    __fensure_direction(s, __kStreamDirection_In);
    
    r = __ungetc(ch, s);

catch:
    __funlock(s);
    return r;
}
