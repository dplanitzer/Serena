//
//  excpt_sethandler.c
//  libc
//
//  Created by Dietmar Planitzer on 7/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/exception.h>
#include <kpi/syscall.h>


int excpt_sethandler(int flags, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler)
{
    return (int)_syscall(SC_excpt_sethandler, flags, handler, old_handler);
}
