//
//  gets.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


char *gets(char *str)
{
    if (str == NULL) {
        return NULL;
    }

    char* p = str;

    while (true) {
        const int ch = getchar();

        if (ch == EOF || ch == '\n') {
            break;
        }

        *p++ = (char)ch;
    }
    *p = '\0';
    
    return str;
}
