//
//  __i32toa.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__itoa.h>
#include <_absdiv.h>
#include <string.h>


char* _Nonnull __i32toa(int32_t val, ia_sign_format_t sign_mode, i32a_t* _Nonnull out)
{
    char *ep = &out->buffer[I32A_BUFFER_SIZE - 1];
    char *p = ep;
    char sign;

    if (val < 0) {
        // Check for the smallest possible negative value and handle it specially.
        // We do it this way because negating the smallest possible value causes
        // an overflow and we end up with the original value again. Secondly we
        // do not want to have to implement two loops: one for positive and one
        // for negative values because that would effectively cause us to implement
        // the same algorithm twice (with only diff being that we'd subtract the
        // remainder from '0' instead of adding it). Doing that would be stupid.
        if (val == INT32_MIN) {
            out->length = 11;
            out->offset = 0;
            memcpy(out->buffer, "-2147483648", 12);
            return out->buffer;
        }

        val = -val;
        sign = '-';
    } else {
        sign = (sign_mode == ia_sign_plus_minus) ? '+' : '\0';
    }

    *p-- = '\0';
    do {
        const div_t r = div(val, 10);

        *p-- = '0' + (char)r.rem;
        val = r.quot;
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
