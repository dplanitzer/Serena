//
//  abort.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <sys/exception.h>

_Noreturn void abort(void)
{
    excpt_raise(EXCPT_FORCED_ABORT, NULL);
    /* NOT REACHED */
}
