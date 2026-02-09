//
//  __fmt_fp.c
//  libc
//
//  Created by Dietmar Planitzer on 2/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/__fmt.h>
#include <ext/limits.h>
#include <ext/math.h>
#include <string.h>

static void __fmt_format_fp(fmt_t* _Nonnull _Restrict self, char conversion, va_list* _Nonnull _Restrict ap)
{
    switch (conversion) {
        case 'f':   break; // XXX
        case 'F':   break; // XXX
        case 'e':   break; // XXX
        case 'E':   break; // XXX
        case 'a':   break; // XXX
        case 'A':   break; // XXX
        case 'g':   break; // XXX
        case 'G':   break; // XXX
        default:    break;
    }
}

void __fmt_init_fp(fmt_t* _Nonnull _Restrict self, void* _Nullable _Restrict s, fmt_putc_func_t _Nonnull putc_f, fmt_write_func_t _Nonnull write_f, bool doContCountingOnError)
{
    __fmt_common_init(self, s, putc_f, write_f, doContCountingOnError);
    self->format_cb = __fmt_format_fp;
}
