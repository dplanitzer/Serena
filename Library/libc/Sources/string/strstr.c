//
//  strstr.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <__stddef.h>


char * _Nullable strstr(const char * _Nonnull str, const char * _Nonnull sub_str)
{
    const char* s = str;
    const char* sub_s = sub_str;

    for (;;) {
        if (*sub_s == '\0') {
            return (char*) str;
        }
        if (*s == '\0') {
            return NULL;
        }

        if (*sub_s++ != *s++) {
            s = ++str;
            sub_s = sub_str;
        }
    }
}
