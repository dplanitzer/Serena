//
//  exit.c
//  sh
//
//  Created by Dietmar Planitzer on 3/19/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include <stdio.h>
#include <stdlib.h>



int cmd_exit(ShellContextRef _Nonnull pContext, int argc, char** argv)
{
    exit((argc > 1) ? atoi(argv[1]) : EXIT_SUCCESS);
    /* NOT REACHED */
    return 0;
}
