//
//  kern/string.h
//  libc
//
//  Created by Dietmar Planitzer on 5/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_STRING_H
#define _KERN_STRING_H 1

#include <machine/abi/_size.h>
#include <machine/abi/_ssize.h>
#include <System/_cmndef.h>
#include <stdbool.h>

__CPP_BEGIN

extern ssize_t String_Length(const char* _Nonnull pStr);
extern ssize_t String_LengthUpTo(const char* _Nonnull pStr, ssize_t strsz);

extern char* _Nonnull String_Copy(char* _Nonnull pDst, const char* _Nonnull pSrc);
extern char* _Nonnull String_CopyUpTo(char* _Nonnull pDst, const char* _Nonnull pSrc, ssize_t count);

extern bool String_Equals(const char* _Nonnull pLhs, const char* _Nonnull pRhs);
extern bool String_EqualsUpTo(const char* _Nonnull pLhs, const char* _Nonnull pRhs, ssize_t count);

// Copies 'count' contiguous bytes in memory from 'src' to 'dst'. The behavior is
// undefined if the source and destination regions overlap. Copies the data
// moving from the low address to the high address.
extern void* _Nonnull memcpy(void* _Nonnull _Restrict dst, const void* _Nonnull _Restrict src, size_t count);

// Copies 'count' contiguous bytes in memory from 'src' to 'dst'. The source and
// destination regions may overlap.
extern void* _Nonnull memmove(void* _Nonnull _Restrict dst, const void* _Nonnull _Restrict src, size_t count);

// Sets all bytes in the given range to 'c'
extern void* _Nonnull memset(void* _Nonnull dst, int c, size_t count);

__CPP_END

#endif /* _KERN_STRING_H */
