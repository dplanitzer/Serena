//
//  misc.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"


SYSCALL_1(dispose, int od)
{
    ProcessRef pp = vp->proc;

    return UResourceTable_DisposeResource(&pp->uResourcesTable, pa->od);
}

SYSCALL_0(coninit)
{
    extern errno_t SwitchToFullConsole(void);

    return SwitchToFullConsole();
}
