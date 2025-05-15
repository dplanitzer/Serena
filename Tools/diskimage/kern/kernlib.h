//
//  kern/kernlib.h
//  diskimage
//
//  Created by Dietmar Planitzer on 5/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_KERNLIB_H
#define _KERN_KERNLIB_H 1

#include <System/_align.h>
#include <System/_cmndef.h>
#include <System/_math.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

__CPP_BEGIN

//
// NOTE: All kernel related code must be compiled with a pure C environment. Eg
// no POSIX extensions (since those extensions would collide with our own POSIX-
// like definitions).
//
// Windows: compile with /std:c11
//

//XXX should find a way to share with sys or include the platform definitions
#define R_OK    1
#define W_OK    2
#define X_OK    4
#define F_OK    0

#define O_RDONLY    0x0001
#define O_WRONLY    0x0002
#define O_RDWR      (O_RDONLY | O_WRONLY)
#define O_APPEND    0x0004
#define O_EXCL      0x0008
#define O_TRUNC     0x0010
#define O_NONBLOCK  0x0020

#define SEEK_SET    0   /* Set the file position to 'offset' */
#define SEEK_CUR    1   /* Add 'offset' to the current file position */
#define SEEK_END    2   /* Add 'offset' to the end of the file */

#define IOResourceCommand(__cmd) (__cmd)
#define IOChannelCommand(__cmd) -(__cmd)
#define IsIOChannelCommand(__cmd) ((__cmd) < 0)

// Returns the type of an I/O channel. The type indicates to which kind of
// I/O resource the channel is connected and thus which kind of operations are
// supported by the channel.
#define kIOChannelCommand_GetType   IOChannelCommand(1)
typedef enum IOChannelType {
    kIOChannelType_Terminal,
    kIOChannelType_File,
    kIOChannelType_Directory,
    kIOChannelType_Pipe,
    kIOChannelType_Driver,
    kIOChannelType_Filesystem,
    kIOChannelType_Process,
} IOChannelType;

// Returns the mode with which the I/O channel was opened.
// unsigned int get_mode(int ioc)
#define kIOChannelCommand_GetMode   IOChannelCommand(2)

// Updates the mode of an I/O channel. Enables 'mode' on the channel if 'setOrClear'
// is != 0 and disables 'mode' if 'setOrClear' == 0.
// The following modes may be changed:
// - O_APPEND
// - O_NONBLOCK
// errno_t set_mode(int ioc, int setOrClear, unsigned int mode)
#define kIOChannelCommand_SetMode   IOChannelCommand(3)
//XXX


#define ONE_SECOND_IN_NANOS (1000l * 1000l * 1000l)
#define kQuantums_Infinity      INT32_MAX
#define kQuantums_MinusInfinity INT32_MIN


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


extern bool ul_ispow2(unsigned long n);
extern bool ull_ispow2(unsigned long long n);

#define u_ispow2(__n)\
ul_ispow2((unsigned long)__n)

#if __SIZE_WIDTH == 32
#define siz_ispow2(__n) \
ul_ispow2((unsigned long)__n)
#elif __SIZE_WIDTH == 64
#define siz_ispow2(__n) \
ull_ispow2((unsigned long long)__n)
#else
#error "unable to define siz_ispow2()"
#endif


extern unsigned long ul_pow2_ceil(unsigned long n);
extern unsigned long long ull_pow2_ceil(unsigned long long n);

#define u_pow2_ceil(__n)\
(unsigned int)ul_pow2_ceil((unsigned long)__n)

#if __SIZE_WIDTH == 32
#define siz_pow2_ceil(__n) \
(size_t)ul_pow2_ceil((unsigned long)__n)
#elif __SIZE_WIDTH == 64
#define siz_pow2_ceil(__n) \
(size_t)ull_pow2_ceil((unsigned long long)__n)
#else
#error "unable to define siz_pow2_ceil()"
#endif


extern unsigned int ul_log2(unsigned long n);
extern unsigned int ull_log2(unsigned long long n);

#define u_log2(__n)\
ul_log2((unsigned long)__n)

#if __SIZE_WIDTH == 32
#define siz_log2(__n) \
ul_log2((unsigned long)__n)
#elif __SIZE_WIDTH == 64
#define siz_log2(__n) \
ull_log2((unsigned long long)__n)
#else
#error "unable to define siz_log2()"
#endif


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


// Required minimum size is (string length byte + sign byte + longest digit sequence + 1 NUL byte) -> 1 + 64 (binary 64bit) + 1 + 1 = 25 bytes
// A digit string is generated in a canonical representation: string length, sign, digits ..., NUL
#define DIGIT_BUFFER_CAPACITY 67

// 'buf' must be at least DIGIT_BUFFER_CAPACITY bytes big
extern char* _Nonnull __i32toa(int32_t val, char* _Nonnull digits);
extern char* _Nonnull __i64toa(int64_t val, char* _Nonnull digits);

// 'buf' must be at least DIGIT_BUFFER_CAPACITY bytes big
// 'radix' must be 8, 10 or 16
extern char* _Nonnull __ui32toa(uint32_t val, int radix, bool isUppercase, char* _Nonnull digits);
extern char* _Nonnull __ui64toa(uint64_t val, int radix, bool isUppercase, char* _Nonnull digits);

extern int _atoi(const char *str, char **str_end, int base);

__CPP_END

#endif /* _KERN_KERNLIB_H */
