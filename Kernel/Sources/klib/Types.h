//
//  Types.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Types_h
#define Types_h

#include <System/abi/_dmdef.h>
#include <System/abi/_floattypes.h>
#include <System/abi/_limits.h>
#include <System/_align.h>
#include <System/_cmndef.h>
#include <System/_math.h>
#include <System/_noreturn.h>
#include <System/_offsetof.h>
#include <System/_syslimits.h>
#include <System/_varargs.h>
#include <System/Types.h>

#ifndef _Locked
#define _Locked
#endif

#define CHAR_PTR_MAX    ((char*)__UINTPTR_MAX)



// Convert a size_t to a ssize_t with clamping
#define __SSizeByClampingSize(ub) (ssize_t)__min(ub, SSIZE_MAX)

enum {
    kRootUserId = 0,
    kRootGroupId = 0
};

typedef struct _User {
    UserId  uid;
    GroupId gid;
} User;


// A callback function that takes a single (context) pointer argument
typedef void (* _Nonnull Closure1Arg_Func)(void* _Nullable pContext);


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
