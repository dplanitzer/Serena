//
//  kern/string.h
//  libc
//
//  Created by Dietmar Planitzer on 5/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_STRING_H
#define _KERN_STRING_H 1

#include <_size.h>
#include <_ssize.h>
#include <_cmndef.h>
#include <stdbool.h>

__CPP_BEGIN

extern ssize_t strlen(const char* _Nonnull str);
extern ssize_t strnlen(const char* _Nonnull str, ssize_t strsz);

extern char* _Nonnull strcpy(char* _Nonnull _Restrict dst, const char* _Nonnull _Restrict src);
extern char* _Nonnull strncpy(char* _Nonnull _Restrict dst, const char* _Nonnull _Restrict src, ssize_t count);

extern bool strcmp(const char* _Nonnull _Restrict lhs, const char* _Nonnull _Restrict rhs);
extern bool strncmp(const char* _Nonnull _Restrict lhs, const char* _Nonnull _Restrict rhs, ssize_t count);

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
