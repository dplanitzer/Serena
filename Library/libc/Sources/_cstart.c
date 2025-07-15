//
//  _cstart.c
//  libc
//
//  Created by Dietmar Planitzer on 8/31/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__stddef.h>
#include <stdlib.h>
#include <sys/proc.h>


extern int main(int argc, char *argv[]);

// start() that implements the standard C runtime startup function.
void start(pargs_t* _Nonnull argsp)
{
    __stdlibc_init(argsp);

    exit(main(argsp->argc, argsp->argv));
}
