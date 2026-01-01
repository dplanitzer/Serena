//
//  __u64toa.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__crt.h>
#include <__itoa.h>
#include <string.h>


// 'radix' must be 2, 8, 10 or 16
char* _Nonnull __u64toa(uint64_t val, int radix, bool isUppercase, i64a_t* _Nonnull out)
{
    const char* const ds = (isUppercase) ? "0123456789ABCDEF" : "0123456789abcdef";
    char *ep = &out->buffer[I64A_BUFFER_SIZE - 1];
    char *p = ep;
    iu64_t xy[2], q, r;

    *p-- = '\0';
    switch (radix) {
        case 2:
            do {
                *p-- = ds[(uint8_t)val & 0x1u];
                val >>= 1ull;
            } while (val);
            break;

        case 16:
            do {
                *p-- = ds[(uint8_t)val & 0x0fu];
                val >>= 4ull;
            } while (val);
            break;

        default:
            xy[0].u64 = val;
            xy[1].u64 = radix;

            do {
                _divmodu64(xy, &q, &r);
                *p-- = ds[r.u64];
                xy[0].u64 = q.u64;
            } while (xy[0].u64);
            break;
    }

    p++;
    
    out->length = ep - p;
    out->offset = p - out->buffer;

    return p;
}
