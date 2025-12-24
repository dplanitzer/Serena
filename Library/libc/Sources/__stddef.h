//
//  __stddef.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef ___STDDEF_H
#define ___STDDEF_H 1

#include <_math.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#if ___STDC_HOSTED__ == 1
#include <sys/proc.h>
#endif

#define CPU_PAGE_SIZE   4096

#if defined(__ILP32__)
typedef uint32_t uword_t;
#define WORD_SIZE       4
#define WORD_SIZMASK    3
#define WORD_SHIFT      2
#define WORD_FROM_BYTE(b) ((b) << 24) | ((b) << 16) | ((b) << 8) | (b)
#elif defined(__LLP64__) || defined(__LP64__)
typedef uint64_t uword_t;
#define WORD_SIZE       8
#define WORD_SIZMASK    7
#define WORD_SHIFT      3
#define WORD_FROM_BYTE(b) ((b) << 56) | ((b) << 48) | ((b) << 40) | ((b) << 32) | ((b) << 24) | ((b) << 16) | ((b) << 8) | (b)
#else
#error "unknown data model"
#endif


#if ___STDC_HOSTED__ == 1
extern pargs_t* __gProcessArguments;

extern int _divmods64(long long dividend, long long divisor, long long* quotient, long long* remainder);

extern void __stdlibc_init(pargs_t* _Nonnull argsp);
extern void __malloc_init(void);
extern void __locale_init(void);
extern void __exit_init(void);
extern void __stdio_init(void);

extern bool __is_pointer_NOT_freeable(void* ptr);


// Max length of an i32a string: sign char + longest possible digit sequence + NUL character
#define I32A_BUFFER_SIZE    (1 + 32 + 1)

// Max length of an i64a string: sign char + longest possible digit sequence + NUL character
#define I64A_BUFFER_SIZE    (1 + 64 + 1)

typedef struct i32a_t {
    int8_t  length;     // Length of the generated string
    int8_t  offset;     // where in 'buffer' the string starts
    char    buffer[I32A_BUFFER_SIZE];      // Generated characters; right aligned
} i32a_t;

// The i64a_t data type is an extended version of the i32a_t data type that has
// extra room for the required additional digits. So doing a cast of the form
// (i32a_t*)&my_i64a_t_value is valid.
typedef struct i64a_t {
    int8_t  length;     // Length of the generated string
    int8_t  offset;     // where in 'buffer' the string starts
    char    buffer[I64A_BUFFER_SIZE];   // Generated characters; right aligned
} i64a_t;


// Controls how the __i32toa and __i64toa functions format the sign
typedef enum ia_sign_format_t {
    ia_sign_minus_only = 0,
    ia_sign_plus_minus
} ia_sign_format_t;


extern char* _Nonnull __i32toa(int32_t val, ia_sign_format_t sign_mode, i32a_t* _Nonnull out);
extern char* _Nonnull __i64toa(int64_t val, ia_sign_format_t sign_mode, i64a_t* _Nonnull out);

// 'radix' must be 8, 10 or 16
extern char* _Nonnull __u32toa(uint32_t val, int radix, bool isUppercase, i32a_t* _Nonnull out);
extern char* _Nonnull __u64toa(uint64_t val, int radix, bool isUppercase, i64a_t* _Nonnull out);

extern int __strtoi64(const char * _Restrict _Nonnull str, char ** _Restrict str_end, int base, long long min_val, long long max_val, int max_digits, long long * _Restrict _Nonnull result);
#endif

#endif /* ___STDDEF_H */
