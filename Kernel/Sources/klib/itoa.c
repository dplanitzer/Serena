//
//  itoa.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <__itoa.h>
#include <kern/kernlib.h>


char * _Nullable itoa(int val, char * _Nonnull buf, int radix)
{
    i32a_t i32a;
    char* p;

    if (buf) {
        switch (radix) {
            case 10:
                p = __i32toa((int32_t)val, ia_sign_minus_only, &i32a);
                break;
        
            case 2:
            case 8:
            case 16:
                p = __u32toa((uint32_t)val, radix, false, &i32a);
                break;

            default:
                return NULL;
        }
        memcpy(buf, p, i32a.length + 1);
    }
    return buf;
}

char * _Nullable ltoa(long val, char * _Nonnull buf, int radix)
{
    return itoa(val, buf, radix);
}
