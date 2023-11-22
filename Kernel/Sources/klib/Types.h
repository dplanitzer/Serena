//
//  Types.h
//  Apollo
//
//  Created by Dietmar Planitzer on 7/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Types_h
#define Types_h

#include <_align.h>
#include <_booldef.h>
#include <_dmdef.h>
#include <_kbidef.h>
#include <_math.h>
#include <_nulldef.h>
#include <_syslimits.h>
#include <_varargs.h>

#ifndef _Weak
#define _Weak
#endif

#ifndef _Noreturn
#define _Noreturn   void
#endif

#ifdef __VBCC__
#define offsetof(type, member) __offsetof(type, member)
#else
#define offsetof(type, member) \
    ((ByteCount)((char *)&((type *)0)->member - (char *)0))
#endif

// The Byte type represents raw, untyped memory. Raw memory may be reinterpreted
// or converted to typed memory but this requires the execution of some piece of
// code that knows how to interpret or rearrange the bits in untyped memory to
// make them conforming to the semantics of the desired type.
typedef unsigned char Byte;

#define BYTE_PTR_MIN    ((Byte*)0ul)
#define BYTE_PTR_MAX    ((Byte*)__UINTPTR_MAX)


// A ByteCount is a signed integral type that represents a count of bytes. This
// type corresponds in width and semantics to the POSIX ssize_t. A byte count is
// guaranteed that it is as wide as UByteCount and at least as wide as Int/UInt.
// Kernel code should exclusively use this type to represent counts of bytes
// because it is safer to use than UByteCount. Eg since it is signed, subtraction
// is actually commutative (whereas it is not with UByteCount since snall - large
// may cause a wrap around while large - small with the same values won't).
typedef __ssize_t ByteCount;

#define BYTE_COUNT_MIN  __SSIZE_MIN
#define BYTE_COUNT_MAX  __SSIZE_MAX
#define BYTE_COUNT_WIDTH __SSIZE_WIDTH

// Convert a UByteCount to a ByteCount with clamping
#define __ByteCountByClampingUByteCount(ub) (ByteCount)__min(ub, BYTE_COUNT_MAX)


// A UByteCount is an unsigned integral type that represents a count of bytes.
// This type should be exclusively used in the interface between the kernel and
// user space to represent the kernel side of a C99 size_t type. However this
// type should be immediately converted to a ByteCount with proper clamping or
// overflow check and the kernel code should then use ByteCount internally.
typedef __size_t UByteCount;

#define UBYTE_COUNT_MIN 0ul
#define UBYTE_COUNT_MAX __SIZE_MAX
#define UBYTE_COUNT_WIDTH __SIZE_WIDTH

#if BYTE_COUNT_WIDTH != UBYTE_COUNT_WIDTH
    #error("ByteCount and UByteCount must have the same width")
#endif


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


// Signed integer types
typedef signed char         Int8;
typedef short               Int16;
typedef long                Int32;
typedef long long           Int64;

#define INT8_MIN    -128
#define INT8_MAX     127
#define INT16_MIN   -32768
#define INT16_MAX    32767
#define INT32_MIN   -2147483648l
#define INT32_MAX    2147483647l
#define INT64_MIN   -9223372036854775808ll
#define INT64_MAX    9223372036854775807ll;


// Unsigned integer types
typedef unsigned char       UInt8;
typedef unsigned short      UInt16;
typedef unsigned long       UInt32;
typedef unsigned long long  UInt64;

#define UINT8_MIN   0u
#define UINT8_MAX   255u
#define UINT16_MIN  0u
#define UINT16_MAX  65535u
#define UINT32_MIN  0ul
#define UINT32_MAX  4294967295ul
#define UINT64_MIN  0ull
#define UINT64_MAX  18446744073709551615ull;


// Integer types which represent the natural word size of the processor.
// These are the preferred integer types. You should only use integer
// types with specific bit-width guarantees (eg Int8) if there is a clear
// technical reason that you want this specific size over another size.
#if __LP64__
typedef Int64   Int;
typedef UInt64  UInt;

#define INT_MIN     INT64_MIN
#define INT_MAX     INT64_MAX
#define UINT_MIN    UINT64_MIN
#define UINT_MAX    UINT64_MAX
#elif __ILP32__
typedef Int32   Int;
typedef UInt32  UInt;

#define INT_MIN     INT32_MIN
#define INT_MAX     INT32_MAX
#define UINT_MIN    UINT32_MIN
#define UINT_MAX    UINT32_MAX
#else
#error "don't know how to define Int and UInt"
#endif


// Floating-point types
typedef float   Float32;
typedef double  Float64;
typedef struct _Float96 {
    UInt32  words[3];   // 12 bytes in memory (M68000+)
} Float96;


// Character / String types
typedef char                Character;


// Various Kernel API types
typedef Int     FilesystemId;
typedef Int     ProcessId;
typedef UInt32  FilePermissions;
typedef UInt32  UserId;
typedef UInt32  GroupId;

enum {
    kRootUserId = 0,
    kRootGroupId = 0
};

typedef struct _User {
    UserId  uid;
    GroupId gid;
} User;


// A callback function that takes a single (context) pointer argument
typedef void (* _Nonnull Closure1Arg_Func)(Byte* _Nullable pContext);


// Minimum size for types
// (Spelled out in a comment because C is stupid and doesn't give us a simple & easy
// way to statically compare the size of two types. Supporting that would be just
// too easy you know and the language only has had like 50 years to mature...)
// 
// UserId       <= Int
// GroupId      <= Int
// ErrorCode    <= Int
// ProcessId    <= Int
// FilesystemId <= Int


// Size constructors
#define SIZE_GB(x)  ((UInt)(x) * 1024ul * 1024ul * 1024ul)
#define SIZE_MB(x)  ((Int)(x) * 1024 * 1024)
#define SIZE_KB(x)  ((Int)(x) * 1024)


// Int
extern Int Int_NextPowerOf2(Int n);


// UInt
extern UInt UInt_NextPowerOf2(UInt n);


// Int64
extern const Character* _Nonnull Int64_ToString(Int64 val, Int base, Int fieldWidth, Character paddingChar, Character* _Nonnull pString, Int maxLength);
extern Int Int64_DivMod(Int64 dividend, Int64 divisor, Int64* _Nullable quotient, Int64* _Nullable remainder);


// UInt64
extern const Character* _Nonnull UInt64_ToString(UInt64 val, Int base, Int fieldWidth, Character paddingChar, Character* _Nonnull pString, Int maxLength);


// String
extern ByteCount String_Length(const Character* _Nonnull pStr);
extern ByteCount String_LengthUpTo(const Character* _Nonnull pStr, ByteCount strsz);

extern Character* _Nonnull String_Copy(Character* _Nonnull pDst, const Character* _Nonnull pSrc);
extern Character* _Nonnull String_CopyUpTo(Character* _Nonnull pDst, const Character* _Nonnull pSrc, ByteCount count);

extern Bool String_Equals(const Character* _Nonnull pLhs, const Character* _Nonnull pRhs);
extern Bool String_EqualsUpTo(const Character* _Nonnull pLhs, const Character* _Nonnull pRhs, ByteCount count);

#endif /* Types_h */
