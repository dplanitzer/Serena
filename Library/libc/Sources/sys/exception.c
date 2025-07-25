//
//  exception.c
//  libc
//
//  Created by Dietmar Planitzer on 7/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/exception.h>
#include <kpi/syscall.h>


excpt_handler_t _Nullable excpt_sethandler(int scope, int flags, excpt_handler_t _Nullable handler)
{
    return (excpt_handler_t)_syscall(SC_excpt_sethandler, scope, flags, handler);
}
