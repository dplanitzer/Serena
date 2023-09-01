//
//  _dmdef.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/31/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __DMDEF_H
#define __DMDEF_H 1

#define __CHAR_WIDTH 8
#define __CHAR_MAX_BASE_10_DIGITS 3

#define __SHORT_WIDTH 16
#define __SHORT_MAX_BASE_10_DIGITS 5


// Supported data models:
// ILP32 (4/4/4): int, long and pointers are 32bit wide
// LP64  (4/8/8): int is 32bit wide, long and pointers are 64bit wide
// See: <https://en.cppreference.com/w/c/language/arithmetic_types>
//
// Select the data model for which the C library should be compiled by passing
// one of the following declarations as a pre-processor define to the compiler:
// __ILP32__
// __LP64__
#ifdef __ILP32__
//
// ILP32
//

#define __INT_WIDTH 32
#define __INT_MIN -2147483648
#define __INT_MAX 2147483647
#define __INT_MAX_BASE_10_DIGITS 10

#define __UINT_WIDTH 32
#define __UINT_MAX 4294967295u

#define __LONG_WIDTH 32
#define __LONG_MAX_BASE_10_DIGITS 10

#define __LLONG_WIDTH 64
#define __LLONG_MAX_BASE_10_DIGITS 19


// intmax_t
typedef long long __intmax_t;

#define __INTMAX_WIDTH 64
#define __INTMAX_MIN -9223372036854775808ll
#define __INTMAX_MAX 9223372036854775807ll
#define __INTMAX_MAX_BASE_10_DIGITS 19


// uintmax_t
typedef unsigned long long __uintmax_t;

#define __UINTMAX_WIDTH 64
#define __UINTMAX_MAX 18446744073709551615ull
#define __UINTMAX_MAX_BASE_10_DIGITS 19


// intptr_t
typedef long __intptr_t;

#define __INTPTR_WIDTH 32
#define __INTPTR_MIN -2147483648l
#define __INTPTR_MAX 2147483647l


// uintptr_t
typedef unsigned long __uintptr_t;

#define __UINTPTR_WIDTH 32
#define __UINTPTR_MAX 4294967295ul


// ptrdiff_t
typedef long __ptrdiff_t;

#define __PTRDIFF_WIDTH 32
#define __PTRDIFF_MIN -2147483648l
#define __PTRDIFF_MAX 2147483647l


// size_t
typedef unsigned long __size_t;

#define __SIZE_WIDTH 32
#define __SIZE_MAX 4294967295ul


// ssize_t
typedef long __ssize_t;

#define __SSIZE_WIDTH 32
#define __SSIZE_MIN -2147483648l
#define __SSIZE_MAX 2147483647l

#elif __LP64__
//
// LP64
//

#define __INT_WIDTH 32
#define __INT_MIN -2147483648
#define __INT_MAX 2147483647
#define __INT_MAX_BASE_10_DIGITS 10

#define __UINT_WIDTH 32
#define __UINT_MAX 4294967295u

#define __LONG_WIDTH 64
#define __LONG_MAX_BASE_10_DIGITS 10

#define __LLONG_WIDTH 64
#define __LLONG_MAX_BASE_10_DIGITS 19


// intmax_t
typedef long long __intmax_t;

#define __INTMAX_WIDTH 64
#define __INTMAX_MIN -9223372036854775808ll
#define __INTMAX_MAX 9223372036854775807ll
#define __INTMAX_MAX_BASE_10_DIGITS 19


// uintmax_t
typedef unsigned long long __uintmax_t;

#define __UINTMAX_WIDTH 64
#define __UINTMAX_MAX 18446744073709551615ull
#define __UINTMAX_MAX_BASE_10_DIGITS 19


// intptr_t
typedef long long __intptr_t;

#define __INTPTR_WIDTH 64
#define __INTPTR_MIN -9223372036854775808ll
#define __INTPTR_MAX 9223372036854775807ll


// uintptr_t
typedef unsigned long long __uintptr_t;

#define __UINTPTR_WIDTH 64
#define __UINTPTR_MAX 18446744073709551615ull


// ptrdiff_t
typedef long long __ptrdiff_t;

#define __PTRDIFF_WIDTH 64
#define __PTRDIFF_MIN -9223372036854775808ll
#define __PTRDIFF_MAX 9223372036854775807ll


// size_t
typedef unsigned long long __size_t;

#define __SIZE_WIDTH 64
#define __SIZE_MAX 18446744073709551615ull


// ssize_t
typedef long long __ssize_t;

#define __SSIZE_WIDTH 64
#define __SSIZE_MIN -9223372036854775808ll
#define __SSIZE_MAX 9223372036854775807ll

#else

#error "unknown data model"

#endif

#endif /* __DMDEF_H */
