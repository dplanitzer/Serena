//
//  _cstart.c
//  libc
//
//  Created by Dietmar Planitzer on 8/31/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__stdlib.h>


extern int main(int argc, char *argv[]);

// start() that implements the standard C runtime startup function.
void start(proc_ctx_t* _Nonnull pctx)
{
    __stdlibc_init(pctx);

    exit(main(pctx->argc, pctx->argv));
}
