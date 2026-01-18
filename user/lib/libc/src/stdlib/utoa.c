//
//  utoa.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <__itoa.h>


char *utoa(unsigned int val, char *buf, int radix)
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
                errno = EINVAL;
                return NULL;
        }
    }
    return buf;
}

char *ultoa(unsigned long val, char *buf, int radix)
{
    return utoa(val, buf, radix);
}
