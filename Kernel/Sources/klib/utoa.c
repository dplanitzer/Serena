//
//  utoa.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <__itoa.h>
#include <kern/kernlib.h>


char * _Nullable utoa(unsigned int val, char * _Nonnull buf, int radix)
{
    i32a_t i32a;
    char* p;

    if (buf) {
        switch (radix) {
            case 2:
            case 8:
            case 10:
            case 16:
                p = __u32toa((uint32_t)val, radix, false, &i32a);
                memcpy(buf, p, i32a.length + 1);
                break;

            default:
                return NULL;
        }
    }
    return buf;
}

char * _Nullable ultoa(unsigned long val, char * _Nonnull buf, int radix)
{
    return utoa(val, buf, radix);
}
