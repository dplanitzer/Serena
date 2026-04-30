//
//  __stdlib_init.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__stdlib.h>
#include <stdio.h>
#include <string.h>
#include <kpi/kei.h>
#include <serena/mtx.h>
#include <vcpu/__vcpu.h>

const proc_ctx_t* _Nonnull __gProcCtx;
kei_func_t*     __gKeiTab;

spinlock_t               __gAtExitLock;
at_exit_func_t _Nullable __gAtExitFuncs[ATEXIT_MAX];
int                      __gAtExitFuncsCount;
volatile bool            __gIsExiting;

mtx_t __g_dtoa_mtx[2];


void __stdlibc_init(proc_ctx_t* _Nonnull pctx)
{
    __gProcCtx = pctx;


    const proc_aux_entry_t* ae = pctx->aux;
    for (;;) {
        if (ae->type == AT_END) {
            break;
        }
        else if (ae->type == AT_KEI) {
            __gKeiTab = ae->u.p;
        }
        
        ae++;
    }
    if (__gKeiTab == NULL) {
        for(;;);    // just hang
    }


    __gAtExitFuncsCount = 0;
    __gIsExiting = false;
    __gAtExitLock = SPINLOCK_INIT;

    mtx_init(&__g_dtoa_mtx[0]);
    mtx_init(&__g_dtoa_mtx[1]);

    __env_init(pctx->envc, pctx->envv);
    __vcpu_init();
    __malloc_init();
    __locale_init();
    __proc_init();
    __stdio_init();
}
