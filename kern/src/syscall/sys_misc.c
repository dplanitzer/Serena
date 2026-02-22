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

SYSCALL_3(excpt_sethandler, int flags, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    return vcpu_set_excpt_handler(vp, pa->handler, pa->old_handler);
}

SYSCALL_0(test)
{
    return 0;
}
