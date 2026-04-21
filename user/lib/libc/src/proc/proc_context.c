//
//  proc_context.c
//  libc
//
//  Created by Dietmar Planitzer on 4/21/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <__stdlib.h>
#include <serena/process.h>


const proc_ctx_t* _Nonnull proc_context(void)
{
    return __gProcCtx;
}
