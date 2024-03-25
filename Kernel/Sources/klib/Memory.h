//
//  Memory.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/14/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Memory_h
#define Memory_h

#include <klib/Types.h>


// Copies 'count' contiguous bytes in memory from 'src' to 'dst'. The behavior is
// undefined if the source and destination regions overlap. Copies the data
// moving from the low address to the high address.
extern void* _Nonnull memcpy(void* _Nonnull restrict dst, const void* _Nonnull restrict src, size_t count);

// Copies 'count' contiguous bytes in memory from 'src' to 'dst'. The source and
// destination regions may overlap.
extern void* _Nonnull memmove(void* _Nonnull restrict dst, const void* _Nonnull restrict src, size_t count);

// Sets all bytes in the given range to 'c'
extern void* _Nonnull memset(void* _Nonnull dst, int c, size_t count);

#endif /* Memory_h */
