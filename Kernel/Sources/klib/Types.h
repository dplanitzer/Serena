//
//  Types.h
//  Apollo
//
//  Created by Dietmar Planitzer on 7/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Types_h
#define Types_h

#include <_aligndef.h>
#include <_booldef.h>
#include <_nulldef.h>
#include <_varargs.h>

#ifndef _Weak
#define _Weak
#endif

#ifndef _Noreturn
#define _Noreturn   void
#endif


// The Byte type represents raw, untyped memory. Raw memory may be reinterpreted
// or converted to typed memory but this requires the execution of some piece of
// code that knows how to interpret or rearrange the bits in untyped memory to
// make them conforming to the semantics of the desired type.
typedef unsigned char Byte;


// Boolean type
// Note that a boolean value of true must have bit #0 set. All other bits may or
// may not be set in addition to bit #0. The reason for this design choice is
// that by requiring only one bit to be set for a true value that we can take
// advantage of CPU bit set/get instructions to implement an atomic boolean set
// operation that can return the old value of the boolean. That way we do not
// need to turn off interrupts while manipulating the atomic bool. Functions
// which accept boolean values as input should check for ==0 (false) and != 0
// (true) instead of checking for a specific bit pattern to detect a true value.
// Functions which return a boolean value should return the canonical values
// defined below.
typedef __bool  Bool;
#define true    __true
#define false   __false


// Integer types
typedef signed char         Int8;
typedef unsigned char       UInt8;
typedef short               Int16;
typedef unsigned short      UInt16;
typedef long                Int32;
typedef unsigned long       UInt32;
typedef long long           Int64;
typedef unsigned long long  UInt64;

// Integer types which represent the natural word size of the processor.
// These are the preferred integer types. You should only use integer
// types with specific bit-width guarantees (eg Int8) if there is a clear
// technical reason that you want this specific size over another size.
#if __LP64__
typedef Int64               Int;
typedef UInt64              UInt;
#elif __ILP32__
typedef Int32               Int;
typedef UInt32              UInt;
#else
#error "don't know how to define Int and UInt"
#endif

typedef float               Float32;
typedef double              Float64;
typedef struct _Float96 {
    UInt32  words[3];   // 12 bytes in memory (M68000+)
} Float96;


// Character / String types
typedef char                Character;


#define INT8_MIN    -128
#define INT8_MAX     127
#define INT16_MIN   -32768
#define INT16_MAX    32767
#define INT32_MIN   -2147483648l
#define INT32_MAX    2147483647l
#define INT64_MIN   -9223372036854775808ll
#define INT64_MAX    9223372036854775807ll;

#define UINT8_MIN   0u
#define UINT8_MAX   255u
#define UINT16_MIN  0u
#define UINT16_MAX  65535u
#define UINT32_MIN  0ul
#define UINT32_MAX  4294967295ul
#define UINT64_MIN  0ull
#define UINT64_MAX  18446744073709551615ull;

#if __LP64__
#define INT_MIN     INT64_MIN
#define INT_MAX     INT64_MAX
#define UINT_MIN    UINT64_MIN
#define UINT_MAX    UINT64_MAX
#elif __ILP32__
#define INT_MIN     INT32_MIN
#define INT_MAX     INT32_MAX
#define UINT_MIN    UINT32_MIN
#define UINT_MAX    UINT32_MAX
#endif

#if __LP64__
#define BYTE_PTR_MIN    ((Byte*)0LL)
#define BYTE_PTR_MAX    ((Byte*)0xffffffffffffffffLL)
#elif __ILP32__
#define BYTE_PTR_MIN    ((Byte*)0L)
#define BYTE_PTR_MAX    ((Byte*)0xffffffffL)
#else
#error "don't know how to define Byte* min and max values"
#endif


// A callback function that takes a single (context) pointer argument
typedef void (* _Nonnull Closure1Arg_Func)(Byte* _Nullable pContext);


// Basics
#define SIZE_GB(x)  ((UInt)(x) * 1024ul * 1024ul * 1024ul)
#define SIZE_MB(x)  ((Int)(x) * 1024 * 1024)
#define SIZE_KB(x)  ((Int)(x) * 1024)

#define abs(x) (((x) < 0) ? -(x) : (x))
#define min(x, y) (((x) < (y) ? (x) : (y)))
#define max(x, y) (((x) > (y) ? (x) : (y)))

#define __ROUND_UP_TO_POWER_OF_2(x, mask)   (((x) + (mask)) & ~(mask))
#define __ROUND_DOWN_TO_POWER_OF_2(x, mask) ((x) & ~(mask))

#if __LP64__
#define align_up_byte_ptr(x, a)     (Byte*)(__ROUND_UP_TO_POWER_OF_2((UInt64)(x), (UInt64)(a) - 1))
#elif __ILP32__
#define align_up_byte_ptr(x, a)     (Byte*)(__ROUND_UP_TO_POWER_OF_2((UInt32)(x), (UInt32)(a) - 1))
#else
#error "don't know how to define align_up_byte_ptr()"
#endif
#if __LP64__
#define align_down_byte_ptr(x, a)     (Byte*)(__ROUND_DOWN_TO_POWER_OF_2((UInt64)(x), (UInt64)(a) - 1))
#elif __ILP32__
#define align_down_byte_ptr(x, a)     (Byte*)(__ROUND_DOWN_TO_POWER_OF_2((UInt32)(x), (UInt32)(a) - 1))
#else
#error "don't know how to define align_down_byte_ptr()"
#endif


// Int
extern Int Int_NextPowerOf2(Int n);

#define Int_RoundUpToPowerOf2(x, a)     __ROUND_UP_TO_POWER_OF_2(x, (Int)(a) - 1)
#define Int_RoundDownToPowerOf2(x, a)   __ROUND_DOWN_TO_POWER_OF_2(x, (Int)(a) - 1)


// UInt
extern UInt UInt_NextPowerOf2(UInt n);

#define UInt_RoundUpToPowerOf2(x, a)    __ROUND_UP_TO_POWER_OF_2(x, (UInt)(a) - 1)
#define UInt_RoundDownToPowerOf2(x, a)  __ROUND_DOWN_TO_POWER_OF_2(x, (UInt)(a) - 1)


// Int64
extern const Character* _Nonnull Int64_ToString(Int64 val, Int base, Int fieldWidth, Character paddingChar, Character* _Nonnull pString, Int maxLength);
extern Int Int64_DivMod(Int64 dividend, Int64 divisor, Int64* _Nullable quotient, Int64* _Nullable remainder);


// UInt64
extern const Character* _Nonnull UInt64_ToString(UInt64 val, Int base, Int fieldWidth, Character paddingChar, Character* _Nonnull pString, Int maxLength);


// String
extern Bool String_Equals(const Character* _Nonnull pLhs, const Character* _Nonnull pRhs);

#endif /* Types_h */
