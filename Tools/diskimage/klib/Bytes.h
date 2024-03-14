//
//  Bytes.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef klib_Bytes_h
#define klib_Bytes_h

#include <klib/Types.h>


// Copies 'n' contiguous bytes in memory from 'pSrc' to 'pDst'.
extern void Bytes_CopyRange(void* _Nonnull pDst, const void* _Nonnull pSrc, size_t n);

// Zeros out 'len' contiguous bytes in memory starting at 'pBytes'
extern void Bytes_ClearRange(void* _Nonnull pBytes, size_t len);

// Sets all bytes in the given range to 'byte'
extern void Bytes_SetRange(void* _Nonnull pBytes, size_t len, int c);

#endif /* klib_Bytes_h */
