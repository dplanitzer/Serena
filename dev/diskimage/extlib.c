//
//  extlib.c
//  diskimage
//
//  Created by Dietmar Planitzer on 1/29/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdbool.h>
#include <ext/stdlib.h>
#include <stdio.h>


// Valid values for 'radix' are: 2, 8, 10, 16
char * _Nullable itoa(int val, char * _Nullable buf, int radix)
{
    const char* r;

    switch (radix) {
        case 8:  r = "%o"; break;
        case 10: r = "%d"; break;
        case 16: r = "%x"; break;
        default: errno = EINVAL; return NULL;
    }

    if (buf) {
        sprintf(buf, r, val);
    }
    return buf;
}

char * _Nullable ltoa(long val, char * _Nullable buf, int radix)
{
    const char* r;

    switch (radix) {
        case 8:  r = "%lo"; break;
        case 10: r = "%ld"; break;
        case 16: r = "%lx"; break;
        default: errno = EINVAL; return NULL;
    }

    if (buf) {
        sprintf(buf, r, val);
    }
    return buf;
}

char * _Nullable lltoa(long long val, char * _Nullable buf, int radix)
{
    const char* r;

    switch (radix) {
        case 8:  r = "%llo"; break;
        case 10: r = "%lld"; break;
        case 16: r = "%llx"; break;
        default: errno = EINVAL; return NULL;
    }

    if (buf) {
        sprintf(buf, r, val);
    }
    return buf;
}

char * _Nullable utoa(unsigned int val, char * _Nullable buf, int radix)
{
    const char* r;

    switch (radix) {
        case 8:  r = "%o"; break;
        case 10: r = "%u"; break;
        case 16: r = "%x"; break;
        default: errno = EINVAL; return NULL;
    }

    if (buf) {
        sprintf(buf, r, val);
    }
    return buf;
}

char * _Nullable ultoa(unsigned long val, char * _Nullable buf, int radix)
{
    const char* r;

    switch (radix) {
        case 8:  r = "%lo"; break;
        case 10: r = "%lu"; break;
        case 16: r = "%lx"; break;
        default: errno = EINVAL; return NULL;
    }

    if (buf) {
        sprintf(buf, r, val);
    }
    return buf;
}

char * _Nullable ulltoa(unsigned long long val, char * _Nullable buf, int radix)
{
    const char* r;

    switch (radix) {
        case 8:  r = "%llo"; break;
        case 10: r = "%llu"; break;
        case 16: r = "%llx"; break;
        default: errno = EINVAL; return NULL;
    }

    if (buf) {
        sprintf(buf, r, val);
    }
    return buf;
}
