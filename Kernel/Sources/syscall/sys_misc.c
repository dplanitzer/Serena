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

SYSCALL_4(excpt_sethandler, int scope, int flags, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    switch (pa->scope) {
        case EXCPT_SCOPE_VCPU:
            if (pa->old_handler) {
                *(pa->old_handler) = vp->excpt_handler;
            }
            vp->excpt_handler = *(pa->handler);
            break;

        case EXCPT_SCOPE_PROC:
            Process_SetExceptionHandler(vp->proc, pa->handler, pa->old_handler);
            break;

        default:
            err = EINVAL;
            break;
    }

    return err;
}
