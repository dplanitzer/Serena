//
//  _dmdef.h
//  libsystem
//
//  Created by Dietmar Planitzer on 8/31/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __ABI_DMDEF_H
#define __ABI_DMDEF_H 1

#if defined(__M68K__)
#define __ILP32__ 1
#elif defined(_M_X64)
#define __LLP64__ 1
#elif defined(_M_IX86)
#define __ILP32__ 1
#else
#error "Unknown platform"
#endif


#define __CHAR_WIDTH 8
#define __CHAR_MAX_BASE_10_DIGITS 3

#define __SHORT_WIDTH 16
#define __SHORT_MAX_BASE_10_DIGITS 5


// Supported data models:
// ILP32 (4/4/4): int, long and pointers are 32bit wide
// LP64  (4/8/8): int is 32bit wide, long and pointers are 64bit wide
// LLP64 (4/4/8): int and long are 32bit; pointers are 64bit wide
// See: <https://en.cppreference.com/w/c/language/arithmetic_types>
//
// This header auto-selects the data model based on CPU specific pre-processor
// symbols defined by the C/C++ compiler used to compile this file:
// __ILP32__    (Serena, POSIX and Windows style 32bit address space)
// __LP64__     (Serena & POSIX style 64bit address space)
// __LLP64__    (Windows style 64bit address space)
#ifdef __ILP32__
//
// ILP32
//

#define __INT_WIDTH 32
#define __INT_MIN 0x80000000
#define __INT_MAX 0x7fffffff
#define __INT_MAX_BASE_10_DIGITS 10

#define __UINT_WIDTH 32
#define __UINT_MAX 4294967295u

#define __LONG_WIDTH 32
#define __LONG_MIN 0x80000000l
#define __LONG_MAX 0x7fffffffl
#define __LONG_MAX_BASE_10_DIGITS 10

#define __ULONG_WIDTH 32
#define __ULONG_MAX 4294967295ul

#define __LLONG_WIDTH 64
#define __LLONG_MIN 0x8000000000000000ll
#define __LLONG_MAX 0x7fffffffffffffffll
#define __LLONG_MAX_BASE_10_DIGITS 19

#define __ULLONG_WIDTH 64
#define __ULLONG_MAX 18446744073709551615ul


// intmax_t
typedef long long __intmax_t;

#define __INTMAX_WIDTH 64
#define __INTMAX_MIN 0x8000000000000000ll
#define __INTMAX_MAX 0x7fffffffffffffffll
#define __INTMAX_MAX_BASE_10_DIGITS 19


// uintmax_t
typedef unsigned long long __uintmax_t;

#define __UINTMAX_WIDTH 64
#define __UINTMAX_MAX 18446744073709551615ull
#define __UINTMAX_MAX_BASE_10_DIGITS 19


// intptr_t
typedef long __intptr_t;

#define __INTPTR_WIDTH 32
#define __INTPTR_MIN 0x80000000
#define __INTPTR_MAX 0x7fffffff


// uintptr_t
typedef unsigned long __uintptr_t;

#define __UINTPTR_WIDTH 32
#define __UINTPTR_MAX 4294967295ul


// ptrdiff_t
typedef long __ptrdiff_t;

#define __PTRDIFF_WIDTH 32
#define __PTRDIFF_MIN 0x80000000l
#define __PTRDIFF_MAX 0x7fffffffl


// size_t
typedef unsigned long __size_t;

#define __SIZE_WIDTH 32
#define __SIZE_MAX 4294967295ul


// ssize_t
typedef long __ssize_t;

#define __SSIZE_WIDTH 32
#define __SSIZE_MIN 0x80000000l
#define __SSIZE_MAX 0x7fffffffl

#elif __LP64__
//
// LP64
//

#define __INT_WIDTH 32
#define __INT_MIN 0x80000000l
#define __INT_MAX 0x7fffffffl
#define __INT_MAX_BASE_10_DIGITS 10

#define __UINT_WIDTH 32
#define __UINT_MAX 4294967295u

#define __LONG_WIDTH 64
#define __LONG_MIN 0x8000000000000000l
#define __LONG_MAX 0x7fffffffffffffffl
#define __LONG_MAX_BASE_10_DIGITS 10

#define __ULONG_WIDTH 64
#define __ULONG_MAX 18446744073709551615ul

#define __LLONG_WIDTH 64
#define __LONG_MIN 0x8000000000000000l
#define __LONG_MAX 0x7fffffffffffffffl
#define __LLONG_MAX_BASE_10_DIGITS 19

#define __ULLONG_WIDTH 64
#define __ULLONG_MAX 18446744073709551615ul


// intmax_t
typedef long __intmax_t;

#define __INTMAX_WIDTH 64
#define __INTMAX_MIN 0x8000000000000000l
#define __INTMAX_MAX 0x7fffffffffffffffl
#define __INTMAX_MAX_BASE_10_DIGITS 19


// uintmax_t
typedef unsigned long __uintmax_t;

#define __UINTMAX_WIDTH 64
#define __UINTMAX_MAX 18446744073709551615ul
#define __UINTMAX_MAX_BASE_10_DIGITS 19


// intptr_t
typedef long __intptr_t;

#define __INTPTR_WIDTH 64
#define __INTPTR_MIN 0x8000000000000000l
#define __INTPTR_MAX 0x7fffffffffffffffl


// uintptr_t
typedef unsigned long __uintptr_t;

#define __UINTPTR_WIDTH 64
#define __UINTPTR_MAX 18446744073709551615ul


// ptrdiff_t
typedef long __ptrdiff_t;

#define __PTRDIFF_WIDTH 64
#define __PTRDIFF_MIN 0x8000000000000000l
#define __PTRDIFF_MAX 0x7fffffffffffffffl


// size_t
typedef unsigned long __size_t;

#define __SIZE_WIDTH 64
#define __SIZE_MAX 18446744073709551615ul


// ssize_t
typedef long __ssize_t;

#define __SSIZE_WIDTH 64
#define __SSIZE_MIN 0x8000000000000000l
#define __SSIZE_MAX 0x7fffffffffffffffl

#elif __LLP64__
//
// LLP64
//

#define __INT_WIDTH 32
#define __INT_MIN 0x80000000
#define __INT_MAX 0x7fffffff
#define __INT_MAX_BASE_10_DIGITS 10

#define __UINT_WIDTH 32
#define __UINT_MAX 4294967295u

#define __LONG_WIDTH 32
#define __LONG_MIN 0x80000000l
#define __LONG_MAX 0x7fffffffl
#define __LONG_MAX_BASE_10_DIGITS 10

#define __ULONG_WIDTH 32
#define __ULONG_MAX 4294967295ul

#define __LLONG_WIDTH 64
#define __LLONG_MIN 0x8000000000000000ll
#define __LLONG_MAX 0x7fffffffffffffffll
#define __LLONG_MAX_BASE_10_DIGITS 19

#define __ULLONG_WIDTH 64
#define __ULLONG_MAX 18446744073709551615ul


// intmax_t
typedef long long __intmax_t;

#define __INTMAX_WIDTH 64
#define __INTMAX_MIN 0x8000000000000000ll
#define __INTMAX_MAX 0x7fffffffffffffffll
#define __INTMAX_MAX_BASE_10_DIGITS 19


// uintmax_t
typedef unsigned long long __uintmax_t;

#define __UINTMAX_WIDTH 64
#define __UINTMAX_MAX 18446744073709551615ull
#define __UINTMAX_MAX_BASE_10_DIGITS 19


// intptr_t
typedef long long __intptr_t;

#define __INTPTR_WIDTH 64
#define __INTPTR_MIN 0x8000000000000000l
#define __INTPTR_MAX 0x7fffffffffffffffl


// uintptr_t
typedef unsigned long long __uintptr_t;

#define __UINTPTR_WIDTH 64
#define __UINTPTR_MAX 18446744073709551615ul


// ptrdiff_t
typedef long long __ptrdiff_t;

#define __PTRDIFF_WIDTH 64
#define __PTRDIFF_MIN 0x8000000000000000l
#define __PTRDIFF_MAX 0x7fffffffffffffffl


// size_t
typedef unsigned long long __size_t;

#define __SIZE_WIDTH 64
#define __SIZE_MAX 18446744073709551615ul


// ssize_t
typedef long long __ssize_t;

#define __SSIZE_WIDTH 64
#define __SSIZE_MIN 0x8000000000000000l
#define __SSIZE_MAX 0x7fffffffffffffffl

#else

#error "unknown data model"

#endif

#if __SSIZE_WIDTH != __SIZE_WIDTH
    #error("__ssize_t and __size_t must have the same bit width")
#endif

#endif /* __ABI_DMDEF_H */
