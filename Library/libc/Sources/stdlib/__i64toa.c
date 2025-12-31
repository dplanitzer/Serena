//
//  __i64toa.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__crt.h>
#include <__itoa.h>
#include <string.h>


char* _Nonnull __i64toa(int64_t val, ia_sign_format_t sign_mode, i64a_t* _Nonnull out)
{
    char *ep = &out->buffer[I64A_BUFFER_SIZE - 1];
    char *p = ep;
    char sign;
    iu64_t xy[2], q, r;
    
    if (val < 0) {
        if (val == INT64_MIN) {
            out->length = 20;
            out->offset = 0;
            memcpy(out->buffer, "-9223372036854775808", 21);
            return out->buffer;
        }

        val = -val;
        sign = '-';
    } else {
        sign = (sign_mode == ia_sign_plus_minus) ? '+' : '\0';
    }

    xy[0].s64 = val;
    xy[1].s64 = 10ll;

    *p-- = '\0';
    do {
        _divs64(xy, &q, &r);
        *p-- = '0' + (char)r.s64;
        xy[0].s64 = q.s64;
    } while (xy[0].s64);
    
    if (sign) {
        *p = sign;
    } else {
        p++;
    }

    out->length = ep - p;
    out->offset = p - out->buffer;

    return p;
}
