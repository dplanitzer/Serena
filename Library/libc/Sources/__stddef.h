//
//  __stddef.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef ___STDDEF_H
#define ___STDDEF_H 1

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <ext/math.h>
#if ___STDC_HOSTED__ == 1
#include <sys/proc.h>
#endif

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

extern void __stdlibc_init(pargs_t* _Nonnull argsp);
extern void __malloc_init(void);
extern void __locale_init(void);
extern void __exit_init(void);
extern void __stdio_init(void);

extern bool __is_pointer_NOT_freeable(void* ptr);

#endif

#endif /* ___STDDEF_H */
