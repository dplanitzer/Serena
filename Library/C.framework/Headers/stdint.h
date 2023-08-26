//
//  stdint.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDINT_H
#define _STDINT_H 1

typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long    int64_t;

typedef signed short        int_fast8_t;
typedef signed short        int_fast16_t;
typedef signed int          int_fast32_t;
typedef signed long long    int_fast64_t;

typedef signed char         int_least8_t;
typedef signed short        int_least16_t;
typedef signed int          int_least32_t;
typedef signed long long    int_least64_t;

typedef int64_t intmax_t;
typedef int32_t intptr_t;


typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;

typedef unsigned short      uint_fast8_t;
typedef unsigned short      uint_fast16_t;
typedef unsigned int        uint_fast32_t;
typedef unsigned long long  uint_fast64_t;

typedef unsigned char       uint_least8_t;
typedef unsigned short      uint_least16_t;
typedef unsigned int        uint_least32_t;
typedef unsigned long long  uint_least64_t;

typedef uint64_t uintmax_t;
typedef uint32_t uintptr_t;


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

#define INTMAX_WIDTH 64
#define INTPTR_WIDTH 32

#define INT8_MIN -128
#define INT16_MIN -32768
#define INT32_MIN -2147483648
#define INT64_MIN -9223372036854775808ll

#define INT_FAST8_MIN INT16_MIN
#define INT_FAST16_MIN INT16_MIN
#define INT_FAST32_MIN INT32_MIN
#define INT_FAST64_MIN INT64_MIN

#define INT_LEAST8_MIN INT8_MIN
#define INT_LEAST16_MIN INT16_MIN
#define INT_LEAST32_MIN INT32_MIN
#define INT_LEAST64_MIN INT64_MIN

#define INTMAX_MIN INT64_MIN
#define INTPTR_MIN INT32_MIN

#define INT8_MAX 127
#define INT16_MAX 32767
#define INT32_MAX 2147483647
#define INT64_MAX 9223372036854775807ll

#define INT_FAST8_MAX INT16_MAX
#define INT_FAST16_MAX INT16_MAX
#define INT_FAST32_MAX INT32_MAX
#define INT_FAST64_MAX INT64_MAX

#define INT_LEAST8_MAX INT8_MAX
#define INT_LEAST16_MAX INT16_MAX
#define INT_LEAST32_MAX INT32_MAX
#define INT_LEAST64_MAX INT64_MAX

#define INTMAX_MAX INT64_MAX
#define INTPTR_MAX INT32_MAX


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

#define UINTMAX_WIDTH 64
#define UINTPTR_WIDTH 32

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

#define UINTMAX_MAX UINT64_MAX
#define UINTPTR_MAX UINT32_MAX


#define INT8_C(val) ((int_least8_t)(val))
#define INT16_C(val) ((int_least16_t)(val))
#define INT32_C(val) ((int_least32_t)(val))
#define INT64_C(val) ((int_least64_t)(val))

#define INTMAX_C(val) ((intmax_t)(val))

#define UINT8_C(val) ((uint_least8_t)(val))
#define UINT16_C(val) ((uint_least16_t)(val))
#define UINT32_C(val) ((uint_least32_t)(val))
#define UINT64_C(val) ((uint_least64_t)(val))

#define UINTMAX_C(val) ((uintmax_t)(val))

#endif /* _STDINT_H */