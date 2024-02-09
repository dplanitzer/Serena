//
//  Types.h
//  Apollo
//
//  Created by Dietmar Planitzer on 7/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Types_h
#define Types_h

#include <abi/_align.h>
#include <abi/_bool.h>
#include <abi/_dmdef.h>
#include <abi/_kbidef.h>
#include <abi/_inttypes.h>
#include <abi/_floattypes.h>
#include <abi/_limits.h>
#include <abi/_math.h>
#include <abi/_noreturn.h>
#include <abi/_nulldef.h>
#include <abi/_offsetof.h>
#include <abi/_syslimits.h>
#include <abi/_varargs.h>
#include <abi/_weak.h>

#ifndef _Locked
#define _Locked
#endif

// The Byte type represents raw, untyped memory. Raw memory may be reinterpreted
// or converted to typed memory but this requires the execution of some piece of
// code that knows how to interpret or rearrange the bits in untyped memory to
// make them conforming to the semantics of the desired type.
typedef unsigned char Byte;

#define BYTE_PTR_MIN    ((Byte*)0ul)
#define BYTE_PTR_MAX    ((Byte*)__UINTPTR_MAX)


// A ssize_t is a signed integral type that represents a count of bytes. This
// type corresponds in width and semantics to the POSIX ssize_t. A byte count is
// guaranteed that it is as wide as size_t and at least as wide as int/unsigned int.
// Kernel code should exclusively use this type to represent counts of bytes
// because it is safer to use than size_t. Eg since it is signed, subtraction
// is actually commutative (whereas it is not with size_t since small - large
// may cause a wrap around while large - small with the same values won't).
typedef __ssize_t ssize_t;

#define SSIZE_MIN  __SSIZE_MIN
#define SSIZE_MAX  __SSIZE_MAX
#define SSIZE_WIDTH __SSIZE_WIDTH

// Convert a size_t to a ssize_t with clamping
#define __SSizeByClampingSize(ub) (ssize_t)__min(ub, SSIZE_MAX)


// A size_t is an unsigned integral type that represents a count of bytes.
// This type should be exclusively used in the interface between the kernel and
// user space to represent the kernel side of a C99 size_t type. However this
// type should be immediately converted to a ssize_t with proper clamping or
// overflow check and the kernel code should then use ssize_t internally.
typedef __size_t size_t;

#define SIZE_MAX __SIZE_MAX
#define SIZE_WIDTH __SIZE_WIDTH


// Various Kernel API types
typedef int         ProcessId;

typedef int32_t     FilesystemId;
typedef int32_t     InodeId;    // XXX should probably be 64bit

typedef uint16_t    FilePermissions;
typedef int8_t      FileType;
typedef int64_t     FileOffset;

typedef uint32_t    UserId;
typedef uint32_t    GroupId;

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
// UserId       <= int
// GroupId      <= int
// errno_t      <= int
// ProcessId    <= int
// FilesystemId <= int


// Size constructors
#define SIZE_GB(x)  ((unsigned long)(x) * 1024ul * 1024ul * 1024ul)
#define SIZE_MB(x)  ((long)(x) * 1024 * 1024)
#define SIZE_KB(x)  ((long)(x) * 1024)


// int
extern int Int_NextPowerOf2(int n);


// unsigned int
extern unsigned int UInt_NextPowerOf2(unsigned int n);


// int32_t
// 'pBuffer' must be at least DIGIT_BUFFER_CAPACITY characters long
extern const char* _Nonnull Int32_ToString(int32_t val, int radix, bool isUppercase, char* _Nonnull pBuffer);


// uint32_t
// 'pBuffer' must be at least DIGIT_BUFFER_CAPACITY characters long
extern const char* _Nonnull UInt32_ToString(uint32_t val, int base, bool isUppercase, char* _Nonnull pBuffer);


// int64_t
// 'pBuffer' must be at least DIGIT_BUFFER_CAPACITY characters long
extern const char* _Nonnull Int64_ToString(int64_t val, int radix, bool isUppercase, char* _Nonnull pBuffer);


// uint64_t
// 'pBuffer' must be at least DIGIT_BUFFER_CAPACITY characters long
extern const char* _Nonnull UInt64_ToString(uint64_t val, int base, bool isUppercase, char* _Nonnull pBuffer);


// String
extern ssize_t String_Length(const char* _Nonnull pStr);
extern ssize_t String_LengthUpTo(const char* _Nonnull pStr, ssize_t strsz);

extern char* _Nonnull String_Copy(char* _Nonnull pDst, const char* _Nonnull pSrc);
extern char* _Nonnull String_CopyUpTo(char* _Nonnull pDst, const char* _Nonnull pSrc, ssize_t count);

extern bool String_Equals(const char* _Nonnull pLhs, const char* _Nonnull pRhs);
extern bool String_EqualsUpTo(const char* _Nonnull pLhs, const char* _Nonnull pRhs, ssize_t count);


// Required minimum size is (string length byte + sign byte + longest digit sequence + 1 NUL byte) -> 1 + 64 (binary 64bit) + 1 + 1 = 25 bytes
// A digit string is generated in a canonical representation: string length, sign, digits ..., NUL
#define DIGIT_BUFFER_CAPACITY 67

// 'buf' must be at least DIGIT_BUFFER_CAPACITY bytes big
extern char* _Nonnull __i32toa(int32_t val, int radix, bool isUppercase, char* _Nonnull digits);
extern char* _Nonnull __i64toa(int64_t val, int radix, bool isUppercase, char* _Nonnull digits);

// 'buf' must be at least DIGIT_BUFFER_CAPACITY bytes big
// 'radix' must be 8, 10 or 16
extern char* _Nonnull __ui32toa(uint32_t val, int radix, bool isUppercase, char* _Nonnull digits);
extern char* _Nonnull __ui64toa(uint64_t val, int radix, bool isUppercase, char* _Nonnull digits);

extern int atoi(const char *str, char **str_end, int base);

#endif /* Types_h */
