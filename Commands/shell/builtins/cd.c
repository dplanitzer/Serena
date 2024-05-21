//
//  cd.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "cmdlib.h"
#include <stdio.h>
#include <stdlib.h>


int cmd_cd(ShellContextRef _Nonnull pContext, int argc, char** argv)
{
    decl_try_err();
    const char* path = (argc > 1) ? argv[1] : "";
    
    if (*path == '\0') {
        printf("%s: expected a path.\n", argv[0]);
        return EXIT_FAILURE;
    }

    err = Process_SetWorkingDirectory(path);
    if (err != EOK) {
        print_error(argv[0], path, err);
    }

    return exit_code(err);
}
