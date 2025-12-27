//
//  kern/kernlib.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_KERNLIB_H
#define _KERN_KERNLIB_H 1

#include <arch/_offsetof.h>
#include <arch/_ssize.h>
#include <_cmndef.h>
#include <_absdiv.h>
#include <_imaxabsdiv.h>
#include <_sortsearch.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>
#include <ext/limits.h>
#include <ext/math.h>
#include <kpi/_access.h>
#include <kpi/_seek.h>

__CPP_BEGIN

//
// This header file is the kernel version of user space's stdlib.h + unistd.h
//

#define CHAR_PTR_MAX    ((char*)__UINTPTR_MAX)


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


extern char * _Nullable itoa(int val, char * _Nonnull buf, int radix);
extern char * _Nullable ltoa(long val, char * _Nonnull buf, int radix);

extern char * _Nullable utoa(unsigned int val, char * _Nonnull buf, int radix);
extern char * _Nullable ultoa(unsigned long val, char * _Nonnull buf, int radix);

extern long strtol(const char * _Restrict str, char ** _Restrict str_end, int base);


// Required minimum size is (string length byte + sign byte + longest digit sequence + 1 NUL byte) -> 1 + 64 (binary 64bit) + 1 + 1 = 25 bytes
// A digit string is generated in a canonical representation: string length, sign, digits ..., NUL
#define DIGIT_BUFFER_CAPACITY 67

__CPP_END

#endif /* _KERN_KERNLIB_H */
