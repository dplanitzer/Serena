//
//  strlen.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <string.h>


size_t strlen(const char * _Nonnull str)
{
    const char* p = str;

    while(*p++ != '\0');

    return p - str - 1;
}
