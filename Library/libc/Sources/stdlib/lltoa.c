//
//  lltoa.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <__stddef.h>


char *lltoa(long long val, char *buf, int radix)
{
    i64a_t i64a;
    char* p;

    if (buf) {
        switch (radix) {
            case 10:
                p = __i64toa((int64_t)val, ia_sign_minus_only, &i64a);
                break;
        
            case 2:
            case 8:
            case 16:
                p = __u64toa((uint64_t)val, radix, false, &i64a);
                break;

            default:
                errno = EINVAL;
                return NULL;
        }
        memcpy(buf, p, i64a.length + 1);
    }
    return buf;
}
