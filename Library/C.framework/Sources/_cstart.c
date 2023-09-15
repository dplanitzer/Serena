//
//  _cstart.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/31/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__stddef.h>
#include <_kbidef.h>
#include <stdlib.h>


extern int main(int argc, char *argv[]);

// start() that implements the standard C semantics.
void start(struct __process_argument_descriptor_t* _Nonnull argsp)
{
    __stdlibc_init();

    exit(main(argsp->argc, argsp->argv));
}