//
//  _Exit.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <unistd.h>


_Noreturn _Exit(int status)
{
    _exit(status);
}
