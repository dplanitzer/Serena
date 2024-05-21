//
//  cls.c
//  sh
//
//  Created by Dietmar Planitzer on 4/4/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include <stdio.h>
#include <stdlib.h>


int cmd_cls(ShellContextRef _Nonnull pContext, int argc, char** argv)
{
    printf("\033[2J\033[H");
    return EXIT_SUCCESS;
}
