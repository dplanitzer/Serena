//
//  Foundation.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Foundation_h
#define Foundation_h

#ifndef __has_feature
#define __has_feature(x) 0
#endif

#if !__has_feature(nullability)
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Nonnull
#define _Nonnull
#endif
#endif


// The Byte type represents raw, untyped memory. Raw memory may be reinterpreted
// or converted to typed memory but this requires the execution of some piece of
// code that knows how to interpret or rearrange the bits in untyped memory to
// make them conforming to the semantics of the desired type.
typedef unsigned char Byte;


// Boolean type
typedef unsigned char   Bool;
#define true    0x01
#define false   0x00


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
#elif __LP32__
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
#define INT32_MIN   -2147483648
#define INT32_MAX    2147483647
#define INT64_MIN   -9223372036854775808LL
#define INT64_MAX    9223372036854775807LL;

#define UINT8_MIN   0
#define UINT8_MAX   255
#define UINT16_MIN  0
#define UINT16_MAX  65535
#define UINT32_MIN  0
#define UINT32_MAX  4294967295
#define UINT64_MIN  0LL
#define UINT64_MAX  18446744073709551615LL;

#if __LP64__
#define INT_MIN     INT64_MIN
#define INT_MAX     INT64_MAX
#define UINT_MIN    UINT64_MIN
#define UINT_MAX    UINT64_MAX
#elif __LP32__
#define INT_MIN     INT32_MIN
#define INT_MAX     INT32_MAX
#define UINT_MIN    UINT32_MIN
#define UINT_MAX    UINT32_MAX
#endif

#define NULL        ((void*)0)

#if __LP64__
#define BYTE_PTR_MIN    ((Byte*)0LL)
#define BYTE_PTR_MAX    ((Byte*)0xffffffffffffffffLL)
#elif __LP32__
#define BYTE_PTR_MIN    ((Byte*)0L)
#define BYTE_PTR_MAX    ((Byte*)0xffffffffL)
#else
#error "don't know how to define Byte* min and max values"
#endif


// Basics
#define abs(x) (((x) < 0) ? -(x) : (x))
#define min(x, y) (((x) < (y) ? (x) : (y)))
#define max(x, y) (((x) > (y) ? (x) : (y)))

#define __ALIGN_MASK(x, mask)   (((x) + (mask)) & ~(mask))
#define align_int(x, a)             __ALIGN_MASK(x, (Int)(a) - 1)
#define align_uint(x, a)            __ALIGN_MASK(x, (UInt)(a) - 1)
#if __LP64__
#define align_byte_ptr(x, a)        (Byte*)(__ALIGN_MASK((UInt64)(x), (UInt64)(a) - 1))
#elif __LP32__
#define align_byte_ptr(x, a)        (Byte*)(__ALIGN_MASK((UInt32)(x), (UInt32)(a) - 1))
#else
#error "don't know how to define align_byte_ptr()"
#endif

#define SIZE_GB(x)  ((Int)(x) * 1024 * 1024 * 1024)
#define SIZE_MB(x)  ((Int)(x) * 1024 * 1024)
#define SIZE_KB(x)  ((Int)(x) * 1024)


// Variable argument lists
typedef unsigned char *va_list;

#define __va_align(type) (__alignof(type)>=4?__alignof(type):4)
#define __va_do_align(vl,type) ((vl)=(unsigned char *)((((unsigned int)(vl))+__va_align(type)-1)/__va_align(type)*__va_align(type)))
#define __va_mem(vl,type) (__va_do_align((vl),type),(vl)+=sizeof(type),((type*)(vl))[-1])

#define va_start(ap, lastarg) ((ap)=(va_list)(&lastarg+1))
#define va_arg(vl,type) __va_mem(vl,type)
#define va_end(vl) ((vl)=0)
#define va_copy(new,old) ((new)=(old))


// Macros to detect errors and to jump to teh 'failed:' label if an error is detected.
#define FailErr(x)      if ((x) != EOK) goto failed
#define FailFalse(p)    if (!(p)) goto failed
#define FailNULL(p)     if ((p) == NULL) goto failed
#define FailZero(p)     if ((p) == 0) goto failed


// The error codes
typedef Int ErrorCode;
#define EOK         0
#define ENOMEM      1
#define ENODATA     2
#define ENOTCDF     3
#define ENOBOOT     4
#define ENODRIVE    5
#define EDISKCHANGE 6
#define ETIMEOUT    7
#define ENODEVICE   8
#define EPARAM      9
#define ERANGE      10
#define EINTR       11
#define EAGAIN      12
#define EPIPE       13
#define EBUSY       14


// Int
extern Int Int_NextPowerOf2(Int n);


// UInt
extern UInt UInt_NextPowerOf2(UInt n);


// Int64
extern const Character* _Nonnull Int64_ToString(Int64 val, Int base, Int fieldWidth, Character paddingChar, Character* _Nonnull pString, Int maxLength);
extern ErrorCode Int64_DivMod(Int64 dividend, Int64 divisor, Int64* _Nullable quotient, Int64* _Nullable remainder);


// UInt64
extern const Character* _Nonnull UInt64_ToString(UInt64 val, Int base, Int fieldWidth, Character paddingChar, Character* _Nonnull pString, Int maxLength);


// Printing
extern void print(const Character* _Nonnull format, ...);


// Asserts
extern void fatalError(const Character* _Nonnull filename, int line);

#define abort() fatalError(__func__, __LINE__)

#if DEBUG
#define assert(cond)   if ((cond) == 0) { abort(); }
#else
#define assert(cond)
#endif

extern void mem_non_recoverable_error(void);

#endif /* Foundation_h */
