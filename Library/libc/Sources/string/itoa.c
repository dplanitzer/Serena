//
//  itoa.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#define _OPEN_SYS_ITOA_EXT
#include <stdlib.h>
#include <string.h>
#include <__stddef.h>


char *itoa(int val, char *buf, int radix)
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
                errno = EINVAL;
                return NULL;
        }
        memcpy(buf, p, i32a.length + 1);
    }
    return buf;
}

char *ltoa(long val, char *buf, int radix)
{
    return itoa(val, buf, radix);
}
