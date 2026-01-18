//
//  kern/kernlib.h
//  diskimage
//
//  Created by Dietmar Planitzer on 5/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_KERNLIB_H
#define _KERN_KERNLIB_H 1

#include <_cmndef.h>
#include <inttypes.h>   // imaxabs(), imaxdiv()
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h> // abort(), abs(), div(), qsort(), bsearch(), itoa(), ltoa(), strtol()
#include <stdnoreturn.h>
#include <ext/limits.h>
#include <ext/math.h>
#include <../../Kernel/Headers/kpi/_access.h>
#include <../../Kernel/Headers/kpi/_seek.h>


//
// NOTE: All kernel related code must be compiled with a pure C environment. Eg
// no POSIX extensions (since those extensions would collide with our own POSIX-
// like definitions).
//
// Windows: compile with /std:c11
//


// Convert a size_t to a ssize_t with clamping
#define __SSizeByClampingSize(ub) (ssize_t)__min(ub, SSIZE_MAX)


// Minimum size for types
// (Spelled out in a comment because C is stupid and doesn't give us a simple & easy
// way to statically compare the size of two types. Supporting that would be just
// too easy you know and the language only has had like 50 years to mature...)
// 
// uid_t        <= int
// gid_t        <= int
// errno_t      <= int
// pid_t        <= int
// fsid_t       <= int


// Size constructors
#define SIZE_GB(x)  ((unsigned long)(x) * 1024ul * 1024ul * 1024ul)
#define SIZE_MB(x)  ((long)(x) * 1024 * 1024)
#define SIZE_KB(x)  ((long)(x) * 1024)


// Required minimum size is (string length byte + sign byte + longest digit sequence + 1 NUL byte) -> 1 + 64 (binary 64bit) + 1 + 1 = 25 bytes
// A digit string is generated in a canonical representation: string length, sign, digits ..., NUL
#define DIGIT_BUFFER_CAPACITY 67

// defined in diskimage.c
extern _Noreturn fatal(const char* _Nonnull format, ...);
extern _Noreturn vfatal(const char* _Nonnull format, va_list ap);

__CPP_END

#endif /* _KERN_KERNLIB_H */
