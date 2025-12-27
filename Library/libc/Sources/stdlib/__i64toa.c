//
//  __i64toa.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__itoa.h>
#include <string.h>

extern int _divmods64(long long dividend, long long divisor, long long* quotient, long long* remainder);


char* _Nonnull __i64toa(int64_t val, ia_sign_format_t sign_mode, i64a_t* _Nonnull out)
{
    char *ep = &out->buffer[I64A_BUFFER_SIZE - 1];
    char *p = ep;
    char sign;
    int64_t q, r;

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

    *p-- = '\0';
    do {
        _divmods64(val, 10, &q, &r);
        *p-- = '0' + (char)r;
        val = q;
    } while (val);
    
    if (sign) {
        *p = sign;
    } else {
        p++;
    }

    out->length = ep - p;
    out->offset = p - out->buffer;

    return p;
}
