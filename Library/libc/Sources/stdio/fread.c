//
//  fread.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


size_t fread(void *buffer, size_t size, size_t count, FILE *s)
{
    if (size == 0 || count == 0) {
        return 0;
    }

    size_t nBytesToRead = size * count;
    size_t nBytesRead = 0;
    while (nBytesToRead-- > 0) {
        const int ch = fgetc(s);

        if (ch == EOF) {
            break;
        }

        ((unsigned char*)buffer)[nBytesRead++] = (unsigned char)ch;
    }

    return nBytesRead / size;
}
