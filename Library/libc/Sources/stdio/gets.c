//
//  gets.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


char *_Nullable gets(char * _Nonnull str)
{
    char* p = str;

    if (p == NULL) {
        return NULL;
    }

    while (true) {
        const int ch = getchar();

        if (ch == EOF || ch == '\n') {
            break;
        }

        *p++ = (char)ch;
    }
    *p = '\0';
    
    return ((p == str && feof(stdin)) || ferror(stdin)) ? NULL : str;
}
