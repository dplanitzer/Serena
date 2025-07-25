//
//  sys_misc.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <kpi/exception.h>


SYSCALL_0(coninit)
{
    extern errno_t SwitchToFullConsole(void);

    return SwitchToFullConsole();
}

SYSCALL_4(excpt_sethandler, int scope, int flags, excpt_handler_t _Nullable handler, void* _Nullable arg)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    excpt_handler_t old_handler = NULL;

    switch (pa->scope) {
        case EXCPT_SCOPE_VCPU:
            old_handler = vp->excpt_handler;
            vp->excpt_handler = pa->handler;
            vp->excpt_arg = pa->arg;
            break;

        case EXCPT_SCOPE_PROC:
            old_handler = Process_SetExceptionHandler(vp->proc, pa->handler, pa->arg);
            break;

        default:
            break;
    }

    return (uintptr_t)old_handler;
}
