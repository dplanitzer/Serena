//
//  __stdlib.h
//  libc
//
//  Created by Dietmar Planitzer on 12/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef ___STDLIB_H
#define ___STDLIB_H 1

#include <limits.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <ext/math.h>
#include <sys/mtx.h>
#include <sys/proc.h>
#include <sys/spinlock.h>

__CPP_BEGIN

typedef void (*at_exit_func_t)(void);


extern pargs_t* __gProcessArguments;

extern spinlock_t               __gAtExitLock;
extern at_exit_func_t _Nullable __gAtExitFuncs[ATEXIT_MAX];
extern int                      __gAtExitFuncsCount;
extern volatile bool            __gIsExiting;



extern void __stdlibc_init(pargs_t* _Nonnull argsp);
extern void __malloc_init(void);
extern void __locale_init(void);
extern void __stdio_init(void);

extern bool __is_pointer_NOT_freeable(void* ptr);


// environ
extern char* _Nullable __getenv(const char *_Nonnull name, size_t nMaxChars, ssize_t* _Nonnull idx);

__CPP_END

#endif /* ___STDLIB_H */
