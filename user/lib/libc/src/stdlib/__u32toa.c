//
//  __u32toa.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__itoa.h>
#include <_absdiv.h>
#include <string.h>


// 'base' must be 2, 8, 10 or 16
char* _Nonnull __u32toa(uint32_t val, int base, bool isUppercase, i32a_t* _Nonnull out)
{
    const char* const ds = (isUppercase) ? __g_digits_16_uc : __g_digits_16_lc;
    char *ep = &out->buffer[I32A_BUFFER_SIZE - 1];
    char *p = ep;

    *p-- = '\0';
    switch (base) {
        case 2:
            do {
                *p-- = ds[val & 0x1u];
                val >>= 1u;
            } while (val);
            break;

        case 16:
            do {
                *p-- = ds[val & 0x0fu];
                val >>= 4u;
            } while (val);
            break;

        default:
            do {
                const udiv_t r = udiv(val, base);

                *p-- = ds[r.rem];
                val = r.quot;
            } while (val);
            break;
    }
    
    p++;

    out->length = ep - p;
    out->offset = p - out->buffer;

    return p;
}
