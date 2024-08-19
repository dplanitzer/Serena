//
//  __u32toa.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <__stddef.h>


// 'radix' must be 2, 8, 10 or 16
char* _Nonnull __u32toa(uint32_t val, int radix, bool isUppercase, i32a_t* _Nonnull out)
{
    const char* const ds = (isUppercase) ? "0123456789ABCDEF" : "0123456789abcdef";
    char *ep = &out->buffer[I32A_BUFFER_SIZE - 1];
    char *p = ep;

    *p-- = '\0';
    switch (radix) {
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
                *p-- = ds[val % radix];
                val /= radix;
            } while (val);
            break;
    }
    
    p++;

    out->length = ep - p;
    out->offset = p - out->buffer;

    return p;
}
