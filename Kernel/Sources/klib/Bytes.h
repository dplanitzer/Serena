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
extern void Bytes_CopyRange(void* _Nonnull pDst, const void* _Nonnull pSrc, size_t n);

// Zeros out 'len' contiguous bytes in memory starting at 'pBytes'
extern void Bytes_ClearRange(void* _Nonnull pBytes, size_t len);

// Sets all bytes in the given range to 'byte'
extern void Bytes_SetRange(void* _Nonnull pBytes, size_t len, int c);

#endif /* Bytes_h */
