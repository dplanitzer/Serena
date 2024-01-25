//
//  stdint.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDINT_H
#define _STDINT_H 1

#include <_cmndef.h>
#include <_dmdef.h>

__CPP_BEGIN

#define INT8_WIDTH 8
#define INT16_WIDTH 16
#define INT32_WIDTH 32
#define INT64_WIDTH 64

#define INT_FAST8_WIDTH 16
#define INT_FAST16_WIDTH 16
#define INT_FAST32_WIDTH 32
#define INT_FAST64_WIDTH 64

#define INT_LEAST8_WIDTH 8
#define INT_LEAST16_WIDTH 16
#define INT_LEAST32_WIDTH 32
#define INT_LEAST64_WIDTH 64

#define INTMAX_WIDTH __INTMAX_WIDTH
#define INTPTR_WIDTH __INTPTR_WIDTH


#define INT8_MIN  0x80
#define INT16_MIN 0x8000
#define INT32_MIN 0x80000000l
#define INT64_MIN 0x8000000000000000ll

#define INT_FAST8_MIN INT16_MIN
#define INT_FAST16_MIN INT16_MIN
#define INT_FAST32_MIN INT32_MIN
#define INT_FAST64_MIN INT64_MIN

#define INT_LEAST8_MIN INT8_MIN
#define INT_LEAST16_MIN INT16_MIN
#define INT_LEAST32_MIN INT32_MIN
#define INT_LEAST64_MIN INT64_MIN

#define INTMAX_MIN __INTMAX_MIN
#define INTPTR_MIN __INTPTR_MIN


#define INT8_MAX 0x7f
#define INT16_MAX 0x7fff
#define INT32_MAX 0x7fffffffl
#define INT64_MAX 0x7fffffffffffffffll

#define INT_FAST8_MAX INT16_MAX
#define INT_FAST16_MAX INT16_MAX
#define INT_FAST32_MAX INT32_MAX
#define INT_FAST64_MAX INT64_MAX

#define INT_LEAST8_MAX INT8_MAX
#define INT_LEAST16_MAX INT16_MAX
#define INT_LEAST32_MAX INT32_MAX
#define INT_LEAST64_MAX INT64_MAX

#define INTMAX_MAX __INTMAX_MAX
#define INTPTR_MAX __INTPTR_MAX


#define UINT8_WIDTH 8
#define UINT16_WIDTH 16
#define UINT32_WIDTH 32
#define UINT64_WIDTH 64

#define UINT_FAST8_WIDTH 16
#define UINT_FAST16_WIDTH 16
#define UINT_FAST32_WIDTH 32
#define UINT_FAST64_WIDTH 64

#define UINT_LEAST8_WIDTH 8
#define UINT_LEAST16_WIDTH 16
#define UINT_LEAST32_WIDTH 32
#define UINT_LEAST64_WIDTH 64

#define UINTMAX_WIDTH __UINTMAX_WIDTH
#define UINTPTR_WIDTH __UINTPTR_WIDTH


#define UINT8_MAX 255
#define UINT16_MAX 65535
#define UINT32_MAX 4294967295u
#define UINT64_MAX 18446744073709551615ull

#define UINT_FAST8_MAX UINT16_MAX
#define UINT_FAST16_MAX UINT16_MAX
#define UINT_FAST32_MAX UINT32_MAX
#define UINT_FAST64_MAX UINT64_MAX

#define UINT_LEAST8_MAX UINT8_MAX
#define UINT_LEAST16_MAX UINT16_MAX
#define UINT_LEAST32_MAX UINT32_MAX
#define UINT_LEAST64_MAX UINT64_MAX

#define UINTMAX_MAX __UINTMAX_MAX
#define UINTPTR_MAX __UINTPTR_MAX


#define INT8_C(val) ((int_least8_t)(val))
#define INT16_C(val) ((int_least16_t)(val))
#define INT32_C(val) ((int_least32_t)(val)l)
#define INT64_C(val) ((int_least64_t)(val)ll)

#define INTMAX_C(val) ((intmax_t)(val)ll)

#define UINT8_C(val) ((uint_least8_t)(val)u)
#define UINT16_C(val) ((uint_least16_t)(val)u)
#define UINT32_C(val) ((uint_least32_t)(val)ul)
#define UINT64_C(val) ((uint_least64_t)(val)ull)

#define UINTMAX_C(val) ((uintmax_t)(val)ull)


typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed long         int32_t;
typedef signed long long    int64_t;

typedef signed short        int_fast8_t;
typedef signed short        int_fast16_t;
typedef signed long         int_fast32_t;
typedef signed long long    int_fast64_t;

typedef signed char         int_least8_t;
typedef signed short        int_least16_t;
typedef signed long         int_least32_t;
typedef signed long long    int_least64_t;


typedef __intmax_t intmax_t;
typedef __intptr_t intptr_t;


typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned long       uint32_t;
typedef unsigned long long  uint64_t;

typedef unsigned short      uint_fast8_t;
typedef unsigned short      uint_fast16_t;
typedef unsigned long       uint_fast32_t;
typedef unsigned long long  uint_fast64_t;

typedef unsigned char       uint_least8_t;
typedef unsigned short      uint_least16_t;
typedef unsigned long       uint_least32_t;
typedef unsigned long long  uint_least64_t;


typedef __uintmax_t uintmax_t;
typedef __uintptr_t uintptr_t;

__CPP_END

#endif /* _STDINT_H */
