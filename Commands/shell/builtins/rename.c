//
//  rename.c
//  sh
//
//  Created by Dietmar Planitzer on 5/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "cmdlib.h"
#include <stdio.h>
#include <stdlib.h>



int cmd_rename(ShellContextRef _Nonnull pContext, int argc, char** argv)
{
    decl_try_err();
    const char* oldPath = (argc > 1) ? argv[1] : "";
    const char* newPath = (argc > 2) ? argv[2] : "";

    if (*oldPath == '\0') {
        printf("%s: expected a path.\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (*newPath == '\0') {
        printf("%s: expected a destination path.\n", argv[0]);
        return EXIT_FAILURE;
    }


    err = File_Rename(oldPath, newPath);
    if (err != EOK) {
        print_error(argv[0], oldPath, err);
    }

    return exit_code(err);
}
