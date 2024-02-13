//
//  cd.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int cmd_cd(InterpreterRef _Nonnull self, int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "";
    
    if (*path == '\0') {
        printf("%s: expected a path.\n", argv[0]);
        return EXIT_FAILURE;
    }

    const errno_t err = Process_SetWorkingDirectory(path);
    if (err != 0) {
        printf("%s: %s.\n", argv[0], strerror(err));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
