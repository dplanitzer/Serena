//
//  ulltoa.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <ext/stdlib.h>
#include <string.h>
#include <__itoa.h>


char *ulltoa(unsigned long long val, char *buf, int radix)
{
    i64a_t i64a;
    char* p;

    if (buf) {
        switch (radix) {
            case 2:
            case 8:
            case 10:
            case 16:
                p = __u64toa((uint64_t)val, radix, false, &i64a);
                memcpy(buf, p, i64a.length + 1);
                break;

            default:
                errno = EINVAL;
                return NULL;
        }
    }
    return buf;
}
