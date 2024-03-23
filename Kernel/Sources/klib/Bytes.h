//
//  Bytes.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Bytes_h
#define Bytes_h

#include <klib/Types.h>


// Copies 'n' contiguous bytes in memory from 'pSrc' to 'pDst'.
extern void* _Nonnull memmove(void* _Nonnull pDst, const void* _Nonnull pSrc, size_t n);

// Sets all bytes in the given range to 'c'
extern void* _Nonnull memset(void* _Nonnull dst, int c, size_t count);

#endif /* Bytes_h */
