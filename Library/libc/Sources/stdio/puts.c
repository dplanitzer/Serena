//
//  puts.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <limits.h>


int puts(const char *str)
{
    const int nCharsWritten = fputs(str, stdout);
    
    if (nCharsWritten >= 0) {
        const int r = fputc('\n', stdout);
        
        if (r != EOF) {
            return (nCharsWritten < INT_MAX) ? nCharsWritten + 1 : INT_MAX;
        }
    }
    return EOF;
}
