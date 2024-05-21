//
//  makedir.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "cmdlib.h"
#include <stdio.h>
#include <stdlib.h>



int cmd_makedir(ShellContextRef _Nonnull pContext, int argc, char** argv)
{
    decl_try_err();
    const char* path = (argc > 1) ? argv[1] : "";

    if (*path == '\0') {
        printf("%s: expected a path\n", argv[0]);
        return EXIT_FAILURE;
    }

    err = Directory_Create(path, 0755);
    if (err != EOK) {
        print_error(argv[0], path, err);
    }

    return exit_code(err);
}
