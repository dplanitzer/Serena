//
//  __stdlib.h
//  libc
//
//  Created by Dietmar Planitzer on 12/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef ___STDLIB_H
#define ___STDLIB_H 1

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <ext/math.h>
#include <sys/mtx.h>
#include <sys/proc.h>

__CPP_BEGIN

#define AT_EXIT_FUNCS_CAPACITY   32
typedef void (*at_exit_func_t)(void);


extern pargs_t* __gProcessArguments;

extern mtx_t                    __gAtExitLock;
extern at_exit_func_t _Nullable __gAtExitFuncs[AT_EXIT_FUNCS_CAPACITY];
extern int                      __gAtExitFuncsCount;
extern volatile bool            __gAtExitEnabled;



extern void __stdlibc_init(pargs_t* _Nonnull argsp);
extern void __malloc_init(void);
extern void __locale_init(void);
extern void __stdio_init(void);

extern bool __is_pointer_NOT_freeable(void* ptr);

__CPP_END

#endif /* ___STDLIB_H */
