//
//  abort.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <stdlib.h>
#include <signal.h>


_Noreturn abort(void)
{
    sigsend(SIG_SCOPE_PROC, 0, SIGABRT);
    for (;;);
}
