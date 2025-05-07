//
//  _astart.c
//  libc
//
//  Created by Dietmar Planitzer on 8/31/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__stddef.h>
#include <stdlib.h>


extern void main_closure(int argc, char *argv[]);

// Serena style process start function. Differences to the standard C start are:
// - invokes main_closure() instead of main().
// - it does not invoke exit() when main_closure() returns.
//
// A process in Serena is a collection of dispatch queues rather than threads.
// Every process starts out with one dispatch queue which is known as the main
// dispatch queue. This is a serial dispatch queue which means that it executes
// one closure after another. Closures are dispatched to dispatch queues and the
// queue executes them as soon as possible. The main_closure() function is the
// first closure that is executed on the main dispatch queue of the process.
// This closure typically takes care of initializing the application
// and it then dispatches one or more additional closures for execution on the
// main dispatch queue. Consequently the start function here does not terminate
// the process when main_closure() returns. Once the process is ready to
// terminate, one of its closures should invoke exit() with a suitable exit code.
void start(os_procargs_t* _Nonnull argsp)
{
    __stdlibc_init(argsp);

    main_closure(argsp->argc, argsp->argv);
}
