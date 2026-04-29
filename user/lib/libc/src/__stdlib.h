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
#include <serena/mtx.h>
#include <serena/process.h>
#include <serena/spinlock.h>

__CPP_BEGIN

typedef void (*at_exit_func_t)(void);


extern const proc_ctx_t* _Nonnull __gProcCtx;

extern size_t  __gEnvironCount;
extern char**  __gInitialEnviron;

extern spinlock_t               __gAtExitLock;
extern at_exit_func_t _Nullable __gAtExitFuncs[ATEXIT_MAX];
extern int                      __gAtExitFuncsCount;
extern volatile bool            __gIsExiting;



extern void __stdlibc_init(proc_ctx_t* _Nonnull argsp);
extern void __malloc_init(void);
extern void __locale_init(void);
extern void __proc_init(void);
extern void __stdio_init(void);
extern void __env_init(size_t envc, char ** initial_envp);


// environ
extern char* _Nullable __getenv(const char *_Nonnull name, size_t nMaxChars, ssize_t* _Nonnull idx);

__CPP_END

#endif /* ___STDLIB_H */
