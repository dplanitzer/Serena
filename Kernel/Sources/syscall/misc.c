//
//  misc.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"


SYSCALL_3(ioctl, int fd, int cmd, va_list _Nullable ap)
{
    return Process_Iocall((ProcessRef)p, pa->fd, pa->cmd, pa->ap);
}

SYSCALL_1(dispose, int od)
{
    return Process_DisposeUResource((ProcessRef)p, pa->od);
}

SYSCALL_0(coninit)
{
    extern errno_t SwitchToFullConsole(void);

    return SwitchToFullConsole();
}
