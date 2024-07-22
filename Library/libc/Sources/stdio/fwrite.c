//
//  fwrite.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


size_t fwrite(const void *buffer, size_t size, size_t count, FILE *s)
{
    if (size == 0 || count == 0) {
        return 0;
    }

    size_t nBytesToWrite = size * count;
    size_t nBytesWritten = 0;
    while (nBytesToWrite-- > 0) {
        const int r = fputc(((const unsigned char*)buffer)[nBytesWritten++], s);

        if (r == EOF) {
            break;
        }
    }

    return nBytesWritten / size;
}

#if 0
//XXX general approach of bulk writing
static errno_t __IOChannel_Write(const char* _Nonnull pBuffer, size_t nBytes)
{
    ssize_t nBytesWritten = 0;

    while (nBytesWritten < nBytes) {
        const ssize_t r = IOChannel_Write(kIOChannel_Stdout, pBuffer, nBytes);
        
        if (r < 0) {
            return (errno_t) -r;
        }
        nBytesWritten += nBytes;
    }

    return 0;
}
#endif
