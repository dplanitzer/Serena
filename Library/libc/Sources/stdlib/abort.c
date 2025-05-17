//
//  abort.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <stdlib.h>


_Noreturn abort(void)
{
    _Exit(EXIT_FAILURE);
}
